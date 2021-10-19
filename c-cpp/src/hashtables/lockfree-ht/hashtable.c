/*
 * File:
 *   hashtable.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Hashtable
 *   Implementation of an integer set using a stm-based/lock-free hashtable.
 *   The hashtable contains several buckets, each represented by a linked
 *   list, since hashing distinct keys may lead to the same bucket.
 *
 * Copyright (c) 2009-2010.
 *
 * hashtable.c is part of Synchrobench
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

#include "hashtable.h"

void ht_delete(ht_intset_t *set) {
  node_t *node, *next;
  int i;
  
  for (i=0; i < maxhtlength; i++) {
    node = set->buckets[i]->head;
    while (node != NULL) {
      next = node->next;
      free(node);
      node = next;
    }
    free(set->buckets[i]);
  }
  free(set->buckets);
  free(set);
}

int ht_size(ht_intset_t *set) {
	int size = 0;
	node_t *node;
	int i;
	
	for (i=0; i < maxhtlength; i++) {
		node = set->buckets[i]->head->next;
		while (node->next) {
			size++;
			node = node->next;
		}
		
	}
	return size;
}

int floor_log_2(unsigned int n) {
	int pos = 0;
	printf("n result = %d\n", n);
	if (n >= 1<<16) { n >>= 16; pos += 16; }
	if (n >= 1<< 8) { n >>=  8; pos +=  8; }
	if (n >= 1<< 4) { n >>=  4; pos +=  4; }
	if (n >= 1<< 2) { n >>=  2; pos +=  2; }
	if (n >= 1<< 1) {           pos +=  1; }
	printf("floor result = %d\n", pos);
	return ((n == 0) ? (-1) : pos);
}

ht_intset_t *ht_new() {
	ht_intset_t *set;
	int i;
	
	if ((set = (ht_intset_t *)malloc(sizeof(ht_intset_t))) == NULL) {
		perror("malloc");
		exit(1);
	}  
        if ((set->buckets = (void *)malloc((maxhtlength + 1)* sizeof(intset_t *))) == NULL) {
	perror("malloc");
	exit(1);
	}  

	for (i=0; i < maxhtlength; i++) {
		set->buckets[i] = set_new();
	}
	return set;
}
