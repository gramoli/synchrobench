/*
 * File:
 *   skiplist-lock.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list implementation of an integer set
 *
 * Copyright (c) 2009-2010.
 *
 * skiplist-lock.h is part of Synchrobench
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

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include <atomic_ops.h>
#include "common.h"
#include "ptst.h"
#include "garbagecoll.h"

/*
 * number of unique blk sizes we want to deal with
 * (1 for node and 1 for next pointer array)
 */
#define MAX_SIZES 2

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY		4
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE 		1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

extern pthread_key_t preds_key;
extern pthread_key_t succs_key;
extern volatile AO_t stop;
extern unsigned int global_seed;
/* Skip list level */
#ifdef TLS
extern __thread unsigned int *rng_seed;
#else /* ! TLS */
extern pthread_key_t rng_seed_key;
#endif /* ! TLS */
extern unsigned int levelmax;

#define TRANSACTIONAL                   d->unit_tx

typedef intptr_t val_t;
#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

#ifdef MUTEX
typedef pthread_mutex_t ptlock_t;
#  define INIT_LOCK(lock)		pthread_mutex_init(lock, NULL);
#  define DESTROY_LOCK(lock)		pthread_mutex_destroy(lock)
#  define LOCK(lock)			pthread_mutex_lock(lock)
#  define UNLOCK(lock)			pthread_mutex_unlock(lock)
#else
typedef pthread_spinlock_t ptlock_t;
#  define INIT_LOCK(lock)		pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE);
#  define DESTROY_LOCK(lock)		pthread_spin_destroy(lock)
#  define LOCK(lock)			pthread_spin_lock(lock)
#  define UNLOCK(lock)			pthread_spin_unlock(lock)
#endif

typedef struct sl_node {
	val_t val; 
	int toplevel;
	struct sl_node** next;
	volatile int marked;
	volatile int fullylinked;
	ptlock_t lock;	
} sl_node_t;

typedef struct sl_intset {
	sl_node_t *head;
} sl_intset_t;

inline void *xmalloc(size_t size);
inline int rand_100();

int get_rand_level();
int floor_log_2(unsigned int n);

/* 
 * Create a new node without setting its next fields. 
 */
sl_node_t *sl_new_simple_node(val_t val, int toplevel, int transactional, ptst_t *ptst);
/* 
 * Create a new node with its next field. 
 * If next=NULL, then this create a tail node. 
 */
sl_node_t *sl_new_node(val_t val, sl_node_t *next, int toplevel, int 
transactional, ptst_t *ptst);
void sl_delete_node(sl_node_t *n, ptst_t *ptst);
sl_intset_t *sl_set_new(ptst_t *ptst);
void sl_set_delete(sl_intset_t *set, ptst_t *ptst);
int sl_set_size(sl_intset_t *set);

void set_subsystem_init(void);
