/*
 * File:
 *   versioned-linkedlist.c
 * Description:
 *   The original Versioned Linked List. This algorithm exploits a versioned try-lock
 *   to achieve optimal concurrency as explained in:
 *   A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang.
 *   arXiv:1502.01633, February 2015 and DISC 2015
 *
 * versioned-linkedlist.c is part of Synchrobench
 *
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "intset.h"
#include "versioned-linkedlist.h"

/* common operations for set algorithms */
#include "mixin.c"

/* wait-free contains */
int set_contains(intset_t *set, val_t val) {
    node_t* curr = set->head;

    while (curr->val < val) {
        curr = curr->next;
    }

    /* if value is present and not logically deleted */
    return (curr->val == val && !curr->deleted);
}

/* short traversal that records version of prev */
static int validate(val_t val, node_t** prev, node_t** curr, verlock_t* prev_version) {
partial_abort:
    *prev_version = get_version(&(*prev)->lock);

    if ((*prev)->deleted){
        /* full abort */
        return false;
    }

    *curr = (*prev)->next;
    while ((*curr)->val < val) {
        *prev_version = get_version(&(*curr)->lock);

        if ((*curr)->deleted){
            goto partial_abort;
        }

        *prev = *curr;
        *curr = (*curr)->next;
    }

    return true;
}

/* wait-free traverse used in updated */
static void traverse(val_t val, node_t** prev, node_t** curr, node_t* start) {
    *prev = start;
    *curr = start;

    /* until position is reached, recording prev node */
    while ((*curr)->val < val) {
        *prev = *curr;
        *curr = (*curr)->next;
    }

}

int set_insert(intset_t *set, val_t val){
    node_t* prev = NULL;
    node_t* curr = NULL;
    node_t* new = NULL;
    verlock_t prev_version;

/* full abort: restart from traversal */
restart_from_traverse:
    traverse(val, &prev, &curr, set->head);

/* partial abort: restart from validate */
restart_from_validate:

    /* pre-locking validation */
    if (!validate(val, &prev, &curr, &prev_version)) {
        /* prev is no longer appropriate for use, traverse again */
        goto restart_from_traverse;
    }

    /* value is logically deleted */
    if (curr->deleted) {
        goto restart_from_traverse;
    }

    /* value already exists in the set */
    if (curr->val == val) {
        return false;
    }

    /* pre-allocate a new node with value */
    if (new == NULL) {
        new = new_node(val, NULL);
    }

    /* pre-link new node to next node */
    new->next = curr;

    /* attempt to lock at validated version */
    if (!try_lock_at_version(&prev->lock, prev_version)) {
        /* version of prev has changed since pre-lock validation, need to validate it again */
        goto restart_from_validate;
    }

    /* link new to prev */
    prev->next = new;

    unlock_and_increment_version(&prev->lock);

    return true;
}

int set_remove(intset_t *set, val_t val) {
    node_t* prev = NULL;
    node_t* curr = NULL;
    verlock_t prev_version;

/* full abort: restart from traversal */
restart_from_traverse:
    traverse(val, &prev, &curr, set->head);

/* partial abort: restart from validate */
restart_from_validate:

    /* pre-locking validation */
    if (!validate(val, &prev, &curr, &prev_version)) {
        /* prev is no longer appropriate for use, traverse again */
        goto restart_from_traverse;
    }

    /* if value is not present or is logically deleted */
    if (curr->val != val || curr->deleted) {
        return false;
    }

    /* attempt to lock at validated version */
    if (!try_lock_at_version(&prev->lock, prev_version)) {
        /* version of prev has changed since pre-lock validation, need to validate it again */
        goto restart_from_validate;
    }

    /* lock node at current version */
    spinlock(&curr->lock);

    /* logical delete */
    curr->deleted = true;

    /* physical delete */
    prev->next = curr->next;

    unlock_and_increment_version(&curr->lock);
    unlock_and_increment_version(&prev->lock);

    return true;
}
