/*
 * File:
 *   queue.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Double-ended queue (deque) implementation
 * 
 * Copyright (c) 2008-2009.
 *
 * queue.c is part of Synchrobench
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

#include "tm.h"

#ifdef DEBUG
#define IO_FLUSH                        fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0xFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_PUSHRATE                50

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define ATOMIC_CAS_MB(a, e, v)          (AO_compare_and_swap_full((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))

static volatile AO_t stop;

#define TRANSACTIONAL                   d->unit_tx

typedef intptr_t val_t;
#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

#ifdef DEBUG
#define IO_FLUSH                        fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define RIGHT 1
#define LEFT  0

/* Capacity of the deque */
#define MAX 131072
/* left and right marker */
extern val_t lhint;
extern val_t rhint;
/* 
 * Right Null, Left Null, and Dummy Null values 
 * that should be unique in the deque
 */
#define RN   VAL_MAX
#define LN   VAL_MIN
#define DN   VAL_MIN+1

/* 
 * A: array[0..MAX+1] of element initially there is some k in [0, MAX] 
 * such that A[i] = <LN,0> for all i in [0,k] 
 * and A[i] = <RN,0> for all i in [k+1,MAX+1]. 
 */
typedef struct node {
	int val;
	int ctr;
} node_t;

/* 
 * Size of MAX+2 to accomodate leftmost and rightmost 
 * locations that always contain null.
 */
node_t *A[MAX+2]; 
/* Circular array */
typedef node_t *circarr_t;

circarr_t *queue_new();
void queue_delete(circarr_t *arr);
int queue_size(circarr_t *arr);
