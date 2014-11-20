/******************************************************************************
 * replay.c
 * 
 * Replay the log output of search-structure runs.
 * Must build set_harness.c with DO_WRITE_LOG defined.
 * 
 * Copyright (c) 2002-2003, K A Fraser
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "portable_defns.h"

#define RMAX_THREADS 256
#define VERIFY_ORDERINGS

#define LOG_REPLAYED     (1<<26)
#define LOG_KEY_MASK     0xffffff

typedef struct log_st
{
    interval_t   start, end;
    unsigned int data;       /* key, and replay flag */
    void *val, *old_val;     /* op changed mapping from old_val to val */
} log_t;

#define REPLAYED(_l) ((_l)->data & LOG_REPLAYED)

static log_t *global_log;
static int nr_threads, nr_updates, nr_keys;
static int *key_offsets;
static int *success;
static unsigned int next_key = 0;
static pthread_mutex_t key_lock;


/*
 * GLOBAL LOGS SORTED ON:
 *  1. Key value
 *  2. Start time
 * 
 * Replayer deals with each key value in turn.
 */
static int compare(const void *t1, const void *t2)
{
    const log_t *l1 = t1;
    const log_t *l2 = t2;
    const int k1 = l1->data & LOG_KEY_MASK;
    const int k2 = l2->data & LOG_KEY_MASK;

    if ( k1 < k2 ) return(-1);
    if ( k1 > k2 ) return(+1);

    if ( l1->start < l2->start ) return(-1);

    return(+1);
}


static int do_op(log_t *log, void **key_state)
{
    if ( REPLAYED(log) || (log->old_val != *key_state) ) return(0);
    *key_state = log->val;
    log->data |= LOG_REPLAYED;
    return(1);
}


static void undo_op(log_t *log, void **key_state)
{
    assert(REPLAYED(log));
    log->data &= ~LOG_REPLAYED;
    *key_state = log->old_val;
}


/* Sink down element @pos of @heap. */
static void down_heap(log_t **heap, int *heap_offsets, log_t *log, int pos)
{
    int sz = (int)heap[0], nxt;
    log_t *tmp;
    while ( (nxt = (pos << 1)) <= sz )
    {
        if ( ((nxt+1) <= sz) && (heap[nxt+1]->end < heap[nxt]->end) ) nxt++;
        if ( heap[nxt]->end > heap[pos]->end ) break;
        heap_offsets[heap[pos] - log] = nxt;
        heap_offsets[heap[nxt] - log] = pos;
        tmp = heap[pos];
        heap[pos] = heap[nxt];
        heap[nxt] = tmp;
        pos = nxt;
    }
}

/* Float element @pos up @heap. */
static void up_heap(log_t **heap, int *heap_offsets, log_t *log, int pos)
{
    log_t *tmp;
    while ( pos > 1 )
    {
        if ( heap[pos]->end > heap[pos>>1]->end ) break;
        heap_offsets[heap[pos]    - log] = pos >> 1;
        heap_offsets[heap[pos>>1] - log] = pos;
        tmp = heap[pos];
        heap[pos]    = heap[pos>>1];
        heap[pos>>1] = tmp;
        pos >>= 1;
    }
}


/* Delete @entry from @heap. */
static void remove_entry(log_t **heap, int *heap_offsets, 
                         log_t *log, log_t *entry)
{
    int sz = (int)heap[0];
    int pos = heap_offsets[entry - log];
    heap_offsets[heap[sz] - log] = pos;
    heap[pos] = heap[sz];
    heap[0] = (void *)(--sz);
    if ( (pos > 1) && (heap[pos]->end < heap[pos>>1]->end) )
    { 
        up_heap(heap, heap_offsets, log, pos);
    }
    else
    {
        down_heap(heap, heap_offsets, log, pos);
    }
}


/* Add new entry @new to @heap. */
static void add_entry(log_t **heap, int *heap_offsets, log_t *log, log_t *new)
{
    int sz = (int)heap[0];
    heap[0] = (void *)(++sz);
    heap_offsets[new - log] = sz;
    heap[sz] = new;
    up_heap(heap, heap_offsets, log, sz);
}


/*
 * This linearisation algorithm is a depth-first search of all feasible
 * orderings. At each step, the next available operation is selected.
 * The set of "available" operations is those which:
 *  (1) have not already been selected on this search path
 *  (2) are operations whose results are correct given current state
 *      (eg. a failed delete couldn't be selected if the key is in the set!)
 *  (3) have start times <= the earliest end time in the set.
 * (1) ensures that each operation happens only once. (2) ensures that
 * abstract state is consistent between operations. (3) ensures that time
 * ordering is conserved.
 */
static int linearise_ops_for_key(
    log_t *log, int nr_items, log_t **stack, 
    log_t **cutoff_heap, int *heap_offsets, void **key_state)
{
    int i;
    log_t **sp = stack;
    interval_t cutoff;
    
    /* Construct cutoff heap. */
    cutoff_heap[0] = (void *)nr_items;
    for ( i = 0; i < nr_items; i++ ) 
    {
        cutoff_heap[i+1] = log + i;
        heap_offsets[i]  = i+1;
    }
    for ( i = nr_items>>1; i > 0; i-- ) 
    {
        down_heap(cutoff_heap, heap_offsets, log, i);
    }

    cutoff = cutoff_heap[1]->end;

    for ( i = 0; ; )
    {
        while ( (i < nr_items) && (log[i].start <= cutoff) )
        {
            if ( !do_op(&log[i], key_state) ) { i++; continue; }

            *sp++ = &log[i];
            
            /* Done? */
            if ( (sp - stack) == nr_items ) goto success;
            
            remove_entry(cutoff_heap, heap_offsets, log, &log[i]);
            cutoff = cutoff_heap[1]->end;
            i = 0;
        }
    
        /* Failure? */
        if ( (sp - stack) == 0 )
        {
            for ( i = -3; i < nr_items + 3; i++ )
            {
#if 1
                printf("%08x -> %08x -- %d: %08x -> %08x\n",
                       (unsigned int)log[i].start,
                       (unsigned int)log[i].end,
                       log[i].data & LOG_KEY_MASK,
                       (unsigned int)log[i].old_val, 
                       (unsigned int)log[i].val);    
#endif
            }
            return(0);
        }

        i = *--sp - log;
        undo_op(&log[i], key_state);
        add_entry(cutoff_heap, heap_offsets, log, &log[i]);
        cutoff = cutoff_heap[1]->end;
        i++;
    }

 success:
    return(1);
}


static void *thread_start(void *arg)
{
    unsigned long tid = (unsigned long)arg;
    unsigned int our_key;
    int ch_start, ch_end, start, end, nr_items, *heap_offsets;
    log_t **stack;
    log_t **cutoff_heap;
    interval_t cutoff;
    void *key_state;
#ifdef VERIFY_ORDERINGS
    int i;
#endif

    stack        = malloc((nr_threads*nr_updates+1)*sizeof(log_t*));
    cutoff_heap  = malloc((nr_threads*nr_updates+1)*sizeof(*cutoff_heap));
    heap_offsets = malloc((nr_threads*nr_updates+1)*sizeof(*heap_offsets));
    if ( !stack || !cutoff_heap || !heap_offsets )
    {
        fprintf(stderr, "Error allocating space for stacks\n");
        return(NULL);
    }

 again:
    pthread_mutex_lock(&key_lock);
    our_key = next_key++;
    pthread_mutex_unlock(&key_lock);
    if ( our_key >= nr_keys ) goto out;

    start    = key_offsets[our_key];
    end      = key_offsets[our_key+1];
    nr_items = end - start;

    printf("[Thread %lu] ++ Linearising key %d (%d events)\n",
           tid, our_key, nr_items);

#if 0
    {
        int i;
        for ( i = start; i < end; i++ )
        {
            printf("%04d/%04d -- %08x -> %08x -- %d: %08x -> %08x\n", 
                   our_key, i - start,
                   (unsigned int)global_log[i].start,
                   (unsigned int)global_log[i].end,
                   global_log[i].data & LOG_KEY_MASK,
                   (unsigned int)global_log[i].old_val, 
                   (unsigned int)global_log[i].val);    
        }
    }
#endif

    /*
     * We divide operations into independent chunks. A chunk is a maximal
     * sequence of operations, ordered on start time, that does not
     * overlap with any operation in any other chunk. Clearly, finding
     * a linearisation for each chunk produces a total schedule.
     */
    success[our_key] = 1;
    key_state        = 0;
    for ( ch_start = start; ch_start < end; ch_start = ch_end )
    {
        cutoff = global_log[ch_start].end;
        for ( ch_end = ch_start; ch_end < end; ch_end++ )
        {
            if ( global_log[ch_end].start > cutoff ) break;
            if ( global_log[ch_end].end > cutoff )
                cutoff = global_log[ch_end].end;
        }

        /* Linearise chunk ch_start -> ch_end. */
        success[our_key] = linearise_ops_for_key(
            &global_log[ch_start], 
            ch_end - ch_start,
            &stack[ch_start - start], 
            cutoff_heap, 
            heap_offsets,
            &key_state);

        if ( !success[our_key] )
        {
            printf("[Thread %lu] -- Linearisation FAILED for key %d\n",
                   tid, our_key);
            goto again;
        }
    }

    printf("[Thread %lu] -- Linearisation %s for key %d\n",
           tid, (success[our_key] ? "found" : "FAILED"), our_key);

#ifdef VERIFY_ORDERINGS
    printf("[Thread %lu] ++ Verifying key %d\n", tid, our_key);
    cutoff    = 0;
    key_state = 0;
    for ( i = 0; i < nr_items; i++ )
    {
        stack[i]->data &= ~LOG_REPLAYED; /* stop valid_op() from choking */
        if ( !do_op(stack[i], &key_state) || (stack[i]->end < cutoff) )
        {
            int j;
            fprintf(stderr, "\t*** INTERNAL ERROR: "
                    "Assigned ordering is invalid!\n");
            for ( j = (i < 2) ? 0 : (i-2); j < i+6; j++ )
            {
                printf("%08x -> %08x -- %d: %08x -> %08x\n",
                       (unsigned int)stack[j]->start,
                       (unsigned int)stack[j]->end,
                       stack[j]->data & LOG_KEY_MASK,
                       (unsigned int)stack[j]->old_val, 
                       (unsigned int)stack[j]->val);
            }
            exit(-1);
        }
        if ( stack[i]->start > cutoff ) cutoff = stack[i]->start;
    }
    printf("[Thread %lu] -- Verified key %d\n", tid, our_key);
#endif

    goto again;

 out:
    return(NULL);
}


int main(int argc, char **argv)
{
    pthread_t thread[RMAX_THREADS];
    int fd, i, j, failed = 0, nr_cpus;
    unsigned long log_header[3];

    if ( argc != 2 )
    {
        fprintf(stderr, "%s <log name>\n", argv[0]);
        exit(1);
    }

    nr_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if ( nr_cpus > RMAX_THREADS ) nr_cpus = RMAX_THREADS;

    if ( (fd = open(argv[1], O_RDONLY, 0)) == -1 )
    {
        fprintf(stderr, "Error opening log\n");
        exit(-1);
    }
    
    /* Grok the log header. */
    read(fd, log_header, sizeof(log_header));
    nr_threads = log_header[0];
    nr_updates = log_header[1];
    nr_keys    = log_header[2];
    printf("Read log header: nr_updates=%d, nr_threads=%d, nr_keys=%d\n",
           nr_updates, nr_threads, nr_keys);

    /* Allocate state for processing log entries. */
    global_log  = malloc((nr_threads*nr_updates+1)*sizeof(log_t));
    key_offsets = malloc((nr_keys+1)*sizeof(*key_offsets));
    success     = malloc(nr_keys*sizeof(*success));
    if ( !global_log || !key_offsets || !success )
    {
        fprintf(stderr, "Error allocating space for log\n");
        exit(-1);
    }
    
    /* Read log entries, and sort into key and timestamp order. */
    read(fd, global_log, nr_threads*nr_updates*sizeof(log_t));
    global_log[nr_threads*nr_updates].data = LOG_KEY_MASK; /* sentinel */

    printf("Sorting logs..."); fflush(stdout);
    qsort(global_log, nr_threads*nr_updates, sizeof(log_t), compare);
    printf(" done\n");

    /* Find offsets of key regions in global table. */
    key_offsets[0] = 0;
    nr_keys        = 0;
    for ( i = 0; i < (nr_threads * nr_updates); i = j )
    {
        j = i+1;
        while ( (global_log[j].data & LOG_KEY_MASK) ==
                (global_log[i].data & LOG_KEY_MASK) ) j++;
        key_offsets[++nr_keys] = j;
    }

    /* Set up a bunch of worker threads.... */
    pthread_mutex_init(&key_lock, NULL);
    for ( i = 0; i < nr_cpus; i++ )
    {
        if ( pthread_create(&thread[i], NULL, thread_start, (void *)i) )
        {
            fprintf(stderr, "Error creating thread %d (%d)\n", i, errno);
            exit(1);
        }
    }

    /* ...and wait for them all to complete. */
    for ( i = 0; i < nr_cpus; i++ )
    {
        pthread_join(thread[i], NULL);
    }

    /* Summarise results from worker threads. */
    for ( i = 0; i < nr_keys; i++ )
    {
        if ( success[i] ) continue;
        printf("FAILED on key %d\n", i);
        failed++;
    }

    if ( failed )
    {
        printf("Failed on %d keys\n", failed);
        return(1);
    }

    printf("All assigned orderings are valid\n");
    return(0);
}
