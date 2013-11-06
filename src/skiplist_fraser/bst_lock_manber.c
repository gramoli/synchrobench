/******************************************************************************
 * bst_lock_manber.c
 * 
 * Lock-based binary search trees (BSTs), based on:
 *  Udi Manber and Richard E. Ladner.
 *  "Concurrency control in a dynamic search structure".
 *  ACM Transactions on Database Systems, Vol. 9, No. 3, September 1984.
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

#define __SET_IMPLEMENTATION__

#include <ucontext.h>
#include <signal.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include "portable_defns.h"
#include "gc.h"
#include "set.h"

#define GARBAGE_FLAG   1
#define REDUNDANT_FLAG 2

#define IS_GARBAGE(_n)   ((int)(_n)->v & GARBAGE_FLAG)
#define MK_GARBAGE(_n)   \
    ((_n)->v = (setval_t)((unsigned long)(_n)->v | GARBAGE_FLAG))

#define IS_REDUNDANT(_n) ((int)(_n)->v & REDUNDANT_FLAG)
#define MK_REDUNDANT(_n) \
    ((_n)->v = (setval_t)((unsigned long)(_n)->v | REDUNDANT_FLAG))

#define GET_VALUE(_n) ((setval_t)((unsigned long)(_n)->v & ~3UL))

#define FOLLOW(_n, _k) (((_n)->k < (_k)) ? (_n)->r : (_n)->l)

typedef struct node_st node_t;
typedef struct set_st set_t;

struct node_st
{
    setkey_t k;
    setval_t v;
    node_t *l, *r, *p;
    int copy;
    mcs_lock_t lock;
};

struct set_st
{
    node_t root;
};

static int gc_id, hook_id;

#define LOCK(_n, _pqn)   mcs_lock(&(_n)->lock, (_pqn))
#define UNLOCK(_n, _pqn) mcs_unlock(&(_n)->lock, (_pqn))


static node_t *weak_search(node_t *n, setkey_t k)
{
    while ( (n != NULL) && (n->k != k) ) n = FOLLOW(n, k);
    return n;
}


static node_t *strong_search(node_t *n, setkey_t k, qnode_t *qn)
{
    node_t *b = n;
    node_t *a = FOLLOW(b, k);

 retry:
    while ( (a != NULL) && (a->k != k) )
    {
        b = a;
        a = FOLLOW(a, k);
    }

    if ( a == NULL )
    {
        LOCK(b, qn);
        if ( IS_GARBAGE(b) )
        {
            UNLOCK(b, qn);
            a = b->p;
            goto retry;
        }
        else if ( (a = FOLLOW(b, k)) != NULL )
        {
            UNLOCK(b, qn);
            goto retry;
        }

        a = b;
    }
    else
    {
        LOCK(a, qn);
        if ( IS_GARBAGE(a) )
        {
            UNLOCK(a, qn);
            a = a->p;
            goto retry;
        }
        else if ( IS_REDUNDANT(a) )
        {
            UNLOCK(a, qn);
            a = a->r;
            goto retry;
        }
    }

    return a;
}


static void redundancy_removal(ptst_t *ptst, void *x)
{
    node_t *d, *e, *r;
    qnode_t d_qn, e_qn;
    setkey_t k;

    if ( x == NULL ) return;

    e = x;
    k = e->k;

    if ( e->copy )
    {
        r = weak_search(e->l, k);
        assert((r == NULL) || !IS_REDUNDANT(r) || (r->r == e));
        assert(r != e);
        redundancy_removal(ptst, r);
    }

    do {
        if ( IS_GARBAGE(e) ) return;
        d = e->p;
        LOCK(d, &d_qn);
        if ( IS_GARBAGE(d) ) UNLOCK(d, &d_qn);
    }
    while ( IS_GARBAGE(d) );
    
    LOCK(e, &e_qn);

    if ( IS_GARBAGE(e) || !IS_REDUNDANT(e) ) goto out_de;

    if ( d->l == e ) 
    {
        d->l = e->l;
    }
    else
    {
        assert(d->r == e);
        d->r = e->l;
    }

    assert(e->r != NULL);
    assert(e->r->k == k);
    assert(e->r->copy);
    assert(!IS_GARBAGE(e->r));
    assert(!e->copy);

    MK_GARBAGE(e);
    
    if ( e->l != NULL ) e->l->p = d;

    e->r->copy = 0;

    gc_free(ptst, e, gc_id);

 out_de:
    UNLOCK(d, &d_qn);
    UNLOCK(e, &e_qn);
}


/* NB. Node X is not locked on entry. */
static void predecessor_substitution(ptst_t *ptst, set_t *s, node_t *x)
{
    node_t *a, *b, *e, *f, **pac;
    qnode_t a_qn, b_qn, e_qn, f_qn;
    setkey_t k;

    b = x;
    k = x->k;

    do {
        if ( (b == NULL) || (b->v != NULL) ) return;
        a = b->p;
        LOCK(a, &a_qn);
        if ( IS_GARBAGE(a) ) UNLOCK(a, &a_qn);
    }
    while ( IS_GARBAGE(a) );

 regain_lock:
    LOCK(b, &b_qn);

    /*
     * We do nothing if:
     *  1. The node is already deleted (and is thus garbage); or
     *  2. The node is redundant (redundancy removal will do it); or
     *  3. The node has been reused.
     * These can all be checked by looking at the value field.
     */
    if ( b->v != NULL ) goto out_ab;

    /*
     * If this node is a copy, then we can do redundancy removal right now. 
     * This is an improvement over Manber and Ladner's work.
     */
    if ( b->copy )
    {
        e = weak_search(b->l, k);
        UNLOCK(b, &b_qn);
        assert((e == NULL) || !IS_REDUNDANT(e) || (e->r == b));
        assert(e != b);
        redundancy_removal(ptst, e);
        goto regain_lock;
    }

    pac = (a->k < k) ? &a->r : &a->l;
    assert(*pac == b);
    assert(b->p == a);

    if ( (b->l == NULL) || (b->r == NULL) )
    {
        if ( b->r == NULL ) *pac = b->l; else *pac = b->r;
        MK_GARBAGE(b);
        if ( *pac != NULL ) (*pac)->p = a;
        gc_free(ptst, b, gc_id);
        goto out_ab;
    }
    else
    {
        e = strong_search(b->l, b->k, &e_qn);
        assert(!IS_REDUNDANT(e) && !IS_GARBAGE(e) && (b != e));
        assert(e->k < b->k);
        f = gc_alloc(ptst, gc_id);
        f->k = e->k;
        f->v = GET_VALUE(e);
        f->copy = 1;
        f->r = b->r;
        f->l = b->l;
        mcs_init(&f->lock);
        LOCK(f, &f_qn);

        e->r = f;
        MK_REDUNDANT(e);
        *pac = f;
        f->p = a;
        f->r->p = f;
        f->l->p = f;

        MK_GARBAGE(b);
        gc_free(ptst, b, gc_id);
        gc_add_ptr_to_hook_list(ptst, e, hook_id);
        UNLOCK(e, &e_qn);
        UNLOCK(f, &f_qn);
    }

 out_ab:
    UNLOCK(a, &a_qn);
    UNLOCK(b, &b_qn);
}


set_t *set_alloc(void)
{
    set_t *s;

    s = malloc(sizeof(*s));
    mcs_init(&s->root.lock);
    s->root.k = SENTINEL_KEYMIN;
    /* Dummy root isn't redundant, nor is it garbage. */
    s->root.v = (setval_t)(~3UL);
    s->root.l = NULL;
    s->root.r = NULL;
    s->root.p = NULL;
    s->root.copy = 0;

    return s;
}


setval_t set_update(set_t *s, setkey_t k, setval_t v, int overwrite)
{
    node_t  *a, *new;
    qnode_t  qn;
    setval_t ov = NULL;
    ptst_t  *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    a = strong_search(&s->root, k, &qn);
    if ( a->k != k )
    {
        new = gc_alloc(ptst, gc_id);
        mcs_init(&new->lock);
        new->k = k;
        new->v = v;
        new->l = NULL;
        new->r = NULL;
        new->p = a;
        new->copy = 0;
        if ( a->k < k ) a->r = new; else a->l = new;
    }
    else
    {
        /* Direct A->V access is okay, as A isn't garbage or redundant. */
        ov = a->v;
        if ( overwrite || (ov == NULL) ) a->v = v;
    }

    UNLOCK(a, &qn);

    critical_exit(ptst);

    return ov;
}


setval_t set_remove(set_t *s, setkey_t k)
{
    node_t *a;
    qnode_t qn;
    setval_t v = NULL;
    ptst_t *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    a = strong_search(&s->root, k, &qn);    
    /* Direct check of A->V is okay, as A isn't garbage or redundant. */
    if ( (a->k == k) && (a->v != NULL) )
    {
        v = a->v;
        a->v = NULL;
        UNLOCK(a, &qn);
        predecessor_substitution(ptst, s, a);
    }
    else
    {
        UNLOCK(a, &qn);
    }

    critical_exit(ptst);

    return v;
}


setval_t set_lookup(set_t *s, setkey_t k)
{
    node_t *n;
    setval_t v = NULL;
    ptst_t *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    n = weak_search(&s->root, k);
    if ( n != NULL ) v = GET_VALUE(n);

    critical_exit(ptst);
    return v;
}


void _init_set_subsystem(void)
{
    gc_id = gc_add_allocator(sizeof(node_t));
    hook_id = gc_add_hook(redundancy_removal);
}
