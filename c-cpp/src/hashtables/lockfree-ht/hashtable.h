/*
 * File:
 *   hashtable.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Hashtable structure
 *
 * Copyright (c) 2009-2010.
 *
 * hashtable.h is part of Synchrobench
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

#include "../../linkedlists/lockfree-list/intset.h"

#define DEFAULT_MOVE                    0
#define DEFAULT_SNAPSHOT                0
#define DEFAULT_LOAD                    1
#define DEFAULT_ELASTICITY              4
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE               1

#define MAXHTLENGTH                     65536

/* Hashtable length (# of buckets) */
extern unsigned int maxhtlength;

/* Hashtable seed */
#ifdef TLS
extern __thread unsigned int *rng_seed;
#else /* ! TLS */
extern pthread_key_t rng_seed_key;
#endif /* ! TLS */

typedef struct ht_intset {
  intset_t **buckets;
} ht_intset_t;

void ht_delete(ht_intset_t *set);
int ht_size(ht_intset_t *set);
int floor_log_2(unsigned int n);
ht_intset_t *ht_new();
