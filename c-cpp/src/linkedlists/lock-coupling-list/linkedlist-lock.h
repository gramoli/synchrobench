/*
 * File:
 *   linkedlist-lock.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lock-based linked list implementation of an integer set
 *
 * Copyright (c) 2009-2010.
 *
 * linkedlist-lock.c is part of Synchrobench
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

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_LOCKTYPE                1
#define DEFAULT_ALTERNATE		0
#define DEFAULT_EFFECTIVE		1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define ATOMIC_CAS_MB_FBAR(a, e, v)     (AO_compare_and_swap_full((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))

#define ATOMIC_CAS_MB_NOBAR(a, e, v)    (AO_compare_and_swap((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))

static volatile AO_t stop;

#define TRANSACTIONAL                   d->unit_tx

typedef intptr_t val_t;
#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

#ifdef MUTEX
typedef pthread_mutex_t ptlock_t;
#  define INIT_LOCK(lock)				pthread_mutex_init((pthread_mutex_t *) lock, NULL);
#  define DESTROY_LOCK(lock)			pthread_mutex_destroy((pthread_mutex_t *) lock)
#  define LOCK(lock)					pthread_mutex_lock((pthread_mutex_t *) lock)
#  define UNLOCK(lock)					pthread_mutex_unlock((pthread_mutex_t *) lock)
#else
typedef pthread_spinlock_t ptlock_t;
#  define INIT_LOCK(lock)				pthread_spin_init((pthread_spinlock_t *) lock, PTHREAD_PROCESS_PRIVATE);
#  define DESTROY_LOCK(lock)			pthread_spin_destroy((pthread_spinlock_t *) lock)
#  define LOCK(lock)					pthread_spin_lock((pthread_spinlock_t *) lock)
#  define UNLOCK(lock)					pthread_spin_unlock((pthread_spinlock_t *) lock)
#endif

typedef struct node_l {
  val_t val;
  struct node_l *next;
  volatile ptlock_t lock;
} node_l_t;

typedef struct intset_l {
  node_l_t *head;
} intset_l_t;

node_l_t *new_node_l(val_t val, node_l_t *next, int transactional);
intset_l_t *set_new_l();
void set_delete_l(intset_l_t *set);
int set_size_l(intset_l_t *set);
void node_delete_l(node_l_t *node);


