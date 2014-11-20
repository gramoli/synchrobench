/******************************************************************************
 * gc.c
 * 
 * A fully recycling epoch-based garbage collector. Works by counting
 * threads in and out of critical regions, to work out when
 * garbage queues can be fully deleted.
 * 
 * Copyright (c) 2001-2003, K A Fraser
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "portable_defns.h"
#include "gc.h"

//#define MINIMAL_GC
/*#define YIELD_TO_HELP_PROGRESS*/
//#define PROFILE_GC

/* Recycled nodes are filled with this value if WEAK_MEM_ORDER. */
#define INVALID_BYTE 0
#define INITIALISE_NODES(_p,_c) memset((_p), INVALID_BYTE, (_c));

/* Number of unique block sizes we can deal with. */
#define MAX_SIZES 20

#define MAX_HOOKS 4

/*
 * The initial number of allocation chunks for each per-blocksize list.
 * Popular allocation lists will steadily increase the allocation unit
 * in line with demand.
 */
#define ALLOC_CHUNKS_PER_LIST 10

/*
 * How many times should a thread call gc_enter(), seeing the same epoch
 * each time, before it makes a reclaim attempt?
 */
#define ENTRIES_PER_RECLAIM_ATTEMPT 100

/*
 *  0: current epoch -- threads are moving to this;
 * -1: some threads may still throw garbage into this epoch;
 * -2: no threads can see this epoch => we can zero garbage lists;
 * -3: all threads see zeros in these garbage lists => move to alloc lists.
 */
#ifdef WEAK_MEM_ORDER
#define NR_EPOCHS 4
#else
#define NR_EPOCHS 3
#endif

/*
 * A chunk amortises the cost of allocation from shared lists. It also
 * helps when zeroing nodes, as it increases per-cacheline pointer density
 * and means that node locations don't need to be brought into the cache 
 * (most architectures have a non-temporal store instruction).
 */
#define BLKS_PER_CHUNK 100
typedef struct chunk_st chunk_t;
struct chunk_st
{
    chunk_t *next;             /* chunk chaining                 */
    unsigned int i;            /* the next entry in blk[] to use */
    void *blk[BLKS_PER_CHUNK];
};

static struct gc_global_st
{
    CACHE_PAD(0);

    /* The current epoch. */
    VOLATILE unsigned int current;
    CACHE_PAD(1);

    /* Exclusive access to gc_reclaim(). */
    VOLATILE unsigned int inreclaim;
    CACHE_PAD(2);

    /*
     * RUN-TIME CONSTANTS (to first approximation)
     */

    /* Memory page size, in bytes. */
    unsigned int page_size;

    /* Node sizes (run-time constants). */
    int nr_sizes;
    int blk_sizes[MAX_SIZES];

    /* Registered epoch hooks. */
    int nr_hooks;
    hook_fn_t hook_fns[MAX_HOOKS];
    CACHE_PAD(3);

    /*
     * DATA WE MAY HIT HARD
     */

    /* Chain of free, empty chunks. */
    chunk_t * VOLATILE free_chunks;

    /* Main allocation lists. */
    chunk_t * VOLATILE alloc[MAX_SIZES];
    VOLATILE unsigned int alloc_size[MAX_SIZES];
#ifdef PROFILE_GC
    VOLATILE unsigned int total_size;
    VOLATILE unsigned int allocations;
    VOLATILE unsigned int num_reclaims;
#endif
} gc_global;


/* Per-thread state. */
struct gc_st
{
    /* Epoch that this thread sees. */
    unsigned int epoch;

    /* Number of calls to gc_entry() since last gc_reclaim() attempt. */
    unsigned int entries_since_reclaim;

#ifdef YIELD_TO_HELP_PROGRESS
    /* Number of calls to gc_reclaim() since we last yielded. */
    unsigned int reclaim_attempts_since_yield;
#endif

    /* Used by gc_async_barrier(). */
    void *async_page;
    int   async_page_state;

    /* Garbage lists. */
    chunk_t *garbage[NR_EPOCHS][MAX_SIZES];
    chunk_t *garbage_tail[NR_EPOCHS][MAX_SIZES];
    chunk_t *chunk_cache;

    /* Local allocation lists. */
    chunk_t *alloc[MAX_SIZES];
    unsigned int alloc_chunks[MAX_SIZES];

    /* Hook pointer lists. */
    chunk_t *hook[NR_EPOCHS][MAX_HOOKS];
};


#define MEM_FAIL(_s)                                                         \
do {                                                                         \
    fprintf(stderr, "OUT OF MEMORY: %d bytes at line %d\n", (_s), __LINE__); \
    exit(1);                                                                 \
} while ( 0 )


/* Allocate more empty chunks from the heap. */
#define CHUNKS_PER_ALLOC 1000
static chunk_t *alloc_more_chunks(void)
{
    int i;
    chunk_t *h, *p;

    h = p = ALIGNED_ALLOC(CHUNKS_PER_ALLOC * sizeof(*h));
    if ( h == NULL ) MEM_FAIL(CHUNKS_PER_ALLOC * sizeof(*h));

    for ( i = 1; i < CHUNKS_PER_ALLOC; i++ )
    {
        p->next = p + 1;
        p++;
    }

    p->next = h;

    return(h);
}


/* Put a chain of chunks onto a list. */
static void add_chunks_to_list(chunk_t *ch, chunk_t *head)
{
    chunk_t *h_next, *new_h_next, *ch_next;
    ch_next    = ch->next;
    new_h_next = head->next;
    do { ch->next = h_next = new_h_next; WMB_NEAR_CAS(); }
    while ( (new_h_next = CASPO(&head->next, h_next, ch_next)) != h_next );
}


/* Allocate a chain of @n empty chunks. Pointers may be garbage. */
static chunk_t *get_empty_chunks(int n)
{
    int i;
    chunk_t *new_rh, *rh, *rt, *head;

 retry:
    head = gc_global.free_chunks;
    new_rh = head->next;
    do {
        rh = new_rh;
        rt = head;
        WEAK_DEP_ORDER_RMB();
        for ( i = 0; i < n; i++ )
        {
            if ( (rt = rt->next) == head )
            {
                /* Allocate some more chunks. */
                add_chunks_to_list(alloc_more_chunks(), head);
                goto retry;
            }
        }
    }
    while ( (new_rh = CASPO(&head->next, rh, rt->next)) != rh );

    rt->next = rh;
    return(rh);
}


/* Get @n filled chunks, pointing at blocks of @sz bytes each. */
static chunk_t *get_filled_chunks(int n, int sz)
{
    chunk_t *h, *p;
    char *node;
    int i;

#ifdef PROFILE_GC
    ADD_TO(gc_global.total_size, n * BLKS_PER_CHUNK * sz);
    ADD_TO(gc_global.allocations, 1);
#endif

    node = ALIGNED_ALLOC(n * BLKS_PER_CHUNK * sz);
    if ( node == NULL ) MEM_FAIL(n * BLKS_PER_CHUNK * sz);
#ifdef WEAK_MEM_ORDER
    INITIALISE_NODES(node, n * BLKS_PER_CHUNK * sz);
#endif

    h = p = get_empty_chunks(n);
    do {
        p->i = BLKS_PER_CHUNK;
        for ( i = 0; i < BLKS_PER_CHUNK; i++ )
        {
            p->blk[i] = node;
            node += sz;
        }
    }
    while ( (p = p->next) != h );

    return(h);
}


/*
 * gc_async_barrier: Cause an asynchronous barrier in all other threads. We do 
 * this by causing a TLB shootdown to be propagated to all other processors. 
 * Each time such an action is required, this function calls:
 *   mprotect(async_page, <page size>, <new flags>)
 * Each thread's state contains a memory page dedicated for this purpose.
 */
#ifdef WEAK_MEM_ORDER
static void gc_async_barrier(gc_t *gc)
{
    mprotect(gc->async_page, gc_global.page_size,
             gc->async_page_state ? PROT_READ : PROT_NONE);
    gc->async_page_state = !gc->async_page_state;
}
#else
#define gc_async_barrier(_g) ((void)0)
#endif


/* Grab a level @i allocation chunk from main chain. */
static chunk_t *get_alloc_chunk(gc_t *gc, int i)
{
    chunk_t *alloc, *p, *new_p, *nh;
    unsigned int sz;

    alloc = gc_global.alloc[i];
    new_p = alloc->next;

    do {
        p = new_p;
        while ( p == alloc )
        {
            sz = gc_global.alloc_size[i];
            nh = get_filled_chunks(sz, gc_global.blk_sizes[i]);
            ADD_TO(gc_global.alloc_size[i], sz >> 3);
            gc_async_barrier(gc);
            add_chunks_to_list(nh, alloc);
            p = alloc->next;
        }
        WEAK_DEP_ORDER_RMB();
    }
    while ( (new_p = CASPO(&alloc->next, p, p->next)) != p );

    p->next = p;
    assert(p->i == BLKS_PER_CHUNK);
    return(p);
}


#ifndef MINIMAL_GC
/*
 * gc_reclaim: Scans the list of struct gc_perthread looking for the lowest
 * maximum epoch number seen by a thread that's in the list code. If it's the
 * current epoch, the "nearly-free" lists from the previous epoch are 
 * reclaimed, and the epoch is incremented.
 */
static void gc_reclaim(void)
{
    ptst_t       *ptst, *first_ptst, *our_ptst = NULL;
    gc_t         *gc = NULL;
    unsigned long curr_epoch;
    chunk_t      *ch, *t;
    int           two_ago, three_ago, i, j;
    
    /* Barrier to entering the reclaim critical section. */
    if ( gc_global.inreclaim || CASIO(&gc_global.inreclaim, 0, 1) ) return;

    /*
     * Grab first ptst structure *before* barrier -- prevent bugs
     * on weak-ordered architectures.
     */
    first_ptst = ptst_first();
    MB();
    curr_epoch = gc_global.current;

    /* Have all threads seen the current epoch, or not in mutator code? */
    for ( ptst = first_ptst; ptst != NULL; ptst = ptst_next(ptst) )
    {
        if ( (ptst->count > 1) && (ptst->gc->epoch != curr_epoch) ) goto out;
    }

    /*
     * Three-epoch-old garbage lists move to allocation lists.
     * Two-epoch-old garbage lists are cleaned out.
     */
    two_ago   = (curr_epoch+2) % NR_EPOCHS;
    three_ago = (curr_epoch+1) % NR_EPOCHS;
    if ( gc_global.nr_hooks != 0 )
        our_ptst = (ptst_t *)pthread_getspecific(ptst_key);
    for ( ptst = first_ptst; ptst != NULL; ptst = ptst_next(ptst) )
    {
        gc = ptst->gc;

        for ( i = 0; i < gc_global.nr_sizes; i++ )
        {
#ifdef WEAK_MEM_ORDER
            int sz = gc_global.blk_sizes[i];
            if ( gc->garbage[two_ago][i] != NULL )
            {
                chunk_t *head = gc->garbage[two_ago][i];
                ch = head;
                do {
                    int j;
                    for ( j = 0; j < ch->i; j++ )
                        INITIALISE_NODES(ch->blk[j], sz);
                }
                while ( (ch = ch->next) != head );
            }
#endif

            /* NB. Leave one chunk behind, as it is probably not yet full. */
            t = gc->garbage[three_ago][i];
            if ( (t == NULL) || ((ch = t->next) == t) ) continue;
            gc->garbage_tail[three_ago][i]->next = ch;
            gc->garbage_tail[three_ago][i] = t;
            t->next = t;
            add_chunks_to_list(ch, gc_global.alloc[i]);
        }

        for ( i = 0; i < gc_global.nr_hooks; i++ )
        {
            hook_fn_t fn = gc_global.hook_fns[i];
            ch = gc->hook[three_ago][i];
            if ( ch == NULL ) continue;
            gc->hook[three_ago][i] = NULL;

            t = ch;
            do { for ( j = 0; j < t->i; j++ ) fn(our_ptst, t->blk[j]); }
            while ( (t = t->next) != ch );

            add_chunks_to_list(ch, gc_global.free_chunks);
        }
    }

    #ifdef PROFILE_GC
    ADD_TO(gc_global.num_reclaims, 1);
    #endif

    /* Update current epoch. */
    WMB();
    gc_global.current = (curr_epoch+1) % NR_EPOCHS;

 out:
    gc_global.inreclaim = 0;
}
#endif /* MINIMAL_GC */


void *gc_alloc(ptst_t *ptst, int alloc_id)
{
    gc_t *gc = ptst->gc;
    chunk_t *ch;

    ch = gc->alloc[alloc_id];
    if ( ch->i == 0 )
    {
        if ( gc->alloc_chunks[alloc_id]++ == 100 )
        {
            gc->alloc_chunks[alloc_id] = 0;
            add_chunks_to_list(ch, gc_global.free_chunks);
            gc->alloc[alloc_id] = ch = get_alloc_chunk(gc, alloc_id);
        }
        else
        {
            chunk_t *och = ch;
            ch = get_alloc_chunk(gc, alloc_id);
            ch->next  = och->next;
            och->next = ch;
            gc->alloc[alloc_id] = ch;        
        }
    }

    return ch->blk[--ch->i];
}


static chunk_t *chunk_from_cache(gc_t *gc)
{
    chunk_t *ch = gc->chunk_cache, *p = ch->next;

    if ( ch == p )
    {
        gc->chunk_cache = get_empty_chunks(100);
    }
    else
    {
        ch->next = p->next;
        p->next  = p;
    }

    p->i = 0;
    return(p);
}


void gc_free(ptst_t *ptst, void *p, int alloc_id) 
{
#ifndef MINIMAL_GC
    gc_t *gc = ptst->gc;
    chunk_t *prev, *new, *ch = gc->garbage[gc->epoch][alloc_id];

    if ( ch == NULL )
    {
        gc->garbage[gc->epoch][alloc_id] = ch = chunk_from_cache(gc);
        gc->garbage_tail[gc->epoch][alloc_id] = ch;
    }
    else if ( ch->i == BLKS_PER_CHUNK )
    {
        prev = gc->garbage_tail[gc->epoch][alloc_id];
        new  = chunk_from_cache(gc);
        gc->garbage[gc->epoch][alloc_id] = new;
        new->next  = ch;
        prev->next = new;
        ch = new;
    }

    ch->blk[ch->i++] = p;
#endif
}


void gc_add_ptr_to_hook_list(ptst_t *ptst, void *ptr, int hook_id)
{
    gc_t *gc = ptst->gc;
    chunk_t *och, *ch = gc->hook[gc->epoch][hook_id];

    if ( ch == NULL )
    {
        gc->hook[gc->epoch][hook_id] = ch = chunk_from_cache(gc);
    }
    else
    {
        ch = ch->next;
        if ( ch->i == BLKS_PER_CHUNK )
        {
            och       = gc->hook[gc->epoch][hook_id];
            ch        = chunk_from_cache(gc);
            ch->next  = och->next;
            och->next = ch;
        }
    }

    ch->blk[ch->i++] = ptr;
}


void gc_unsafe_free(ptst_t *ptst, void *p, int alloc_id)
{
    gc_t *gc = ptst->gc;
    chunk_t *ch;

    ch = gc->alloc[alloc_id];
    if ( ch->i < BLKS_PER_CHUNK )
    {
        ch->blk[ch->i++] = p;
    }
    else
    {
        gc_free(ptst, p, alloc_id);
    }
}


void gc_enter(ptst_t *ptst)
{
#ifdef MINIMAL_GC
    ptst->count++;
    MB();
#else
    gc_t *gc = ptst->gc;
    int new_epoch, cnt;
 
 retry:
    cnt = ptst->count++;
    MB();
    if ( cnt == 1 )
    {
        new_epoch = gc_global.current;
        if ( gc->epoch != new_epoch )
        {
            gc->epoch = new_epoch;
            gc->entries_since_reclaim        = 0;
#ifdef YIELD_TO_HELP_PROGRESS
            gc->reclaim_attempts_since_yield = 0;
#endif
        }
        else if ( gc->entries_since_reclaim++ == 100 )
        //else if ( gc->entries_since_reclaim++ == 5000 )
        {
            ptst->count--;
#ifdef YIELD_TO_HELP_PROGRESS
            if ( gc->reclaim_attempts_since_yield++ == 10000 )
            {
                gc->reclaim_attempts_since_yield = 0;
                sched_yield();
            }
#endif
            gc->entries_since_reclaim = 0;
            gc_reclaim();
            goto retry;    
        }
    }
#endif
}


void gc_exit(ptst_t *ptst)
{
    MB();
    ptst->count--;
}


gc_t *gc_init(void)
{
    gc_t *gc;
    int   i;

    gc = ALIGNED_ALLOC(sizeof(*gc));
    if ( gc == NULL ) MEM_FAIL(sizeof(*gc));
    memset(gc, 0, sizeof(*gc));

#ifdef WEAK_MEM_ORDER
    /* Initialise shootdown state. */
    gc->async_page = mmap(NULL, gc_global.page_size, PROT_NONE, 
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ( gc->async_page == (void *)MAP_FAILED ) MEM_FAIL(gc_global.page_size);
    gc->async_page_state = 1;
#endif

    gc->chunk_cache = get_empty_chunks(100);

    /* Get ourselves a set of allocation chunks. */
    for ( i = 0; i < gc_global.nr_sizes; i++ )
    {
        gc->alloc[i] = get_alloc_chunk(gc, i);
    }
    for ( ; i < MAX_SIZES; i++ )
    {
        gc->alloc[i] = chunk_from_cache(gc);
    }

    return(gc);
}


int gc_add_allocator(int alloc_size)
{
    int ni, i = gc_global.nr_sizes;
    while ( (ni = CASIO(&gc_global.nr_sizes, i, i+1)) != i ) i = ni;
    gc_global.blk_sizes[i]  = alloc_size;
    gc_global.alloc_size[i] = ALLOC_CHUNKS_PER_LIST;
    gc_global.alloc[i] = get_filled_chunks(ALLOC_CHUNKS_PER_LIST, alloc_size);
    return i;
}


void gc_remove_allocator(int alloc_id)
{
    /* This is a no-op for now. */
}


int gc_add_hook(hook_fn_t fn)
{
    int ni, i = gc_global.nr_hooks;
    while ( (ni = CASIO(&gc_global.nr_hooks, i, i+1)) != i ) i = ni;
    gc_global.hook_fns[i] = fn;
    return i;    
}


void gc_remove_hook(int hook_id)
{
    /* This is a no-op for now. */
}


void _destroy_gc_subsystem(void)
{
#ifdef PROFILE_GC
    printf("Total heap: %u bytes (%.2fMB) in %u allocations\n",
           gc_global.total_size, (double)gc_global.total_size / 1000000,
           gc_global.allocations);
    printf("Num reclaims = %lu\n", gc_global.num_reclaims);
#endif
}


void _init_gc_subsystem(void)
{
    memset(&gc_global, 0, sizeof(gc_global));

    gc_global.page_size   = (unsigned int)sysconf(_SC_PAGESIZE);
    gc_global.free_chunks = alloc_more_chunks();

    gc_global.nr_hooks = 0;
    gc_global.nr_sizes = 0;
}
