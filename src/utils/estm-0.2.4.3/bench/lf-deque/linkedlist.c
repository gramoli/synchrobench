/*
 * File:
 *   linkedlist.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Linked list benchmark.
 *
 * Copyright (c) 2008-2009.
 *
 * linkedlist.c is part of Microbench
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

#include "linkedlist.h"

int set_contains(intset_t *set, val_t val, int transactional)
{
	int result;
	node_t *prev, *next;
	val_t v;
	
#ifdef DEBUG
	printf("++> set_contains(%d)\n", val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
		case 0: /* Unprotected */
			prev = set->head;
			next = prev->next;
			while (next->val < val) {
				prev = next;
				next = prev->next;
			}
			result = (next->val == val);
			break;
			
		case 1: /* Normal transaction */
			TX_START(0, RO, NL);
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD(&next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			result = (v == val);
			TX_END;
			break;
			
		case 2:
		case 3:
		case 4: /* Unit transaction */
			TX_START(0, RO, EL);
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD((stm_word_t *) &next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}		  
			TX_END;
			result = (v == val);
			break;
			
		case 5: /* harris lock-free */
			result = harris_find(set, val);
			
		default:
			result=0;
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);
	}
	
	return result;
}

int set_add(intset_t *set, val_t val, int transactional)
{
	int result;
	node_t *prev, *next;
	val_t v;
	
#ifdef DEBUG
	printf("++> set_add(%d)\n", val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
		case 0: /* Unprotected */
			prev = set->head;
			next = prev->next;
			while (next->val < val) {
				prev = next;
				next = prev->next;
			}
			result = (next->val != val);
			if (result) {
				prev->next = new_node(val, next, transactional);
			}
			break;
			
		case 1:
		case 2: /* Normal transaction */
			TX_START(1, RW, NL);
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD(&next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			result = (v != val);
			if (result) {
				TX_STORE(&prev->next, new_node(val, next, transactional));
			}
			TX_END;
			break;
			
		case 3:
		case 4: /* Unit transaction */
			TX_START(1, RW, EL);
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD((stm_word_t *) &next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			result = (v != val);
			if (result) {
				TX_STORE(&prev->next, new_node(val, next, transactional));
			}
			TX_END;
			break;
			
		case 5: /* harris lock-free */
			result = harris_insert(set, val);
			
		default:
			result=0;
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);
	}
	
	return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
	int result;
	node_t *prev, *next;
	val_t v;
	node_t *n;
	
#ifdef DEBUG
	printf("++> set_remove(%d)\n", val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
		case 0: /* Unprotected */
			prev = set->head;
			next = prev->next;
			while (next->val < val) {
				prev = next;
				next = prev->next;
			}
			result = (next->val == val);
			if (result) {
				prev->next = next->next;
				free(next);
			}
			break;
			
		case 1:
		case 2:
		case 3: /* Normal transaction */
			TX_START(2, RW, NL);
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD(&next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			result = (v == val);
			if (result) {
				n = (node_t *)TX_LOAD(&next->next);
				TX_STORE(&prev->next, n);
				/* Free memory (delayed until commit) */
				FREE(next, sizeof(node_t));
			}
			TX_END;
			break;
			
		case 4: /* Unit transaction */
			TX_START(2, RW, EL);
			prev = set->head;
			next = (node_t *)TX_LOAD(&prev->next);
			while (1) {
				v = TX_LOAD((stm_word_t *) &next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			result = (v == val);
			if (result) {
				n = (node_t *)TX_LOAD(&next->next);
				TX_STORE(&prev->next, n);
				// Free memory (delayed until commit)
				FREE(next, sizeof(node_t));
			}
			TX_END;
			break;
			
		case 5: /* harris lock-free */
			result = harris_delete(set, val);
			
		default:
			result=0;
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);
	}
	
	return result;
}


