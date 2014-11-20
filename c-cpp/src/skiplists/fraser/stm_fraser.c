/******************************************************************************
 * stm_fraser.c
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

typedef struct stm_blk_st stm_blk;
typedef struct stm_tx_entry_st stm_tx_entry;
typedef struct stm_tx_st stm_tx;
typedef struct stm_st stm;

struct stm_blk_st {
    void *data;
};

struct stm_tx_entry_st {
    stm_blk *b;
    void    *old;
    void    *new;
    stm_tx_entry *next;
};

struct stm_tx_st {
    int status;
    int rc;
    stm_tx *next_free;
    stm_tx_entry *reads;
    stm_tx_entry *writes;
    stm_tx_entry *alloc_ptr, *check;
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
    CACHE_PAD(0);
} priv_t;

static priv_t priv_ptst[MAX_THREADS];
static int gc_blk_id;  /* Allocation id for block descriptors. */
static int do_padding; /* Should all allocations be padded to a cache line? */

#define ALLOCATOR_SIZE(_s) (do_padding ? CACHE_LINE_SIZE : (_s))

#define ARENA_SIZE      40960
#define DESCRIPTOR_SIZE  4096

#define TXS_IN_PROGRESS 0
#define TXS_READ_PHASE  1
#define TXS_FAILED      2
#define TXS_SUCCESSFUL  3

#define is_descriptor(_p)     ((unsigned long)(_p) & 1)
#define ptr_to_descriptor(_p) ((stm_tx *)((unsigned long)(_p) & ~1))
#define make_marked_ptr(_p)   ((void *)((unsigned long)(_p) | 1))

/* Is transaction read-only? */
#define read_only(_t)         ((_t)->writes == NULL)

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

    assert(t->status >= TXS_FAILED);

    t->next_free = priv->next_descriptor;
    priv->next_descriptor = t;

    if ( t->status == TXS_SUCCESSFUL )
    {
        for ( ent = t->writes; ent != NULL; ent = ent->next )
        {
            gc_free(ptst, ent->old, t->gc_data_id);
        }
    }
    else
    {
        for ( ent = t->writes; ent != NULL; ent = ent->next )
        {
            gc_unsafe_free(ptst, ent->new, t->gc_data_id);
        }
    }    
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


static void *read_blk_data(ptst_t *ptst, stm_blk *b)
{
    void *data;
    stm_tx *t;
    int status;
    stm_tx_entry **pent;

    for ( ; ; ) 
    {
        data = b->data;
        if ( !is_descriptor(data) ) return data;

        t = ptr_to_descriptor(data);
        rc_up_descriptor(t);
        if ( b->data != data )
        {
            rc_down_descriptor(ptst, t);
            continue;
        }

        /*
         * Commit even when we could just read from descriptor, as it gets
         * the descriptor out of the way in future.
         */
        commit_stm_tx(ptst, t);
    }
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
    stm_blk *b;
    b       = gc_alloc(ptst, gc_blk_id);
    b->data = gc_alloc(ptst, mem->gc_data_id);
    return b;
}


void free_stm_blk(ptst_t *ptst, stm *mem, stm_blk *b)
{
    /*
     * We have to use read_stm_blk(), as some doomed transaction may still
     * install a marked pointer here while in its write phase.
     */
    void *data = read_blk_data(ptst, b);
    assert(!is_descriptor(data));
    gc_free(ptst, data, mem->gc_data_id);
    gc_free(ptst, b,    gc_blk_id);
}


void *init_stm_blk(ptst_t *ptst, stm *mem, stm_blk *b)
{
    return b->data;
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
    priv->cur_tx = t;
    return t;

 nesting:
    fprintf(stderr, "No nesting of transactions is allowed\n");
    return NULL;
}


bool_t commit_stm_tx(ptst_t *ptst, stm_tx *t)
{
    int desired_status, other_status, old_status, new_status, final_status;
    void *marked_tx, *data;
    stm_tx *other;
    stm_tx_entry **other_pent, *ent;
    priv_t *priv = &priv_ptst[ptst->id];

    if ( priv->cur_tx == t ) priv->cur_tx = NULL;

    marked_tx = make_marked_ptr(t);
    desired_status = TXS_FAILED;

    /*
     * PHASE 1: WRITE-CHECKING PHASE.
     */
    if ( (t->status == TXS_IN_PROGRESS) && ((ent = t->writes) != NULL) )
    {
        /* Others should see up-to-date contents of descriptor. */
        WMB();

        do {
            for ( ; ; )
            {
                data = CASPO(&ent->b->data, ent->old, marked_tx);
                if ( (data == ent->old) || (data == marked_tx) ) break;
                
                if ( !is_descriptor(data) ) goto fail;
                
                other = ptr_to_descriptor(data);
                rc_up_descriptor(other);
                if ( ent->b->data != data )
                {
                    rc_down_descriptor(ptst, other);
                    continue;
                }
                
                commit_stm_tx(ptst, other);
            }
        }
        while ( (ent = ent->next) != NULL );
    }

    /* On success we linearise at this point. */
    WEAK_DEP_ORDER_WMB();

    /*
     * PHASE 2: READ-CHECKING PHASE.
     */
    if ( (t->status <= TXS_READ_PHASE) && (t->reads != NULL) )
    {
        if ( !read_only(t) )
        {
            CASIO(&t->status, TXS_IN_PROGRESS, TXS_READ_PHASE);
            MB_NEAR_CAS();
        }
        else MB();
       
        for ( ent = t->reads; ent != NULL; ent = ent->next )
        {
            for ( ; ; ) 
            {
                data = ent->b->data;
                if ( data == ent->old ) break;

                /* Someone else made progress at our expense. */
                if ( !is_descriptor(data) ) goto fail;
                other = ptr_to_descriptor(data);

                /*
                 * Descriptor always belongs to a contending operation.
                 * Before continuing, we must increment the reference count.
                 */
                assert(other != t);
                rc_up_descriptor(other);
                if ( ent->b->data != data )
                {
                    rc_down_descriptor(ptst, other);
                    continue;
                }
                
                /*
                 * What we do now depends on the status of the contending
                 * operation. This is easy for any status other than
                 * TXS_READ_PHASE -- usually we just check against the
                 * appropriate 'old' or 'new' data pointer. Transactions
                 * in their read-checking phase must be aborted, or helped
                 * to completion, depending on relative ordering of the
                 * transaction descriptors.
                 */
                while ( (other_status = other->status) == TXS_READ_PHASE )
                {
                    if ( t < other )
                    {
                        CASIO(&other->status, TXS_READ_PHASE, TXS_FAILED);
                    }
                    else
                    {
                        rc_up_descriptor(other);
                        commit_stm_tx(ptst, other);
                    }
                }

                other_pent = search_stm_tx_entry(&other->writes, ent->b);
                assert(*other_pent != NULL);
                data = (other_status == TXS_SUCCESSFUL) ? 
                    (*other_pent)->new : (*other_pent)->old;
                rc_down_descriptor(ptst, other);
                if ( data != ent->old ) goto fail;

                break;
            }
        }
    }

    desired_status = TXS_SUCCESSFUL;

 fail:
    if ( read_only(t) )
    {
        /* A very fast path: we can immediately reuse the descriptor. */
        t->next_free = priv->next_descriptor;
        priv->next_descriptor = t;
        return desired_status == TXS_SUCCESSFUL;
    }

    /* Loop until we push the status to a "final decision" value. */
    old_status = t->status;
    while ( old_status <= TXS_READ_PHASE )
    {
        new_status = CASIO(&t->status, old_status, desired_status);
        if ( old_status == new_status ) break;
        old_status = new_status;
    }
    WMB_NEAR_CAS();
    
    /*
     * PHASE 3: CLEAN-UP.
     */
    final_status = t->status;
    for ( ent = t->writes; ent != NULL; ent = ent->next )
    {
        /* If CAS fails, someone did it for us already. */
        (void)CASPO(&ent->b->data, marked_tx,
                    (final_status == TXS_SUCCESSFUL) ? ent->new: ent->old);
    }

    rc_down_descriptor(ptst, t);
    return final_status == TXS_SUCCESSFUL;
}


bool_t validate_stm_tx(ptst_t *ptst, stm_tx *t)
{
    stm_tx_entry *ent;

    RMB();

    for ( ent = t->reads; ent != NULL; ent = ent->next )
    {
        if ( read_blk_data(ptst, ent->b) != ent->old ) goto fail;        
    }

    for ( ent = t->writes; ent != NULL; ent = ent->next )
    {
        if ( read_blk_data(ptst, ent->b) != ent->old ) goto fail;
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

    pent = search_stm_tx_entry(&t->writes, b);
    ent  = *pent;
    if ( (ent != NULL) && (ent->b == b) ) goto found;

    pent = search_stm_tx_entry(&t->reads, b);
    ent  = *pent;
    if ( (ent != NULL) && (ent->b == b) ) goto found;

    ent = alloc_stm_tx_entry(t);
    ent->b    = b;
    ent->old  = read_blk_data(ptst, b);
    ent->new  = ent->old;
    ent->next = *pent;
    *pent = ent;

    assert(!is_descriptor(ent->new));
    return ent->new;

 found:
    result = ent->new;
    ent = t->check;
    if ( read_blk_data(ptst, ent->b) != ent->old ) goto fail;
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
    sigjmp_buf *penv;
    void *result;

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
        ent = alloc_stm_tx_entry(t);
        ent->b   = b;
        ent->old = read_blk_data(ptst, b);
    }

    ent->new  = gc_alloc(ptst, t->gc_data_id);
    ent->next = *w_pent;
    *w_pent = ent;
    memcpy(ent->new, ent->old, t->blk_size);

    assert(!is_descriptor(ent->old));
    assert(!is_descriptor(ent->new));
    return ent->new;

 found:
    result = ent->new;
    ent = t->check;
    if ( read_blk_data(ptst, ent->b) != ent->old ) goto fail;
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
    stm_tx_entry **pent, *ent;
    void *data;

    pent = search_stm_tx_entry(&t->writes, b);
    ent  = *pent;
    if ( (ent != NULL) && (ent->b == b) ) 
    { 
        *pent = ent->next;
        data = ent->new;
        assert(!is_descriptor(data));
        gc_free(ptst, data, t->gc_data_id);
        return; 
    }

    pent = search_stm_tx_entry(&t->reads, b);
    ent  = *pent;
    if ( (ent != NULL) && (ent->b == b) )
    { 
        *pent = ent->next; 
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

    do_padding = pad_data;
    gc_blk_id = gc_add_allocator(ALLOCATOR_SIZE(sizeof(stm_blk)));
    memset(priv_ptst, 0, sizeof(priv_ptst));

    act.sa_handler = handle_fault;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGSEGV, &act, NULL);
}
