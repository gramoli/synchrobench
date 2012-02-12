/*
 * File:
 *   intset.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Integer set operations
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

#include "rbtree.c"

typedef rbtree_t intset_t;
typedef intptr_t val_t;

static long compare(const void *a, const void *b)
{
  return ((val_t)a - (val_t)b);
}

intset_t *set_new()
{
  return rbtree_alloc(&compare);
}

void set_delete(intset_t *set)
{
  rbtree_free(set);
}

int set_size(intset_t *set)
{
  int size;
  node_t *n;

  if (!rbtree_verify(set, 0)) {
    printf("Validation failed!\n");
    exit(1);
  }

  size = 0;
  for (n = firstEntry(set); n != NULL; n = successor(n))
    size++;

  return size;
}

int set_contains(intset_t *set, val_t val, int transactional)
{
	int result = 0;
	void *v;

	v = (void *)val;
	
	switch(transactional) {
		case 0:	
			result = rbtree_contains(set, v);
			break;
			
		case 1: /* Normal transaction */	
			TX_START(NL);
			result = TMrbtree_contains(set, (void *)val);
			TX_END;
			break;
    
		case 2:
		case 3:
		case 4: /* Elastic transaction */
			TX_START(EL);
			result = TMrbtree_contains(set, (void *)val);
			TX_END;
			break;
			
		default:
			result=0;
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);
	}
	return result;
}

/* 
 * Adding to the rbtree may require rotations (at least in this implementation)  
 * This operation requires strong dependencies between numerous transactional 
 * operations, hence, the concurrency gain with elastic transactions is slight.
 */ 
int set_add(intset_t *set, val_t val, int transactional)
{
  int result = 0;

  switch(transactional) {
	  case 0:
		  result = rbtree_insert(set, (void *)val, (void *)val);
		  break;
		  
	  case 1:
	  case 2: /* Normal transaction */	
		  TX_START(NL);
		  result = TMrbtree_insert(set, (void *)val, (void *)val);
		  TX_END;
		  break;
	  case 3:
	  case 4: /* Elastic transaction */
		  TX_START(EL);
		  result = TMrbtree_insert(set, (void *)val, (void *)val);
		  TX_END;
		  break;
		  
	  default:
		  result=0;
		  printf("number %d do not correspond to elasticity.\n", transactional);
		  exit(1);
  }

  return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
	int result = 0;
	node_t *next;
	void *v;
	
	next = NULL;
	v = (void *) val;

	switch(transactional) {
		case 0: /* Unprotected */
			result = rbtree_delete(set, (void *)val);
			break;
			
		case 1: 
		case 2:
		case 3: /* Normal transaction */
			TX_START(NL);
			result = TMrbtree_delete(set, (void *)val);
			TX_END;
			break;
				
		case 4: /* Elastic transaction */
			TX_START(EL);
			result = TMrbtree_delete(set, (void *)val);
			TX_END;
			break;
			
		default:
			result=0;
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);
	}
	return result;
}
