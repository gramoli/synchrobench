/*
 * File:
 *   mod_stats.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Module for gathering global statistics about transactions.
 *
 * Copyright (c) 2007-2009.
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
#include <stdio.h>
#include <string.h>

#include "mod_stats.h"

#include "atomic.h"
#include "stm.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

typedef struct tx_stats {               /* Transaction statistics */
  unsigned long commits;                /* Total number of commits (cumulative) */
  unsigned long aborts;                 /* Total number of aborts (cumulative) */
  unsigned long retries;                /* Number of consecutive aborts (retries) */
  unsigned long max_retries;            /* Maximum number of consecutive aborts (retries) */
} tx_stats_t;

static int key;
static int initialized = 0;

static tx_stats_t global_stats = { 0, 0, 0, 0 };

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Return aggregate statistics about transactions.
 */
int stm_get_global_stats(const char *name, void *val)
{
  if (!initialized) {
    fprintf(stderr, "Module mod_stats not initialized\n");
    exit(1);
  }

  if (strcmp("global_nb_commits", name) == 0) {
    *(unsigned long *)val = global_stats.commits;
    return 1;
  }
  if (strcmp("global_nb_aborts", name) == 0) {
    *(unsigned long *)val = global_stats.aborts;
    return 1;
  }
  if (strcmp("global_max_retries", name) == 0) {
    *(unsigned long *)val = global_stats.max_retries;
    return 1;
  }

  return 0;
}

/*
 * Return statistics about current thread.
 */
int stm_get_local_stats(TXPARAMS const char *name, void *val)
{
  tx_stats_t *stats;

  if (!initialized) {
    fprintf(stderr, "Module mod_stats not initialized\n");
    exit(1);
  }

  stats = (tx_stats_t *)stm_get_specific(TXARGS key);
  assert(stats != NULL);

  if (strcmp("nb_commits", name) == 0) {
    *(unsigned long *)val = stats->commits;
    return 1;
  }
  if (strcmp("nb_aborts", name) == 0) {
    *(unsigned long *)val = stats->aborts;
    return 1;
  }
  if (strcmp("max_retries", name) == 0) {
    *(unsigned long *)val = stats->max_retries;
    return 1;
  }

  return 0;
}

/*
 * Called upon thread creation.
 */
static void on_thread_init(TXPARAMS void *arg)
{
  tx_stats_t *stats;

  if ((stats = (tx_stats_t *)malloc(sizeof(tx_stats_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  stats->commits = 0;
  stats->aborts = 0;
  stats->retries = 0;
  stats->max_retries = 0;

  stm_set_specific(TXARGS key, stats);
}

/*
 * Called upon thread deletion.
 */
static void on_thread_exit(TXPARAMS void *arg)
{
  tx_stats_t *stats;
  unsigned long max;

  stats = (tx_stats_t *)stm_get_specific(TXARGS key);
  assert(stats != NULL);

  ATOMIC_FETCH_ADD_FULL(&global_stats.commits, stats->commits);
  ATOMIC_FETCH_ADD_FULL(&global_stats.aborts, stats->aborts);
 retry:
  max = ATOMIC_LOAD(&global_stats.max_retries);
  if (stats->max_retries > max) {
    if (ATOMIC_CAS_FULL(&global_stats.max_retries, max, stats->max_retries) == 0)
      goto retry;
  }

  free(stats);
}

/*
 * Called upon transaction commit.
 */
static void on_commit(TXPARAMS void *arg)
{
  tx_stats_t *stats;

  stats = (tx_stats_t *)stm_get_specific(TXARGS key);
  assert(stats != NULL);

  stats->commits++;
  stats->retries = 0;
}

/*
 * Called upon transaction abort.
 */
static void on_abort(TXPARAMS void *arg)
{
  tx_stats_t *stats;

  stats = (tx_stats_t *)stm_get_specific(TXARGS key);
  assert(stats != NULL);

  stats->aborts++;
  stats->retries++;
  if (stats->max_retries < stats->retries)
    stats->max_retries = stats->retries;
}

/*
 * Initialize module.
 */
void mod_stats_init()
{
  if (initialized)
    return;

  stm_register(on_thread_init, on_thread_exit, NULL, on_commit, on_abort, NULL);
  key = stm_create_specific();
  if (key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  initialized = 1;
}
