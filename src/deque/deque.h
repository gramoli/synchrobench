/*
 * File:
 *   deque.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Double-ended queue.
 * 
 * Copyright (c) 2008-2009.
 *
 * deque.h is part of Synchrobench
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

#include "herlihy.h"

int deque_rightpush(circarr_t *queue, val_t val, int transactional);
int deque_leftpush(circarr_t *queue, val_t val, int transactional);

int deque_leftpop(circarr_t *queue, int transactional);
int deque_rightpop(circarr_t *queue, int transactional);

int seq_rightpush(circarr_t *queue, val_t val, int transactional);
int seq_leftpush(circarr_t *queue, val_t val, int transactional);


