/*
 * File:
 *   intset.c
 * Author(s):
 *   Tyler Crain <tyler.crain@irisa.fr>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Operations of the speculation-friendly tree abstraction
 *
 * Copyright (c) 2009-2010.
 *
 * test_move.c is part of Synchrobench
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
#define CHECK_FIRST

#ifdef NO_UNITLOADS
# define TX_UNIT_LOAD(a) TX_LOAD(a)
# define UNIT_LOAD(a)    TX_LOAD(a)
#endif

int avl_contains(avl_intset_t *set, val_t key, int transactional, int id)
{
  int result = 0;

#ifdef SEQUENTIAL /* Unprotected */	
  val_t val;
  avl_node_t *next;

  next = set->root;
  
  while(next != NULL) {
    val = next->key;
    
    if(key == val) {
      if(next->deleted) {
	//next = next->left;
	return 0;
      } else {
	return 1;
      }
    } else if(val < key) {
      next = next->right;
    } else {
      next = next->left;
    }
    
  }
  return 0;
  
#elif defined TINY10B
  result = avl_search(key, set, id);
#endif
  
  return result;
}

inline int avl_seq_add(avl_intset_t *set, val_t key) {
  avl_node_t *new_node, *next, *prev = NULL;
  val_t val = 0;
  
  next = set->root;
  
  while(next != NULL) {
    prev = next;
    val = prev->key;
    
    if(key == val) {
      if(!prev->deleted) {
	return 0;
      } else {
	prev->deleted = 0;
	return 1;
      }
    } else if(key > val) {
      next = prev->right;
    } else {
      next = prev->left;
    }
    
  }
  
  //insert the node
  new_node = avl_new_simple_node(key, key, 0);
  if(key > val) {
    prev->right = new_node;
  } else {
    prev->left = new_node;
  }
  
  return 1;
  
}

int avl_add(avl_intset_t *set, val_t key, int transactional, int id)
{
  int result = 0;

  if (!transactional) {
		
#ifdef TFAVLSEQ
    result = tfavl_add(set, key, key);
#else	
    avl_req_seq_add(NULL, set->root, key, key, 0, &result);
#endif
	
  } else {

#ifdef SEQUENTIAL
    avl_req_seq_add(NULL, set->root, key, key, 0, &result);
#elif defined(MICROBENCH) && defined(TINY10B)
    result = avl_insert(key, key, set, id);
#endif
		
  }
	
  return result;
}

#ifdef TFAVLSEQ

int tfavl_move(avl_intset_t *set, int key1, int key2) {
  avl_node_t *next, *prev, *new_node, *first;

  next = set->root;
  while(next != NULL) {
    prev = next;
    if(prev->key == key1) {
      if(prev->deleted) {
	first = prev;
      } else {
	return 0;
      }
    } else if(prev->key < key1) {
      next = prev->right;
    } else {
      next = prev->left;
    }
  }

  next = set->root;
  while(next != NULL) {
    prev = next;
    if(prev->key == key2) {
      if(prev->deleted) {
	first->deleted = 1;
	prev->deleted = 0;
	return 1;
      } else {
	return 0;
      }
    } else if(prev->key < key2) {
      next = prev->right;
    } else {
      next = prev->left;
    }
  }

  new_node = avl_new_simple_node(key2, key2, 0);
  if(prev->key < key2) {
    first->deleted = 1;
    prev->right = new_node;
  } else {
    first->deleted = 1;
    prev->left = new_node;;
  }
  
  return 1;
}


int tfavl_delete(avl_intset_t *set, int key) {
  int ret;
  avl_node_t *next, *prev, *parent;

  next = set->root;
  prev = next;
  while(next != NULL) {
    parent = prev;
    prev = next;
    if(prev->key == key) {
      if(prev->deleted) {
	ret = 0;
      } else {
	prev->deleted = 1;
	ret = 1;
      }
      return ret;
    } else if(prev->key < key) {
      next = prev->right;
    } else {
      next = prev->left;
    }
  }

  return 0;
}

int tfavl_add(avl_intset_t *set, int val, int key) {
  avl_node_t *next, *prev, *new_node;
  
  next = set->root;
  while(next != NULL) {
    prev = next;
    if(prev->key == key) {
      if(prev->deleted) {
	prev->deleted = 0;
	return 1;
      } else {
	return 0;
      }
    } else if(prev->key < key) {
      next = prev->right;
    } else {
      next = prev->left;
    }
  }

  new_node = avl_new_simple_node(val, key, 0);
  if(prev->key < key) {
    prev->right = new_node;
  } else {
    prev->left = new_node;;
  }

  return 1;
}

#endif

int avl_move(avl_intset_t *set, int val1, int val2, int transactional, int id) {
  int result;
  if(!transactional) {
#ifdef TFAVLSEQ
  result = tfavl_move(set, val1, val2);
#else
    if(avl_contains(set, val1, 0, id) && !avl_contains(set, val2, 0, id)) {
      avl_req_seq_delete(NULL, set->root, val1, 0, &result);
      avl_req_seq_add(NULL, set->root, val2, val2, 0, &result);
    }
#endif
  }
  else {
#if defined(MICROBENCH) && defined(TINY10B)
    result = avl_mv(val1, val2, set, id);
#endif
  }
  return result;
}

int avl_snapshot(avl_intset_t *set, int transactional, int id) {
  int result = 0;

  if(!transactional) {
    rec_seq_ss(set->root->left, &result);
  } else {
#if defined(MICROBENCH) && defined(TINY10B)
    result = avl_ss(set, id);
#endif
  }
  return result;
}

void rec_seq_ss(avl_node_t *node, int *size) {
  if(node == NULL) {
    return;
  }
  if(!node->deleted) {
    *size += node->key;
  }
  rec_seq_ss(node->left, size);
  rec_seq_ss(node->right, size);

  return;
}

#if defined(MICROBENCH)
int avl_remove(avl_intset_t *set, val_t key, int transactional, int id)
#else
int avl_remove(avl_intset_t *set, val_t key, int transactional)
#endif
{
  int result = 0;
	
#ifdef SEQUENTIAL

#ifdef TFAVLSEQ
  result = tfavl_delete(set, key);
#else	
  avl_req_seq_delete(NULL, set->root, key, 0, &result);
#endif

#elif defined TINY10B

#if defined(MICROBENCH)
  result = avl_delete(key, set, id);
#else
  result = avl_delete(key, set);
#endif

#endif
	
  return result;
}

int avl_req_seq_delete(avl_node_t *parent, avl_node_t *node, val_t key, int go_left, int *success) {
  avl_node_t *succs;
  int ret;

  if(node != NULL) {
    if(key == node->key) {
      if(node->deleted) {
	*success = 0;
	return 0;
      } else {
	//remove and find successor
	if(node->left == NULL) {
	  if(go_left) {
	    parent->left = node->right;
#ifdef SEPERATE_BALANCE2
	    if(node->bnode == NULL) {
	      avl_new_balance_node(node, 0);
	    }
	    if(parent->bnode == NULL) {
	      avl_new_balance_node(parent, 0);
	    }
	    parent->bnode->left = node->bnode->right;
	    if(node->bnode->right != NULL) {
	      parent->bnode->left->parent = parent->bnode;
	    }
#endif
	  } else {
	    parent->right = node->right;
#ifdef SEPERATE_BALANCE2
	    if(parent->bnode == NULL) {
	      avl_new_balance_node(parent, 0);
	    }
	    if(node->bnode != NULL) {
	      parent->bnode->right = node->bnode->right;
	      if(node->bnode->right != NULL) {
		parent->bnode->right->parent = parent->bnode;
	      }
	    } else {
	      parent->bnode->right = NULL;
	    }

#endif
	  }
#ifdef SEPERATE_BALANCE
	  if(node->bnode != NULL) {
	    free(node->bnode);
	  }
#endif
	  free(node);
	  *success = 1;
	  return 1;
	} else if(node->right == NULL) {
	  if(go_left) {
	    parent->left = node->left;
#ifdef SEPERATE_BALANCE2
	    if(parent->bnode == NULL) {
	      avl_new_balance_node(parent, 0);
	    }
	    if(node->bnode != NULL) {
	      parent->bnode->left = node->bnode->left;
	      if(node->bnode->left != NULL) {
		parent->bnode->left->parent = parent->bnode;
	      }
	    } else {
	      parent->bnode->left = NULL;
	    }
#endif
	  } else {
	    parent->right = node->left;
#ifdef SEPERATE_BALANCE2
	    if(node->bnode != NULL) {
	      parent->bnode->right = node->bnode->left;
	      if(node->bnode->left != NULL) {
		parent->bnode->right->parent = parent->bnode;
	      }
	    } else {
	      parent->bnode->right = NULL;
	    }
#endif
	  }
#ifdef SEPERATE_BALANCE
	  if(node->bnode != NULL) {
	    free(node->bnode);
	  }
#endif
	  free(node);
	  *success = 1;
	  return 1;
	} else {
	  ret = avl_seq_req_find_successor(node, node->right, 0, &succs);
	  succs->right = node->right;
#ifdef SEPERATE_BALANCE2
	  if(node->bnode == NULL) {
	    avl_new_balance_node(node, 0);
	  }
	  succs->bnode->right = node->bnode->right;
	  if(node->bnode->right != NULL) {
	    succs->bnode->right->parent = succs->bnode;
	  }
#endif
	  succs->left = node->left;
#ifdef SEPERATE_BALANCE2
	  if(node->bnode == NULL) {
	    avl_new_balance_node(node, 0);
	  }
	  succs->bnode->left = node->bnode->left;
	  if(node->bnode->left != NULL) {
	    succs->bnode->left->parent = succs->bnode;
	  }
#endif
	  if(go_left) {
	    parent->left = succs;
#ifdef SEPERATE_BALANCE2
	    parent->bnode->left = succs->bnode;
	    succs->bnode->parent = parent->bnode;
#endif
	  } else {
	    parent->right = succs;
#ifdef SEPERATE_BALANCE2
	    parent->bnode->right = succs->bnode;
	    succs->bnode->parent = parent->bnode;
#endif
	  }
#ifdef SEPERATE_BALANCE
	  if(node->bnode != NULL) {
	    free(node->bnode);
	  }
#endif
	  free(node);
	  *success = 1;
	  return avl_seq_propogate(parent, succs, go_left);
	}
      }
    } else if(key > node->key) {
      ret = avl_req_seq_delete(node, node->right, key, 0, success);
    } else {
      ret = avl_req_seq_delete(node, node->left, key, 1, success);
    }
    
    if(ret != 0) {
      return avl_seq_propogate(parent, node, go_left);
    }
    return 0;

  } else {
    //did not find the node
    *success = -1;
    return 0;
  }
  
  return 0;

}


inline int avl_req_seq_add(avl_node_t *parent, avl_node_t *node, val_t val, val_t key, int go_left, int *success) {
  avl_node_t *new_node;
  int ret;
#ifdef SEPERATE_BALANCE2
  balance_node_t *bnode;
#endif
  
  if(node != NULL) {    
    if(key == node->key) {
      if(!node->deleted) {
	//found the node
	*success = -1;
	return 0;
      } else {
	node->deleted = 0;
	node->val = val;
	*success = 1;
	return 0;
      }
    } else if(key > node->key) {
      ret = avl_req_seq_add(node, node->right, val, key, 0, success);
    } else {
      ret = avl_req_seq_add(node, node->left, val, key, 1, success);
    }
    
    if(ret != 0) {
      return avl_seq_propogate(parent, node, go_left);
    }
    return 0;

  } else {
    //insert the node
    new_node = avl_new_simple_node(val, key, 0);
#ifdef SEPERATE_BALANCE2
    bnode = avl_new_balance_node(new_node, 0);
    bnode->parent = parent->bnode;
#endif
    if(key > parent->key) {
      parent->right = new_node;
#ifdef SEPERATE_BALANCE2
      parent->bnode->right = bnode;
#endif
    } else {
      parent->left = new_node;
#ifdef SEPERATE_BALANCE2
      parent->bnode->left = bnode;
#endif
    }
    //printf("seq insert key %d\n", key);
    *success = 1;
  }
  
  return 1;
  
}



int avl_seq_propogate(avl_node_t *parent, avl_node_t *thenode, int go_left) {
  int new_localh;
#ifdef SEPERATE_BALANCE
  balance_node_t *node;
  node = thenode->bnode;
#else
  avl_node_t *node;
  node = thenode;
#endif

  if(node->left != NULL) {
    node->lefth = node->left->localh;
  } else {
    node->lefth = 0;
  }
  if(node->right != NULL) {
    node->righth = node->right->localh;
  } else {
    node->righth = 0;
  }

  new_localh = 1 + max(node->lefth, node->righth);
  if(new_localh != node->localh) {
    node->localh = new_localh;

    //need to do rotations
    if(parent != NULL) {
#ifdef SEPERATE_BALANCE
      avl_seq_check_rotations(parent, thenode, node->lefth, node->righth, go_left);
#else
      avl_seq_check_rotations(parent, node, node->lefth, node->righth, go_left);
#endif
    }
    return 1;
  }
  return 0;
}

int avl_seq_check_rotations(avl_node_t *parent, avl_node_t *node, int lefth, int righth, int go_left) {
  int local_bal, lbal, rbal;

  local_bal = lefth - righth;

  if(local_bal > 1) {
#ifdef SEPERATE_BALANCE
    lbal = node->bnode->left->lefth - node->bnode->left->righth;
#else
    lbal = node->left->lefth - node->left->righth;
#endif
    if(lbal >= 0) {
      avl_seq_right_rotate(parent, node, go_left);
    } else {
      avl_seq_left_right_rotate(parent, node, go_left);
    }

  } else if(local_bal < -1) {
#ifdef SEPERATE_BALANCE
    rbal = node->bnode->right->lefth - node->bnode->right->righth;
#else
    rbal = node->right->lefth - node->right->righth;
#endif
    if(rbal < 0) {
      avl_seq_left_rotate(parent, node, go_left);
    } else {
      avl_seq_right_left_rotate(parent, node, go_left);
    }
  }

  return 1;
}


int avl_seq_right_rotate(avl_node_t *parent, avl_node_t *place, int go_left) {
  avl_node_t *u, *v;
#ifdef SEPERATE_BALANCE
  balance_node_t *ub, *vb, *pb;
  pb = parent->bnode;
#endif

  v = place;
  u = place->left;
  if(u == NULL || v == NULL) {
    return 0;
  }
#ifdef SEPERATE_BALANCE
  ub = u->bnode;
  vb = v->bnode;
  if(ub == NULL) {
    ub = avl_new_balance_node(u, 0);
  }
  if(vb == NULL) {
    vb = avl_new_balance_node(v, 0);
  }

#endif
  if(go_left) {
    parent->left = u;
#ifdef SEPERATE_BALANCE2
    pb->left = ub;
    ub->parent = pb;
#endif
  } else {
    parent->right = u;
#ifdef SEPERATE_BALANCE2
    pb->right = ub;
    ub->parent = pb;
#endif
  }

  v->left = u->right;
  u->right = v;

#ifdef SEPERATE_BALANCE2
  vb->left = ub->right;
  ub->right = vb;
#endif
#ifdef SEPERATE_BALANCE
  //now need to do propogations
  ub->localh = vb->localh - 1;
  vb->lefth = ub->righth;
  vb->localh = 1 + max(vb->lefth, vb->righth);
  ub->righth = vb->localh;
#else
  //now need to do propogations
  u->localh = v->localh - 1;
  v->lefth = u->righth;
  v->localh = 1 + max(v->lefth, v->righth);
  u->righth = v->localh;
#endif
  return 1;
}


int avl_seq_left_rotate(avl_node_t *parent, avl_node_t *place, int go_left) {
  avl_node_t *u, *v;
#ifdef SEPERATE_BALANCE
  balance_node_t *ub, *vb, *pb;
  pb = parent->bnode;
#endif

  v = place;
  u = place->right;
  if(u == NULL || v == NULL) {
    return 0;
  }
#ifdef SEPERATE_BALANCE
  ub = u->bnode;
  vb = v->bnode;
  if(ub == NULL) {
    ub = avl_new_balance_node(u, 0);
  }
  if(vb == NULL) {
    vb = avl_new_balance_node(v, 0);
  }
#endif
  if(go_left) {
    parent->left = u;
#ifdef SEPERATE_BALANCE2
    pb->left = ub;
    ub->parent = pb;
#endif
  } else {
    parent->right = u;
#ifdef SEPERATE_BALANCE2
    pb->right = ub;
    ub->parent = pb;
#endif
  }

  v->right = u->left;
  u->left = v;

#ifdef SEPERATE_BALANCE2
  vb->right = ub->left;
  ub->left = vb;
#endif
#ifdef SEPERATE_BALANCE
  //now need to do propogations
  ub->localh = vb->localh - 1;
  vb->righth = ub->lefth;
  vb->localh = 1 + max(vb->lefth, vb->righth);
  ub->lefth = vb->localh;
#else
  //now need to do propogations
  u->localh = v->localh - 1;
  v->righth = u->lefth;
  v->localh = 1 + max(v->lefth, v->righth);
  u->lefth = v->localh;
#endif
  
  return 1;
}

int avl_seq_left_right_rotate(avl_node_t *parent, avl_node_t *place, int go_left) {
  
  avl_seq_left_rotate(place, place->left, 1);
  avl_seq_right_rotate(parent, place, go_left);
  return 1;
}

int avl_seq_right_left_rotate(avl_node_t *parent, avl_node_t *place, int go_left) {
  
  avl_seq_right_rotate(place, place->right, 0);
  avl_seq_left_rotate(parent, place, go_left);
  return 1;
}

int avl_seq_req_find_successor(avl_node_t *parent, avl_node_t *node, int go_left, avl_node_t** succs) {
  int ret;

  //smallest node in right sub-tree
  if(node->left == NULL) {
    //this is the succs
    if(go_left) {
      parent->left = node->right;
#ifdef SEPERATE_BALANCE2
      if(node->bnode == NULL) {
	avl_new_balance_node(node, 0);
      }
      if(parent->bnode == NULL) {
	avl_new_balance_node(parent, 0);
      }
      parent->bnode->left = node->bnode->right;
      if(node->bnode->right != NULL) {
	parent->bnode->left->parent = parent->bnode;
      }
#endif
#ifdef SEPERATE_BALANCE
      parent->bnode->lefth = node->bnode->localh - 1;
#else 
      parent->lefth = node->localh - 1;
#endif
    } else {
      parent->right = node->right;
#ifdef SEPERATE_BALANCE2
      if(node->bnode == NULL) {
	avl_new_balance_node(node, 0);
      }
      if(parent->bnode == NULL) {
	avl_new_balance_node(parent, 0);
      }
      parent->bnode->right = node->bnode->right;
      if(node->bnode->right != NULL) {
	parent->bnode->right->parent = parent->bnode;
      }
#endif
#ifdef SEPERATE_BALANCE
      parent->bnode->righth = node->bnode->localh - 1;
#else
      parent->righth = node->localh - 1;
#endif
    }
    *succs = node;
    return 1;
  }
  ret = avl_seq_req_find_successor(node, node->left, 1, succs);
  if(ret == 1) {
    return avl_seq_propogate(parent, node, go_left);
  }
  return ret;
}

#ifdef TINY10B

#ifdef MICROBENCH
int avl_search(val_t key, const avl_intset_t *set, int id) {
#else
int avl_search(val_t key, const avl_intset_t *set) {
#endif
  int done;
  intptr_t del = 0;
  avl_node_t *place;
  val_t k;
#ifndef MICROBENCH
  long id;
#endif
#ifdef DEL_COUNT
  int del_val;
#endif

#ifdef PRINT_INFO
  printf("searching %d thread: %d\n", key, thread_getId());
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif

  TX_START(NL);
  place = set->root;
  done = 0;
  
#ifdef DEL_COUNT
  del_val = avl_find(key, &place, &k, id);
#else
  avl_find(key, &place, &k, id);
#endif
  
  if(k != key) {
    done = 1;
  }
  
  if(!done) {
    del = (intptr_t)TX_LOAD(&place->deleted);
  }

  TX_END;

#ifdef DEL_COUNT
  if(del_val > DEL_THRESHOLD) {
    if(set->active_del[id]) {
      set->active_del[id] = 0;
    }
  } else {
    if(!set->active_del[id]) {
      set->active_del[id] = 1;
    }
  }
#endif

  if(done) {
    return 0;
  } else if(del) {
    return 0;
  }
  return 1;

}


 int avl_find(val_t key, avl_node_t **place, val_t *val, int id)
 {

  avl_node_t *parent;

  parent = *place;

  return avl_find_parent(key, place, &parent, val, id);
}

 int avl_find_parent(val_t key, avl_node_t **place, avl_node_t **parent, val_t *val, int id)
{
  avl_node_t *next, *placet, *parentt = NULL;
  val_t valt;
  
  next = *place;
  placet = *parent;
  valt = *val;

  while(1) { 
    parentt = placet;
    placet = next;
    valt = (placet)->key;

    if(key == valt) {
      break;
    }

    if(key != valt) {
      if(key > valt) {
	next = (avl_node_t*)TX_LOAD(&(placet)->right);
      } else {
	next = (avl_node_t*)TX_LOAD(&(placet)->left);
      }
      if(next == NULL) {
	break;
      }
    }
  }
  *val = valt;
  *place = placet;
  *parent = parentt;

  return 0;
}

#ifdef MICROBENCH
int avl_insert(val_t v, val_t key, const avl_intset_t *set, int id) {
#else
int avl_insert(val_t v, val_t key, const avl_intset_t *set) {
#endif
  avl_node_t *place, *new_node;
  intptr_t del;
  int done, go_left = 0;
  int ret;
  val_t k;
#ifndef MICROBENCH
  long id;
#endif
#ifdef DEL_COUNT
  int del_val = 0;
#endif

#ifdef PRINT_INFO
  printf("inserting %d thread: %d\n", key, thread_getId());  
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif

  TX_START(NL);
  place = set->root;
  ret = 0;
  done = 0;
  
#ifdef DEL_COUNT
  del_val = avl_find(key, &place, &k, id);
#else
  avl_find(key, &place, &k, id);
#endif
  if(k == key) {
    done = 1;
  } else if(key > k){
    go_left = 0;
  } else {
    go_left = 1;
  }

  if(done) {
    del = (intptr_t)TX_LOAD(&place->deleted);
    if(del) {
      TX_STORE(&place->deleted, 0);
#ifdef KEYMAP
      TX_STORE(&place->val, v);
#endif
      ret = 1;
    } else {
      ret = 0;
    }
  } else {

#ifdef CHANGE_KEY
    if((del = (intptr_t)TX_LOAD(&place->deleted))) {
      TX_STORE(&place->deleted, 0);
      TX_STORE(&place->key, key);
      TX_STORE(&place->val, v);
      ret = 1;
    } else {
#endif
    
      new_node = avl_new_simple_node(v, key, 1);
      if(go_left) {
	TX_STORE(&place->left, new_node);
      } else {
	TX_STORE(&place->right, new_node);
      }
      ret = 2;
#ifdef CHANGE_KEY
    }
#endif
  }

  TX_END;

#ifdef DEL_COUNT
  if(del_val > DEL_THRESHOLD) {
    if(set->active_del[id]) {
      set->active_del[id] = 0;
    }
  } else {
    if(!set->active_del[id]) {
      set->active_del[id] = 1;
    }
  }
#endif

  return ret;
}

#ifdef MICROBENCH
 int avl_ss(avl_intset_t *set, int id) {
#else
   int avl_ss(avl_intset_t *set) {
#endif
     //  int size = 0;

   /*   TX_START(NL); */
   /*   rec_ss((avl_node_t *)TX_LOAD(&set->root->left), &size); */
   /*   TX_END; */

   /*   return size; */
   /* } */

   /* void rec_ss(avl_node_t *node, int *size) { */
   /*   if(node == NULL) { */
   /*     return; */
   /*   } */
   /*   if(!TX_LOAD(&node->deleted)) { */
   /*     *size += node->key; */
   /*   } */
   /*   rec_ss((avl_node_t *)TX_LOAD(&node->left), size); */
   /*   rec_ss((avl_node_t *)TX_LOAD(&node->right), size); */

     return 0;
   }



#ifdef SMART_MOVE

#ifdef MICROBENCH
 int avl_mv(val_t key, val_t new_key, const avl_intset_t *set, int id) {
#else
   int avl_mv(val_t key, val_t new_key, const avl_intset_t *set) {
#endif
  avl_node_t *place, *new_place, *new_node;
  intptr_t del;
  int done, go_left = 0;
  int ret;
  val_t k;
#ifndef MICROBENCH
  long id;
#endif
#ifdef DEL_COUNT
  int del_val = 0;
#endif

#ifdef PRINT_INFO
  printf("smart moving %d thread: %d\n", key, thread_getId());  
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif


  TX_START(NL);

  place = set->root;

  new_place = set->root;

  ret = 0;
  rem = 1;
  done = 0;

#ifdef DEL_COUNT
  del_val = avl_find(key, &place, &k, id);
#else
  avl_find(key, &place, &k, id);
#endif
  if(k == key) {
    done = 1;
  } else if(key > k){
    go_left = 0;
  } else {
    go_left = 1;
  }

  if(done) {
    if((del = (intptr_t)TX_LOAD(&place->deleted))) {
      //found the node deleted, cannot do the move
      ret = 0;
    } else {
      //node is not deleted, can move
      ret = 1;
    }
  } else {
    //did not find the node, cannot do move
    ret = 0;
  }

  if(ret > 0) {
    done = 0;
    
#ifdef DEL_COUNT
    del_val = avl_find(new_key, &new_place, &k, id);
#else
    avl_find(new_key, &new_place, &k, id);
#endif
    if(k == new_key) {
      done = 1;
    } else if(new_key > k){
      go_left = 0;
    } else {
      go_left = 1;
    }
    
    if(done) {
      if((del = (intptr_t)TX_LOAD(&new_place->deleted))) {
	//found the node deleted, can move here
	ret = 1;
      } else {
	//node is not deleted, canont move here
	ret = 0;
      }
    } else {
      //did not find the node, can move here
      ret = 2;
    }
    
    if(ret > 0) {
      //delete the node, just do a lazy delete for now
      TX_STORE(&place->deleted, 1);

      //insert the new node
      if(ret == 1) {
	TX_STORE(&new_place->deleted, 0);
#ifdef KEYMAP
	TX_STORE(&new_place->val, TX_LOAD(&place->val));
#endif
      } else if(ret == 2) {
	new_node = avl_new_simple_node(TX_LOAD(&place->val), new_key, 1);
	if(go_left) {
	  TX_STORE(&new_place->left, new_node);
	} else {
	  TX_STORE(&new_place->right, new_node);
	}
	ret = 2;
      }
    }
  }

  TX_END;

#ifdef DEL_COUNT
#ifndef MICROBENCH
  id = thread_getId();
#endif
  if(del_val > DEL_THRESHOLD) {
    if(set->active_del[id]) {
      set->active_del[id] = 0;
    }
  } else {
    if(!set->active_del[id]) {
      set->active_del[id] = 1;
    }
  }
#endif

  return ret;

}

#else


#ifdef MICROBENCH
 int avl_mv(val_t key, val_t new_key, const avl_intset_t *set, int id) {
#else
   int avl_mv(val_t key, val_t new_key, const avl_intset_t *set) {
#endif
  int ret;
#ifndef MICROBENCH
  long id = 0;
#endif

#ifdef PRINT_INFO
  printf("moving %d thread: %d\n", key, thread_getId());  
#endif

  TX_START(NL);

#ifdef MICROBENCH
  if(!avl_search(new_key, set, id)) {
#else
    if(!avl_search(new_key, set)) {
#endif
      
#ifdef MICROBENCH
      ret = avl_delete(key, set, id);
#else
      ret = avl_delete(key, set);
#endif
      
    if(ret > 0) {
      
#ifdef MICROBENCH
      ret = avl_insert(new_key, new_key, set, id);
#else
      ret = avl_insert(new_key, new_key, set);
#endif
      
    }
  }    
    
  TX_END;
    
  return ret;
}


#endif /* SMART_MOVE */


#if defined(MICROBENCH)
int avl_delete(val_t key, const avl_intset_t *set, int id) {
#else
int avl_delete(val_t key, const avl_intset_t *set) {
#endif
  avl_node_t *place, *parent;
  intptr_t del;
  int done;
  int ret;
  val_t k;
#if !defined(SEPERATE_BALANCE2) || defined(SEPERATE_BALANCE2NLDEL)
#ifdef REMOVE_LATER
  remove_list_item_t *next_remove;
#endif
#endif
#ifndef MICROBENCH
  long id;
#endif
#ifdef DEL_COUNT
  int del_val;
#endif

#ifdef PRINT_INFO
  printf("deleting %d thread: %d\n", key, thread_getId());  
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif

  TX_START(NL);
  place = set->root;
  parent = set->root;
  ret  = 0;
  done = 0;

#ifdef DEL_COUNT
  del_val = avl_find_parent(key, &place, &parent, &k, id);
#else
  avl_find_parent(key, &place, &parent, &k, id);
#endif
  if(k != key) {
    done = 1;
    ret = 0;
  }
  if(!done) {
    del = (intptr_t)TX_LOAD(&place->deleted);
    if(del) {
      ret = 0;
    } else {
      TX_STORE(&place->deleted, 1);
      ret = 1;
    }
  }

#if defined(SEPERATE_BALANCE2DEL) || defined(REMOVE_LATER)
  if(ret == 1 && set->active_remove && ((avl_node_t *)TX_UNIT_LOAD(&place->left) == NULL || (avl_node_t *)TX_UNIT_LOAD(&place->right) == NULL)) {

#ifdef REMOVE_LATER
      next_remove = (remove_list_item_t*)MALLOC(sizeof(remove_list_item_t));
      next_remove->item = place;
      next_remove->parent = parent;
      next_remove->next = (remove_list_item_t*)TX_UNIT_LOAD(&set->to_remove_later[id]);
      TX_STORE(&set->to_remove_later[id], next_remove);
#else
    TX_STORE(&set->to_remove[id], place);
    TX_STORE(&set->to_remove_parent[id], parent);

#endif

  }

#endif
  TX_END;

#ifdef DEL_COUNT
#ifndef MICROBENCH
  id = thread_getId();
#endif
  if(del_val > DEL_THRESHOLD) {
    if(set->active_del[id]) {
      set->active_del[id] = 0;
    }
  } else {
    if(!set->active_del[id]) {
      set->active_del[id] = 1;
    }
  }
#endif

  return ret;
}
 

#ifdef REMOVE_LATER
 int finish_removal(avl_intset_t *set, int id) {
   remove_list_item_t *next, *tmp;
   avl_node_t *place, *parent;
   free_list_item *free_item;
   free_list_item *free_list;
   int ret;
   int id = 99;

   next = set->to_remove_later[id];
   set->to_remove_later[id] = NULL;

   while(next != NULL) {
     parent = next->parent;
     place = next->item;

     ret = remove_node(parent, place);
     if(ret > 1) {
#ifdef SEPERATE_MAINTENANCE
       TX_START(NL);
#endif
       //free_list = set->t_free_list[id];
       free_list = (free_list_item *)TX_UNIT_LOAD(&set->t_free_list[id]);
       //add it to the garbage collection
       free_item = (free_list_item *)MALLOC(sizeof(free_list_item));
       free_item->to_free = place;
#ifdef SEPERATE_MAINTENANCE
       free_item->next = free_list;
       TX_STORE(&set->t_free_list[id], free_item);
       TX_END;
#else
       free_item->next = NULL;
       while(free_list->next != NULL) {
	 free_list = free_list->next;
       }
       //Should only do this if in a transaction
       TX_STORE(&free_list->next, free_item);
#endif
       
     }
     tmp = next;
     next = next->next;
     free(tmp);
   }
   return 0;
 }
#endif

int remove_node(avl_node_t *parent, avl_node_t *place) {
  avl_node_t *right_child, *left_child, *parent_child;
  int go_left;
  val_t v;
  int ret;
  intptr_t rem, del;
  val_t lefth = 0, righth = 0, new_localh;
#ifdef CHECK_FIRST
  val_t parent_localh;
#endif
#ifdef SEPERATE_BALANCE
  balance_node_t *bnode;
  bnode = parent->bnode;
#ifdef SEPERATE_BALANCE2
  balance_node_t *child_bnode;
#endif
#endif

  TX_START(NL);
#ifdef CHANGE_KEY
  v = (val_t)TX_UNIT_LOAD(&place->key);
#else
  v = place->key;
#endif
  ret = 1;
  right_child = NULL;
  left_child = NULL;
  parent_child = NULL;
  rem = (intptr_t)TX_LOAD(&place->removed);
  del = (intptr_t)TX_LOAD(&place->deleted);
  if(!rem && del) {
#ifdef CHECK_FIRST
#ifdef SEPERATE_BALANCE
    if(bnode != NULL) {
      parent_localh = (val_t)TX_UNIT_LOAD(&bnode->localh);
    } else {
      parent_localh = 1;
    }
#else
#endif
#endif
    left_child = (avl_node_t *)TX_LOAD(&place->left);
    right_child = (avl_node_t *)TX_LOAD(&place->right);
    
    if(left_child == NULL || right_child == NULL) {
      rem = (intptr_t)TX_LOAD(&parent->removed);
      if(!rem) {
	//make sure parent is correct
	if(v > parent->key) {
	  parent_child = (avl_node_t*)TX_LOAD(&parent->right);
	  go_left = 0;
	} else {
	  parent_child = (avl_node_t*)TX_LOAD(&parent->left);
	  go_left = 1;
	}
	  
	// this section here is only here because something weird 
	// with the unit loads it gets stuck if it is below
	if(left_child == NULL && right_child == NULL) {
	  if(go_left) {
	    lefth = 0;
#ifdef SEPERATE_BALANCE
	    if(bnode != NULL) {
	      righth = (val_t)TX_UNIT_LOAD(&bnode->righth);
	    } else {
	      righth = 0;
	    }
#else
	    righth = (val_t)TX_UNIT_LOAD(&parent->righth);
#endif
	  } else {
	    righth = 0;
#ifdef SEPERATE_BALANCE
	    if(bnode != NULL) {
	      lefth = (val_t)TX_UNIT_LOAD(&bnode->lefth);
	    } else {
	      lefth = 0;
	    }
#else
	    lefth = (val_t)TX_UNIT_LOAD(&parent->lefth);
#endif
	  }
	}
	  
	if(parent_child == place) {
	  //remove the node
	  if(go_left) {
	    if(left_child == NULL) {
	      TX_STORE(&parent->left, right_child);
	    } else {
	      TX_STORE(&parent->left, left_child);
	    }
	  } else {
	    if(left_child == NULL) {
	      TX_STORE(&parent->right, right_child);
	    } else {
	      TX_STORE(&parent->right, left_child);
	    }
	  }
	  
	  if(left_child == NULL && right_child == NULL) {
	    if(go_left) {
#ifdef SEPERATE_BALANCE
	      if(bnode != NULL) {
		TX_STORE(&bnode->lefth, 0);
	      }
#endif
	      lefth = 0;
	    } else {
#ifdef SEPERATE_BALANCE
	      if(bnode != NULL) {
		TX_STORE(&bnode->righth, 0);
	      }
#endif
	      righth = 0;
	    }
	    
	    // Could also do a read here to check if 
	    // should change before writing?
	    // Good or bad for performance? Dunno?
	    new_localh = 1 + max(righth, lefth);
#ifdef CHECK_FIRST
	    if(parent_localh != new_localh) {
#ifdef SEPERATE_BALANCE
	      if(bnode != NULL) {
		TX_STORE(&bnode->localh, new_localh);
	      }
#endif
	    }
#else
#ifdef SEPERATE_BALANCE
	    if(bnode != NULL) {
	      TX_STORE(&bnode->localh, new_localh);
	    }
#endif
#endif
	  }
	  FREE(place, sizeof(avl_node_t));
	  // Should update parent heights?
	  ret = 2;
	}
      }
    }
  }
  TX_END;
  
  return ret;
}

int avl_rotate(avl_node_t *parent, int go_left, avl_node_t *node) {
  int ret = 0;
  avl_node_t *child_addr = NULL;
  
  ret = avl_single_rotate(parent, go_left, node, 0, 0, &child_addr);
  if(ret == 2) {
    //Do a LRR
    ret = avl_single_rotate(node, 1, child_addr, 1, 0, &child_addr);
    if(ret > 0) {
      avl_single_rotate(parent, go_left, child_addr, 0, 1, NULL);
    }
  } else if(ret == 3) {
    //Do a RLR
    ret = avl_single_rotate(node, 0, child_addr, 0, 1, &child_addr);
    if(ret > 0) {
      avl_single_rotate(parent, go_left, child_addr, 1, 0, NULL);
    }

  }
  return ret;
}

int avl_single_rotate(avl_node_t *parent, int go_left, avl_node_t *node, int left_rotate, int right_rotate, avl_node_t **child_addr) {
  val_t lefth, righth, bal, child_localh;
  avl_node_t *child, *check;
  int ret;
  intptr_t rem1, rem2;
#ifdef SEPERATE_BALANCE
  balance_node_t *bnode;
#endif

  TX_START(NL);
  ret = 0;
  rem1 = (intptr_t)TX_LOAD(&node->removed);
  rem2 = (intptr_t)TX_LOAD(&parent->removed);
  if(rem1 || rem2) {
    ret = 0;
  } else {
    if(go_left) {
      check = (avl_node_t*)TX_LOAD(&parent->left);
    } else {
      check = (avl_node_t*)TX_LOAD(&parent->right);
    }
    if(check == node) {
#ifdef SEPERATE_BALANCE
      //do update to values here
      bnode = node->bnode;
      lefth = TX_UNIT_LOAD(&bnode->lefth);
      righth = TX_UNIT_LOAD(&bnode->righth);
#else
      lefth = TX_LOAD(&node->lefth);
      righth = TX_LOAD(&node->righth);
#endif
      bal = lefth - righth;
      
      if(bal >= 2 || right_rotate) {
	child = (avl_node_t *)TX_LOAD(&node->left);
	if(child != NULL) {
#ifdef SEPERATE_BALANCE	  
	  if(child->bnode == NULL) {
	    avl_new_balance_node(child, 0);
	    child_localh = 1;
	  } else {
	    child_localh = (val_t)TX_UNIT_LOAD(&child->bnode->localh);
	  }
#else
	  child_localh = (val_t)TX_LOAD(&child->localh);
#endif
	  if(lefth - child_localh == 0) {
	    ret = avl_right_rotate(parent, go_left, node, lefth, righth, child, child_addr, right_rotate);
	  }
	}
	
      } else if(bal <= -2 || left_rotate) {
	child = (avl_node_t *)TX_LOAD(&node->right);
	if(child != NULL) {
#ifdef SEPERATE_BALANCE
	  if(child->bnode == NULL) {
	    avl_new_balance_node(child, 0);
	    child_localh = 1;
	  } else {
	    child_localh = (val_t)TX_UNIT_LOAD(&child->bnode->localh);
	  }
#else
	  child_localh = (val_t)TX_LOAD(&child->localh);
#endif
	  if(righth - child_localh == 0) {
	    ret = avl_left_rotate(parent, go_left, node, lefth, righth, child, child_addr, left_rotate);
	  }
	}
      }
    }
  }
  TX_END;

  return ret;
}

int avl_right_rotate(avl_node_t *parent, int go_left, avl_node_t *node, 
		     val_t lefth, val_t righth, avl_node_t *left_child, 
		     avl_node_t **left_child_addr, int do_rotate) {
  val_t left_lefth, left_righth, left_bal, localh;
  avl_node_t *left_right_child;
#ifdef SEPERATE_BALANCE
  balance_node_t *bnode, *lchild_bnode;
#ifdef SEPERATE_BALANCE2
  balance_node_t *other_bnode;
#endif
#endif

#ifdef SEPERATE_BALANCE
  lchild_bnode = left_child->bnode;
  if(lchild_bnode == NULL) {
    left_lefth = 0;
    left_righth = 0;
  } else {
    left_lefth = (val_t)TX_UNIT_LOAD(&lchild_bnode->lefth);
    left_righth = (val_t)TX_UNIT_LOAD(&lchild_bnode->righth);
  }
#else
  left_lefth = (val_t)TX_LOAD(&left_child->lefth);
  left_righth = (val_t)TX_LOAD(&left_child->righth);
#endif

  if(left_child_addr != NULL) {
    *left_child_addr = left_child;
  }

  left_bal = left_lefth - left_righth;
  if(left_bal < 0 && !do_rotate) {
    //should do a double rotate
    return 2;
  }

  left_right_child = (avl_node_t *)TX_LOAD(&left_child->right);

  TX_STORE(&node->left, left_right_child);

  TX_STORE(&node->lefth, left_righth);

  TX_STORE(&node->localh, 1 + max(left_righth, righth));

#ifdef SEPERATE_BALANCE
  localh = (val_t)TX_UNIT_LOAD(&node->bnode->localh);
  if(left_bal >= 0) {
    TX_STORE(&lchild_bnode->localh, localh-1);
  } else {
    TX_STORE(&lchild_bnode->localh, localh);
  }
#else
  localh = (val_t)TX_LOAD(&node->localh);
  if(left_bal >= 0) {
    TX_STORE(&left_child->localh, localh-1);
  } else {
    TX_STORE(&left_child->localh, localh);
  }
#endif
 
#ifdef SEPERATE_BALANCE
  TX_STORE(&lchild_bnode->righth, bnode->localh);
#else
  TX_STORE(&left_child->righth, 1 + max(left_righth, righth));
#endif


  TX_STORE(&left_child->right, node);
#ifdef SEPERATE_BALANCE2
  other_bnode = left_child->bnode;
  TX_STORE(&other_bnode->right, bnode);
  TX_STORE(&bnode->parent, other_bnode);
#endif


  if(go_left) {
    TX_STORE(&parent->left, left_child);
#ifdef SEPERATE_BALANCE2
    bnode = parent->bnode;
    TX_STORE(&bnode->left, other_bnode);
    TX_STORE(&other_bnode->parent, bnode);
#endif
  } else {
    TX_STORE(&parent->right, left_child);
#ifdef SEPERATE_BALANCE2
    bnode = parent->bnode;
    TX_STORE(&bnode->right, other_bnode);
    TX_STORE(&other_bnode->parent, bnode);
#endif
  }
  return 1;
}



int avl_left_rotate(avl_node_t *parent, int go_left, avl_node_t *node, val_t lefth, val_t righth, avl_node_t *right_child, avl_node_t **right_child_addr, int do_rotate) {
  val_t right_lefth, right_righth, right_bal, localh;
  avl_node_t *left_child, *right_left_child;
#ifdef SEPERATE_BALANCE
  balance_node_t *bnode, *rchild_bnode;
#endif
#ifdef SEPERATE_BALANCE2
  balance_node_t *other_bnode;
#endif

#ifdef SEPERATE_BALANCE
  rchild_bnode = right_child->bnode;
  if(rchild_bnode == NULL) {
    right_lefth = 0;
    right_righth = 0;
  } else {
    right_lefth = (val_t)TX_UNIT_LOAD(&rchild_bnode->lefth);
    right_righth = (val_t)TX_UNIT_LOAD(&rchild_bnode->righth);
  }
#else
  right_lefth = (val_t)TX_LOAD(&right_child->lefth);
  right_righth = (val_t)TX_LOAD(&right_child->righth);
#endif

  if(right_child_addr != NULL) {
    *right_child_addr = right_child;
  }

  right_bal = right_lefth - right_righth;
  if(right_bal > 0 && !do_rotate) {
    //should do a double rotate
    return 3;
  }

  right_left_child = (avl_node_t *)TX_LOAD(&right_child->left);
  left_child = (avl_node_t *)TX_LOAD(&node->left);

  TX_STORE(&node->right, right_left_child);

  TX_STORE(&node->righth, right_lefth);
  TX_STORE(&node->localh, 1 + max(right_lefth, lefth));

  localh = (val_t)TX_LOAD(&node->localh);
  if(right_bal < 0) {
    TX_STORE(&right_child->localh, localh-1);
  } else {
    TX_STORE(&right_child->localh, localh);
  }

  TX_STORE(&right_child->lefth, 1 + max(right_lefth, lefth));

  TX_STORE(&right_child->left, node);

  if(go_left) {
    TX_STORE(&parent->left, right_child);
#ifdef SEPERATE_BALANCE2
    bnode = parent->bnode;
    TX_STORE(&bnode->left, other_bnode);
    TX_STORE(&other_bnode->parent, bnode);
#endif
  } else {
    TX_STORE(&parent->right, right_child);
#ifdef SEPERATE_BALANCE2
    bnode = parent->bnode;
    TX_STORE(&bnode->right, other_bnode);
    TX_STORE(&other_bnode->parent, bnode);
#endif
  }
  return 1;
}


#if !defined(SEPERATE_BALANCE2)

int avl_propagate(avl_node_t *node, int left, int *should_rotate) {
  avl_node_t *child;
  val_t height, other_height, child_localh = 0, new_localh;
  intptr_t rem;
  int ret = 0;
  int is_reliable = 0;
#ifdef CHECK_FIRST
  val_t localh;
#endif
#ifdef SEPERATE_BALANCE1
  balance_node_t *bnode, *child_bnode;
  bnode = node->bnode;
#endif
  
#ifdef SEPERATE_BALANCE
  TX_START(NL);
#endif
  *should_rotate = 0;
  ret = 0;
#ifndef SEPERATE_BALANCE
  rem = (intptr_t)UNIT_LOAD(&node->removed);
#else
  rem = (intptr_t)TX_UNIT_LOAD(&node->removed);
#endif
  if(rem == 0) {

#ifdef CHECK_FIRST
    //remove this?
    //why work if unit read here? and now down below
#ifdef SEPERATE_BALANCE1
    localh = (val_t)TX_UNIT_LOAD(&bnode->localh);
#else
    //localh = (val_t)TX_UNIT_LOAD(&node->localh);
    localh = node->localh;
#endif
#endif

    if(left) {
#ifdef SEPERATE_BALANCE
      child = (avl_node_t *)TX_UNIT_LOAD(&node->left);
#else 
      child = (avl_node_t *)UNIT_LOAD(&node->left);
#endif
    } else {
#ifdef SEPERATE_BALANCE
      child = (avl_node_t *)TX_UNIT_LOAD(&node->right);
#else
      child = (avl_node_t *)UNIT_LOAD(&node->right);
#endif
    }
    
    if(child != NULL) {
#ifdef SEPERATE_BALANCE1
      child_bnode = child->bnode;
#endif
      if(left) {
#ifdef SEPERATE_BALANCE1
	height = (val_t)TX_UNIT_LOAD(&bnode->lefth);
#else
	height = node->lefth;
#endif
      } else {
#ifdef SEPERATE_BALANCE1
	height = (val_t)TX_UNIT_LOAD(&bnode->righth);
#else
	height = node->righth;
#endif
      }
      
#ifdef SEPERATE_BALANCE1
      child_localh = (val_t)TX_UNIT_LOAD(&child_bnode->localh);
#else
      child_localh = child->localh;
#endif
      if(height - child_localh == 0) {
	is_reliable = 1;
      }

      if(left) {
#ifdef SEPERATE_BALANCE1
	other_height = (val_t)TX_UNIT_LOAD(&bnode->righth);
#else
	other_height = node->righth;
#endif
      } else {
#ifdef SEPERATE_BALANCE1
	other_height = (val_t)TX_UNIT_LOAD(&bnode->lefth);
#else
	other_height = node->lefth;
#endif
      }

      //check if should rotate
      if(abs(other_height - child_localh) >= 2) {
	*should_rotate = 1;
      }
      
      if(!is_reliable) { 
	if(left) {
#ifdef SEPERATE_BALANCE1
	  TX_STORE(&bnode->lefth, child_localh);
#else
	  node->lefth = child_localh;
#endif
	} else {
#ifdef SEPERATE_BALANCE1
	  TX_STORE(&bnode->righth, child_localh);
#else
	  node->righth = child_localh;
#endif
	}
      
	//Could also do a read here to check if should change before writing?
	//Good or bad for performance? Dunno?
	new_localh = 1 + max(child_localh, other_height);
#ifdef CHECK_FIRST
	if(localh != new_localh) {
#ifdef SEPERATE_BALANCE1
	  TX_STORE(&bnode->localh, new_localh);
#else
	  node->localh = new_localh;
#endif
	  ret = 1;
	}
#else
#ifdef SEPERATE_BALANCE1
	TX_STORE(&bnode->localh, new_localh);
#else
	node->localh = new_localh;
#endif
	ret = 1;
#endif
      }
    }
#ifndef SEPERATE_BALANCE
    else {
      if(left) {
	node->lefth = 0;
	node->localh = 1 + max(0, node->righth);
      } else {
	node->righth = 0;
	node->localh = 1 + max(0, node->lefth);
      }
    }
#endif
  }
#ifdef SEPERATE_BALANCE
  TX_END;
#endif
  return ret;  
}

#else /* ndef SEPERATE_BALANCE2 */


#ifdef SEPERATE_BALANCE2DEL

 int check_remove_list(avl_intset_t *set, ulong *num_rem, free_list_item *free_list) {
   int i;
   avl_node_t *next, *parent;
   int rem_succs;
   free_list_item *next_list_item;
   balance_node_t *bnode;
   
   for(i = 0; i < set->nb_threads; i++) {
     TX_START(NL);
     parent = (avl_node_t*)TX_UNIT_LOAD(&set->to_remove_parent[i]);
     next = (avl_node_t*)TX_UNIT_LOAD(&set->to_remove[i]);
     TX_END;       
     if(next != NULL && parent != NULL && next != set->to_remove_seen[i]) {
       if(parent->bnode != NULL && next->bnode != NULL) {
	 rem_succs = remove_node(parent, next);
	 //rem_succs = 0;
	 if(rem_succs > 1) {
	   printf("Removed, %d\n", next->key);
	   bnode = next->bnode;
	   if(bnode != NULL) {
	     bnode->left = NULL;
	     bnode->right = NULL;
	     bnode->removed = 1;
	   }
	   *num_rem = *num_rem + 1;
	 
	   //add to the list for garbage collection
	   next_list_item = (free_list_item *)malloc(sizeof(free_list_item));
	   next_list_item->next = NULL;
	   next_list_item->to_free = next;
	   free_list->next = next_list_item;
	   free_list = next_list_item;	 
	 }  
	 set->to_remove_seen[i] = next;
       }
     }
   }
   return 1;
 }

#endif


int avl_propagate(balance_node_t *node, int left, int *should_rotate) {

  balance_node_t *child;
  val_t height, other_height, child_localh = 0, new_localh;
  int ret = 0;
  int is_reliable = 0;
#ifdef CHECK_FIRST
  val_t localh;
#endif
  *should_rotate = 0;
  ret = 0;

#ifdef CHECK_FIRST
  //remove this?
  //why work if unit read here? and now down below
  localh = node->localh;
#endif
  
  if(left) {
    child = node->left;
  } else {
    child = node->right;
  }
  
  if(child != NULL) {
    if(left) {
      height = node->lefth;
    } else {
      height = node->righth;
    }
    
    child_localh = child->localh;
    
    if(height - child_localh == 0) {
      is_reliable = 1;
    }
    
    if(left) {
      other_height = node->righth;
    } else {
      other_height = node->lefth;
    }

    //check if should rotate
    if(abs(other_height - child_localh) >= 2) {
      *should_rotate = 1;
    }
    
    if(!is_reliable) { 
      if(left) {
	node->lefth = child_localh;
      } else {
	node->righth = child_localh;
      }
      
      //Could also do a read here to check if should change before writing?
      //Good or bad for performance? Dunno?
      new_localh = 1 + max(child_localh, other_height);
#ifdef CHECK_FIRST
      if(localh != new_localh) {
	node->localh = new_localh;
	ret = 1;
      }
#else
      node->localh = new_localh;
      ret = 1;
#endif
    }
  }
  return ret;  

}

#endif /* def SEPERATE_BALANCE2 */

 int recursive_tree_propagate(avl_intset_t *set, int id, int num_threads) {
#ifdef DEL_COUNT
  int stopm, i;
#endif
  avl_node_t *node;

  //Get to the end of the free list so you can add new elements

#ifdef DEL_COUNT
  stopm = 0;
  while(!stopm) {
    stopm = 0;
    for(i = 0; i < set->nb_threads; i++) {
      if(set->active_del[i]) {
	stopm = 1;
	break;
      }
    }
#ifndef MICROBENCH
#ifndef SEQAVL
    if(thread_getDone()) {
      return 0;
    }
#endif
#else

#ifdef ICC
  if(stop != 0) {
#else
    if(AO_load_full(&stop) != 0) {
#endif /* ICC */
      return 0;
    }
#endif
    if(!stopm)  {
      usleep(DEL_COUNT_SLEEP);
    }
  }
#endif

#ifdef SEPERATE_BALANCE2
  recursive_node_propagate(set, set->root->bnode, NULL, next, 0, 0);
#else

  if(num_threads > 1) {
    TX_START(NL);
    node = (avl_node_t*)TX_LOAD(&set->root->left);
    if(node != NULL) {
      if(id > 0) {
	node = (avl_node_t*)TX_LOAD(&node->left);
      } else {
	node= (avl_node_t*)TX_LOAD(&node->right);
      }
    }
    TX_END;
    recursive_node_propagate(set, node, NULL, 0, 0);
  } else {
    recursive_node_propagate(set, set->root, NULL, 0, 0);
  }
#endif

  return 1;
}

#ifdef SEPERATE_BALANCE2

balance_node_t* check_expand(balance_node_t *node, int go_left) {
  avl_node_t *anode;
  balance_node_t *bnode;

  anode = node->anode;

  if(node->removed) {
    return NULL;
  }

  TX_START(NL);
  if(go_left) {
    anode = (avl_node_t*)TX_UNIT_LOAD(&anode->left);
  } else {
    anode = (avl_node_t*)TX_UNIT_LOAD(&anode->right);
  }
#ifdef SEPERATE_BALANCE2NLDEL
  if(anode != NULL) {
    if(TX_UNIT_LOAD(&anode->removed)) {
      anode = NULL;
    }
  }
#endif
  TX_END;

  if(anode != NULL) {
    bnode = avl_new_balance_node(anode, 0);
    bnode->parent = node;
    if(go_left) {
      node->left = bnode;
    } else {
      node->right = bnode;
    }
    return bnode;
  }

  return NULL;
}


 int recursive_node_propagate(avl_intset_t *set, balance_node_t *node, balance_node_t *parent, free_list_item *free_list, uint depth, uint depth_del) {
   balance_node_t *left, *right, *root;
   int rem_succs;
   intptr_t rem;
   int should_rotatel, should_rotater;
   avl_node_t *anode;
#ifdef BUSY_WAITING
   uint sleepTime;
#endif

  if(node == NULL) {
    return 1;
  }

#ifndef MICROBENCH
#ifndef SEQAVL
    if(thread_getDone()) {
      return 0;
    }
#endif
#else
#ifdef ICC
  if(stop != 0) {
#else
    if(AO_load_full(&stop) != 0) {
#endif /* ICC */
      return 0;
    } 
#endif


#ifdef SEPERATE_BALANCE2DEL
  check_remove_list(set, set->nb_removed, free_list);
#endif


  if(node->removed) {
    return 0;
  }

  anode = node->anode;

  TX_START(NL);
  rem = (intptr_t)TX_UNIT_LOAD(&anode->removed);
  TX_END;

  if(rem) {
    node->removed = 1;
    if(parent->left == node) {
      parent->left = NULL;
      parent->lefth = 0;
    } else {
      parent->right = NULL;
      parent->righth = 0;
    }
    return 0;
  }

  left = node->left;
  right = node->right;

  if(left == NULL) {
    left = check_expand(node, 1);
  }

  if(right == NULL) {
    right = check_expand(node, 0);
  }

  rem_succs = 0;
  if((left == NULL || right == NULL) && parent != NULL && !parent->removed) {
    rem_succs = remove_node(parent->anode, node->anode);
    if(rem_succs > 1) {

      node->removed = 1;
      if(parent->left == node) {
	parent->left = NULL;
	parent->lefth = 0;
      } else {
	parent->right = NULL;
	parent->righth = 0;
      }
      
      set->nb_removed++;
      
      //add to the list for garbage collection
      next_list_item = (free_list_item *)malloc(sizeof(free_list_item));
      next_list_item->next = NULL;
      next_list_item->to_free = node->anode;
      free_list->next = next_list_item;
      free_list = next_list_item;

      return 1;
    }
  }

  if(left != NULL) {
    recursive_node_propagate(set, left, node, free_list, depth, depth_del);
  }
  if(right != NULL) {
    recursive_node_propagate(set, right, node, free_list, depth, depth_del);
  }


#ifndef MICROBENCH
#ifndef SEQAVL
  if(thread_getDone()) {
    return 0;
  }
#endif
#endif

  
#ifdef SEPERATE_MAINTENANCE
#ifdef ENABLE_SLEEPING
  usleep((THROTTLE_TIME)/(set->deleted_count + (set->current_deleted_count * set->current_deleted_count) + 1));
#elif defined BUSY_WAITING
  sleepTime = (THROTTLE_TIME)/(set->deleted_count + (set->current_deleted_count * set->current_deleted_count) + 1);
  while(sleepTime > 0) {
    sleepTime--;
  }
#endif
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
  if(thread_getDone()) {
    return 0;
  }
#endif
#endif

  root = set->root->bnode;

  //should do just left or right rotate here?
  left = node->left;
  
  if(left != NULL && left != root) {
    set->nb_suc_propogated += avl_propagate(left, 1, &should_rotatel);
    set->nb_suc_propogated += avl_propagate(left, 0, &should_rotater);
    if(should_rotatel || should_rotater) {

      set->nb_suc_rotated += avl_rotate(node->anode, 1, left->anode, free_list);
      set->nb_rotated++;
    }
    set->nb_propogated += 2;
  }
  
  //should do just left or right rotate here?
  right = node->right;
  if(right != NULL && right != root) {
    set->nb_suc_propogated += avl_propagate(right, 1, &should_rotatel);
    set->nb_suc_propogated+= avl_propagate(right, 0, &should_rotater);
    if(should_rotatel || should_rotater) {
      set->nb_suc_rotated += avl_rotate(node->anode, 0, right->anode, 
					free_list);
      set->nb_rotated++;
    }
    set->nb_propogated += 2;
  }

  return 1;
}


#else

 int recursive_node_propagate(avl_intset_t *set, avl_node_t *node, avl_node_t *parent, uint depth, uint depth_del) {
   avl_node_t *left, *right, *root;
  intptr_t rem, del;
  int rem_succs;
  int should_rotatel, should_rotater;
  free_list_item *next_list_item;
#ifdef BUSY_WAITING
  uint sleepTime;
#endif


#ifndef MICROBENCH
#ifndef SEQAVL
    if(thread_getDone()) {
      return 0;
    }
#endif
#else
#ifdef ICC
  if(stop != 0) {
#else
    if(AO_load_full(&stop) != 0) {
#endif /* ICC */
      return 0;
    } 
#endif

  if(node == NULL) {
    return 1;
  }

#ifdef NON_UNIT
  left = (avl_node_t*)UNIT_LOAD(&node->left);
  right = (avl_node_t*)UNIT_LOAD(&node->right);
  rem = (intptr_t)UNIT_LOAD(&node->removed);
  del = (intptr_t)UNIT_LOAD(&node->deleted);
#else
  TX_START(NL);
  left = (avl_node_t*)TX_UNIT_LOAD(&node->left);
  right = (avl_node_t*)TX_UNIT_LOAD(&node->right);
  rem = (intptr_t)TX_UNIT_LOAD(&node->removed);
  del = (intptr_t)TX_UNIT_LOAD(&node->deleted);
  TX_END;
#endif

   if(!rem) { 

    rem_succs = 0;
    if((left == NULL && right == NULL) && del && parent != NULL) {
      rem_succs = remove_node(parent, node);
      if(rem_succs > 1) {
      	return 1;
      }
    }


#ifndef MICROBENCH
#ifndef SEQAVL
  if(thread_getDone()) {
    return 0;
  }
#endif
#endif
    
#ifdef SEPERATE_MAINTENANCE
#ifdef ENABLE_SLEEPING
    usleep((THROTTLE_TIME)/(set->deleted_count + 1));
#elif defined BUSY_WAITING
    sleepTime = (THROTTLE_TIME)/(set->deleted_count + 1);
    while(sleepTime > 0) {
      sleepTime--;
    }
#endif
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
  if(thread_getDone()) {
    return 0;
  }
#endif
#endif

    
    if(left != NULL) {
      recursive_node_propagate(set, left, node, depth, depth_del);
    }
    if(right != NULL) {
      recursive_node_propagate(set, right, node, depth, depth_del);
    }

    root = set->root;

    //should do just left or right rotate here?
#ifdef NON_UNIT
    left = (avl_node_t*)UNIT_LOAD(&node->left);
    rem = (intptr_t)UNIT_LOAD(&node->removed);
#else
    TX_START(NL);
    left = (avl_node_t*)TX_UNIT_LOAD(&node->left);
    rem = (intptr_t)TX_UNIT_LOAD(&node->removed);
    TX_END;
#endif

    if(left != NULL && !rem && left != root) {
      avl_propagate(left, 1, &should_rotatel);
      avl_propagate(left, 0, &should_rotater);
      if(should_rotatel || should_rotater) {
	avl_rotate(node, 1, left);
      }
    }
    
    //should do just left or right rotate here?
#ifdef NON_UNIT
    right = (avl_node_t*)UNIT_LOAD(&node->right);
    rem = (intptr_t)UNIT_LOAD(&node->removed);
#else
    TX_START(NL);
    right = (avl_node_t*)TX_UNIT_LOAD(&node->right);
    rem = (intptr_t)TX_UNIT_LOAD(&node->removed);
    TX_END;
#endif

    if(right != NULL && !rem && right != root) {
      avl_propagate(right, 1, &should_rotatel);
      avl_propagate(right, 0, &should_rotater);
      if(should_rotatel || should_rotater) {
	avl_rotate(node, 0, right);
      }
    }
   }

  return 1;
}

#endif

#ifdef KEYMAP

val_t avl_get(val_t key, const avl_intset_t *set) {
  int done;
  intptr_t del;
  avl_node_t *place;
  val_t val;
  val_t k;
  int id = 99;
#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif  

  TX_START(NL);
  place = set->root;
  done = 0;
  
  avl_find(key, &place, &k, id);
  
  if(k != key) {
    done = 1;
  }
  if(!done) {
    del = (intptr_t)TX_LOAD(&place->deleted);
    val = (val_t)TX_LOAD(&place->val);
  }

  TX_END;

  if(done) {
     return NULL;
  } else if(del) {
     return NULL;
  }
   return val;
}


int avl_update(val_t v, val_t key, const avl_intset_t *set) {
  avl_node_t *place, *new_node;
  intptr_t del;
  int done, go_left = 0;
  int ret;
  val_t k;
  int id = 99;
#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif

#ifdef PRINT_INFO
  printf("updating %d, thread: %d\n", key, thread_getId());  
#endif

  TX_START(NL);
  place = set->root;
  ret = 0;
  done = 0;
  
  avl_find(key, &place, &k, id);
  if(k == key) {
    done = 1;
  } else if(key > k){
    go_left = 0;
  } else {
    go_left = 1;
  }

  if(done) {
    if((del = (intptr_t)TX_LOAD(&place->deleted))) {
      TX_STORE(&place->deleted, 0);
      TX_STORE(&place->val, v);
      ret = 1;
    } else {
      TX_STORE(&place->val, v);
      ret = 0;
    }
  } else {
    new_node = avl_new_simple_node(v, key, 1);
    if(go_left) {
      TX_STORE(&place->left, new_node);
    } else {
      TX_STORE(&place->right, new_node);
    }
    ret = 2;
  }
  TX_END;
  return ret;
}

#endif



#ifndef MICROBENCH
#ifdef SEQUENTIAL

/* =============================================================================
 * rbtree_insert
 * -- Returns TRUE on success
 * =============================================================================
 */
bool_t
rbtree_insert (rbtree_t* r, void* key, void* val) {
  int ret;
  
  avl_req_seq_add(NULL, ((avl_intset_t *)r)->root, (val_t)val, (val_t)key, 0, &ret);
  
  if(ret > 0) {
    return 1;
  }
  return 0;
}


/* =============================================================================
 * rbtree_delete
 * =============================================================================
 */
bool_t
rbtree_delete (rbtree_t* r, void* key) {
  int ret;

  avl_req_seq_delete(NULL, ((avl_intset_t *)r)->root, (val_t)key, 0, &ret);

  if(ret > 0) {
    return 1;
  }
  return 0;

}

#ifdef KEYMAP
/* =============================================================================
 * rbtree_update
 * -- Return FALSE if had to insert node first
 * =============================================================================
 */
bool_t
rbtree_update (rbtree_t* r, void* key, void* val) {
  int ret;

  avl_req_seq_update(NULL, ((avl_intset_t *)r)->root, (val_t)val, (val_t)key, 0, &ret);

  if(ret > 0) {
    return 1;
  }
  return 0;
}


/* =============================================================================
 * rbtree_get
 * =============================================================================
 */
void*
rbtree_get (rbtree_t* r, void* key) {
  val_t ret;
  ret = avl_seq_get((avl_intset_t *)r, (val_t)key, 0);
  return (void *)ret;
}
#endif

/* =============================================================================
 * rbtree_contains
 * =============================================================================
 */
bool_t
rbtree_contains (rbtree_t* r, void* key) {
  int ret;

  ret = avl_contains((avl_intset_t*) r, (val_t)key, 0, thread_getId());
  return ret;
}

#endif



/*
=============================================================================
  * TMrbtree_insert
  * -- Returns TRUE on success
  *
=============================================================================
  */
TM_CALLABLE
bool_t
TMrbtree_insert (TM_ARGDECL  rbtree_t* r, void* key, void* val) {
  int ret;

  ret = avl_insert((val_t)val, (val_t)key, (avl_intset_t *)r);
  if(ret > 0) {
    return 1;
  }
  return 0;
}

/*
=============================================================================
  * TMrbtree_delete
  *
=============================================================================
  */
TM_CALLABLE
bool_t
TMrbtree_delete (TM_ARGDECL  rbtree_t* r, void* key) {
  int ret;

  ret = avl_delete((val_t)key, (avl_intset_t *)r);
  if(ret > 0) {
    return 1;
  }
  return 0;
}


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
TMrbtree_update (TM_ARGDECL  rbtree_t* r, void* key, void* val) {
  int ret;

  ret = avl_update((val_t)val, (val_t)key, (avl_intset_t *)r);
  if(ret > 0) {
    return 0;
  }
  return 1;
}

/*
=============================================================================
  * TMrbtree_get
  *
=============================================================================
  */
TM_CALLABLE
void*
TMrbtree_get (TM_ARGDECL  rbtree_t* r, void* key) {
  val_t ret;

  ret =  (void *)avl_get((val_t)key, (avl_intset_t *)r);
  return ret;
}

#endif

/*
=============================================================================
  * TMrbtree_contains
  *
=============================================================================
  */
TM_CALLABLE
bool_t
TMrbtree_contains (TM_ARGDECL  rbtree_t* r, void* key) {
  int ret;
  ret = avl_search((val_t)key, (avl_intset_t*) r);
  return ret;
}

#endif



#ifdef SEQUENTIAL

val_t avl_seq_get(avl_intset_t *set, val_t key, int transactional)
{
  val_t val;
  avl_node_t *next;

  next = set->root;
  
  while(next != NULL) {
    val = next->key;
    
    if(key == val) {
      if(next->deleted) {
	return 0;
      } else {
	return next->val;
      }
    } else if(val < key) {
      next = next->right;
    } else {
      next = next->left;
    }
  }
  return 0;
}


inline int avl_req_seq_update(avl_node_t *parent, avl_node_t *node, val_t val, val_t key, int go_left, int *success) {
  avl_node_t *new_node;
  int ret;
  
  if(node != NULL) {    
    if(key == node->key) {
      if(!node->deleted) {
	//found the node
	node->val = val;
	*success = 1;
	return 0;
      } else {
	node->deleted = 0;
	node->val = val;
	*success = 0;
	return 0;
      }
    } else if(key > node->key) {
      ret = avl_req_seq_update(node, node->right, val, key, 0, success);
    } else {
      ret = avl_req_seq_update(node, node->left, val, key, 1, success);
    }
    
    if(ret != 0) {
      return avl_seq_propogate(parent, node, go_left);
    }
    return 0;

  } else {
    //insert the node
    new_node = avl_new_simple_node(val, key, 0);
    if(key > parent->key) {
      parent->right = new_node;
    } else {
      parent->left = new_node;
    }
    *success = -1;
  }
  
  return 1;
  
}

#endif

#ifdef SEPERATE_MAINTENANCE

 void do_maintenance_thread(avl_intset_t *tree, int id, int num_threads)
 {


#ifdef NO_MAINTENANCE
  return;
#endif

  //do maintenance, but only when there have been enough modifications
  //not doing this here, but maybe should?
  
#ifndef MICROBENCH
  TM_THREAD_ENTER(); 
  while(!thread_getDone()) {
#else

#ifdef ICC
  while (stop == 0) {
#else
    while (AO_load_full(&stop) == 0) {
#endif /* ICC */

#endif


#ifndef MICROBENCH
  if(!thread_getDone()) {
#endif
    recursive_tree_propagate(tree, id, num_threads);

#ifndef MICROBENCH
  }
#endif

#ifndef MICROBENCH
  }
  TM_THREAD_EXIT();
#else
  }
#endif
  
}

  
#else


void do_maintenance(avl_intset_t *tree) {
  int done;
  int i;
  ulong *tmp;
  free_list_item *next, *tmp_item;
  long nb_threads;
  ulong *nb_committed;
  long id;
  ulong *t_nb_trans;
  ulong *t_nb_trans_old;

  nb_threads = tree->nb_threads;
  t_nb_trans = tree->t_nbtrans;
  t_nb_trans_old = tree->t_nbtrans_old;
  nb_committed = tree->nb_committed;

  //TODO FIX THIS SHOULD NOT BE 0
  id = 0;
  //id = thread_getId();
  
  //need to do garbage collection here
  done = 0;
  for(i = 0; i < nb_threads; i++) {
    t_nb_trans[i] = nb_committed[i];
    if(t_nb_trans[i] == t_nb_trans_old[i]) {
      done = 1;
	  break;
    }
  }
  if(!done) {
    tmp = (t_nb_trans_old);
    (t_nb_trans_old) = (t_nb_trans);
    (t_nb_trans) = tmp;
    next = tree->free_list->next;
    while(next != NULL) {
      free(next->to_free);
      tmp_item = next;
      next = next->next;
      free(tmp_item);
    }
    tree->free_list->next = NULL;
   
#ifndef SEPERATE_BALANCE2 
    //free the ones from your own main thread
    next = tree->t_free_list[id]->next;
    while(next != NULL) {
      if(next->to_free != NULL) {
	free(next->to_free);
      }
      tmp_item = next;
      next = next->next;
      free(tmp_item);
    }
    tree->t_free_list[id]->next = NULL;
#endif

  }
  
  recursive_tree_propagate(tree, tree->free_list);
}


void check_maintenance(avl_intset_t *tree) {
  long id;
  long nextHelp;

  //TODO FIX THIS SHOULD NOT BE 0
  id = 0;
  //id = thread_getId();
  nextHelp = tree->next_maintenance;
  
  if(nextHelp == id) {
    if(tree->nb_committed[id] >= THROTTLE_UPDATE + tree->nb_committed_old[id]) {
      do_maintenance(tree);
      tree->nb_committed_old[id] = tree->nb_committed[id];
    } 
    tree->next_maintenance = (nextHelp + 1) % tree->nb_threads;
  }
  
}

#endif
#endif

