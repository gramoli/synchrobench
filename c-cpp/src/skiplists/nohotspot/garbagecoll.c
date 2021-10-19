/*
 * garbagecoll.c: garbage collection for the skip list
 *
 * Author: Ian Dick, 2013.
 */

/*

Module overview

This module includes all the functions needed to perform
garbage collection during concurrenct use of the skip list.
The approach used here is based on the one used by Keir Fraser
in his lock-free skip list implementation, available here:
http://www.cl.cam.ac.uk/research/srg/netos/lock-free

*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "ptst.h"
#include "garbagecoll.h"
#include "skiplist.h"

/*
 * number of unique blk sizes we can deal with
 * (1 for node and 1 for index node)
 *
 */
#define MAX_SIZES 2

#define NUM_EPOCHS 3
#define MAX_HOOKS 4

#define CHUNKS_PER_ALLOC 1000
#define BLKS_PER_CHUNK 100
#define ALLOC_CHUNKS_PER_LIST 300

#define ADD_TO(_v,_x)                                                   \
do {                                                                    \
    unsigned long __val = (_v);                                         \
    while (!CAS(&(_v),__val,__val+(_x)))                                \
        __val = (_v);                                                   \
} while ( 0 )

#define MINIMAL_GC
/*#define PROFILE_GC*/

typedef struct gc_chunk gc_chunk;
struct gc_chunk {
        struct gc_chunk *next;       /* chunk chaining */
        unsigned int i;              /* next entry in blk to use */
        void *blk[BLKS_PER_CHUNK];
};

static struct gc_global {

        CACHE_PAD(0);

        VOLATILE int current;           /* the current epoch */

        CACHE_PAD(1);

        VOLATILE unsigned long inreclaim;/* excl access to gc_reclaim() */

        CACHE_PAD(2);

        int page_size;                  /* memory page size in bytes */
        unsigned long node_sizes;
        int blk_sizes[MAX_SIZES];
        unsigned long hooks;
        gc_hookfn hook_fns[MAX_HOOKS];
        gc_chunk * VOLATILE free_chunks; /* free, empty chunks */
        gc_chunk * VOLATILE alloc[MAX_SIZES];
        VOLATILE unsigned long alloc_size[MAX_SIZES];

#ifdef PROFILE_GC
        VOLATILE unsigned long total_size;
        VOLATILE unsigned long allocations;
        VOLATILE unsigned long alloc_chunk_num;
        VOLATILE unsigned long num_reclaims;
        VOLATILE unsigned long num_frees;
#endif
} gc_global;

/* Per-thread state */
struct gc_st {
        unsigned int epoch;     /* epoch seen by this thread */

        /* number of calls to gc_entry since last gc_reclaim() attempt */
        unsigned int entries_since_reclaim;

        /* used with gc_async_barrier() */
        void *async_page;
        int async_page_state;

        /* garbage lists */
        gc_chunk *garbage[NUM_EPOCHS][MAX_SIZES];
        gc_chunk *garbage_tail[NUM_EPOCHS][MAX_SIZES];
        gc_chunk *chunk_cache;

        /* local allocation lists */
        gc_chunk *alloc[MAX_SIZES];
        unsigned int alloc_chunks[MAX_SIZES];

        /* hook pointer lists */
        gc_chunk *hook[NUM_EPOCHS][MAX_HOOKS];
};

/* - Private function forward declarations - */

static gc_chunk* gc_alloc_more_chunks(void);
static void gc_add_chunks_to_list(gc_chunk *ch, gc_chunk *head);
static gc_chunk* gc_get_empty_chunks(int n);
static struct gc_chunk* gc_get_filled_chunks(int n, int sz);
static struct gc_chunk* gc_get_alloc_chunk(gc_st *gc, int i);
#ifndef MINIMAL_GC
static void gc_reclaim(void);
#endif
static struct gc_chunk* gc_chunk_from_cache(gc_st *gc);

/* - Private function defintions - */

/**
 * alloc_more_chunks - alloc more empty chunks from the heap
 *
 * Returns a chain of chunks.
 */
static struct gc_chunk* gc_alloc_more_chunks(void)
{
        int i;
        gc_chunk *h, *p;

        h = ALIGNED_ALLOC(CHUNKS_PER_ALLOC * sizeof(gc_chunk));
        if (!h) {
                perror("Couldn't malloc chunks\n");
                exit(1);
        }
        p = h;

        for (i = 1; i < CHUNKS_PER_ALLOC; i++) {
                p->next = p + 1;
                ++p;
        }
        p->next = h;    /* close the loop */

        return (h);
}

/**
 * gc_add_chunks_to_list - put a chain of chunks onto a list
 * @ch: the chain of chunks to add
 * @head: the head of the list to add to
 */
static void gc_add_chunks_to_list(gc_chunk *ch, gc_chunk *head)
{
        gc_chunk *h_next, *ch_next;

        ch_next = ch->next;
        do {
                ch->next = h_next = head->next;
        } while (!CAS(&head->next, h_next, ch_next));
}

/**
 * gc_get_empty_chunks - allocate a chain of @n empty chunks
 * @n: the number of empty chunks to allocate
 *
 * Returns a pointer to the head of the chain.
 * Note: Pointers may be garbage.
 */
static gc_chunk* gc_get_empty_chunks(int n)
{
        int i;
        gc_chunk *new_rh, *rh, *rt, *head;

retry:

        head = gc_global.free_chunks;
        do {
                new_rh = head->next;
                rh = new_rh;
                rt = head;
                for (i = 0; i < n; i++) {
                        if ((rt = rt->next) == head) {
                                gc_add_chunks_to_list(
                                        gc_alloc_more_chunks(),head);
                                goto retry;
                        }
                }
        } while (!CAS(&head->next, rh, rt->next));

        rt->next = rh; /* close the loop */
        return rh;
}

/**
 * gc_get_filled_chunks - get @n chunks pointing at size @sz blks
 * @n: the number of filled chunks to get
 * @sz: the size of blocks pointed to by each chunk
 *
 * Returns a pointer to the head of the chunk chain.
 */
static gc_chunk* gc_get_filled_chunks(int n, int sz)
{
        gc_chunk *h, *p;
        char *node;
        int i;

#ifdef PROFILE_GC
        ADD_TO(gc_global.total_size, (unsigned long) (n * BLKS_PER_CHUNK * sz));
        ADD_TO(gc_global.allocations, 1);
#endif

        node = ALIGNED_ALLOC(n * BLKS_PER_CHUNK * sz);
        if (!node) {
                perror("malloc failed: gc_get_filled_chunks\n");
                exit(1);
        }

        h = gc_get_empty_chunks(n);
        p = h;
        do {
                p->i = BLKS_PER_CHUNK;
                for (i = 0; i < BLKS_PER_CHUNK; i++) {
                        p->blk[i] = node;
                        node += sz;
                }
        } while ((p = p->next) != h);

        return h;
}

/**
 * gc_get_alloc_chunk - grab a chunk from the main chain
 * @gc: the per-thread state for use by the calling thread
 * @i: the level of the chunk to grab
 */
static gc_chunk* gc_get_alloc_chunk(gc_st *gc, int i)
{
        gc_chunk *alloc, *p, *nh;
        unsigned int sz;

        alloc = gc_global.alloc[i];
        do {
                p = alloc->next;
                while (p == alloc) {

                        #ifdef PROFILE_GC
                        ADD_TO(gc_global.alloc_chunk_num, 1);
                        #endif

                        sz = gc_global.alloc_size[i];
                        nh = gc_get_filled_chunks(sz,
                                        gc_global.blk_sizes[i]);
                        ADD_TO(gc_global.alloc_size[i], sz >> 3);
                        /* gc_async_barrier(gc); */
                        gc_add_chunks_to_list(nh, alloc);
                        p = alloc->next;
                }
        } while (!CAS(&alloc->next, p, p->next));

        p->next = p;
        assert(BLKS_PER_CHUNK == p->i);

        return p;
}

#ifndef MINIMAL_GC
/**
 * gc_reclaim - attempt to reclaim memory chunks from previous epochs
 *
 * Notes: Scan the list of perthread structs looking for the lowest
 * max epoch number seen. If the lowest is the current epoch, then
 * the "nearly-free" lists from the previous epoch are reclaimed, and
 * the epoch is incremented.
 */
static void gc_reclaim(void)
{
        ptst_t  *ptst, *first_ptst, *our_ptst = NULL;
        gc_st   *gc = NULL;
        int     two_ago, three_ago, i, j;
        gc_chunk *ch, *t;
        unsigned long   curr_epoch;

        /* barrier to entering the reclaim critical section */
        if (gc_global.inreclaim || !CAS(&gc_global.inreclaim, 0, 1))
                return;

        /*
         * grab first ptst structure *before* barrier -- prevents
         * bugs on weak-ordered archs
         */
        first_ptst = ptst_first();

        BARRIER();

        curr_epoch = gc_global.current;

        /* Have all threads seen the current epoch in mutator code? */
        for (ptst = first_ptst; NULL != ptst; ptst = sl_ptst_next(ptst)) {
                if ((ptst->count > 1) && (ptst->gc->epoch != curr_epoch))
                        goto out;
        }

        /*
         * Three-epoch-old garbage lists move to allocation lists.
         * Two-epoch-old garbage lists are cleaned out.
         * XXX
         */
        two_ago   = (curr_epoch + 2) % NUM_EPOCHS;
        three_ago = (curr_epoch + 1) % NUM_EPOCHS;

        if ( 0 != gc_global.hooks)
                our_ptst = (ptst_t *) pthread_getspecific(ptst_key);

        for (ptst = first_ptst; NULL != ptst; ptst = ptst_next(ptst)) {
                gc = ptst->gc;
                for (i = 0; i < gc_global.node_sizes; i++) {
                        /*
                         * leave one chunk behind as it probably
                         * isn't full yet
                         */
                        t = gc->garbage[three_ago][i];
                        if ((NULL == t) || ((ch = t->next) == t))
                                continue;
                        gc->garbage_tail[three_ago][i]->next = ch;
                        gc->garbage_tail[three_ago][i] = t;
                        t->next = t;
                        gc_add_chunks_to_list(ch, gc_global.alloc[i]);
                }

                /* XXX do we need this ? */

                for (i = 0; i < gc_global.hooks; i++) {
                        gc_hookfn fn = gc_global.hook_fns[i];
                        ch = gc->hook[three_ago][i];
                        if (NULL == ch)
                                continue;
                        gc->hook[three_ago][i] = NULL;

                        t = ch;
                        do {
                                for (j = 0; j < t->i; j++)
                                        fn(our_ptst, t->blk[j]);
                        } while ((t = t->next) != ch);

                        gc_add_chunks_to_list(ch, gc_global.free_chunks);
                }
        }

        #ifdef PROFILE_GC
        ADD_TO(gc_global.num_reclaims, 1);
        #endif

        /* update the current epoch */
        BARRIER();
        gc_global.current = (curr_epoch + 1) % NUM_EPOCHS;

out:
        gc_global.inreclaim = 0;
}
#endif

/**
 * gc_chunk_from_cache - ...
 * @gc: ....
 *
 * Returns a reference to the chunk retrieved from the cache.
 */
static gc_chunk* gc_chunk_from_cache(gc_st *gc)
{
        gc_chunk *ch = gc->chunk_cache;
        gc_chunk *p  = ch->next;

        if (ch == p) {
                gc->chunk_cache = gc_get_empty_chunks(100);
        } else {
                ch->next = p->next;
                p->next = p;
        }
        p->i = 0;

        return p;
}

/* - Public garbage collection routines - */

/**
 * gc_alloc - ...
 * @ptst: the thread-local structure
 * @alloc_id: ....
 *
 * Returns a reference to the alloc'd chunk.
 */
void* gc_alloc(ptst_t *ptst, int alloc_id)
{
        gc_st *gc = ptst->gc;
        gc_chunk *ch;

        ch = gc->alloc[alloc_id];
        if (0 == ch->i) {
                if (100 == gc->alloc_chunks[alloc_id]++) {
                        gc->alloc_chunks[alloc_id] = 0;
                        gc_add_chunks_to_list(ch, gc_global.free_chunks);
                        ch = gc_get_alloc_chunk(gc, alloc_id);
                        gc->alloc[alloc_id] = ch;
                } else {
                        gc_chunk *och = ch;
                        ch = gc_get_alloc_chunk(gc, alloc_id);
                        ch->next = och->next;
                        och->next = ch;
                        gc->alloc[alloc_id] = ch;
                }
        }

        return ch->blk[--ch->i];
}

/**
 * gc_free - free some memory
 * @ptst: the per-thread state
 * @p: pointer to the memory to free
 * @alloc_id: the level of the memory being free'd
 */
void gc_free(ptst_t *ptst, void *p, int alloc_id)
{
#ifndef MINIMAL_GC

        gc_st *gc = ptst->gc;
        gc_chunk *prev, *new;
        gc_chunk *ch = gc->garbage[gc->epoch][alloc_id];

        if (NULL == ch) {
                ch = gc_chunk_from_cache(gc);
                gc->garbage[gc->epoch][alloc_id] = ch;
                gc->garbage_tail[gc->epoch][alloc_id] = ch;
        } else if (BLKS_PER_CHUNK == ch->i) {
                prev = gc->garbage_tail[gc->epoch][alloc_id];
                new = gc_chunk_from_cache(gc);
                gc->garbage[gc->epoch][alloc_id] = new;
                new->next = ch;
                prev->next = new;
                ch = new;
        }

        ch->blk[ch->i++] = p;

        #ifdef PROFILE_GC
        ADD_TO(gc_global.num_frees, 1);
        #endif
#endif
}

/**
 * gc_add_ptr_to_hook_list - ...
 * @ptst: per-thread state
 * @p: the ptr to add to the hook list
 * @hook_id: the hook id we are using
 */
void gc_add_ptr_to_hook_list(ptst_t *ptst, void *p, int hook_id)
{
        gc_st *gc = ptst->gc;
        gc_chunk *och;
        gc_chunk *ch = gc->hook[gc->epoch][hook_id];

        if (NULL == ch) {
                ch = gc_chunk_from_cache(gc);
                gc->hook[gc->epoch][hook_id] = ch;
        } else {
                ch = ch->next;
                if (BLKS_PER_CHUNK == ch->i) {
                        och = gc->hook[gc->epoch][hook_id];
                        ch = gc_chunk_from_cache(gc);
                        ch->next = och->next;
                        och->next = ch;
                }
        }

        ch->blk[ch->i++] = p;
}

/**
 * gc_unsafe_free - ...
 * @ptst: per-thread state
 * @p: pointer to the memory we are freeing
 * @alloc_id: the level of memory we are freeing
 */
void gc_unsafe_free(ptst_t *ptst, void *p, int alloc_id)
{
        gc_st *gc = ptst->gc;
        gc_chunk *ch;

        ch = gc->alloc[alloc_id];
        if (ch->i < BLKS_PER_CHUNK)
                ch->blk[ch->i++] = p;
        else
                gc_free(ptst, p, alloc_id);
}

/**
 * gc_enter - enter a critical setion
 * @ptst: per-thread state
 */
void gc_enter(ptst_t *ptst)
{
#ifdef MINIMAL_GC
        ptst->count++;
        BARRIER();
#else
        gc_st *gc = ptst->gc;
        int new_epoch, cnt;

retry:

        cnt = ptst->count++;
        BARRIER();
        if (1 == cnt) {
                new_epoch = gc_global.current;
                if (gc->epoch != new_epoch) {
                        gc->epoch = new_epoch;
                        gc->entries_since_reclaim = 0;
                } else if (gc->entries_since_reclaim++ == 100) {
                        --ptst->count;
                        gc->entries_since_reclaim = 0;
                        gc_reclaim();
                        goto retry;
                }
        }
#endif
}

/**
 * gc_exit - exit a critical section
 * @ptst: per-thread state
 */
void gc_exit(ptst_t *ptst)
{
        BARRIER();
        ptst->count--;
}

/**
 * gc_init - initialise a garbage collection structure and return it
 *
 * Returns the initialised garbage collection structure.
 */
gc_st* gc_init(void)
{
        gc_st *gc;
        int i;

        gc = ALIGNED_ALLOC(sizeof(gc_st));
        if (NULL == gc) {
                perror("malloc failed: gc_init\n");
                exit(1);
        }
        memset(gc, 0, sizeof(*gc));

        gc->chunk_cache = gc_get_empty_chunks(100);

        /* get ourselves a set of allocation chunks */
        for (i = 0; i < gc_global.node_sizes; i++)
                gc->alloc[i] = gc_get_alloc_chunk(gc, i);
        for ( ; i < MAX_SIZES; i++)
                gc->alloc[i] = gc_chunk_from_cache(gc);

        return gc;
}

/**
 * gc_add_allocator - add a new gc allocator
 * @alloc_size: the size blocks to use for this allocator
 *
 * Returns the old number of node sizes.
 */
int gc_add_allocator(int alloc_size)
{
        int i = gc_global.node_sizes;

        while (!CAS(&(gc_global.node_sizes), i, i+1))
                i = gc_global.node_sizes;

        gc_global.blk_sizes[i] = alloc_size;
        gc_global.alloc_size[i] = ALLOC_CHUNKS_PER_LIST;
        gc_global.alloc[i] = gc_get_filled_chunks(ALLOC_CHUNKS_PER_LIST,
                                                  alloc_size);

        #ifdef PROFILE_GC
        printf("Added a new allocator of size %d bytes ", alloc_size);
        printf("with alloc size %lu bytes\n", gc_global.alloc_size[i]);
        #endif

        return i;
}

/**
 * gc_remove_allocator -
 * @alloc_id: ...
 */
void gc_remove_allocator(int alloc_id)
{
        /* noop */
}

/**
 * gc_add_hook - ...
 * @fn: the function to add
 *
 * Returns ...
 */
int gc_add_hook(gc_hookfn fn)
{
        int i = gc_global.hooks;

        while (!CAS(&gc_global.hooks, i, i+1))
                i = gc_global.hooks;

        gc_global.hook_fns[i] = fn;

        return i;
}

/**
 * gc_remove_hook - ...
 * @hook_id: ...
 */
void gc_remove_hook(int hook_id)
{
        /* noop */
}

/**
 * gc_subsystem_destroy - ...
 */
void gc_subsystem_destroy(void)
{
#ifdef PROFILE_GC
        printf("Total heap: %lu bytes (%.2fMB) in %lu allocations\n",
                gc_global.total_size,
                (double) gc_global.total_size / 1000000,
                gc_global.allocations);
        printf("Node alloc size = %lu\n", gc_global.alloc_size[0]);
        printf("Inode alloc size = %lu\n", gc_global.alloc_size[1]);
        printf("alloc chunk num = %lu\n", gc_global.alloc_chunk_num);
        printf("Num reclaims = %lu\n", gc_global.num_reclaims);
        printf("Num frees = %lu\n", gc_global.num_frees);
#endif
}

/**
 * gc_subsystem_init - initialise the garbage collection subsystem
 *
 * Note: only happens once at the start of the program.
 */
void gc_subsystem_init(void)
{
        memset(&gc_global, 0, sizeof(gc_global));

        gc_global.page_size = (unsigned int) sysconf(_SC_PAGESIZE);
        gc_global.free_chunks = gc_alloc_more_chunks();

        gc_global.hooks = 0;
        gc_global.node_sizes = 0;
}
