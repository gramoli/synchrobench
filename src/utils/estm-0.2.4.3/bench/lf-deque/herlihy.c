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

/* ################################################################### *
 * HARRIS' LINKED LIST
 * ################################################################### */

int is_marked_ref(long i) {
	// 1 means true, 0 false
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

int harris_find(intset_t *set, val_t val) {
	node_t *right_node, *left_node;
	left_node = set->head;
	
	right_node = harris_search(set, val, &left_node);
	if ((!right_node->next) || right_node->val != val)
		return 0;
	else 
		return 1;
}

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


#define RIGHT 1
#define LEFT  0

type element = record val: valtype; ctr: int end 

/* 
 * A: array[0..MAX+1] of element initially there is some k in [0,MAX] 
 * such that A[i] = <LN,0> for all i in [0,k] 
 * and A[i] = <RN,0> for all i in [k+1,MAX+1]. 
 */


int oracle(int direction) {
	
	return 0;	
}

int rightpush(intset_t *set, val_t val) { // v is not RN or LN		
	while (1) { 
		k := oracle(RIGHT); // find index of leftmost RN 
		prev := A[k-1]; // read (supposed) rightmost non-RN value 
		cur := A[k]; // read (supposed) leftmost RN value 
		if (prev.val != RN and cur.val = RN) { // oracle is right 
			if (k = MAX+1) return 0; // full: A[MAX] != RN 
			if ATOMIC_CAS_MB(&A[k-1], prev, <prev.val,prev.ctr+1>) // try to bump up prev.ctr 
				if ATOMIC_CAS_MB(&A[k], cur, <v,cur.ctr+1>) // try to push new value 
					return 1; // it worked! 
		} // end if (prev.val != RN and cur.val = RN) 
	} // end while 
}

int rightpop() {
	while (1) { // keep trying till return val or empty 
		k := oracle(RIGHT); // find index of leftmost RN 
		cur := A[k-1]; // read (supposed) value to be popped 
		next := A[k]; // read (supposed) leftmost RN 
		if (cur.val != RN and next.val = RN) { // oracle is right 
			if (cur.val = LN and A[k-1] = cur) // adjacent LN and RN 
				return 0; 
			if ATOMIC_CAS_MB(&A[k], next, <RN,next.ctr+1>) // try to bump up next.ctr 
				if ATOMIC_CAS_MB(&A[k-1], cur, <RN,cur.ctr+1>) // try to remove value 
					return cur.val; // it worked; return removed value 
		} // end if (cur.val != RN and next.val = RN) 
	} // end while 
}


