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


#include "hashtable.h"

int ht_contains(ht_intset_t *set, int val, int transactional);
int ht_add(ht_intset_t *set, int val, int transactional);
int ht_remove(ht_intset_t *set, int val, int transactional);

/* 
 * Move an element from one bucket to another.
 * It is equivalent to changing the key associated with some value.
 */
int ht_move(ht_intset_t *set, int val1, int val2, int transactional);

/*
 * Atomic snapshot of the hash table.
 * It parses the whole hash table to sum all elements.
 *
 * Observe that this particular operation (atomic snapshot) cannot be implemented using 
 * elastic transactions in combination with the move operation, however, normal transactions
 * compose with elastic transactions.
 */
int ht_snapshot(ht_intset_t *set, int transactional);
