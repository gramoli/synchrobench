/******************************************************************************
 * skip_lock.c (Variable-granularity Mutexes)
 * 
 * Mutex only taken for write operations (reads are unprotected). Write 
 * mutexes come in three flavours, selected by a compile-time flag.
 * 
 * If FAT_MTX is defined:
 *  A skip list is protected by one mutex for the entire list. Note that this
 *  differs from skip_bm.c, which  takes the mutex for read operations as well.
 * 
 * If TINY_MTX is defined:
 *  Mutex per forward pointer in each node.
 * 
 * If neither flag is defined:
 *  Mutex per node.
 * 
 * Copyright (c) 2001-2003, K A Fraser
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

#define __SET_IMPLEMENTATION__

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "portable_defns.h"
#include "ptst.h"
#include "set.h"


/*
 * SKIP LIST
 */

typedef struct node_st node_t;
typedef struct set_st set_t;
typedef VOLATILE node_t *sh_node_pt;

typedef struct ptr_st ptr_t;
struct ptr_st
{
#ifdef TINY_MTX /* mutex per forward pointer */
    mcs_lock_t m;
#endif
    sh_node_pt      p;
};

struct node_st
{
    int             level;
    setkey_t       k;
    setval_t       v;
#ifndef FAT_MTX
    mcs_lock_t m;
#endif
    ptr_t           next[1];
};

struct set_st
{
#ifdef FAT_MTX
    mcs_lock_t m;
#endif
    node_t          head;
};

static int gc_id[NUM_LEVELS];

/*
 * LOCKING
 */

#ifdef FAT_MTX

#define LIST_LOCK(_l,_qn)            ((void)mcs_lock((void*)&(_l)->m, (_qn)))
#define LIST_UNLOCK(_l,_qn)          ((void)mcs_unlock((void*)&(_l)->m, (_qn)))
#define NODE_LOCK(_x,_qn)            ((void)0)
#define NODE_UNLOCK(_x,_qn)          ((void)0)
#define PTR_UPDATE_LOCK(_x,_i,_qn)   ((void)0)
#define PTR_UPDATE_UNLOCK(_x,_i,_qn) ((void)0)
#define PTR_DELETE_LOCK(_x,_i,_qn)   ((void)0)
#define PTR_DELETE_UNLOCK(_x,_i,_qn) ((void)0)

#else

#define LIST_LOCK(_l,_qn)         ((void)0)
#define LIST_UNLOCK(_l,_qn)       ((void)0)

/* We take the main node lock to get exclusive rights on insert/delete ops. */
#define NODE_LOCK(_x,_qn)         ((void)mcs_lock((void*)&(_x)->m, (_qn)))
#define NODE_UNLOCK(_x,_qn)       ((void)mcs_unlock((void*)&(_x)->m, (_qn)))

#ifdef TINY_MTX

/*
 * Predecessor's pointer is locked before swinging (on delete), or
 * replumbing (on insert).
 */
#define PTR_UPDATE_LOCK(_x, _i, _qn)   \
    ((void)mcs_lock((void*)&(_x)->next[(_i)].m, (_qn)))
#define PTR_UPDATE_UNLOCK(_x, _i, _qn) \
    ((void)mcs_unlock((void*)&(_x)->next[(_i)].m, (_qn)))
/*
 * When deleting a node, we take the lock on each of its pointers in turn,
 * to prevent someone from inserting a new node directly after, or deleting
 * immediate successor.
 */
#define PTR_DELETE_LOCK(_x, _i, _qn)   PTR_UPDATE_LOCK(_x,_i,(_qn))
#define PTR_DELETE_UNLOCK(_x, _i, _qn) PTR_UPDATE_UNLOCK(_x,_i,(_qn))

#else /* LITTLE_MTX */

/*
 * Predecessor must certainly be locked for insert/delete ops. So we take
 * the only lock we can.
 */
#define PTR_UPDATE_LOCK(_x, _i, _qn)   NODE_LOCK(_x,(_qn))
#define PTR_UPDATE_UNLOCK(_x, _i, _qn) NODE_UNLOCK(_x,(_qn))
/*
 * We can't lock individual pointers. There's no need anyway, since we have
 * the node's lock already (to allow us exclusive delete rights).
 */
#define PTR_DELETE_LOCK(_x, _i, _qn)   ((void)0)
#define PTR_DELETE_UNLOCK(_x, _i, _qn) ((void)0)

#endif

#endif


/*
 * PRIVATE FUNCTIONS
 */

/*
 * Random level generator. Drop-off rate is 0.5 per level.
 * Returns value 1 <= level <= NUM_LEVELS.
 */
static int get_level(ptst_t *ptst)
{
    unsigned long r = rand_next(ptst);
    int l = 1;
    r = (r >> 4) & ((1 << (NUM_LEVELS-1)) - 1);
    while ( (r & 1) ) { l++; r >>= 1; }
    return(l);
}


/*
 * Allocate a new node, and initialise its @level field.
 * NB. Initialisation will eventually be pushed into garbage collector,
 * because of dependent read reordering.
 */
static node_t *alloc_node(ptst_t *ptst)
{
    int l;
    node_t *n;
    l = get_level(ptst);
    n = gc_alloc(ptst, gc_id[l - 1]);
    n->level = l;
#ifndef FAT_MTX
    mcs_init(&n->m);
#endif
#ifdef TINY_MTX
    for ( l = 0; l < n->level; l++ )
    {
        mcs_init(&n->next[l].m);
    }
#endif
    return(n);
}


/* Free a node to the garbage collector. */
static void free_node(ptst_t *ptst, sh_node_pt n)
{
    gc_free(ptst, (void *)n, gc_id[n->level - 1]);
}


/*
 * Find and lock predecessor at level @i of node with key @k. This
 * predecessor must have key >= @x->k.
 */
#ifndef FAT_MTX
static sh_node_pt get_lock(sh_node_pt x, setkey_t k, int i, qnode_t *qn)
{
    sh_node_pt y;
    setkey_t  y_k;

    for ( ; ; )
    {
        READ_FIELD(y,   x->next[i].p);
        READ_FIELD(y_k, y->k);
        if ( y_k >= k ) break;
    retry:
        x = y;
    }

    PTR_UPDATE_LOCK(x, i, qn); /* MB => no need for READ_FIELD on x or y. */
    y = x->next[i].p;
    if ( y->k < k )
    {
        PTR_UPDATE_UNLOCK(x, i, qn);
        goto retry;
    }

    return(x);
}
#else
#define get_lock(_x,_k,_i,_qn) (_x)
#endif


/*
 * Search for first non-deleted node, N, with key >= @k at each level in @l.
 * RETURN VALUES:
 *  Array @pa: @pa[i] is non-deleted predecessor of N at level i
 *  MAIN RETURN VALUE: N at level 0.
 */
static sh_node_pt search_predecessors(set_t *l, setkey_t k, sh_node_pt *pa)
{
    sh_node_pt x, y;
    setkey_t  y_k;
    int        i;

    x = &l->head;
    for ( i = NUM_LEVELS - 1; i >= 0; i-- )
    {
        for ( ; ; )
        {
            READ_FIELD(y,   x->next[i].p);
            READ_FIELD(y_k, y->k);
            if ( y_k >= k ) break;
            x = y; /* remember largest predecessor so far */
        }

        if ( pa ) pa[i] = x;
    }

    return(y);
}


/*
 * PUBLIC FUNCTIONS
 */

set_t *set_alloc(void)
{
    set_t *l;
    node_t *n;
    int i;

    n = malloc(sizeof(*n) + (NUM_LEVELS-1)*sizeof(ptr_t));
    memset(n, 0, sizeof(*n) + (NUM_LEVELS-1)*sizeof(ptr_t));
    n->k = SENTINEL_KEYMAX;

    l = malloc(sizeof(*l) + (NUM_LEVELS-1)*sizeof(ptr_t));
    l->head.k = SENTINEL_KEYMIN;
    l->head.level = NUM_LEVELS;
#ifdef FAT_MTX
    mcs_init(&l->m);
#else
    mcs_init(&l->head.m);
#endif
    for ( i = 0; i < NUM_LEVELS; i++ )
    {
        l->head.next[i].p = n;
#ifdef TINY_MTX
        mcs_init(&l->head.next[i].m);
#endif
    }

    return(l);
}


setval_t set_update(set_t *l, setkey_t k, setval_t v, int overwrite)
{
    setval_t  ov = NULL;
    ptst_t    *ptst;
    sh_node_pt update[NUM_LEVELS];
    sh_node_pt x, y;
    int        i;
    qnode_t    l_qn, x_qn, y_qn;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();
    LIST_LOCK(l, &l_qn);

    (void)search_predecessors(l, k, update);

    x = get_lock(update[0], k, 0, &x_qn);
    y = x->next[0].p;
    if ( y->k == k )
    {
        ov = y->v;
        if ( overwrite ) y->v = v;
        PTR_UPDATE_UNLOCK(x, 0, &x_qn);
        goto out;
    }

    /* Not in the list, so do the insertion. */
    y    = alloc_node(ptst);
    y->k = k;
    y->v = v;
    NODE_LOCK(y, &y_qn);

    for ( i = 0; i < y->level; i++ )
    {
        if ( i != 0 ) x = get_lock(update[i], k, i, &x_qn);
        y->next[i].p = x->next[i].p;
        WMB();
        x->next[i].p = y;
        PTR_UPDATE_UNLOCK(x, i, &x_qn);
    }

    NODE_UNLOCK(y, &y_qn);

 out:
    LIST_UNLOCK(l, &l_qn);
    critical_exit(ptst);
    return(ov);
}


setval_t set_remove(set_t *l, setkey_t k)
{
    setval_t  v = NULL;
    ptst_t    *ptst;
    sh_node_pt update[NUM_LEVELS];
    sh_node_pt x, y;
    int        i;
    qnode_t    l_qn, x_qn, y_qn, yd_qn;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();
    LIST_LOCK(l, &l_qn);

    y = search_predecessors(l, k, update);

#ifdef FAT_MTX
    if ( y->k != k ) goto out;
#else
    y = update[0];
    for ( ; ; ) 
    {
        setkey_t y_k;
        y = y->next[0].p; /* no need for READ_FIELD() */
        READ_FIELD(y_k, y->k);
        if ( y_k > k ) goto out;
        NODE_LOCK(y, &y_qn);
        if ( (y_k == k) && (y_k <= y->next[0].p->k) ) break; 
        NODE_UNLOCK(y, &y_qn);
    }
#endif

    /* @y is the correct node, and we have it locked, so now delete it. */
    for ( i = y->level - 1; i >= 0; i-- )
    {
        x = get_lock(update[i], k, i, &x_qn);
        PTR_DELETE_LOCK(y, i, &yd_qn);
        x->next[i].p = y->next[i].p;
        WMB();
        y->next[i].p = x;
        PTR_DELETE_UNLOCK(y, i, &yd_qn);
        PTR_UPDATE_UNLOCK(x, i, &x_qn);
    }

    v = y->v;
    free_node(ptst, y);
    NODE_UNLOCK(y, &y_qn);

 out:
    LIST_UNLOCK(l, &l_qn);
    critical_exit(ptst);
    return(v);
}


setval_t set_lookup(set_t *l, setkey_t k)
{
    setval_t  v = NULL;
    ptst_t    *ptst;
    sh_node_pt x;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    x = search_predecessors(l, k, NULL);
    if ( x->k == k ) READ_FIELD(v, x->v);

    critical_exit(ptst);
    return(v);
}


void _init_set_subsystem(void)
{
    int i;

    for ( i = 0; i < NUM_LEVELS; i++ )
    {
        gc_id[i] = gc_add_allocator(sizeof(node_t) + i*sizeof(ptr_t));
    }
}
