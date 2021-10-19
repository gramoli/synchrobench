/*
 * File:
 *   gc.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Epoch-based garbage collector.
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
#include <stdint.h>

#include <pthread.h>

#include "gc.h"

#include "atomic.h"
#include "stm.h"

/* TODO: could be made much more efficient by allocating large chunks. */

/* ################################################################### *
 * DEFINES
 * ################################################################### */

#define MAX_THREADS                     1024
#define EPOCH_MAX                       (~(gc_word_t)0)

#ifndef NO_PERIODIC_CLEANUP
# ifndef CLEANUP_FREQUENCY
#  define CLEANUP_FREQUENCY             1
# endif /* ! CLEANUP_FREQUENCY */
#endif /* ! NO_PERIODIC_CLEANUP */

#ifdef DEBUG
/* Note: stdio is thread-safe */
# define IO_FLUSH                       fflush(NULL)
# define PRINT_DEBUG(...)               printf(__VA_ARGS__); fflush(NULL)
#else /* ! DEBUG */
# define IO_FLUSH
# define PRINT_DEBUG(...)
#endif /* ! DEBUG */

/* ################################################################### *
 * TYPES
 * ################################################################### */

enum {                                  /* Descriptor status */
  GC_NULL = 0,
  GC_BUSY = 1,
  GC_FREE_EMPTY = 2,
  GC_FREE_FULL = 3
};

typedef struct mem_block {              /* Block of allocated memory */
  void *addr;                           /* Address of memory */
  struct mem_block *next;               /* Next block */
} mem_block_t;

typedef struct mem_region {             /* A list of allocated memory blocks */
  struct mem_block *blocks;             /* Memory blocks */
  gc_word_t ts;                         /* Allocation timestamp */
  struct mem_region *next;              /* Next region */
} mem_region_t;

typedef struct tm_thread {              /* Descriptor of an active thread */
  gc_word_t used;                       /* Is this entry used? */
  pthread_t thread;                     /* Thread descriptor */
  gc_word_t ts;                         /* Start timestamp */
  mem_region_t *head;                   /* First memory region(s) assigned to thread */
  mem_region_t *tail;                   /* Last memory region(s) assigned to thread */
#ifndef NO_PERIODIC_CLEANUP
  unsigned int frees;                   /* How many blocks have been freed? */
#endif /* ! NO_PERIODIC_CLEANUP */
} tm_thread_t;

static volatile tm_thread_t *threads;   /* Array of active threads */
static volatile gc_word_t nb_threads;   /* Number of active threads */

static gc_word_t (*current_epoch)();    /* Read the value of the current epoch */

#ifdef TLS
static __thread int thread_idx;
#else /* ! TLS */
static pthread_key_t thread_idx;
#endif /* ! TLS */

/* ################################################################### *
 * STATIC
 * ################################################################### */

/*
 * Returns the index of the CURRENT thread.
 */
static inline int gc_get_idx()
{
#ifdef TLS
  return thread_idx;
#else /* ! TLS */
  return (int)pthread_getspecific(thread_idx);
#endif /* ! TLS */
}

/*
 * Compute a lower bound on the minimum start time of all active transactions.
 */
static inline gc_word_t gc_compute_min(gc_word_t now)
{
  int i;
  gc_word_t min, ts;
  stm_word_t used;

  PRINT_DEBUG("==> gc_compute_min(%d)\n", gc_get_idx());

  min = now;
  for (i = 0; i < MAX_THREADS; i++) {
    used = (gc_word_t)ATOMIC_LOAD(&threads[i].used);
    if (used == GC_NULL)
      break;
    if (used != GC_BUSY)
      continue;
    /* Used entry */
    ts = (gc_word_t)ATOMIC_LOAD(&threads[i].ts);
    if (ts < min)
      min = ts;
  }

  PRINT_DEBUG("==> gc_compute_min(%d,m=%lu)\n", gc_get_idx(), (unsigned long)min);

  return min;
}

/*
 * Free block list.
 */
static inline void gc_clean_blocks(mem_block_t *mb)
{
  mem_block_t *next_mb;

  while (mb != NULL) {
    PRINT_DEBUG("==> free(%d,a=%p)\n", gc_get_idx(), mb->addr);
    free(mb->addr);
    next_mb = mb->next;
    free(mb);
    mb = next_mb;
  }
}

/*
 * Free region list.
 */
static inline void gc_clean_regions(mem_region_t *mr)
{
  mem_region_t *next_mr;

  while (mr != NULL) {
    gc_clean_blocks(mr->blocks);
    next_mr = mr->next;
    free(mr);
    mr = next_mr;
  }
}

/*
 * Garbage-collect old data associated with a thread.
 */
void gc_cleanup_thread(int idx, gc_word_t min)
{
  mem_region_t *mr;

  PRINT_DEBUG("==> gc_cleanup_thread(%d,m=%lu)\n", idx, (unsigned long)min);

  if (threads[idx].head == NULL) {
    /* Nothing to clean up */
    return;
  }

  while (min > threads[idx].head->ts) {
    gc_clean_blocks(threads[idx].head->blocks);
    mr = threads[idx].head->next;
    free(threads[idx].head);
    threads[idx].head = mr;
    if(mr == NULL) {
      /* All memory regions deleted */
      threads[idx].tail = NULL;
      break;
    }
  }
}

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Initialize GC library (to be called from main thread).
 */
void gc_init(gc_word_t (*epoch)())
{
  int i;

  PRINT_DEBUG("==> gc_init()\n");

  current_epoch = epoch;
  if ((threads = (tm_thread_t *)malloc(MAX_THREADS * sizeof(tm_thread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  for (i = 0; i < MAX_THREADS; i++) {
    threads[i].used = GC_NULL;
    threads[i].ts = EPOCH_MAX;
    threads[i].head = threads[i].tail = NULL;
#ifndef NO_PERIODIC_CLEANUP
    threads[i].frees = 0;
#endif /* ! NO_PERIODIC_CLEANUP */
  }
  nb_threads = 0;
#ifndef TLS
  if (pthread_key_create(&thread_idx, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
#endif /* ! TLS */
}

/*
 * Clean up GC library (to be called from main thread).
 */
void gc_exit()
{
  int i;

  PRINT_DEBUG("==> gc_exit()\n");

  /* Make sure that all threads have been stopped */
  if (ATOMIC_LOAD(&nb_threads) != 0) {
    fprintf(stderr, "Error: some threads have not been cleaned up\n");
    exit(1);
  }
  /* Clean up memory */
  for (i = 0; i < MAX_THREADS; i++)
    gc_clean_regions(threads[i].head);

  free((void *)threads);
}

/*
 * Initialize thread-specific GC resources (to be called once by each thread).
 */
void gc_init_thread()
{
  int i, idx;
  gc_word_t used;

  PRINT_DEBUG("==> gc_init_thread()\n");

  if (ATOMIC_FETCH_INC_FULL(&nb_threads) >= MAX_THREADS) {
    fprintf(stderr, "Error: too many concurrent threads created\n");
    exit(1);
  }
  /* Find entry in threads array */
  i = 0;
  /* TODO: not wait-free */
  while (1) {
    used = (gc_word_t)ATOMIC_LOAD(&threads[i].used);
    if (used != GC_BUSY) {
      if (ATOMIC_CAS_FULL(&threads[i].used, used, GC_BUSY) != 0) {
        idx = i;
        /* Sets lower bound to current time (transactions by this thread cannot happen before) */
        ATOMIC_STORE(&threads[idx].ts, current_epoch());
        break;
      }
      /* CAS failed: anotehr thread must have acquired slot */
      assert (threads[i].used != GC_NULL);
    }
    if (++i >= MAX_THREADS)
      i = 0;
  }
#ifdef TLS
  thread_idx = idx;
#else /* ! TLS */
  pthread_setspecific(thread_idx, (void *)idx);
#endif /* ! TLS */

  PRINT_DEBUG("==> gc_init_thread(i=%d)\n", idx);
}

/*
 * Clean up thread-specific GC resources (to be called once by each thread).
 */
void gc_exit_thread()
{
  int idx = gc_get_idx();

  PRINT_DEBUG("==> gc_exit_thread(%d)\n", idx);

  /* No more lower bound for this thread */
  ATOMIC_STORE(&threads[idx].ts, EPOCH_MAX);
  /* Release slot */
  ATOMIC_STORE(&threads[idx].used, threads[idx].head == NULL ? GC_FREE_EMPTY : GC_FREE_FULL);
  ATOMIC_FETCH_DEC_FULL(&nb_threads);
  /* Leave memory for next thread to cleanup */
}

/*
 * Set new epoch (to be called by each thread, typically when starting
 * new transactions to indicate their start timestamp).
 */
void gc_set_epoch(gc_word_t epoch)
{
  int idx = gc_get_idx();

  PRINT_DEBUG("==> gc_set_epoch(%d,%lu)\n", idx, (unsigned long)epoch);

  if (epoch >= EPOCH_MAX) {
    fprintf(stderr, "Exceeded maximum epoch number: 0x%lx\n", (unsigned long)epoch);
    /* Do nothing (will prevent data from being garbage collected) */
    return;
  }

  /* Do not need a barrier as we only compute lower bounds */
  ATOMIC_STORE(&threads[idx].ts, epoch);
}

/*
 * Free memory (the thread must indicate the current timestamp).
 */
void gc_free(void *addr, gc_word_t epoch)
{
  mem_region_t *mr;
  mem_block_t *mb;
  int idx = gc_get_idx();

  PRINT_DEBUG("==> gc_free(%d,%lu)\n", idx, (unsigned long)epoch);

  if (threads[idx].head == NULL || threads[idx].head->ts < epoch) {
    /* Allocate a new region */
    if ((mr = (mem_region_t *)malloc(sizeof(mem_region_t))) == NULL) {
      perror("malloc");
      exit(1);
    }
    mr->ts = epoch;
    mr->blocks = NULL;
    mr->next = NULL;
    if (threads[idx].head == NULL) {
      threads[idx].head = threads[idx].tail = mr;
    } else {
      threads[idx].tail->next = mr;
      threads[idx].tail = mr;
    }
  } else {
    /* Add to current region */
    mr = threads[idx].head;
  }

  /* Allocate block */
  if ((mb = (mem_block_t *)malloc(sizeof(mem_block_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  mb->addr = addr;
  mb->next = mr->blocks;
  mr->blocks = mb;

#ifndef NO_PERIODIC_CLEANUP
  threads[idx].frees++;
  if (threads[idx].frees % CLEANUP_FREQUENCY == 0)
    gc_cleanup();
#endif /* ! NO_PERIODIC_CLEANUP */
}

/*
 * Garbage-collect old data associated with the current thread (should
 * be called periodically).
 */
void gc_cleanup()
{
  gc_word_t min;
  int idx = gc_get_idx();

  PRINT_DEBUG("==> gc_cleanup(%d)\n", idx);

  if (threads[idx].head == NULL) {
    /* Nothing to clean up */
    return;
  }

  min = gc_compute_min(current_epoch());

  gc_cleanup_thread(idx, min);
}

/*
 * Garbage-collect old data associated with all threads (should be
 * called periodically).
 */
void gc_cleanup_all()
{
  int i;
  gc_word_t min = EPOCH_MAX;

  PRINT_DEBUG("==> gc_cleanup_all()\n");

  for (i = 0; i < MAX_THREADS; i++) {
    if ((gc_word_t)ATOMIC_LOAD(&threads[i].used) == GC_NULL)
      break;
    if ((gc_word_t)ATOMIC_LOAD(&threads[i].used) == GC_FREE_FULL) {
      if (ATOMIC_CAS_FULL(&threads[i].used, GC_FREE_FULL, GC_BUSY) != 0) {
        if (min == EPOCH_MAX)
          min = gc_compute_min(current_epoch());
        gc_cleanup_thread(i, min);
        ATOMIC_STORE(&threads[i].used, threads[i].head == NULL ? GC_FREE_EMPTY : GC_FREE_FULL);
      }
    }
  }
}

/*
 * Reset all epochs for all threads (must be called with all threads
 * stopped and out of transactions, e.g., upon roll-over).
 */
void gc_reset()
{
  int i;

  PRINT_DEBUG("==> gc_reset()\n");

  assert(nb_threads == 0);

  for (i = 0; i < MAX_THREADS; i++) {
    if (threads[i].used == GC_NULL)
      break;
    gc_clean_regions(threads[i].head);
    threads[i].ts = EPOCH_MAX;
    threads[i].head = threads[i].tail = NULL;
#ifndef NO_PERIODIC_CLEANUP
    threads[i].frees = 0;
#endif /* ! NO_PERIODIC_CLEANUP */
  }
}
