/*
 * File:
 *   intset.h
 * Author(s):
 *   Tyler Crain <tyler.crain@irisa.fr>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Operations of the speculation-friendly tree abstraction 
 *
 * Copyright (c) 2009-2010.
 *
 * intset.h is part of Synchrobench
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

#include <unistd.h>
#include "sftree.h"
#ifndef MICROBENCH
#include "thread.h"
#endif


#ifdef TFAVLSEQ
int tfavl_add(avl_intset_t *set, int val, int key);
#endif

//Wrappers?
int avl_contains(avl_intset_t *set, val_t key, int transactional, int id);
int avl_add(avl_intset_t *set, val_t key, int transactional, int id);
#if defined(MICROBENCH)
int avl_remove(avl_intset_t *set, val_t key, int transactional, int id);
#else
int avl_remove(avl_intset_t *set, val_t key, int transactional);
#endif

int avl_move(avl_intset_t *set, int val1, int val2, int transactional, int id);
int avl_snapshot(avl_intset_t *set, int transactional, int id);



#if defined(SEQUENTIAL) || defined(MICROBENCH)

void rec_seq_ss(avl_node_t *node, int *size);

//seqential calls
int avl_req_seq_delete(avl_node_t *parent, avl_node_t *node, val_t key, int go_left, int *success);

inline int avl_req_seq_add(avl_node_t *parent, avl_node_t *node, val_t val, val_t key, int go_left, int *success);

int avl_seq_propogate(avl_node_t *parent, avl_node_t *node, int go_left);

int avl_seq_check_rotations(avl_node_t *parent, avl_node_t *node, int lefth, int righth, int go_left);

int avl_seq_right_rotate(avl_node_t *parent, avl_node_t *place, int left_child);

int avl_seq_left_rotate(avl_node_t *parent, avl_node_t *place, int go_left);

int avl_seq_left_right_rotate(avl_node_t *parent, avl_node_t *place, int go_left);

int avl_seq_right_left_rotate(avl_node_t *parent, avl_node_t *place, int go_left);

int avl_seq_req_find_successor(avl_node_t *parent, avl_node_t *node, int go_left, avl_node_t** succs);



#ifdef KEYMAP

val_t avl_seq_get(avl_intset_t *set, val_t key, int transactional);

inline int avl_req_seq_update(avl_node_t *parent, avl_node_t *node, val_t val, val_t key, int go_left, int *success);

#endif

#endif



//Actual STM methods

#ifdef TINY10B

#ifdef MICROBENCH
int avl_search(val_t key, const avl_intset_t *set, int id);
#else
int avl_search(val_t key, const avl_intset_t *set);
#endif

int avl_find(val_t key, avl_node_t **place, val_t *k, int id);

int avl_find_parent(val_t key, avl_node_t **place, avl_node_t **parent, val_t *k, int id);

#ifdef MICROBENCH
int avl_insert(val_t val, val_t key, const avl_intset_t *set, int id);
#else
int avl_insert(val_t val, val_t key, const avl_intset_t *set);
#endif


#ifdef MICROBENCH
int avl_ss(avl_intset_t *set, int id);
#else
int avl_ss(avl_intset_t *set);
#endif

void rec_ss(avl_node_t *node, int *size);

#ifdef MICROBENCH
int avl_mv(val_t key, val_t new_key, const avl_intset_t *set, int id);
#else
int avl_mv(val_t key, val_t new_key, const avl_intset_t *set);
#endif

#if defined(MICROBENCH)
int avl_delete(val_t key, const avl_intset_t *set, int id);
#else
int avl_delete(val_t key, const avl_intset_t *set);
#endif

int remove_node(avl_node_t *parent, avl_node_t *place);

int avl_rotate(avl_node_t *parent, int go_left, avl_node_t *node);

int avl_single_rotate(avl_node_t *parent, int go_left, avl_node_t *node, int left_rotate, int right_rotate, avl_node_t **child_addr);


int avl_right_rotate(avl_node_t *parent, int go_left, avl_node_t *node, val_t lefth, val_t righth, avl_node_t *left_child, avl_node_t **left_child_addr, int do_rotate);

int avl_left_rotate(avl_node_t *parent, int go_left, avl_node_t *node, val_t lefth, val_t righth, avl_node_t *right_child, avl_node_t **right_child_addr, int do_rotate);

int recursive_tree_propagate(avl_intset_t *set, int id, int num_threads);

#ifdef REMOVE_LATER
int finish_removal(avl_intset_t *set, int id);
#endif

#ifdef SEPERATE_BALANCE2

#ifdef SEPERATE_BALANCE2DEL
int check_remove_list(avl_intset_t *set, ulong *num_rem, free_list_item *free_list);
#endif

int recursive_node_propagate(avl_intset_t *set, balance_node_t *node, balance_node_t *parent, free_list_item *free_list, uint depth, uint depth_del);

int avl_propagate(balance_node_t *node, int left, int *should_rotate);

balance_node_t* check_expand(balance_node_t *node, int go_left);

#else

int recursive_node_propagate(avl_intset_t *set, avl_node_t *node, avl_node_t *parent, uint depth, uint depth_del);

int avl_propagate(avl_node_t *node, int left, int *should_rotate);

#endif

#ifdef KEYMAP
val_t avl_get(val_t key, const avl_intset_t *set);
int avl_update(val_t v, val_t key, const avl_intset_t *set);
#endif

#ifndef MICROBENCH
#ifdef SEQUENTIAL
/* =============================================================================
 * rbtree_insert
 * -- Returns TRUE on success
 * =============================================================================
 */
bool_t
rbtree_insert (rbtree_t* r, void* key, void* val);

/* =============================================================================
 * rbtree_delete
 * =============================================================================
 */
bool_t
rbtree_delete (rbtree_t* r, void* key);

#ifdef KEYMAP

/* =============================================================================
 * rbtree_update
 * -- Return FALSE if had to insert node first
 * =============================================================================
 */
bool_t
rbtree_update (rbtree_t* r, void* key, void* val);


/* =============================================================================
 * rbtree_get
 * =============================================================================
 */
void*
rbtree_get (rbtree_t* r, void* key);

#endif


/* =============================================================================
 * rbtree_contains
 * =============================================================================
 */
bool_t
rbtree_contains (rbtree_t* r, void* key);

#endif
#endif


#ifdef SEPERATE_MAINTENANCE
void do_maintenance_thread(avl_intset_t *tree, int id, int num_threads);
#else
void check_maintenance(avl_intset_t *tree);
void do_maintenance(avl_intset_t *tree);
#endif


#ifndef MICROBENCH
/*
=============================================================================
  * TMrbtree_insert
  * -- Returns TRUE on success
  *
=============================================================================
  */
TM_CALLABLE
bool_t
TMrbtree_insert (TM_ARGDECL  rbtree_t* r, void* key, void* val);

/*
=============================================================================
  * TMrbtree_delete
  *
=============================================================================
  */
TM_CALLABLE
bool_t
TMrbtree_delete (TM_ARGDECL  rbtree_t* r, void* key);

#ifdef KEYMAP
/*
=============================================================================
  * TMrbtree_update
  * -- Return FALSE if had to insert node first
  *
=============================================================================
  */
TM_CALLABLE
bool_t
TMrbtree_update (TM_ARGDECL  rbtree_t* r, void* key, void* val);

/*
=============================================================================
  * TMrbtree_get
  *
=============================================================================
  */
TM_CALLABLE
void*
TMrbtree_get (TM_ARGDECL  rbtree_t* r, void* key);

#endif

/*
=============================================================================
  * TMrbtree_contains
  *
=============================================================================
  */
TM_CALLABLE
bool_t
TMrbtree_contains (TM_ARGDECL  rbtree_t* r, void* key);

#endif


#endif /* TINY10B */

//This is ok here?
#ifndef max
	#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif
