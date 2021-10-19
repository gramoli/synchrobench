/*
 * File:
 *   versioned-lock.h
 * Description:
 *   Implements the versioned try-lock as explained in:
 *   A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang.
 *   arXiv:1502.01633, February 2015 and DISC 2015
 *
 * versioned-lock.h is part of Synchrobench
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

#ifndef _VERSIONED_LOCK_H
#define _VERSIONED_LOCK_H

#include <stdint.h>
#include <stdatomic.h>

typedef uint32_t verlock_t;

verlock_t get_version(_Atomic(verlock_t)* lock);
int try_lock_at_version(_Atomic(verlock_t)* lock, verlock_t version);
void spinlock(_Atomic(verlock_t)* lock);
void unlock_and_increment_version(_Atomic(verlock_t)* lock);
verlock_t unlock_increment_and_get_version(_Atomic(verlock_t)* lock);
void unlock_without_increment_version(_Atomic(verlock_t)* lock);

#endif
