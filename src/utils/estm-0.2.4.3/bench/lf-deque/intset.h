/*
 * File:
 *   intset.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Integer set stress test.
 *
 * Copyright (c) 2007-2008.
 *
 * This program is free software: you can redistribute it and/or
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
#define RO                              0
#define RW                              0

#define TX_START(id, ro, type)          { sigjmp_buf *_e = stm_get_env(); stm_tx_attr_t _a = {id, ro}; sigsetjmp(*_e, 0); stm_start(_e, &_a, type)
#define TX_LOAD(addr)                   stm_load((stm_word_t *)addr)
#define TX_STORE(addr, val)             stm_store((stm_word_t *)addr, (stm_word_t)val)
#define TX_END							stm_commit(); }

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

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static volatile AO_t stop;

/* ################################################################### *
 * INT SET
 * ################################################################### */

#define TRANSACTIONAL                   d->unit_tx

typedef intptr_t val_t;
#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

typedef struct node {
	val_t val;
	struct node *next;
} node_t;

typedef struct intset {
	node_t *head;
} intset_t;

node_t *new_node(val_t val, node_t *next, int transactional);
intset_t *set_new();
void set_delete(intset_t *set);
int set_size(intset_t *set);
