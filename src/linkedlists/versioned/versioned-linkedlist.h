/*
 * File:
 *   versioned-linkedlist.h
 * Description:
 *   The original Versioned Linked List. This algorithm exploits a versioned try-lock
 *   to achieve optimal concurrency as explained in:
 *   A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang.
 *   arXiv:1502.01633, February 2015 and DISC 2015
 *
 * versioned-linkedlist.h is part of Synchrobench
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

#ifndef _VERSIONED_LINKEDLIST_H
#define _VERSIONED_LINKEDLIST_H

#include <stdbool.h>
#include "../../utils/versioned-lock/versioned-lock.h"

#define ALGONAME "Versioned Linked List"

struct node {
  val_t val;
  struct node* next;
  int deleted;
  _Atomic(verlock_t) lock;
};

struct intset {
  node_t* head;
};

#endif
