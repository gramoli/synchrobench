/*
 * File:
 *   skiplist.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip List Integer Set
 *
 * Copyright (c) 2008-2009.
 *
 * This program is free software; you can redistribute it and/or
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

#include "stm.h"
#include "mod_mem.h"

#ifdef DEBUG
#define IO_FLUSH                        fflush(NULL)
/* Note: stdio is thread-safe */
#endif

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */

#define TX_START(type)                  { sigjmp_buf *_e = stm_get_env(); sigsetjmp(*_e, 0); stm_start(_e, 0, type)
#define TX_LOAD(addr)                   stm_load((stm_word_t *)addr)
#define TX_STORE(addr, val)             stm_store((stm_word_t *)addr, (stm_word_t)val)
#define TX_END							stm_commit(); }

#define MALLOC(size)                    stm_malloc(size)
#define FREE(addr, size)                stm_free(addr, size)

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0xFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define ATOMIC_CAS_MB(a, e, v)          (AO_compare_and_swap_full((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_FETCH_AND_INC_FULL(a)    (AO_fetch_and_add1_full((volatile AO_t *)(a)))

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static volatile AO_t stop;
static unsigned int global_seed;

/* ################################################################### *
 * SKIP LIST
 * ################################################################### */

/* Skip list level */
#ifdef TLS
static __thread unsigned int *rng_seed;
#else /* ! TLS */
static pthread_key_t rng_seed_key;
#endif /* ! TLS */
static unsigned int levelmax;

#define TRANSACTIONAL                   d->unit_tx

typedef intptr_t val_t;
typedef intptr_t level_t;
#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX


typedef struct sl_node {
  val_t val;
  intptr_t deleted;
  int toplevel;
  struct sl_node **next;
} sl_node_t;

typedef struct sl_intset {
  sl_node_t *head;
} sl_intset_t;

inline int rand_100()
{
#ifdef TLS
  return rand_r(rng_seed) % 100;
#else /* ! TLS */
  return rand_r((unsigned int *)pthread_getspecific(rng_seed_key)) % 100;
#endif /* ! TLS */
}

int get_rand_level() {
  int i, level = 1;
  for (i = 0; i < levelmax - 1; i++) {
    if (rand_100() < 50)
      level++;
    else
      break;
  }
  /* 1 <= level <= levelmax */
  return level;
}

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
sl_node_t *sl_new_simple_node(val_t val, int toplevel, int transactional)
{
  sl_node_t *node;
	
  if (transactional > 0 && transactional < 6)
    node = (sl_node_t *)MALLOC(sizeof(sl_node_t));
  else
    node = (sl_node_t *)malloc(sizeof(sl_node_t));
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  if (transactional > 0 && transactional < 6)
    node->next = (sl_node_t **)MALLOC(toplevel * sizeof(sl_node_t *));
  else
    node->next = (sl_node_t **)malloc(toplevel * sizeof(sl_node_t *));
  if (node->next == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->toplevel = toplevel;
  node->deleted = 0;

  return node;
}

/* 
 * Create a new node with its next field. 
 * If next=NULL, then this create a tail node. 
 */
sl_node_t *sl_new_node(val_t val, sl_node_t *next, int toplevel, int transactional)
{
  sl_node_t *node;
  int i;

  node = sl_new_simple_node(val, toplevel, transactional);

  for (i = 0; i < levelmax; i++)
    node->next[i] = next;
	
  return node;
}

void sl_delete_node(sl_node_t *n)
{
  free(n->next);
  free(n);
}

sl_intset_t *sl_set_new()
{
  sl_intset_t *set;
  sl_node_t *min, *max;
	
  if ((set = (sl_intset_t *)malloc(sizeof(sl_intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = sl_new_node(VAL_MAX, NULL, levelmax, 0);
  min = sl_new_node(VAL_MIN, max, levelmax, 0);
  set->head = min;
  return set;
}

void sl_set_delete(sl_intset_t *set)
{
  sl_node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->next[0];
    sl_delete_node(node);
    node = next;
  }
  free(set);
}

int sl_set_size(sl_intset_t *set)
{
  int size = 0;
  sl_node_t *node;

  node = set->head->next[0];
  while (node->next[0] != NULL) {
    if (!node->deleted)
      size++;
    node = node->next[0];
  }

  return size;
}
