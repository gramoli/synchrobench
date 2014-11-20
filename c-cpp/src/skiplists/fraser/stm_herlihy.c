/******************************************************************************
 * stm_herlihy.c
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

#include "portable_defns.h"
#include "ptst.h"
#include "gc.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#ifdef SPARC
#include <time.h>
#include <errno.h>
#endif

#define POLITE

typedef struct stm_loc_st stm_loc;
typedef struct stm_blk_st stm_blk;
typedef struct stm_tx_entry_st stm_tx_entry;
typedef struct stm_tx_st stm_tx;
typedef struct stm_st stm;

struct stm_loc_st {
    unsigned long status; /* TXS_FAILED, TXS_SUCCESSFUL, descriptor. */
    void   *old;
    void   *new;
};

struct stm_blk_st {
    stm_loc *loc;
};

struct stm_tx_entry_st {
    stm_blk *b;
    stm_loc *l;
    void    *data;
    stm_tx_entry *next;
};

struct stm_tx_st {
    unsigned int status;
    int rc;
    stm_tx *next_free;
    stm_tx_entry *reads;
    stm_tx_entry *writes;
    stm_tx_entry *alloc_ptr, *check;
    void *dummy;
    int gc_data_id, blk_size; /* copied from 'stm' structure */
    sigjmp_buf *penv;
};

struct stm_st {
    int gc_data_id;
    int blk_size;
};

/* Private per-thread state. The array is indexed off ptst->id. */
typedef struct {
    void *arena, *arena_lim;
    stm_tx *next_descriptor;
    stm_tx *cur_tx;
#ifdef SPARC
    unsigned int random_counter;
#endif
    CACHE_PAD(0);
} priv_t;

static priv_t priv_ptst[MAX_THREADS];
static int gc_blk_id;  /* Allocation id for block descriptors. */
static int gc_loc_id;  /* Allocation id for locators. */
static int do_padding; /* Should all allocations be padded to a cache line? */

#ifdef POLITE
#define MAX_RETRIES      8
#ifdef SPARC
#define MIN_LOG_BACKOFF  4
#define MAX_LOG_BACKOFF 31
#define RANDOM_BITS      8
#define RANDOM_SIZE      (1 << RANDOM_BITS)
#define RANDOM_MASK      (RANDOM_SIZE - 1)
static unsigned int rand_arr[RANDOM_SIZE];
#endif
#endif

static stm_blk *dummy_obj; /* Dummy object (used by red-black trees). */
static void *dummy_data;

#define ALLOCATOR_SIZE(_s) (do_padding ? CACHE_LINE_SIZE : (_s))

#define ARENA_SIZE      40960
#define DESCRIPTOR_SIZE  4096

#define TXS_IN_PROGRESS 0U
#define TXS_FAILED      1U
#define TXS_SUCCESSFUL  2U

#define is_descriptor(_p) (((unsigned long)(_p) & 3) == 0)
#define mk_descriptor(_p) ((stm_tx *)(_p))

/* Is transaction read-only? */
#define read_only(_t)         ((_t)->writes == NULL)

/* Is transaction definitely doomed to fail? */
#define is_stale(_t, _e) \
    (((_t)->status != TXS_IN_PROGRESS) || ((_e)->b->loc != (_e)->l))

bool_t commit_stm_tx(ptst_t *ptst, stm_tx *t);

static void new_arena (priv_t *priv, int size)
{
    priv->arena = malloc(size);
    if ( priv->arena == NULL ) abort();
    priv->arena_lim = (((char *) priv->arena) + size);
}

static void release_descriptor(ptst_t *ptst, stm_tx *t)
{
    stm_tx_entry *ent;
    priv_t *priv = &priv_ptst[ptst->id];
    void *data;

    t->next_free = priv->next_descriptor;
    priv->next_descriptor = t;
}

static int rc_delta_descriptor(stm_tx *t, int delta)
{
    int rc, new_rc = t->rc;

    do { rc = new_rc; }
    while ( (new_rc = CASIO (&t->rc, rc, rc + delta)) != rc );

    return rc;
}

static void rc_up_descriptor(stm_tx *t)
{
    rc_delta_descriptor(t, 2);
    MB();
}

static void rc_down_descriptor(ptst_t *ptst, stm_tx *t)
{
    int old_rc, new_rc, cur_rc = t->rc;

    WMB();

    do {
        old_rc = cur_rc;
        new_rc = old_rc - 2;
        if ( new_rc == 0 ) new_rc = 1;
    }
    while ( (cur_rc = CASIO (&t->rc, old_rc, new_rc)) != old_rc );

    if ( old_rc == 2 ) release_descriptor(ptst, t);
}

static stm_tx *new_descriptor(priv_t *priv)
{
    stm_tx *t;
				
    t = priv->next_descriptor;

    if ( t != NULL ) 
    {
        priv->next_descriptor = t->next_free;
        /* 'Unfree' descriptor, if it was previously freed. */
        if ( (t->rc & 1) == 1 ) rc_delta_descriptor(t, 1);
    }
    else
    {
        t = (stm_tx *) priv->arena;
        priv->arena = ((char *) (priv->arena)) + DESCRIPTOR_SIZE;

        if ( priv->arena >= priv->arena_lim )
        {
            new_arena(priv, ARENA_SIZE);
            t = (stm_tx *) priv->arena;
            priv->arena = ((char *) (priv->arena)) + DESCRIPTOR_SIZE;
        }

        t->next_free = NULL;
        t->rc = 2;
    }

    return t;
}


static stm_tx_entry *alloc_stm_tx_entry(stm_tx *t)
{
    stm_tx_entry *ent = t->alloc_ptr++;
    assert(((unsigned long)t->alloc_ptr - (unsigned long)t) <=
           DESCRIPTOR_SIZE);
    return ent;
}


static stm_tx_entry **search_stm_tx_entry(stm_tx_entry **pnext, stm_blk *b)
{
    stm_tx_entry *next = *pnext;

    while ( (next != NULL) && ((unsigned long)next->b < (unsigned long)b) )
    {
        pnext = &next->next;
        next  = *pnext;
    }

    return pnext;
}


static int contention_wait(ptst_t *ptst, int attempts)
{
#ifdef POLITE
    if ( (attempts > 1) && (attempts <= MAX_RETRIES) )
    {
#ifdef SPARC /* Exactly as it was done by the original authors. */
        priv_t *priv = &priv_ptst[ptst->id];
        struct timespec rqtp;
        unsigned int log_backoff, mask;
        log_backoff = attempts - 2 + MIN_LOG_BACKOFF;
        if ( log_backoff > MAX_LOG_BACKOFF )
            log_backoff = MAX_LOG_BACKOFF;
        mask = (1 << log_backoff) - 1;
        rqtp.tv_nsec = rand_arr[priv->random_counter++ & RANDOM_MASK] & mask;
        rqtp.tv_sec  = 0;
        while ( nanosleep(&rqtp, NULL) != 0 ) continue;
#else
        usleep(1);
#endif
    }

    return attempts < MAX_RETRIES;
#else
    return FALSE;
#endif
}


static void *read_loc_data(ptst_t *ptst, stm_loc *l)
{
    void           *data;
    stm_tx         *t;
    unsigned long   st;
    stm_tx_entry  **pent;
    int attempts = 0;

    for ( ; ; )
    {
        switch ( (st = l->status) )
        {
        case TXS_SUCCESSFUL:
            return l->new;
        case TXS_FAILED:
            return l->old;
        default:
            t = mk_descriptor(st);
            rc_up_descriptor(t);
            if ( l->status == st )
            {
                switch ( t->status )
                {
                case TXS_SUCCESSFUL:
                    rc_down_descriptor(ptst, t);
                    l->status = TXS_SUCCESSFUL;
                    return l->new;
                case TXS_FAILED:
                    rc_down_descriptor(ptst, t);
                    l->status = TXS_FAILED;
                    return l->old;
                default:
                    if ( !contention_wait(ptst, ++attempts) )
                    {
                        attempts = 0;
                        CASIO(&t->status, TXS_IN_PROGRESS, TXS_FAILED);
                    }
                }
            }
            rc_down_descriptor(ptst, t);
        }
    }
}


static stm_loc *install_loc(ptst_t *ptst, stm_tx *t, 
                            stm_blk *b, stm_loc *old_loc)
{
    stm_loc *new_loc = gc_alloc(ptst, gc_loc_id);

    new_loc->status = (unsigned long)t;
    new_loc->new    = gc_alloc(ptst, t->gc_data_id);
    new_loc->old    = read_loc_data(ptst, old_loc);
    memcpy(new_loc->new, new_loc->old, t->blk_size);

    if ( CASPO(&b->loc, old_loc, new_loc) != old_loc )
    {
        gc_unsafe_free(ptst, new_loc->new, t->gc_data_id);
        gc_unsafe_free(ptst, new_loc     , gc_loc_id);
        new_loc = NULL;
    }
    else
    {
        gc_free(ptst, old_loc, gc_loc_id);
    }

    return new_loc;
}


stm *new_stm(ptst_t *ptst, int blk_size)
{
    stm *mem = malloc(CACHE_LINE_SIZE);
    mem->blk_size = blk_size;
    mem->gc_data_id = gc_add_allocator(ALLOCATOR_SIZE(blk_size));
    return mem;
}


void free_stm(ptst_t *ptst, stm *mem)
{
    gc_remove_allocator(mem->gc_data_id);
    free(mem);
}


stm_blk *new_stm_blk(ptst_t *ptst, stm *mem)
{
    stm_blk *b = gc_alloc(ptst, gc_blk_id);
    stm_loc *l = gc_alloc(ptst, gc_loc_id);
    b->loc     = l;
    l->status  = TXS_SUCCESSFUL;
    l->old     = NULL;
    l->new     = gc_alloc(ptst, mem->gc_data_id);
    return b;
}


void free_stm_blk(ptst_t *ptst, stm *mem, stm_blk *b)
{
    stm_loc *l;
    void    *data;

    l    = FASPO(&b->loc, NULL);
    data = read_loc_data(ptst, l);

    gc_free(ptst, data, mem->gc_data_id);
    gc_free(ptst, l, gc_loc_id);
    gc_free(ptst, b, gc_blk_id);
}


void *init_stm_blk(ptst_t *ptst, stm *mem, stm_blk *b)
{
    return b->loc->new;
}


int sizeof_stm_blk(ptst_t *ptst, stm *mem, stm_blk *b)
{
    return mem->blk_size;
}


stm_tx *new_stm_tx(ptst_t *ptst, stm *mem, sigjmp_buf *penv)
{
    priv_t *priv = &priv_ptst[ptst->id];
    stm_tx *t;

    if ( priv->cur_tx != NULL ) goto nesting;
    t = new_descriptor(priv);
    t->status = TXS_IN_PROGRESS;
    t->reads = t->writes = NULL;
    t->alloc_ptr = t->check = (stm_tx_entry *)(t + 1);
    t->gc_data_id = mem->gc_data_id;
    t->blk_size = mem->blk_size;
    t->penv = penv;
    t->dummy = NULL;
    priv->cur_tx = t;
    return t;

 nesting:
    fprintf(stderr, "No nesting of transactions is allowed\n");
    return NULL;
}


bool_t commit_stm_tx(ptst_t *ptst, stm_tx *t)
{
    unsigned int desired_st = TXS_SUCCESSFUL, st;
    stm_tx_entry *ent;
    priv_t *priv = &priv_ptst[ptst->id];

    priv->cur_tx = NULL;

    MB();
    
    for ( ent = t->reads; ent != NULL; ent = ent->next )
    {
        if ( ent->b->loc != ent->l )
            desired_st = TXS_FAILED;
    }

    if ( read_only(t) )
    {
        /* A very fast path: we can immediately reuse the descriptor. */
        if ( t->dummy != NULL )
            gc_unsafe_free(ptst, t->dummy, t->gc_data_id);
        t->next_free = priv->next_descriptor;
        priv->next_descriptor = t;
        return desired_st == TXS_SUCCESSFUL;        
    }

    st = CASIO(&t->status, TXS_IN_PROGRESS, desired_st);
    if ( st == TXS_IN_PROGRESS )
        st = desired_st;

    assert((st == TXS_FAILED) || (st == TXS_SUCCESSFUL));

    WMB_NEAR_CAS();

    for ( ent = t->writes; ent != NULL; ent = ent->next )
    {
        ent->l->status = (unsigned long)st;
        gc_free(ptst, 
                (st == TXS_SUCCESSFUL) ? ent->l->old : ent->l->new, 
                t->gc_data_id);
    }
    
    if ( t->dummy != NULL )
        gc_unsafe_free(ptst, t->dummy, t->gc_data_id);
    
    rc_down_descriptor(ptst, t);

    return st == TXS_SUCCESSFUL;
}


bool_t validate_stm_tx(ptst_t *ptst, stm_tx *t)
{
    stm_tx_entry *ent;

    RMB();

    /* A conflict on a pending update will cause us to get failed. */
    if ( t->status == TXS_FAILED )
        goto fail;

    /* Reads must be explicitly checked. */
    for ( ent = t->reads; ent != NULL; ent = ent->next )
    {
        if ( ent->b->loc != ent->l )
            goto fail;
    }

    return TRUE;

 fail:
    t->status = TXS_FAILED;
    return FALSE;
}


void abort_stm_tx(ptst_t *ptst, stm_tx *t)
{
    t->status = TXS_FAILED;
}


void *read_stm_blk(ptst_t *ptst, stm_tx *t, stm_blk *b)
{
    stm_tx_entry **pent, *ent;
    sigjmp_buf *penv;
    void *result;

    if ( b == dummy_obj )
    {
        if ( t->dummy == NULL )
        {
            t->dummy = gc_alloc(ptst, t->gc_data_id);
            memcpy(t->dummy, dummy_data, t->blk_size);
        }
        return t->dummy;
    }

    pent = search_stm_tx_entry(&t->writes, b);
    ent  = *pent;
    if ( (ent != NULL) && (ent->b == b) ) goto found;

    pent = search_stm_tx_entry(&t->reads, b);
    ent  = *pent;
    if ( (ent != NULL) && (ent->b == b) ) goto found;

    ent = alloc_stm_tx_entry(t);
    ent->b    = b;
    if ( (ent->l = b->loc) == NULL )
        goto fail;
    ent->data = read_loc_data(ptst, ent->l);
    ent->next = *pent;
    *pent = ent;

    return ent->data;

 found:
    result = ent->data;
    ent = t->check;
    if ( is_stale(t, ent) ) goto fail;
    if ( ++t->check == t->alloc_ptr ) t->check = (stm_tx_entry *)(t + 1);
    return result;

 fail:
    penv = t->penv;
    abort_stm_tx(ptst, t);
    commit_stm_tx(ptst, t);
    siglongjmp(*penv, 0);
    assert(0);
    return NULL;
}


void *write_stm_blk(ptst_t *ptst, stm_tx *t, stm_blk *b)
{
    stm_tx_entry **r_pent, **w_pent, *ent;
    stm_loc *loc;
    sigjmp_buf *penv;
    void *result;

    if ( b == dummy_obj )
    {
        if ( t->dummy == NULL )
        {
            t->dummy = gc_alloc(ptst, t->gc_data_id);
            memcpy(t->dummy, dummy_data, t->blk_size);
        }
        return t->dummy;
    }

    w_pent = search_stm_tx_entry(&t->writes, b);
    ent = *w_pent;
    if ( (ent != NULL) && (ent->b == b) ) goto found;

    r_pent = search_stm_tx_entry(&t->reads, b);
    ent = *r_pent;
    if ( (ent != NULL) && (ent->b == b) )
    {
        *r_pent = ent->next;
    }
    else
    {
        ent       = alloc_stm_tx_entry(t);
        ent->b    = b;
        if ( (ent->l = b->loc) == NULL )
            goto fail;
    }

    loc = install_loc(ptst, t, b, ent->l);
    if ( loc == NULL ) goto fail;

    ent->l    = loc;
    ent->data = loc->new;
    ent->next = *w_pent;
    *w_pent = ent;

    return ent->data;

 found:
    result = ent->data;
    ent = t->check;
    if ( is_stale(t, ent) ) goto fail;
    if ( ++t->check == t->alloc_ptr ) t->check = (stm_tx_entry *)(t + 1);
    return result;

 fail:
    penv = t->penv;
    abort_stm_tx(ptst, t);
    commit_stm_tx(ptst, t);
    siglongjmp(*penv, 0);
    assert(0);
    return NULL;
}


void remove_from_tx(ptst_t *ptst, stm_tx *t, stm_blk *b)
{
    if ( dummy_obj == NULL )
    {
        dummy_obj  = b;
        dummy_data = read_loc_data(ptst, b->loc); 
    }
}


static void handle_fault(int sig)
{
    ptst_t *ptst;
    stm_tx *t;

    ptst = critical_enter();
    t = priv_ptst[ptst->id].cur_tx;
    if ( (t != NULL) && !validate_stm_tx(ptst, t) )
    {
        sigjmp_buf *penv = t->penv;
        commit_stm_tx(ptst, t);
        critical_exit(ptst);
        siglongjmp(*penv, 0);
    }

 fail:
    fprintf(stderr, "Error: unhandleable SIGSEGV!\n");
    abort();
}


void _init_stm_subsystem(int pad_data)
{
    struct sigaction act;

#ifdef SPARC
    int i;
    struct timespec rqtp;

    rqtp.tv_sec  = 0;
    rqtp.tv_nsec = 1000;

    while ( nanosleep(&rqtp, NULL) != 0 )
    {
        if ( errno != EINTR )
        {
            printf("Urk! Nanosleep not supported!\n");
            exit(1);
        }
    }

    for ( i = 0; i < RANDOM_SIZE; i++ )
        rand_arr[i] = (unsigned int)random();
#endif

    do_padding = pad_data;
    gc_blk_id = gc_add_allocator(ALLOCATOR_SIZE(sizeof(stm_blk)));
    gc_loc_id = gc_add_allocator(ALLOCATOR_SIZE(sizeof(stm_loc)));
    memset(priv_ptst, 0, sizeof(priv_ptst));

    act.sa_handler = handle_fault;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGSEGV, &act, NULL);
}
