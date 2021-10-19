/*
 * skiplist.c: definitions of the skip list data stucture
 *
 * Author: Ian Dick, 2013
 *
 */

/*

Module overview

This module provides the basic structures used to create the
skip list data structure, as described in:
Crain, T., Gramoli, V., Raynal, M. (2013)
"No Hot-Spot Non-Blocking Skip List", to appear in
The 33rd IEEE International Conference on Distributed Computing
Systems (ICDCS).

One approach to designing a skip list data structure is to have
one kind of node, and to assign each node in the list a level.
A node with level l has l backwards pointers and l forward pointers,
one for each level of the skip list level. Skip lists designed this
way have index level nodes that are implicit, are are handled by
multiple pointers per node.

The approach taken here is to explicitly create the index nodes and
have these as separate structures to regular nodes. The reason for
this is that the background maintenance method used here is much easier
to implement this way.

*/

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "skiplist.h"
#include "background.h"
#include "garbagecoll.h"
#include "ptst.h"

static int gc_id[NUM_SIZES];
static int curr_id;

/* - Public skiplist interface - */

/**
 * node_new - create a new bottom-level node
 * @key: the key for the new node
 * @val: the val for the new node
 * @prev: the prev node pointer for the new node
 * @next: the next node pointer for the new node
 * @level: the level for the new node
 * @ptst: the per-thread state
 *
 * Note: All nodes are originally created with
 * marker set to 0 (i.e. they are unmarked).
 */
node_t* node_new(unsigned long key, void *val, node_t *prev,
                 node_t *next, unsigned int level, ptst_t *ptst)
{
        node_t *node;
        unsigned long i;

        node  = gc_alloc(ptst, gc_id[curr_id]);

        node->key       = key;
        node->val       = val;
        node->prev      = prev;
        node->next      = next;
        node->level     = level;
        node->marker    = 0;
        node->raise_or_remove = 0;

        for (i = 0; i < MAX_LEVELS; i++)
                node->succs[i] = NULL;

        assert (node->next != node);

        return node;
}

/**
 * marker_new - create a new marker node
 * @prev: the prev node pointer for the new node
 * @next: the next node pointer for the new node
 * @ptst: the per-thread state
 */
node_t* marker_new(node_t *prev, node_t *next, ptst_t *ptst)
{
        node_t *node;
        unsigned long i;

        node  = gc_alloc(ptst, gc_id[curr_id]);

        node->key       = 0;
        node->val       = node;
        node->prev      = prev;
        node->next      = next;
        node->level     = 0;
        node->marker    = 1;

        for (i = 0; i < MAX_LEVELS; i++)
                node->succs[i] = NULL;

        assert (node->next != node);

        return node;
}

/**
 * node_delete - delete a bottom-level node
 * @node: the node to delete
 */
void node_delete(node_t *node, ptst_t *ptst)
{
        gc_free(ptst, (void*)node, gc_id[curr_id]);
}

/**
 * marker_delete - delete a marker node
 * @node: the node to delete
 */
void marker_delete(node_t *node, ptst_t *ptst)
{
        gc_free(ptst, (void*)node, gc_id[curr_id]);
}

/**
 * set_new - create a new set implemented as a skip list
 * @bg_start: if 1 start the bg thread, otherwise don't
 *
 * Returns a newly created skip list set.
 * Note: A background thread to update the index levels of the
 * skip list is created and kick-started as part of this routine.
 */
set_t* set_new(int start)
{
        set_t *set;
        ptst_t *ptst;

        ptst = ptst_critical_enter();

        sl_zero = 0; /* the zero index is initially 0 */

        set = malloc(sizeof(set_t));
        if (!set) {
                perror("Failed to malloc a set\n");
                exit(1);
        }

        set->head = node_new(0, NULL, NULL, NULL, 1, ptst);

        bg_init(set);
        if (start)
                bg_start(1);

        ptst_critical_exit(ptst);

        return set;
}

/**
 * set_delete - delete the set
 * @set: the set to delete
 */
void set_delete(set_t *set)
{
        /* stop the background thread */
        bg_stop();

        /* does not dealloc memory! */
}

/**
 * set_print - print the set
 * @set: the skip list set to print
 * @flag: if non-zero include logically deleted nodes in the count
 */
void set_print(set_t *set, int flag)
{
        node_t *head = set->head, *curr = set->head;
        unsigned long i = head->level - 1;
        unsigned long zero = sl_zero;

        /* print the index items */
        while (1) {
                while (NULL != curr) {
                        if (flag && (curr->val != curr))
                                printf("%lu ", curr->key);
                        else if (!flag)
                                printf("%lu ", curr->key);
                        curr = curr->succs[IDX(i,zero)];
                }
                printf("\n");
                curr = head;
                if (0 == i--)
                        break;
        }

        while (NULL != curr) {
                if (flag && (curr->val != curr))
                        printf("%lu (%lu) ", curr->key, curr->level);
                else if (!flag)
                        printf("%lu (%lu) ", curr->key, curr->level);
                curr = curr->next;
        }
        printf("\n");
}

/**
 * set_size - print the size of the set
 * @set: the set to print the size of
 * @flag: if non-zero include logically deleted nodes in the count
 *
 * Return the size of the set.
 */
int set_size(set_t *set, int flag)
{
        node_t *node = set->head;
        int size = 0;

        node = node->next;
        while (NULL != node) {
                if (flag && (node != node->val && NULL != node->val))
                        ++size;
                else if (!flag)
                        ++size;
                node = node->next;
        }

        return size;
}

/**
 * set_subsystem_init - ...
 */
void set_subsystem_init(void)
{
        int i;
        for (i = 0; i < NUM_SIZES; i++) {
                gc_id[i] = gc_add_allocator(sizeof(node_t));
        }
        curr_id = rand() % NUM_SIZES;
}

/**
 * set_print_nodenums - print the number of nodes at each level
 */
void set_print_nodenums(set_t *set, int flag)
{
        unsigned long i = set->head->level - 1, count = 0, zero = sl_zero;
        node_t *head = set->head, *curr = set->head;

        while (1) {
                assert(i < set->head->level);
                while (curr) {
                        if ((flag && ((curr->val != curr) && (curr->val != NULL))) || !flag)
                                ++count;
                        curr = curr->succs[IDX(i, zero)];
                }
                printf("inodes at level %lu = %lu\n", i+1, count);
                curr = head;
                count = 0;
                if (0 == i)
                        break;
                --i;
        }

        while (curr) {
                if ((flag && ((curr->val != curr) && (curr->val != NULL))) || !flag)
                        ++count;
                curr = curr->next;
        }
        printf("nodes at level 0 = %lu\n", count);
}
