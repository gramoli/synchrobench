/*
 * File:
 *   versioned-lock.c
 * Description:
 *   Implements the versioned try-lock as explained in:
 *   A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang.
 *   arXiv:1502.01633, February 2015 and DISC 2015
 *
 * versioned-lock.c is part of Synchrobench
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

#include "versioned-lock.h"

verlock_t get_version(_Atomic(verlock_t)* lock) {
    return (atomic_load(lock) & ~((verlock_t)1));
}

int try_lock_at_version(_Atomic(verlock_t)* lock, verlock_t version) {
    return atomic_compare_exchange_strong(lock, &version, version+1);
}

void spinlock(_Atomic(verlock_t)* lock) {
    while (!try_lock_at_version(lock, get_version(lock))) {};
}

void unlock_and_increment_version(_Atomic(verlock_t)* lock) {
    atomic_fetch_add(lock, 1);
}

verlock_t unlock_increment_and_get_version(_Atomic(verlock_t)* lock) {
    verlock_t next_version = get_version(lock)+2;
    atomic_fetch_add(lock, 1);
    return next_version;
}

void unlock_without_increment_version(_Atomic(verlock_t)* lock) {
    atomic_fetch_sub(lock, 1);
}
