/*
 * File:
 *   hashtable-lock.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lock-based Hashtable
 *   Implementation of an integer set using a lock-based hashtable.
 *   The hashtable contains several buckets, each represented by a linked
 *   list, since hashing distinct keys may lead to the same bucket.
 *
 * Copyright (c) 2009-2010.
 *
 * hashtable-lock.c is part of Synchrobench
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

#include "hashtable-lock.h"

unsigned int maxhtlength;

void ht_delete(ht_intset_t *set) {
	node_l_t *node, *next;
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
	free(set);
}

int ht_size(ht_intset_t *set) {
	int size = 0;
	node_l_t *node;
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
	for (i=0; i < maxhtlength; i++) {
		set->buckets[i] = set_new_l();
	}
	return set;
}

int ht_contains(ht_intset_t *set, int val, int transactional) {
	int addr;
	
	/* Get key */
	addr = val % maxhtlength;
	return set_contains_l(set->buckets[addr], val, transactional);
}

int ht_add(ht_intset_t *set, int val, int transactional) {
	int addr, result;
	
	/* Get key */
	addr = val % maxhtlength;
	result = set_add_l(set->buckets[addr], val, transactional);
	return result;
}

int ht_remove(ht_intset_t *set, int val, int transactional) {
	int addr, result;
	
	/* Get key */
	addr = val % maxhtlength;
	result = set_remove_l(set->buckets[addr], val, transactional);
	
	return result;
}


/* 
 * Move an element in the hashtable (from one linked-list to another)
 */
int ht_move(ht_intset_t *set, int val1, int val2, int transactional) {
	node_l_t *pred1, *curr1, *curr2, *pred2, *newnode;
	int addr1, addr2, result = 0;
	
#ifdef DEBUG
	printf("++> ht_move(%d, %d)\n", (int) val1, (int) val2);
#endif
	
	if (val1 == val2) return 0;
	
	// records pred and succ of val1
	addr1 = val1 % maxhtlength;
	pred1 = set->buckets[addr1]->head;
	curr1 = pred1->next;
	while (curr1->val < val1) {
		pred1 = curr1;
		curr1 = curr1->next;
	}
	// records pred and succ of val2 
	addr2 = val2 % maxhtlength;
	pred2 = set->buckets[addr2]->head;
	curr2 = pred2->next;
	while (curr2->val < val2) {
		pred2 = curr2;
		curr2 = curr2->next;
	}
	// unnecessary move
	if (pred1->val == pred2->val || curr1->val == pred2->val || 
		curr2->val == pred1->val || curr1->val == curr2->val) 
		return 0;
	// acquire locks in order
	if (addr1 < addr2 || (addr1 == addr2 && val1 < val2)) {
		LOCK(&pred1->lock);
		LOCK(&curr1->lock);
		LOCK(&pred2->lock);
		LOCK(&curr2->lock);
	} else {
		LOCK(&pred2->lock);
		LOCK(&curr2->lock);
		LOCK(&pred1->lock);
		LOCK(&curr1->lock);
	}
	// remove val1 and insert val2 
	result = (parse_validate(pred1, curr1) && (val1 == curr1->val) &&
			  parse_validate(pred2, curr2) && (curr2->val != val2));
	if (result) {
		set_mark((long) curr1);
		pred1->next = curr1->next;
		newnode = new_node_l(val2, curr2, 0);
		pred2->next = newnode;
	}
	// release locks in order
	UNLOCK(&pred2->lock);
	UNLOCK(&pred1->lock);
	UNLOCK(&curr2->lock);
	UNLOCK(&curr1->lock);
		
	return result;
}

/* 
 * Read all elements of the hashtable (parses all linked-lists)
 * This cannot be consistent when used with move operation.
 */
int ht_snapshot_unmovable(ht_intset_t *set, int transactional) {
	node_l_t *next, *curr;
	int i;
	int sum = 0;
	
	for (i=0; i < maxhtlength; i++) {
		curr = set->buckets[i]->head;
		next = set->buckets[i]->head->next;
		
  		//pthread_mutex_lock((pthread_mutex_t *) &next->lock);
		LOCK(&next->lock);
	    
		while (next->next) {
			UNLOCK(&next->lock);
			curr = next;
			if (!is_marked_ref((long) next)) sum += next->val;
			next = curr->next;
			LOCK(&next->lock);
		}
		UNLOCK(&next->lock);
	}
	
	return sum;
}


/* 
 * Read all elements of the hashtable (parses all linked-lists)
 */
int ht_snapshot(ht_intset_t *set, int transactional) {
	node_l_t *next, *curr;
	int i;
	int sum = 0;
	
	int m = maxhtlength;
	
	for (i=0; i < m; i++) {
	  do {
	    LOCK(&set->buckets[i]->head->lock);
	    LOCK(&set->buckets[i]->head->next->lock);
	    curr = set->buckets[i]->head;
	    next = set->buckets[i]->head->next;
	  } while (!parse_validate(curr, next));

	  while (next->next) {
	    while(1) {
	      LOCK(&next->next->lock);
	      curr = next;
	      next = curr->next;
	      if (parse_validate(curr, next)) {
		if (!is_marked_ref((long) next)) {
		  sum += curr->val;
		}
		break;
	      }
	    }
	  }
	}
	
	for (i=0; i < m; i++) {
	  curr = set->buckets[i]->head;
	  next = set->buckets[i]->head->next;
	  
	  UNLOCK(&curr->lock);
	  UNLOCK(&next->lock);
	  while (next->next) {
	    curr = next;
	    next = curr->next;
	    UNLOCK(&next->lock);
	  }
	}
	
	return 1;
}
