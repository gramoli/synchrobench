/*
 * File:
 *   intset.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Integer set operations
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

#include "rbtree.h"

typedef rbtree_t intset_t;
typedef intptr_t val_t;

intset_t *set_new();
void set_delete(intset_t *set);
int set_size(intset_t *set);

int set_contains(intset_t *set, val_t val, int transactional);
/* 
 * Adding to the rbtree may require rotations (at least in this implementation)  
 * This operation requires strong dependencies between numerous transactional 
 * operations, hence, the use of normal transaction is necessary for safety.
 */ 
int set_add(intset_t *set, val_t val, int transactional);
int set_remove(intset_t *set, val_t val, int transactional);
