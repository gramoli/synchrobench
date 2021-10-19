/*
 * File:
 *   harris.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Harris implementation of lock-free linked list.
 *
 * Copyright (c) 2008-2009.
 *
 * harris.c is part of Microbench
 * 
 * Microbench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 3
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "harris.h"

/*
 * The five following functions handle the low-order mark bit that indicates
 * whether a node is logically deleted (1) or not (0).
 *  - is_marked_ref returns whether it is marked, 
 *  - (un)set_marked changes the mark,
 *  - get_(un)marked_ref sets the mark before returning the node.
 */
int is_marked_ref(long i) {
    return (int) (i & (LONG_MIN+1));
}

long unset_mark(long i) {
	i &= LONG_MAX-1;
	return i;
}

long set_mark(long i) {
	i = unset_mark(i);
	i += 1;
	return i;
}

long get_unmarked_ref(long w) {
	return unset_mark(w);
}

long get_marked_ref(long w) {
	return set_mark(w);
}

/*
 * harris_search looks for value val, it
 *  - returns right_node owning val (if present) or its immediately higher 
 *    value present in the list (otherwise) and 
 *  - sets the left_node to the node owning the value immediately lower than val. 
 * Encountered nodes that are marked as logically deleted are physically removed
 * from the list, yet not garbage collected.
 */
node_t *harris_search(intset_t *set, val_t val, node_t **left_node) {
	node_t *left_node_next, *right_node;
	left_node_next = set->head;
	
search_again:
	do {
		node_t *t = set->head;
		node_t *t_next = set->head->next;
		
		/* Find left_node and right_node */
		do {
			if (!is_marked_ref((long) t_next)) {
				(*left_node) = t;
				left_node_next = t_next;
			}
			t = (node_t *) get_unmarked_ref((long) t_next);
			if (!t->next) break;
			t_next = t->next;
		} while (is_marked_ref((long) t_next) || (t->val < val));
		right_node = t;
		
		/* Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if (right_node->next && is_marked_ref((long) right_node->next))
				goto search_again;
			else return right_node;
		}
		
		/* Remove one or more marked nodes */
		if (ATOMIC_CAS_MB(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if (right_node->next && is_marked_ref((long) right_node->next))
				goto search_again;
			else return right_node;
		} 
		
	} while (1);
}

/*
 * harris_find returns whether there is a node in the list owning value val.
 */
int harris_find(intset_t *set, val_t val) {
	node_t *right_node, *left_node;
	left_node = set->head;
	
	right_node = harris_search(set, val, &left_node);
	if ((!right_node->next) || right_node->val != val)
		return 0;
	else 
		return 1;
}

/*
 * harris_find inserts a new node with the given value val in the list
 * (if the value was absent) or does nothing (if the value is already present).
 */
int harris_insert(intset_t *set, val_t val) {
	node_t *newnode, *right_node, *left_node;
	left_node = set->head;
	
	do {
		right_node = harris_search(set, val, &left_node);
		if (right_node->val == val)
			return 0;
		newnode = new_node(val, right_node, 0);
		/* mem-bar between node creation and insertion */
		AO_nop_full(); 
		if (ATOMIC_CAS_MB(&left_node->next, right_node, newnode))
			return 1;
	} while(1);
}

/*
 * harris_find deletes a node with the given value val (if the value is present) 
 * or does nothing (if the value is already present).
 * The deletion is logical and consists of setting the node mark bit to 1.
 */
int harris_delete(intset_t *set, val_t val) {
	node_t *right_node, *right_node_next, *left_node;
	left_node = set->head;
	
	do {
		right_node = harris_search(set, val, &left_node);
		if (right_node->val != val)
			return 0;
		right_node_next = right_node->next;
		if (!is_marked_ref((long) right_node_next))
			if (ATOMIC_CAS_MB(&right_node->next, 
							  right_node_next, 
							  get_marked_ref((long) right_node_next)))
				break;
	} while(1);
	if (!ATOMIC_CAS_MB(&left_node->next, right_node, right_node_next))
		right_node = harris_search(set, right_node->val, &left_node);
	return 1;
}



