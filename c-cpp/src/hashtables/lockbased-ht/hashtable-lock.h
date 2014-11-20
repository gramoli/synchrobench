/*
 * File:
 *   hashtable-lock.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lock-based Hashtable
 *   Implementation of an integer set using a lock-based hashtable.
 *   The hashtable contains several buckets, each represented by a linked
 *   list, since hashing distinct keys may lead to the same bucket.
 *
 * Copyright (c) 2009-2010.
 *
 * hashtable-lock.h is part of Synchrobench
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

#include "../linkedlists/lazy-list/intset.h"

#define DEFAULT_MOVE                    0
#define DEFAULT_SNAPSHOT                0
#define DEFAULT_LOAD                    1
#define DEFAULT_ELASTICITY              2
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE               1

#define MAXHTLENGTH                     65536

/* Hashtable length (# of buckets) */
extern unsigned int maxhtlength;

/* ################################################################### *
 * HASH TABLE
 * ################################################################### */

typedef struct ht_intset {
	intset_l_t *buckets[MAXHTLENGTH];
} ht_intset_t;

void ht_delete(ht_intset_t *set);
int ht_size(ht_intset_t *set);
int floor_log_2(unsigned int n);
ht_intset_t *ht_new();
int ht_contains(ht_intset_t *set, int val, int transactional);
int ht_add(ht_intset_t *set, int val, int transactional);
int ht_remove(ht_intset_t *set, int val, int transactional);

/* 
 * Move an element in the hashtable (from one linked-list to another)
 */
int ht_move(ht_intset_t *set, int val1, int val2, int transactional);
/* 
 * Read all elements of the hashtable (parses all linked-lists)
 * This cannot be consistent when used with move operation.
 * TODO: make a coarse-grain version of the snapshot.
 */
int ht_snapshot(ht_intset_t *set, int transactional);

