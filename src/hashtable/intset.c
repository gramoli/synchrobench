/*
 * File:
 *   intset.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Integer set operations accessing the hashtable
 *
 * Copyright (c) 2009-2010.
 *
 * intset.c is part of Synchrobench
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

int ht_contains(ht_intset_t *set, int val, int transactional) {
	int addr;
	
	addr = val % maxhtlength;
	if (transactional == 5)
	  return set_contains(set->buckets[addr], val, 4);
	else
	  return set_contains(set->buckets[addr], val, transactional);
}

int ht_add(ht_intset_t *set, int val, int transactional) {
	int addr;
	
	addr = val % maxhtlength;
	if (transactional == 5)
		return set_add(set->buckets[addr], val, 4);
	else 
		return set_add(set->buckets[addr], val, transactional);
}

int ht_remove(ht_intset_t *set, int val, int transactional) {
	int addr;
    
	addr = val % maxhtlength;
	if (transactional == 5)
		return set_remove(set->buckets[addr], val, 4);
	else
		return set_remove(set->buckets[addr], val, transactional);
}

/* 
 * Move an element from one bucket to another.
 * It is equivalent to changing the key associated with some value.
 *
 * This version allows the removal of val1 while insertion of val2 fails (e.g.
 * because val2 already present. (Despite this partial failure, the move returns 
 * true.) As a result, the data structure size may decrease as moves execute.
 */
int ht_move_naive(ht_intset_t *set, int val1, int val2, int transactional) {
	int result = 0;
	
#ifdef SEQUENTIAL
	
	int addr1, addr2;
		
	addr1 = val1 % maxhtlength;
	addr2 = val2 % maxhtlength;
	result =  (set_remove(set->buckets[addr1], val1, transactional) && 
			   set_add(set->buckets[addr2], val2, transactional));
	
#elif defined STM
	
	int v, addr1, addr2;
	node_t *n, *prev, *next;

	if (transactional > 1) {
	  
	  TX_START(EL);
	  addr1 = val1 % maxhtlength;
	  prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
	  next = (node_t *)TX_LOAD(&prev->next);
	  while(1) {
	    v = TX_LOAD(&next->val);
	    if (v >= val1) break;
	    prev = next;
	    next = (node_t *)TX_LOAD(&prev->next);
	  }
	  if (v == val1) {
	    /* Physically removing */
	    n = (node_t *)TX_LOAD(&next->next);
	    TX_STORE(&prev->next, n);
	    FREE(next, sizeof(node_t));
	    /* Inserting */
	    addr2 = val2 % maxhtlength;
	    prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
	    next = (node_t *)TX_LOAD(&prev->next);
	    while(1) {
	      v = TX_LOAD(&next->val);
	      if (v >= val2) break;
	      prev = next;
	      next = (node_t *)TX_LOAD(&prev->next);
	    }
	    if (v != val2) {
	      TX_STORE(&prev->next, new_node(val2, next, transactional));
	    }
	    /* Even if the key is already in, the operation succeeds */
	    result = 1;
	  } else result = 0;
	  TX_END;

	} else { 

	  TX_START(NL);
	  addr1 = val1 % maxhtlength;
	  prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
	  next = (node_t *)TX_LOAD(&prev->next);
	  while(1) {
	    v = TX_LOAD(&next->val);
	    if (v >= val1) break;
	    prev = next;
	    next = (node_t *)TX_LOAD(&prev->next);
	  }
	  if (v == val1) {
	    /* Physically removing */
	    n = (node_t *)TX_LOAD(&next->next);
	    TX_STORE(&prev->next, n);
	    FREE(next, sizeof(node_t));
	    /* Inserting */
	    addr2 = val2 % maxhtlength;
	    prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
	    next = (node_t *)TX_LOAD(&prev->next);
	    while(1) {
	      v = TX_LOAD(&next->val);
	      if (v >= val2) break;
	      prev = next;
	      next = (node_t *)TX_LOAD(&prev->next);
	    }
	    if (v != val2) {
	      TX_STORE(&prev->next, new_node(val2, next, transactional));
	    }
	    /* Even if the key is already in, the operation succeeds */
	    result = 1;
	  } else result = 0;
	  TX_END;

	}

#elif defined LOCKFREE /* No CAS-based implementation is provided */

	printf("ht_snapshot: No other implementation of atomic snapshot is available\n");
	exit(1);

#endif
	
	return result;
}


/*
 * This version parses the data structure twice to find appropriate values 
 * before updating it.
 */
int ht_move(ht_intset_t *set, int val1, int val2, int transactional) {
  int result = 0;

#ifdef SEQUENTIAL

	int addr1, addr2;
		
	addr1 = val1 % maxhtlength;
	addr2 = val2 % maxhtlength;

	if (set_remove(set->buckets[addr1], val1, 0)) 
	  result = 1;
	set_seq_add(set->buckets[addr2], val2, 0);
	return result;

#elif defined STM

	int v, addr1, addr2;
	node_t *n, *prev, *next, *prev1,  *next1;

	if (transactional > 1) {
	
	  TX_START(EL);
	  result = 0;
	  addr1 = val1 % maxhtlength;
	  prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
	  next = (node_t *)TX_LOAD(&prev->next);
	  while(1) {
	    v = TX_LOAD(&next->val);
	    if (v >= val1) break;
	    prev = next;
	    next = (node_t *)TX_LOAD(&prev->next);
	  }
	  prev1 = prev;
	  next1 = next;
	  if (v == val1) {
	    /* Inserting */
	    addr2 = val2 % maxhtlength;
	    prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
	    next = (node_t *)TX_LOAD(&prev->next);
	    while(1) {
	      v = TX_LOAD(&next->val);
	      if (v >= val2) break;
	      prev = next;
	      next = (node_t *)TX_LOAD(&prev->next);
	    }
	    if (v != val2 && prev != prev1 && prev != next1) {
	      /* Even if the key is already in, the operation succeeds */
	      result = 1;
	      
	      /* Physically removing */
	      n = (node_t *)TX_LOAD(&next1->next);
	      TX_STORE(&prev1->next, n);
	      TX_STORE(&prev->next, new_node(val2, next, transactional));
	      FREE(next1, sizeof(node_t));
	    }
	  } else result = 0;
	  TX_END;
	  
	} else {

	  TX_START(NL);
	  result = 0;
	  addr1 = val1 % maxhtlength;
	  prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
	  next = (node_t *)TX_LOAD(&prev->next);
	  while(1) {
	    v = TX_LOAD(&next->val);
	    if (v >= val1) break;
	    prev = next;
	    next = (node_t *)TX_LOAD(&prev->next);
	  }
	  prev1 = prev;
	  next1 = next;
	  if (v == val1) {
	    /* Inserting */
	    addr2 = val2 % maxhtlength;
	    prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
	    next = (node_t *)TX_LOAD(&prev->next);
	    while(1) {
	      v = TX_LOAD(&next->val);
	      if (v >= val2) break;
	      prev = next;
	      next = (node_t *)TX_LOAD(&prev->next);
	    }
	    if (v != val2 && prev != prev1 && prev != next1) {
	      /* Even if the key is already in, the operation succeeds */
	      result = 1;
	      
	      /* Physically removing */
	      n = (node_t *)TX_LOAD(&next1->next);
	      TX_STORE(&prev1->next, n);
	      TX_STORE(&prev->next, new_node(val2, next, transactional));
	      FREE(next1, sizeof(node_t));
	    }
	  } else result = 0;
	  TX_END;
	
	}

#elif defined LOCKFREE /* No CAS-based implementation is provided */
	
	printf("ht_snapshot: No other implementation of atomic snapshot is available\n");
	exit(1);

#endif
	
	return result;
}

/*
 * This version removes val1 it finds first and re-insert this value if it does 
 * not succeed in inserting val2 so that it can insert somewhere (for the size 
 * to remain unchanged).
 */
int ht_move_orrollback(ht_intset_t *set, int val1, int val2, int transactional) {
  int result = 0;	
	
#ifdef SEQUENTIAL

	int addr1, addr2;		
	addr1 = val1 % maxhtlength;
	addr2 = val2 % maxhtlength;
	result =  (set_remove(set->buckets[addr1], val1, transactional) &&
			   set_add(set->buckets[addr2], val2, transactional));
	
#elif defined STM

	int v, addr1, addr2;
	node_t *n, *prev, *next, *prev1, *next1;

	if (transactional > 1) {

	  TX_START(EL);
	  result = 0;
	  addr1 = val1 % maxhtlength;
	  prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
	  next = (node_t *)TX_LOAD(&prev->next);
	  while(1) {
	    v = TX_LOAD(&next->val);
	    if (v >= val1) break;
	    prev = next;
	    next = (node_t *)TX_LOAD(&prev->next);
	  }
	  prev1 = prev;
	  next1 = next;
	  if (v == val1) {
	    /* Physically removing */
	    n = (node_t *)TX_LOAD(&next->next);
	    TX_STORE(&prev->next, n);
	    /* Inserting */
	    addr2 = val2 % maxhtlength;
	    prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
	    next = (node_t *)TX_LOAD(&prev->next);
	    while(1) {
	      v = TX_LOAD(&next->val);
	      if (v >= val2) break;
	      prev = next;
	      next = (node_t *)TX_LOAD(&prev->next);
	    }
	    if (v != val2) {
	      TX_STORE(&prev->next, new_node(val2, next, transactional));
	      FREE(next1, sizeof(node_t));
	    } else {
	      TX_STORE(&prev1->next, TX_LOAD(&next1));
	    }
	    /* Even if the key is already in, the operation succeeds */
	    result = 1;
	  } else result = 0;
	  TX_END;
	  
	  
	} else {
	  
	  TX_START(NL);
	  result = 0;
	  addr1 = val1 % maxhtlength;
	  prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
	  next = (node_t *)TX_LOAD(&prev->next);
	  while(1) {
	    v = TX_LOAD(&next->val);
	    if (v >= val1) break;
	    prev = next;
	    next = (node_t *)TX_LOAD(&prev->next);
	  }
	  prev1 = prev;
	  next1 = next;
	  if (v == val1) {
	    /* Physically removing */
	    n = (node_t *)TX_LOAD(&next->next);
	    TX_STORE(&prev->next, n);
	    /* Inserting */
	    addr2 = val2 % maxhtlength;
	    prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
	    next = (node_t *)TX_LOAD(&prev->next);
	    while(1) {
	      v = TX_LOAD(&next->val);
	      if (v >= val2) break;
	      prev = next;
	      next = (node_t *)TX_LOAD(&prev->next);
	    }
	    if (v != val2) {
	      TX_STORE(&prev->next, new_node(val2, next, transactional));
	      FREE(next1, sizeof(node_t));
	    } else {
	      TX_STORE(&prev1->next, TX_LOAD(&next1));
	    }
	    /* Even if the key is already in, the operation succeeds */
	    result = 1;
	  } else result = 0;
	  TX_END;
	  
	}
	
#elif defined LOCKFREE /* No CAS-based implementation is provided */

	printf("ht_snapshot: No other implementation of atomic snapshot is available\n");
	exit(1);

#endif
	
	return result;
}


/*
 * Atomic snapshot of the hash table.
 * It parses the whole hash table to sum all elements.
 *
 * Observe that this particular operation (atomic snapshot) cannot be implemented using 
 * elastic transactions in combination with the move operation, however, normal transactions
 * compose with elastic transactions.
 */
int ht_snapshot(ht_intset_t *set, int transactional) {
  int result = 0; 
	
#ifdef SEQUENTIAL

	int i, sum = 0;
	node_t *next;
	
	for (i=0; i < maxhtlength; i++) {
		next = set->buckets[i]->head->next;
		while(next->next) {
			sum += next->val;
			next = next->next;
		}
	}
	result = 1;
		
#elif defined STM
	
	int i, sum = 0;
	node_t *next;

	// always a normal transaction
	TX_START(NL);
	result = 0;
	for (i=0; i < maxhtlength; i++) {
		next = (node_t *)TX_LOAD(&set->buckets[i]->head->next);
		while(next->next) {
			sum += TX_LOAD(&next->val);
			next = (node_t *)TX_LOAD(&next->next);
		}
	}
	TX_END;
	result = 1;

#elif defined LOCKFREE /* No CAS-based implementation is provided */

	printf("ht_snapshot: No other implementation of atomic snapshot is available\n");
	exit(1);
			
#endif
	
	return result;
}
