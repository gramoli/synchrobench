/*
 * File:
 *   skiplist.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list definition 
 *
 * Copyright (c) 2009-2010.
 *
 * skiplist.c is part of Synchrobench
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

#include "skiplist.h"	

unsigned int levelmax;

/*
 * Returns a random level for inserting a new node, results are hardwired to p=0.5, min=1, max=32.
 *
 * "Xorshift generators are extremely fast non-cryptographically-secure random number generators on
 * modern architectures."
 *
 * Marsaglia, George, (July 2003), "Xorshift RNGs", Journal of Statistical Software 8 (14)
 */
int get_rand_level() {
	static uint32_t y = 2463534242UL;
	y^=(y<<13);
	y^=(y>>17);
	y^=(y<<5);
	uint32_t temp = y;
	uint32_t level = 1;
	while (((temp >>= 1) & 1) != 0) {
		++level;
	}
	/* 1 <= level <= levelmax */
	if (level > levelmax) {
		return (int)levelmax;
	} else {
		return (int)level;
	}
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

  if (transactional)
    node = (sl_node_t *)MALLOC(sizeof(sl_node_t) + toplevel * sizeof(sl_node_t *));
  else 
    node = (sl_node_t *)malloc(sizeof(sl_node_t) + toplevel * sizeof(sl_node_t *));
  if (node == NULL) {
    perror("malloc");
    exit(1);
    }

  node->val = val;
  node->toplevel = toplevel;
  node->deleted = 0;

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

  for (i = 0; i < levelmax; i++)
    node->next[i] = next;
	
  return node;
}

void sl_delete_node(sl_node_t *n)
{
  free(n);
}

sl_intset_t *sl_set_new()
{
  sl_intset_t *set;
  sl_node_t *min, *max;
	
  if ((set = (sl_intset_t *)malloc(sizeof(sl_intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = sl_new_node(VAL_MAX, NULL, levelmax, 0);
  min = sl_new_node(VAL_MIN, max, levelmax, 0);
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

unsigned long sl_set_size(sl_intset_t *set)
{
  unsigned long size = 0;
  sl_node_t *node;

  node = set->head->next[0];
  while (node->next[0] != NULL) {
    if (!node->deleted)
      size++;
    node = node->next[0];
  }

  return size;
}
