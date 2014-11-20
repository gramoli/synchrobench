/*
 * File:
 *   newavltree.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   A speculation-friendly tree. Crain, Gramoli, 
 *   Raynal, PPoPP 2012.
 *
 * Copyright (c) 2009-2010.
 *
 * skiplist.c is part of Microbench
 * 
 * Microbench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "newavltree.h"	

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
avl_node_t *avl_new_simple_node(val_t val, val_t key, int transactional)
{
  avl_node_t *node;

  if (transactional) {
    node = (avl_node_t *)MALLOC(sizeof(avl_node_t));
  } else {
    node = (avl_node_t *)malloc(sizeof(avl_node_t));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->key = key;
  node->val = val;
  
  node->deleted = 0;
  node->removed = 0;
  node->left = NULL;
  node->right = NULL;
  node->lefth = 0;
  node->righth = 0;
  node->localh = 1;

  return node;
}

void avl_delete_node(avl_node_t *node) {
  avl_delete_node_free(node, 0);
}

void avl_delete_node_free(avl_node_t *node, int transactional)
{
  if(transactional) {
    FREE(node, sizeof(avl_node_t));
  } else {
    free(node);
  }
}

avl_intset_t *avl_set_new() {
  return avl_set_new_alloc(0, 0);
}

avl_intset_t *avl_set_new_alloc(int transactional, long nb_threads)
{
  avl_intset_t *set;
  avl_node_t *root;
  int i;
	
  if (transactional) {
    set = (avl_intset_t *)MALLOC(sizeof(avl_intset_t));
  } else {
    set = (avl_intset_t *)malloc(sizeof(avl_intset_t));
  }
  if(set == NULL) {
    perror("malloc");
    exit(1);
  }


  set->t_free_list = (free_list_item **)malloc(nb_threads * sizeof(free_list_item *));
  for(i = 0; i < nb_threads; i++) {
    set->t_free_list[i] = (free_list_item *)malloc(sizeof(free_list_item));
    set->t_free_list[i]->next = NULL;
    set->t_free_list[i]->to_free = NULL;
  }

  set->free_list = (free_list_item *)malloc(sizeof(free_list_item));
  set->free_list->next = NULL;

#ifdef SEPERATE_MAINTENANCE

  set->maint_list_start = (free_list_item **)malloc(nb_threads * sizeof(free_list_item *));
  for(i = 0; i < nb_threads; i++) {
    //set->maint_list_start[i] = set->t_free_list[i];
    set->maint_list_start[i] = NULL;
  }

#endif

#ifdef REMOVE_LATER
  set->to_remove_later = (remove_list_item_t **)malloc(nb_threads * sizeof(remove_list_item_t*));
  for(i = 0; i < nb_threads; i++) {
    set->to_remove_later[i] = NULL;
  }
#endif
  set->nb_threads = nb_threads;
  set->deleted_count = 0;
  set->current_deleted_count = 0;
  set->tree_size = 0;
  set->current_tree_size = 0;
  set->nb_propogated = 0;
  set->nb_rotated = 0;
  set->nb_suc_propogated = 0;
  set->nb_suc_rotated = 0;
  set->nb_removed = 0;
  set->active_remove = 1;

  set->t_nbtrans = (ulong *)malloc(nb_threads * sizeof(ulong));
  set->t_nbtrans_old = (ulong *)malloc(nb_threads * sizeof(ulong));
  

  set->nb_committed = (ulong *)malloc(nb_threads * sizeof(ulong));
  for(i = 0; i < nb_threads; i++) {
    set->nb_committed[i] = 0;
  }
  set->nb_committed_old = (ulong *)malloc(nb_threads * sizeof(ulong));
  for(i = 0; i < nb_threads; i++) {
    set->nb_committed_old[i] = 0;
  }
    
  root = avl_new_simple_node(VAL_MAX, VAL_MAX, transactional);

#ifdef SEPERATE_BALANCE2
  avl_new_balance_node(root, transactional);  
#endif

  set->root = root;
  return set;
}

void avl_set_delete(avl_intset_t *set)
{

  avl_set_delete_node(set->root);
  free(set);

}

void avl_set_delete_node(avl_node_t *node) {

  if(node == NULL) {
    return;
  } 
  //delete your subtrees
  avl_set_delete_node(node->left);
  avl_set_delete_node(node->right);
  
  //delete yourself
  avl_delete_node(node);
  return;
}

int avl_balance(avl_intset_t *set) {
  int right, left;
  if(set->root->left == NULL) return 0;

  left = avl_rec_depth(set->root->left->left);
  right = avl_rec_depth(set->root->left->right);
  return left - right;
}

int avl_rec_depth(avl_node_t *node) {
  int left, right;
  if(node == NULL) {
    return 0;
  }
  left = avl_rec_depth(node->left);
  right = avl_rec_depth(node->right);

  if(left > right) {
    return left + 1;
  }
  return right + 1;

}


int avl_set_size(avl_intset_t *set)
{
  int size = 0;

  avl_set_size_node(set->root->left, &size, 0);

  return size;
}

int avl_tree_size(avl_intset_t *set)
{
  int size = 0;

  avl_set_size_node(set->root->left, &size, 1);

  return size;
}

void avl_set_size_node(avl_node_t *node, int* size, int tree) {

  if(node == NULL) {
    return;
  }
  if(tree || node->deleted == 0) {
    *size = *size + 1;
  }
  avl_set_size_node(node->right, size, tree);
  avl_set_size_node(node->left, size, tree);

  return;

}

#ifndef MICROBENCH
#ifdef KEYMAP

/* =============================================================================
 * rbtree_alloc
 * =============================================================================
 */
rbtree_t*
rbtree_alloc (long (*compare)(const void*, const void*), long nb_threads) {
  return (rbtree_t *)avl_set_new_alloc(0, nb_threads);
}


/* =============================================================================
 * TMrbtree_alloc
 * =============================================================================
 */
rbtree_t*
TMrbtree_alloc (TM_ARGDECL  long (*compare)(const void*, const void*)) {
  return (rbtree_t *)avl_set_new_alloc(1, 0);
}


/* =============================================================================
 * rbtree_free
 * =============================================================================
 */
void
rbtree_free (rbtree_t* r) {
  avl_delete_node_free(((avl_intset_t *)r)->root, 0);
  free((avl_intset_t *)r);
}


/* =============================================================================
 * TMrbtree_free
 * =============================================================================
 */
void
TMrbtree_free (TM_ARGDECL  rbtree_t* r) {
  avl_delete_node_free(((avl_intset_t *)r)->root, 1);
  FREE((avl_intset_t *)r, sizeof(avl_intset_t));
}

#endif
#endif
