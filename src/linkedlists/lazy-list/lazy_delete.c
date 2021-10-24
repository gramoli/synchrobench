/*
 * File:
 *   lazy.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lazy linked list implementation of an integer set based on Heller et al. algorithm
 *   "A Lazy Concurrent List-Based Set Algorithm"
 *   S. Heller, M. Herlihy, V. Luchangco, M. Moir, W.N. Scherer III, N. Shavit
 *   p.3-16, OPODIS 2005
 *
 * Copyright (c) 2009-2010.
 *
 * lazy.c is part of Synchrobench
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

#include "lazy.h"

/** Checking if first bit is 0 or 1 using bitwise operation (passing in pointer) */
inline int is_marked_ref(long i) {
	return (int) (i &= LONG_MIN+1);
}

/** Bit set to 0 */
inline long unset_mark(long i) {
	i &= LONG_MAX-1;
	return i;
}

/** Bit set to 1 */
inline long set_mark(long i) {
	i = unset_mark(i);
	i += 1;
	return i;
}

inline node_l_t *get_unmarked_ref(node_l_t *n) {
	return (node_l_t *) unset_mark((long) n);
}

inline node_l_t *get_marked_ref(node_l_t *n) {
	return (node_l_t *) set_mark((long) n);
}

/*
 * Checking that both curr and pred are both unmarked and that pred's next pointer
 * points to curr to verify that the entries are adjacent and present in the list.
 */
inline int parse_validate(node_l_t *pred, node_l_t *curr) {
	return (!is_marked_ref((long) pred->next) && !is_marked_ref((long) curr->next) && (pred->next == curr));
}

int parse_find(intset_l_t *set, val_t val) {
	node_l_t *curr;
	curr = set->head;
	while (curr->val < val)
		curr = get_unmarked_ref(curr->next);
	return ((curr->val == val) && !is_marked_ref((long) curr->next));
}

int parse_insert(intset_l_t *set, val_t val) {
	node_l_t *curr, *pred, *newnode;
	int result, validated, notVal;
	
	while (1) {
		pred = set->head;
		curr = get_unmarked_ref(pred->next);
		while (curr->val < val) {
			pred = curr;
			curr = get_unmarked_ref(curr->next);
		}
		LOCK(&pred->lock);
		LOCK(&curr->lock);
		validated = parse_validate(pred, curr);
		notVal = (curr->val != val);
		result = (validated && notVal);
		if (result) {
			newnode = new_node_l(val, curr, 0);
			pred->next = newnode;
		} 
		UNLOCK(&curr->lock);
		UNLOCK(&pred->lock);
		if(validated)
			return result;
	}
}

/*
 * Logically remove an element by setting a mark bit to 1 
 * before removing it physically.
 *
 * NB. it is not safe to free the element after physical deletion as a 
 * pre-empted find operation may currently be parsing the element.
 * TODO: must implement a stop-the-world garbage collector to correctly 
 * free the memory.
 */
int parse_delete(intset_l_t *set, val_t val) {
    /** pred and curr -- pointers to node_l_t struct */
	node_l_t *pred, *curr;
	int result, validated, isVal;
	while(1) {
        /** set 'pred' to the head of the set */
		pred = set->head;
        /** set 'curr' to NOT be logically deleted, pointing to 'pred->next' */
		curr = get_unmarked_ref(pred->next);
        /* iterate through the entire set until we reach 'val'*/
		while (curr->val < val) {
			pred = curr;
			curr = get_unmarked_ref(curr->next);
		}
        /** lock 'pred' and 'curr' */
		LOCK(&pred->lock);
		LOCK(&curr->lock);
        /** validation checkpoint */
		validated = parse_validate(pred, curr);
        /** checking whether val exists and is valid */
		isVal = val == curr->val;
		result = validated && isVal;
		if (result) {
            /** marking 'curr' as logically deleted */
			curr->next = get_marked_ref(curr->next);
            /** physical deletion */
			pred->next = get_unmarked_ref(curr->next);
            /** free 'curr' and set memory location to NULL */
            free(curr);
		}
		UNLOCK(&curr->lock);
		UNLOCK(&pred->lock);
		if(validated)
			return result;
	}
}
