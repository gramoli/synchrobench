/*
 * File:
 *   intset.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list integer set operations 
 *
 * Copyright (c) 2009-2010.
 *
 * intset.c is part of Synchrobench
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

#include "intset.h"
#include "skiplist.h"
#include "nohotspot_ops.h"

#define MAXLEVEL    32

int sl_contains_old(set_t *set, unsigned int key, int transactional)
{
        return sl_contains(set, (sl_key_t) key);
}

int sl_add_old(set_t *set, unsigned int key, int transactional)
{
        return sl_insert(set, (sl_key_t) key, (val_t) ((long)key));
}

int sl_remove_old(set_t *set, unsigned int key, int transactional)
{
	return sl_delete(set, (sl_key_t) key);
}
