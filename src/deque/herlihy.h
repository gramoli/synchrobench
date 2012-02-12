/*
 * File:
 *   harris.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Obstruction-free double-ended queue extended to circular arrays
 *	 "Obstruction-Free Synchronization: Double-Ended Queues as an Example"
 *	 M. Herlihy, V. Luchangco, M. Moir, p.522, ICDCS 2003.
 * 
 * Copyright (c) 2008-2009.
 *
 * herlihy.h is part of Synchrobench
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

#include "queue.h"

#define ATOMIC_STORE(a, v)           (AO_store_full((volatile AO_t *)(a), (AO_t)(v)))
#define ATOMIC_LOAD(a)               (AO_load_full((volatile AO_t *)(a)))

void printdq(circarr_t *queue);

int herlihy_rightpush_hint(circarr_t *queue, val_t val, int transactional);
int herlihy_rightpop_hint(circarr_t *queue, int transactional);
int herlihy_leftpush_hint(circarr_t *queue, val_t val, int transactional);
int herlihy_leftpop_hint(circarr_t *queue, int transactional);
