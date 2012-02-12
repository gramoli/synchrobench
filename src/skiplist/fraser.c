/*
 * File:
 *   fraser.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lock-based skip list implementation of the Fraser algorithm
 *   "Practical Lock Freedom", K. Fraser, 
 *   PhD dissertation, September 2003
 *   Cambridge University Technical Report UCAM-CL-TR-579 
 *
 * Copyright (c) 2009-2010.
 *
 * fraser.c is part of Synchrobench
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

#include "fraser.h"

unsigned int levelmax;

inline int is_marked(uintptr_t i) {
  return (int)(i & (uintptr_t)0x01);
}

inline uintptr_t unset_mark(uintptr_t i) {
  return (i & ~(uintptr_t)0x01);
}

inline uintptr_t set_mark(uintptr_t i) {
  return (i | (uintptr_t)0x01);
}

inline void fraser_search(sl_intset_t *set, val_t val, sl_node_t **left_list, sl_node_t **right_list) {
  int i;
  sl_node_t *left, *left_next, *right, *right_next;

retry:
  left = set->head;
  for (i = levelmax - 1; i >= 0; i--) {
    left_next = left->next[i];
    if (is_marked((uintptr_t)left_next))
      goto retry;
    /* Find unmarked node pair at this level */
    for (right = left_next; ; right = right_next) {
      /* Skip a sequence of marked nodes */
      while(1) {
        right_next = right->next[i];
        if (!is_marked((uintptr_t)right_next))
          break;
        right = (sl_node_t*)unset_mark((uintptr_t)right_next);
      }
      if (right->val >= val)
        break;
      left = right; 
      left_next = right_next;
    }
    /* Ensure left and right nodes are adjacent */
    if ((left_next != right) && 
        (!ATOMIC_CAS_MB(&left->next[i], left_next, right)))
      goto retry;
    if (left_list != NULL)
      left_list[i] = left;
    if (right_list != NULL)	
      right_list[i] = right;
  }
}

int fraser_find(sl_intset_t *set, val_t val) {
  sl_node_t **succs;
  int result;

  succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
  fraser_search(set, val, NULL, succs);
  result = (succs[0]->val == val && !succs[0]->deleted);
  free(succs);
  return result;
}

inline void mark_node_ptrs(sl_node_t *n) {
  int i;
  sl_node_t *n_next;
	
  for (i=n->toplevel-1; i>=0; i--) {
    do {
      n_next = n->next[i];
      if (is_marked((uintptr_t)n_next))
        break;
    } while (!ATOMIC_CAS_MB(&n->next[i], n_next, set_mark((uintptr_t)n_next)));
  }
}

int fraser_remove(sl_intset_t *set, val_t val) {
  sl_node_t **succs;
  int result;

  succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
  fraser_search(set, val, NULL, succs);
  result = (succs[0]->val == val);
  if (result == 0)
    goto end;
  /* 1. Node is logically deleted when the deleted field is not 0 */
  if (succs[0]->deleted) {
    result = 0;
    goto end;
  }
  ATOMIC_FETCH_AND_INC_FULL(&succs[0]->deleted);
  /* 2. Mark forward pointers, then search will remove the node */
  mark_node_ptrs(succs[0]);
  fraser_search(set, val, NULL, NULL);    
end:
  free(succs);

  return result;
}

int fraser_insert(sl_intset_t *set, val_t v) {
  sl_node_t *new, *new_next, *pred, *succ, **succs, **preds;
  int i;
  int result;

  new = sl_new_simple_node(v, get_rand_level(), 6);
  preds = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
  succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
retry: 	
  fraser_search(set, v, preds, succs);
  /* Update the value field of an existing node */
  if (succs[0]->val == v) {
    /* Value already in list */
    if (succs[0]->deleted) {
      /* Value is deleted: remove it and retry */
      mark_node_ptrs(succs[0]);
      goto retry;
    }
    result = 0;
    sl_delete_node(new);
    goto end;
  }
  for (i = 0; i < new->toplevel; i++)
    new->next[i] = succs[i];
  /* Node is visible once inserted at lowest level */
  if (!ATOMIC_CAS_MB(&preds[0]->next[0], succs[0], new)) 
    goto retry;
  for (i = 1; i < new->toplevel; i++) {
    while (1) {
      pred = preds[i];
      succ = succs[i];
      /* Update the forward pointer if it is stale */
      new_next = new->next[i];
      if ((new_next != succ) && 
          (!ATOMIC_CAS_MB(&new->next[i], unset_mark((uintptr_t)new_next), succ)))
        break; /* Give up if pointer is marked */
      /* Check for old reference to a k node */
      if (succ->val == v)
        succ = (sl_node_t *)unset_mark((uintptr_t)succ->next);
      /* We retry the search if the CAS fails */
      if (ATOMIC_CAS_MB(&pred->next[i], succ, new))
        break;
      fraser_search(set, v, preds, succs);
    }
  }
  result = 1;
end:
  free(preds);
  free(succs);

  return result;
}

