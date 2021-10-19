/*
 * File:
 *   intset.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list integer set operations 
 *
 * Copyright (c) 2009-2010.
 *
 * intset.c is part of Microbench
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

#include "intset.h"

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
  avl_node_t *next, *prev, *parent, *child;

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
	  } else {
	    parent->right = node->right;
	  }
	  free(node);
	  *success = 1;
	  return 1;
	} else if(node->right == NULL) {
	  if(go_left) {
	    parent->left = node->left;
	  } else {
	    parent->right = node->left;
	  }
	  free(node);
	  *success = 1;
	  return 1;
	} else {
	  ret = avl_seq_req_find_successor(node, node->right, 0, &succs);
	  succs->right = node->right;
	  succs->left = node->left;
	  if(go_left) {
	    parent->left = succs;
	  } else {
	    parent->right = succs;
	  }
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

  if(node != NULL) {    
    if(key == node->key) {
      if(!node->deleted) {
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
    if(key > parent->key) {
      parent->right = new_node;
    } else {
      parent->left = new_node;
    }
    *success = 1;
  }
  
  return 1;
  
}



int avl_seq_propogate(avl_node_t *parent, avl_node_t *thenode, int go_left) {
  int new_localh;
  avl_node_t *node;
  node = thenode;

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
      avl_seq_check_rotations(parent, node, node->lefth, node->righth, go_left);
    }
    return 1;
  }
  return 0;
}

int avl_seq_check_rotations(avl_node_t *parent, avl_node_t *node, int lefth, int righth, int go_left) {
  int local_bal, lbal, rbal;

  local_bal = lefth - righth;

  if(local_bal > 1) {
    lbal = node->left->lefth - node->left->righth;
    if(lbal >= 0) {
      avl_seq_right_rotate(parent, node, go_left);
    } else {
      avl_seq_left_right_rotate(parent, node, go_left);
    }

  } else if(local_bal < -1) {
    rbal = node->right->lefth - node->right->righth;
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

  v = place;
  u = place->left;
  if(u == NULL || v == NULL) {
    return 0;
  }
  if(go_left) {
    parent->left = u;
  } else {
    parent->right = u;
  }

  v->left = u->right;
  u->right = v;

  //now need to do propogations
  u->localh = v->localh - 1;
  v->lefth = u->righth;
  v->localh = 1 + max(v->lefth, v->righth);
  u->righth = v->localh;

  return 1;
}


int avl_seq_left_rotate(avl_node_t *parent, avl_node_t *place, int go_left) {
  avl_node_t *u, *v;

  v = place;
  u = place->right;
  if(u == NULL || v == NULL) {
    return 0;
  }
  if(go_left) {
    parent->left = u;
  } else {
    parent->right = u;
  }

  v->right = u->left;
  u->left = v;

  //now need to do propogations
  u->localh = v->localh - 1;
  v->righth = u->lefth;
  v->localh = 1 + max(v->lefth, v->righth);
  u->lefth = v->localh;

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
    if(go_left) {
      parent->left = node->right;
      parent->lefth = node->localh - 1;
    } else {
      parent->right = node->right;
      parent->righth = node->localh - 1;
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

#ifdef PRINT_INFO
  printf("searching %d thread: %d\n", key, thread_getId());
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif

  TX_START(NL);
  set->nb_committed[id]++;
  place = set->root;
  done = 0;
  
  avl_find(key, &place, &k, id);
  
  if(k != key) {
    done = 1;
  }
  
  if(!done) {
    del = (intptr_t)TX_LOAD(&place->deleted);
  }

  TX_END;

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
  intptr_t rem;
  val_t valt;
  
  next = *place;
  placet = *parent;
  valt = *val;

  rem = 1;
  while(1) {
    parentt = placet;
    placet = next;
    rem = 0;

    valt = (placet)->key;

    if(key == valt) {
      rem = (intptr_t)TX_LOAD(&(placet)->removed);
      if(!rem) {
	if(key > parentt->key) {
	  next = (avl_node_t*)TX_LOAD(&(parentt)->right);
	} else {
	  next = (avl_node_t*)TX_LOAD(&(parentt)->left);
	}

	if(next == placet) {
	  break;
	}
      }
    }

    if(key != valt || rem) {
      if(key > valt) {
	next = (avl_node_t*)TX_UNIT_LOAD(&(placet)->right);
      } else {
	next = (avl_node_t*)TX_UNIT_LOAD(&(placet)->left);
      }
      //Do this for nested transactions
      if(next == NULL) {
	rem = (intptr_t)TX_LOAD(&(placet)->removed);
	if(!rem) {
	  if(key > valt) {
	    next = (avl_node_t*)TX_LOAD(&(placet)->right);
	  } else {
	    next = (avl_node_t*)TX_LOAD(&(placet)->left);
	  }
	  if(next == NULL) {
	    break;
	  }
	} else {
	  if(key > valt) {
	    next = (avl_node_t*)TX_UNIT_LOAD(&(placet)->right);
	  } else {
	    next = (avl_node_t*)TX_UNIT_LOAD(&(placet)->left);
	  }
	  if(next == NULL) {
	    if(key <= valt) {
	      next = (avl_node_t*)TX_UNIT_LOAD(&(placet)->right);
	    } else {
	      next = (avl_node_t*)TX_UNIT_LOAD(&(placet)->left);
	    }
	  }
	}
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

#ifdef PRINT_INFO
  printf("inserting %d thread: %d\n", key, thread_getId());  
#endif

#ifndef MICROBENCH
#ifndef SEQAVL
    id = thread_getId();
#endif
#endif

  TX_START(NL);
  set->nb_committed[id]++;
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

#ifdef MICROBENCH
 int avl_ss(avl_intset_t *set, int id) {
#else
   int avl_ss(avl_intset_t *set) {
#endif
     int size = 0;

     TX_START(NL);
     set->nb_committed[id]++;
     rec_ss((avl_node_t *)TX_LOAD(&set->root->left), &size);
     TX_END;

     return size;
 }

 void rec_ss(avl_node_t *node, int *size) {
     if(node == NULL) {
       return;
     }
     if(!TX_LOAD(&node->deleted)) {
       *size += node->key;
     }
     rec_ss((avl_node_t *)TX_LOAD(&node->left), size);
     rec_ss((avl_node_t *)TX_LOAD(&node->right), size);

     return 0;
 }


#ifdef MICROBENCH
 int avl_mv(val_t key, val_t new_key, const avl_intset_t *set, int id) {
#else
   int avl_mv(val_t key, val_t new_key, const avl_intset_t *set) {
#endif
  int ret;
#ifndef MICROBENCH
  long id = 0;
#endif
  /* avl_node_t* place; */
  val_t k;

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

#ifndef MICROBENCH
  long id;
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
  set->nb_committed[id]++;
  place = set->root;
  parent = set->root;
  ret  = 0;
  done = 0;
  avl_find_parent(key, &place, &parent, &k, id);
  while((intptr_t)TX_LOAD(&parent->removed)) {
    place = set->root;
    parent = set->root;
    ret  = 0;
    done = 0;
    avl_find_parent(key, &place, &parent, &k, id);
  }

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

  TX_END;  

  return ret;
}
 

int remove_node(avl_node_t *parent, avl_node_t *place) {
  avl_node_t *right_child, *left_child, *parent_child;
  int go_left;
  val_t v;
  int ret;
  intptr_t rem, del;
  val_t lefth = 0, righth = 0;
  int id = 99;

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
	  TX_STORE(&place->left, parent);
	  TX_STORE(&place->right, parent);
	  
	  //Could also do a read here to check if should change before writing
	  //Good or bad for performance?
	  if(left_child == NULL && right_child == NULL) {
	    if(go_left) {
	      lefth = 0;
	      righth = (val_t)TX_UNIT_LOAD(&parent->righth);
	    } else {
	      righth = 0;
	      lefth = (val_t)TX_UNIT_LOAD(&parent->lefth);
	    }
	  } else {
	    if(go_left) {
	      lefth = (val_t)TX_UNIT_LOAD(&place->localh) - 1;
	      righth = (val_t)TX_UNIT_LOAD(&parent->righth);
	    } else {
	      righth = (val_t)TX_UNIT_LOAD(&place->localh) - 1;
	      lefth = (val_t)TX_UNIT_LOAD(&parent->lefth);
	    }
	  }

	  if(go_left) {
	    TX_STORE(&parent->lefth, lefth);
	  } else {
	    TX_STORE(&parent->righth, righth);
	  }
	  TX_STORE(&parent->localh, 1 + max(righth, lefth));
	  
	  TX_STORE(&place->removed, 1);
	  ret = 2;
	}
      }
    }
  }
  TX_END;
  
  return ret;
}

int avl_rotate(avl_node_t *parent, int go_left, avl_node_t *node, free_list_item **free_list) {
  int ret = 0;
  avl_node_t *child_addr = NULL;
  
  ret = avl_single_rotate(parent, go_left, node, 0, 0, &child_addr, free_list);
  if(ret == 2) {
    //Do a LRR
    ret = avl_single_rotate(node, 1, child_addr, 1, 0, &child_addr, free_list);
    if(ret > 0) {
      avl_single_rotate(parent, go_left, child_addr, 0, 1, NULL, free_list);
    }
  } else if(ret == 3) {
    //Do a RLR
    ret = avl_single_rotate(node, 0, child_addr, 0, 1, &child_addr, free_list);
    if(ret > 0) {
      avl_single_rotate(parent, go_left, child_addr, 1, 0, NULL, free_list);
    }
  }
  return ret;
}


int avl_single_rotate(avl_node_t *parent, int go_left, avl_node_t *node, int left_rotate, int right_rotate, avl_node_t **child_addr, free_list_item** free_list) {
  val_t lefth, righth, bal, child_localh;
  avl_node_t *child, *check;
  int ret;
  intptr_t rem1, rem2;
  int id = 99;

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
      lefth = TX_LOAD(&node->lefth);
      righth = TX_LOAD(&node->righth);
      bal = lefth - righth;
      
      if(bal >= 2 || right_rotate) {
	child = (avl_node_t *)TX_LOAD(&node->left);
	if(child != NULL) {
	  child_localh = (val_t)TX_LOAD(&child->localh);
	  if(lefth - child_localh == 0) {
	    ret = avl_right_rotate(parent, go_left, node, lefth, righth, child, child_addr, right_rotate, free_list);
	  }
	}
	
      } else if(bal <= -2 || left_rotate) {
	child = (avl_node_t *)TX_LOAD(&node->right);
	if(child != NULL) {
	  child_localh = (val_t)TX_LOAD(&child->localh);
	  if(righth - child_localh == 0) {
	    ret = avl_left_rotate(parent, go_left, node, lefth, righth, child, child_addr, left_rotate, free_list);
	  }
	}
      }
    }
  }
  TX_END;

  return ret;
}

int avl_right_rotate(avl_node_t *parent, int go_left, avl_node_t *node, val_t lefth, val_t righth, avl_node_t *left_child, avl_node_t **left_child_addr, int do_rotate, free_list_item** free_list) {
  val_t left_lefth, left_righth, left_bal, localh;
  avl_node_t *right_child, *left_right_child, *new_node;
  free_list_item *next_list_item;
  int id = 99;

  left_lefth = (val_t)TX_LOAD(&left_child->lefth);
  left_righth = (val_t)TX_LOAD(&left_child->righth);

  if(left_child_addr != NULL) {
    *left_child_addr = left_child;
  }

  left_bal = left_lefth - left_righth;
  if(left_bal < 0 && !do_rotate) {
    //should do a double rotate
    return 2;
  }
  
  left_right_child = (avl_node_t *)TX_LOAD(&left_child->right);
  right_child = (avl_node_t *)TX_LOAD(&node->right);
   
#ifdef KEYMAP
  new_node = avl_new_simple_node((val_t)TX_LOAD(&node->val), node->key, 1);
#else
  new_node = avl_new_simple_node(node->val, node->key, 1);
#endif
  new_node->deleted = (intptr_t)TX_LOAD(&node->deleted);
  new_node->left = left_right_child;
  new_node->right = right_child;

  new_node->lefth = left_righth;
  new_node->righth = righth;
  new_node->localh = 1 + max(left_righth, righth);
  localh = (val_t)TX_LOAD(&node->localh);
  if(left_bal >= 0) {
    TX_STORE(&left_child->localh, localh-1);
  } else {
    TX_STORE(&left_child->localh, localh);
  }
  TX_STORE(&node->removed, 1);

  if(go_left) {
    TX_STORE(&parent->left, left_child);
  } else {
    TX_STORE(&parent->right, left_child);
  }

  //for garbage clearing
  next_list_item = (free_list_item *)MALLOC(sizeof(free_list_item));
  next_list_item->next = NULL;
  next_list_item->to_free = node;
  TX_STORE(&((*free_list)->next), next_list_item);
  TX_STORE(&(*free_list), next_list_item);

  TX_STORE(&left_child->righth, new_node->localh);
  TX_STORE(&left_child->right, new_node);

  return 1;
}



int avl_left_rotate(avl_node_t *parent, int go_left, avl_node_t *node, val_t lefth, val_t righth, avl_node_t *right_child, avl_node_t **right_child_addr, int do_rotate, free_list_item** free_list) {
  val_t right_lefth, right_righth, right_bal, localh;
  avl_node_t *left_child, *right_left_child, *new_node;
  free_list_item *next_list_item;
  int id = 99;

  right_lefth = (val_t)TX_LOAD(&right_child->lefth);
  right_righth = (val_t)TX_LOAD(&right_child->righth);

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


#ifdef KEYMAP
  new_node = avl_new_simple_node((val_t)TX_LOAD(&node->val), node->key, 1);
#else
  new_node = avl_new_simple_node(node->val, node->key, 1);
#endif
  new_node->deleted = (intptr_t)TX_LOAD(&node->deleted);
  new_node->right = right_left_child;
  new_node->left = left_child;
  new_node->righth = right_lefth;
  new_node->lefth = lefth;
  new_node->localh = 1 + max(right_lefth, lefth);

  localh = (val_t)TX_LOAD(&node->localh);
  if(right_bal < 0) {
    TX_STORE(&right_child->localh, localh-1);
  } else {
    TX_STORE(&right_child->localh, localh);
  }
  TX_STORE(&node->removed, 1);

  TX_STORE(&node->left, parent);
  TX_STORE(&node->right, parent);


  TX_STORE(&right_child->lefth, new_node->localh);

  TX_STORE(&right_child->left, new_node);

  if(go_left) {
    TX_STORE(&parent->left, right_child);
  } else {
    TX_STORE(&parent->right, right_child);
  }

  //for garbage cleaning
  next_list_item = (free_list_item *)MALLOC(sizeof(free_list_item));
  next_list_item->next = NULL;
  next_list_item->to_free = node;
  TX_STORE(&((*free_list)->next), next_list_item);
  TX_STORE(&(*free_list), next_list_item);

  return 1;
}


int avl_propagate(avl_node_t *node, int left, int *should_rotate) {
  avl_node_t *child;
  val_t height, other_height, child_localh = 0, new_localh;
  intptr_t rem;
  int ret = 0;
  int is_reliable = 0;
  val_t localh;
  
#ifdef SEPERATE_BALANCE
  TX_START(NL);
#endif
  *should_rotate = 0;
  ret = 0;
  rem = (intptr_t)TX_UNIT_LOAD(&node->removed);
  if(rem == 0) {

    localh = node->localh;
    if(left) {
      child = (avl_node_t *)UNIT_LOAD(&node->left);
    } else {
      child = (avl_node_t *)UNIT_LOAD(&node->right);
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
      
	//Could also do a read here to check if should change before writing
	//Good or bad for performance?
	new_localh = 1 + max(child_localh, other_height);
	if(localh != new_localh) {
	  node->localh = new_localh;
	  ret = 1;
	}
      }
    } else {
      if(left) {
	node->lefth = 0;
	node->localh = 1 + max(0, node->righth);
      } else {
	node->righth = 0;
	node->localh = 1 + max(0, node->lefth);
      }
    }
  }
  return ret;  
}


 int recursive_tree_propagate(avl_intset_t *set, free_list_item** free_list, int id, int num_threads) {
  avl_node_t *node;

  if(num_threads > 1) {
    TX_START(NL);
    node = TX_UNIT_LOAD(&set->root->left);
    if(node != NULL) {
      if(id > 0) {
	node = TX_UNIT_LOAD(&node->left);
      } else {
	node = TX_UNIT_LOAD(&node->right);
      }
    }
    TX_END;
    recursive_node_propagate(set, node, NULL, free_list, 0, 0);
  } else {
    recursive_node_propagate(set, set->root, NULL, free_list, 0, 0);
  }

  return 1;
}

 int recursive_node_propagate(avl_intset_t *set, avl_node_t *node, avl_node_t *parent, free_list_item **free_list, uint depth, uint depth_del) {
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
      	//add to the list for garbage collection
      	next_list_item = (free_list_item *)malloc(sizeof(free_list_item));
      	next_list_item->next = NULL;
      	next_list_item->to_free = node;
      	(*free_list)->next = next_list_item;
      	(*free_list) = next_list_item;
	
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
    
    if(left != NULL) {
      recursive_node_propagate(set, left, node, free_list, depth, depth_del);
    }
    if(right != NULL) {
      recursive_node_propagate(set, right, node, free_list, depth, depth_del);
    }

    root = set->root;

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
	avl_rotate(node, 1, left, free_list);
	
      }

    }
    
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
	avl_rotate(node, 0, right, free_list);

      }
    }
  }
  return 1;
}

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
  set->nb_committed[id]++;
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
  set->nb_committed[id]++;
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

/******************************************************************************
 * The following are wrappers for STAMP
 ****************************************************************************/

#ifndef MICROBENCH

#ifdef SEQUENTIAL

/* =============================================================================
 * rbtree_insert
 * -- Returns TRUE on success
 * =============================================================================
 */
bool_t
rbtree_insert (rbtree_t* r, void* key, void* val) {

  if(avl_req_seq_add(NULL, ((avl_intset_t *)r)->root, (val_t)val, (val_t)key, 0, &ret) > 0) {
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

  

  if(avl_req_seq_delete(NULL, ((avl_intset_t *)r)->root, (val_t)key, 0, &ret) > 0) {
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
  return (void*)avl_seq_get((avl_intset_t *)r, (val_t)key, 0);
}
#endif

/* =============================================================================
 * rbtree_contains
 * =============================================================================
 */
bool_t
rbtree_contains (rbtree_t* r, void* key) {
  return avl_contains((avl_intset_t*) r, (val_t)key, 0, thread_getId());
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
 
  if(avl_insert((val_t)val, (val_t)key, (avl_intset_t *)r) > 0) {
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

  if(avl_delete((val_t)key, (avl_intset_t *)r) > 0) {
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

  if(avl_update((val_t)val, (val_t)key, (avl_intset_t *)r) > 0) {
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

  return  (void *)avl_get((val_t)key, (avl_intset_t *)r);
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

  return avl_search((val_t)key, (avl_intset_t*) r);
}

#endif


/******************************************************************************
 * End of wrappers for STAMP
 ****************************************************************************/



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
	//next = next->left;
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
  int done;
  int i;
  ulong *tmp;
  free_list_item *next, *tmp_item, *tmp_free, *freeupto, *nextfreeupto, *freestart;
  long nb_threads;
  ulong *nb_committed;
  ulong *t_nb_trans;
  ulong *t_nb_trans_old;

#ifdef NO_MAINTENANCE
  return;
#endif

  nb_threads = tree->nb_threads;
  t_nb_trans = tree->t_nbtrans;
  t_nb_trans_old = tree->t_nbtrans_old;
  nb_committed = tree->nb_committed;
  
  freestart = tree->free_list;
  freeupto = tree->free_list;
  nextfreeupto = tree->free_list;
  for(i = 0; i < nb_threads; i++) {
    t_nb_trans_old[i] = nb_committed[i];
  }

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

  //check if you can free removed nodes
  done = 0;
  for(i = 0; i < nb_threads; i++) {
    t_nb_trans[i] = nb_committed[i];
    if(t_nb_trans[i] == t_nb_trans_old[i]) {
      done = 1;
      break;
    }
  }

  if(!done) {
    //free removed nodes
    next = freestart;
    while(next != freeupto) {
      free(next->to_free);
      tmp_item = next;
      next = next->next;
      free(tmp_item);
    }
    freestart = freeupto;
    freeupto = nextfreeupto;

  }

#ifndef MICROBENCH
  if(!thread_getDone()) {
#endif

    recursive_tree_propagate(tree, &(tree->free_list), id, num_threads);

    if(!done) {
      nextfreeupto = tree->free_list;
      for(i = 0; i < nb_threads; i++) {
    	t_nb_trans_old[i] = nb_committed[i];
      }
    }

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
  free_list_item *next, *tmp_item;//, **t_list_items;
  long nb_threads;
  ulong *nb_committed;
  long id;
  ulong *t_nb_trans;
  ulong *t_nb_trans_old;

  nb_threads = tree->nb_threads;
  t_nb_trans = tree->t_nbtrans;
  t_nb_trans_old = tree->t_nbtrans_old;
  nb_committed = tree->nb_committed;
  id = 0;
  
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
   
  }
  recursive_tree_propagate(tree, tree->free_list);
}


void check_maintenance(avl_intset_t *tree) {
  long id;
  long nextHelp;

  id = 0;
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

