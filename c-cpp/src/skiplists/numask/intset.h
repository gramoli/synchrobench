/*
 * File:
 *   intset.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list integer set operations 
 *
 * Copyright (c) 2009-2010.
 *
 * intset.h is part of Synchrobench
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
 *
 * NUMASK changes: use search_layer object instead of set struct
 */

#ifndef INTSET_H_
#define INTSET_T_

#include "queue.h"
#include "search.h"

int sl_contains_old(search_layer *sl, unsigned int key, int transactional);
int sl_add_old(search_layer *sl, unsigned int key, int transactional);
int sl_remove_old(search_layer *sl, unsigned int key, int transactional);

#endif /* INTSET_H_ */
