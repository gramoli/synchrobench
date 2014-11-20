/*
 * File:
 *   linkedlist-lock.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lock-based linked list implementation of an integer set
 *
 * Copyright (c) 2009-2010.
 *
 * linkedlist-lock.c is part of Synchrobench
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

node_l_t *new_node_l(val_t val, node_l_t *next, int transactional)
{
  node_l_t *node_l;
  
  node_l = (node_l_t *)malloc(sizeof(node_l_t));
  if (node_l == NULL) {
    perror("malloc");
    exit(1);
  }
  node_l->val = val;
  node_l->next = next;
  INIT_LOCK(&node_l->lock);	
  return node_l;
}

intset_l_t *set_new_l()
{
  intset_l_t *set;
  node_l_t *min, *max;

  if ((set = (intset_l_t *)malloc(sizeof(intset_l_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node_l(VAL_MAX, NULL, 0);
  min = new_node_l(VAL_MIN, max, 0);
  set->head = min;

  return set;
}

void node_delete_l(node_l_t *node) {
   DESTROY_LOCK(&node->lock);
   free(node);
}

void set_delete_l(intset_l_t *set)
{
  node_l_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->next;
    DESTROY_LOCK(&node->lock);
    free(node);
    node = next;
  }
  free(set);
}

int set_size_l(intset_l_t *set)
{
  int size = 0;
  node_l_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}



	
