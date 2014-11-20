/*
 * File:
 *   skiplist-lock.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list implementation of an integer set
 *
 * Copyright (c) 2009-2010.
 *
 * skiplist-lock.c is part of Synchrobench
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

#include "skiplist-lock.h"

unsigned int levelmax;

inline void *xmalloc(size_t size)
{
	void *p = malloc(size);
	if (p == NULL) {
		perror("malloc");
		exit(1);
	}
	return p;
}


/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 */
inline long rand_range(long r) {
	int m = RAND_MAX;
	long d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (long)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}


int get_rand_level() {
	int i, level = 1;
	for (i = 0; i < levelmax - 1; i++) {
		if ((rand_range(100)-1) < 50)
			level++;
		else
			break;
	}
	/* 1 <= level <= levelmax */
	return level;
}

int floor_log_2(unsigned int n) {
	int pos = 0;
	if (n >= 1<<16) { n >>= 16; pos += 16; }
	if (n >= 1<< 8) { n >>=  8; pos +=  8; }
	if (n >= 1<< 4) { n >>=  4; pos +=  4; }
	if (n >= 1<< 2) { n >>=  2; pos +=  2; }
	if (n >= 1<< 1) {           pos +=  1; }
	return ((n == 0) ? (-1) : pos);
}

/* 
 * Create a new node without setting its next fields. 
 */
sl_node_t *sl_new_simple_node(val_t val, int toplevel, int transactional)
{
	sl_node_t *node;
	
	node = (sl_node_t *)xmalloc(sizeof(sl_node_t));
	node->next = (sl_node_t **)xmalloc(toplevel * sizeof(sl_node_t *));
	node->val = val;
	node->toplevel = toplevel;
	node->marked = 0;
	node->fullylinked = 0;
	INIT_LOCK(&node->lock);
	return node;
}

/* 
 * Create a new node with its next field. 
 * If next=NULL, then this create a tail node. 
 */
sl_node_t *sl_new_node(val_t val, sl_node_t *next, int toplevel, int transactional)
{
	sl_node_t *node;
	int i;
	
	node = sl_new_simple_node(val, toplevel, transactional);
	
	for (i = 0; i < toplevel; i++)
		node->next[i] = next;
	
	return node;
}

void sl_delete_node(sl_node_t *n)
{
	DESTROY_LOCK(&n->lock);
	free(n->next);
	free(n);
}

sl_intset_t *sl_set_new()
{
	sl_intset_t *set;
	sl_node_t *min, *max;
	
	set = (sl_intset_t *)xmalloc(sizeof(sl_intset_t));
	max = sl_new_node(VAL_MAX, NULL, levelmax, 0);
	min = sl_new_node(VAL_MIN, max, levelmax, 0);
	max->fullylinked = 1;
	min->fullylinked = 1;
	set->head = min;
	return set;
}

void sl_set_delete(sl_intset_t *set)
{
	sl_node_t *node, *next;
	
	node = set->head;
	while (node != NULL) {
		next = node->next[0];
		sl_delete_node(node);
		node = next;
	}
	free(set);
}

int sl_set_size(sl_intset_t *set)
{
	int size = -1;
	sl_node_t *node;
	
	/* We have at least 2 elements */
	node = set->head->next[0];
	while (node->next[0] != NULL) {
		if (node->fullylinked && !node->marked)
			size++;
		node = node->next[0];
	}
	
	return size;
}
