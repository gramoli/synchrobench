/*
 * File:
 *   intset.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Integer set stress test.
 *
 * Copyright (c) 2007-2008.
 *
 * This program is free software; you can redistribute it and/or
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

#define MALLOC(size)                    stm_malloc(size)

/* ################################################################### *
 * INT SET
 * ################################################################### */

typedef intptr_t val_t;
#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

int *A[MAXLENGTH];

typedef struct node {
	val_t val;
	struct node *next;
	struct node *prev;
} node_t;

typedef struct deque {
	node_t *head;
	node_t *tail;
} deque_t;

node_t *new_node(val_t val, node_t *next, int transactional)
{
  node_t *node;

  if (!transactional) {
    node = (node_t *)malloc(sizeof(node_t));
  } else {
    node = (node_t *)MALLOC(sizeof(node_t));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->next = next;

  return node;
}

intset_t *set_new()
{
  intset_t *set;
  node_t *min, *max;

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node(VAL_MAX, NULL, 0);
  min = new_node(VAL_MIN, max, 0);
  set->head = min;

  return set;
}

void set_delete(intset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->next;
    free(node);
    node = next;
  }
  free(set);
}

int set_size(intset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}
