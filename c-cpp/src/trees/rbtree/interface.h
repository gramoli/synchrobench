/*
 * File:
 *   interface.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Interface macros for the STM that the red-black tree uses
 *
 * Copyright (c) 2009-2010.
 *
 * interface.h is part of Synchrobench
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

#include <atomic_ops.h>

#include "tm.h"

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0xFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY							4
#define DEFAULT_ALTERNATE							  0
#define DEFAULT_EFFECTIVE							  1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

//static volatile AO_t stop;

#ifdef TLS
extern __thread unsigned int *rng_seed;
#else /* ! TLS */
extern pthread_key_t rng_seed_key;
#endif /* ! TLS */

#define TRANSACTIONAL                   d->unit_tx

#define INIT_SET_PARAMETERS            /* Nothing */
#define TM_SHARED_READ(var)            TX_LOAD(&(var))
#define TM_SHARED_READ_P(var)          TX_LOAD(&(var))
#define TM_SHARED_WRITE(var, val)      TX_STORE(&(var), val)
#define TM_SHARED_WRITE_P(var, val)    TX_STORE(&(var), val)
#define TM_MALLOC(size)                MALLOC(size)
#define TM_FREE(ptr)                   FREE(ptr, sizeof(*ptr))
