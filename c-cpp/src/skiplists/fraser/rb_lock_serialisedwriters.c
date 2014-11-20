/******************************************************************************
 * rb_lock_serialisedwriters.c
 * 
 * Lock-based red-black trees, using multi-reader locks.
 * 
 * Updates are serialised on a global mutual-exclusion spinlock.
 * 
 * Updates never need to read-lock, as updates are serialised. Must write-lock
 * for all node changes except colour changes and parent-link updates.
 * 
 * Searches must read-lock down the tree, as they do not serialise.
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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "portable_defns.h"
#include "gc.h"
#include "set.h"

#define IS_BLACK(_v)   ((int_addr_t)(_v)&1)
#define IS_RED(_v)     (!IS_BLACK(_v))
#define MK_BLACK(_v)   ((setval_t)((int_addr_t)(_v)|1))
#define MK_RED(_v)     ((setval_t)((int_addr_t)(_v)&~1))
#define GET_VALUE(_v)  (MK_RED(_v))
#define GET_COLOUR(_v) (IS_BLACK(_v))
#define SET_COLOUR(_v,_c) ((setval_t)((unsigned long)(_v)|(unsigned long)(_c)))

typedef struct node_st node_t;
typedef struct set_st set_t;

struct node_st
{
    setkey_t k;
    setval_t v;
    node_t *l, *r, *p;
    mrsw_lock_t lock;
};

struct set_st
{
    node_t root;
    CACHE_PAD(0);
    mcs_lock_t writer_lock;
};

static node_t null;
static int gc_id;

static void left_rotate(node_t *x)
{
    mrsw_qnode_t p_qn, x_qn, y_qn;
    node_t *y = x->r, *p = x->p;

    wr_lock(&p->lock, &p_qn);
    wr_lock(&x->lock, &x_qn);
    wr_lock(&y->lock, &y_qn);

    /* No need to write-lock to update parent link. */
    if ( (x->r = y->l) != &null ) x->r->p = x;

    x->p = y;
    y->l = x;
    y->p = p;
    if ( x == p->l ) p->l = y; else p->r = y;

    wr_unlock(&y->lock, &y_qn);
    wr_unlock(&x->lock, &x_qn);
    wr_unlock(&p->lock, &p_qn);
}


static void right_rotate(node_t *x)
{
    mrsw_qnode_t p_qn, x_qn, y_qn;
    node_t *y = x->l, *p = x->p;

    wr_lock(&p->lock, &p_qn);
    wr_lock(&x->lock, &x_qn);
    wr_lock(&y->lock, &y_qn);

    /* No need to write-lock to update parent link. */
    if ( (x->l = y->r) != &null ) x->l->p = x;

    x->p = y;
    y->r = x;
    y->p = p;
    if ( x == p->l ) p->l = y; else p->r = y;

    wr_unlock(&y->lock, &y_qn);
    wr_unlock(&x->lock, &x_qn);
    wr_unlock(&p->lock, &p_qn);
}


/* No locks held on entry/exit. Colour changes safe. Rotations lock for us. */
static void delete_fixup(ptst_t *ptst, set_t *s, node_t *x)
{
    node_t *p, *w;

    while ( (x->p != &s->root) && IS_BLACK(x->v) )
    {
        p = x->p;
        
        if ( x == p->l )
        {
            w = p->r;
            if ( IS_RED(w->v) )
            {
                w->v = MK_BLACK(w->v);
                p->v = MK_RED(p->v);
                /* Node W will be new parent of P. */
                left_rotate(p);
                /* Get new sibling W. */
                w = p->r;
            }
            
            if ( IS_BLACK(w->l->v) && IS_BLACK(w->r->v) )
            {
                w->v = MK_RED(w->v);
                x = p;
            }
            else
            {
                if ( IS_BLACK(w->r->v) ) 
                {
                    /* w->l is red => it cannot be null node. */
                    w->l->v = MK_BLACK(w->l->v);
                    w->v  = MK_RED(w->v);
                    right_rotate(w);
                    /* Old w is new w->r. Old w->l is new w.*/
                    w = p->r;
                }
                
                w->v = SET_COLOUR(GET_VALUE(w->v), GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                w->r->v = MK_BLACK(w->r->v);
                left_rotate(p);
                break;
            }
        }
        else /* SYMMETRIC CASE */
        {
            w = p->l;
            if ( IS_RED(w->v) )
            {
                w->v = MK_BLACK(w->v);
                p->v = MK_RED(p->v);
                /* Node W will be new parent of P. */
                right_rotate(p);
                /* Get new sibling W. */
                w = p->l;
            }
            
            if ( IS_BLACK(w->l->v) && IS_BLACK(w->r->v) )
            {
                w->v = MK_RED(w->v);
                x = p;
            }
            else
            {
                if ( IS_BLACK(w->l->v) ) 
                {
                    /* w->r is red => it cannot be the null node. */
                    w->r->v = MK_BLACK(w->r->v);
                    w->v  = MK_RED(w->v);
                    left_rotate(w);
                    /* Old w is new w->l. Old w->r is new w.*/
                    w = p->l;
                }
                
                w->v = SET_COLOUR(GET_VALUE(w->v), GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                w->l->v = MK_BLACK(w->l->v);
                right_rotate(p);
                break;
            }
        }
    }

    x->v = MK_BLACK(x->v);
}


set_t *set_alloc(void)
{
    ptst_t *ptst;
    set_t  *set;
    node_t *root;

    ptst = critical_enter();

    set = (set_t *)malloc(sizeof(*set));

    root = &set->root;
    root->k = SENTINEL_KEYMIN;
    root->v = MK_RED(NULL);
    root->l = &null;
    root->r = &null;
    root->p = NULL;
    mrsw_init(&root->lock);

    mcs_init(&set->writer_lock);

    critical_exit(ptst);

    return set;
}


setval_t set_update(set_t *s, setkey_t k, setval_t v, int overwrite)
{
    ptst_t  *ptst;
    node_t  *x, *p, *g, *y, *new;
    mrsw_qnode_t x_qn;
    qnode_t writer_qn;
    setval_t ov;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    mcs_lock(&s->writer_lock, &writer_qn);

    x = &s->root;
    while ( (y = (k < x->k) ? x->l : x->r) != &null )
    {
        x = y;
        if ( k == x->k ) break;
    }
    
    if ( k == x->k )
    {
        ov = x->v;
        /* Lock X to change mapping. */
        wr_lock(&x->lock, &x_qn);
        if ( overwrite ) x->v = SET_COLOUR(v, GET_COLOUR(ov));
        wr_unlock(&x->lock, &x_qn);
        ov = GET_VALUE(ov);
    }
    else
    {
        ov = NULL;

        new = (node_t *)gc_alloc(ptst, gc_id);
        new->k = k;
        new->v = MK_RED(v);
        new->l = &null;
        new->r = &null;
        new->p = x;
        mrsw_init(&new->lock);

        /* Lock X to change a child. */
        wr_lock(&x->lock, &x_qn);
        if ( k < x->k ) x->l = new; else x->r = new;
        wr_unlock(&x->lock, &x_qn);

        x = new;

        /* No locks held here. Colour changes safe. Rotations lock for us. */
        for ( ; ; )
        {
            if ( (p = x->p) == &s->root )
            {
                x->v = MK_BLACK(x->v);
                break;
            }

            if ( IS_BLACK(p->v) ) break;

            g = p->p;
            if ( p == g->l )
            {
                y = g->r;
                if ( IS_RED(y->v) )
                {
                    p->v = MK_BLACK(p->v);
                    y->v = MK_BLACK(y->v);
                    g->v = MK_RED(g->v);
                    x = g;
                }
                else
                {
                    if ( x == p->r )
                    {
                        x = p;
                        left_rotate(x);
                        /* X and P switched round. */
                        p = x->p;
                    }
                    p->v = MK_BLACK(p->v);
                    g->v = MK_RED(g->v);
                    right_rotate(g);
                    /* G no longer on the path. */
                }
            }
            else /* SYMMETRIC CASE */
            {
                y = g->l;
                if ( IS_RED(y->v) )
                {
                    p->v = MK_BLACK(p->v);
                    y->v = MK_BLACK(y->v);
                    g->v = MK_RED(g->v);
                    x = g;
                }
                else
                {
                    if ( x == p->l )
                    {
                        x = p;
                        right_rotate(x);
                        /* X and P switched round. */
                        p = x->p;
                    }
                    p->v = MK_BLACK(p->v);
                    g->v = MK_RED(g->v);
                    left_rotate(g);
                    /* G no longer on the path. */
                }
            }
        }
    }

    mcs_unlock(&s->writer_lock, &writer_qn);

    critical_exit(ptst);

    return ov;
}


setval_t set_remove(set_t *s, setkey_t k)
{
    ptst_t  *ptst;
    node_t  *x, *y, *z;
    mrsw_qnode_t qn[2], *y_pqn=qn+0, *yp_pqn=qn+1, *t_pqn;
    mrsw_qnode_t z_qn, zp_qn;
    qnode_t writer_qn;
    setval_t ov = NULL;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    mcs_lock(&s->writer_lock, &writer_qn);

    z = &s->root;
    while ( (z = (k < z->k) ? z->l : z->r) != &null )
    {
        if ( k == z->k ) break;
    }

    if ( k == z->k )
    {
        ov = GET_VALUE(z->v);

        if ( (z->l != &null) && (z->r != &null) )
        {
            /* Lock Z. It will get new key copied in. */
            wr_lock(&z->lock, &z_qn);
            y = z->r;
            /*
             * Write-lock from Z to Y. We end up with (YP,Y) locked.
             * Write-coupling is needed so we don't overtake searches for Y.
             */
            wr_lock(&y->lock, y_pqn);
            while ( y->l != &null )
            {
                if ( y->p != z ) wr_unlock(&y->p->lock, yp_pqn);
                y = y->l;
                t_pqn  = yp_pqn;
                yp_pqn = y_pqn;
                y_pqn  = t_pqn;
                wr_lock(&y->lock, y_pqn);
            }
        }
        else
        {
            y = z;
            /* Lock ZP. It will get new child. */
            wr_lock(&z->p->lock, &zp_qn);
            /* Lock Z. It will be deleted. */
            wr_lock(&z->lock, &z_qn);
        }

        /* No need to lock X. Only parent link is modified. */
        x = (y->l != &null) ? y->l : y->r;
        x->p = y->p;

        if ( y == y->p->l ) y->p->l = x; else y->p->r = x;

        if ( y != z )
        {
            z->k = y->k;
            z->v = SET_COLOUR(GET_VALUE(y->v), GET_COLOUR(z->v));
            if ( y->p != z ) wr_unlock(&y->p->lock, yp_pqn);
            wr_unlock(&y->lock, y_pqn);
        }
        else
        {
            wr_unlock(&z->p->lock, &zp_qn);
        }

        wr_unlock(&z->lock, &z_qn);

        gc_free(ptst, y, gc_id);

        if ( IS_BLACK(y->v) ) delete_fixup(ptst, s, x);
    }

    mcs_unlock(&s->writer_lock, &writer_qn);

    critical_exit(ptst);

    return ov;
}


setval_t set_lookup(set_t *s, setkey_t k)
{
    ptst_t  *ptst;
    node_t  *m, *n;
    mrsw_qnode_t qn[2], *m_pqn=&qn[0], *n_pqn=&qn[1], *t_pqn;
    setval_t v = NULL;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    n = &s->root;
    rd_lock(&n->lock, n_pqn);

    while ( (m = (k < n->k) ? n->l : n->r) != &null )
    {
        rd_lock(&m->lock, m_pqn);
        rd_unlock(&n->lock, n_pqn);
        n = m;
        t_pqn = m_pqn;
        m_pqn = n_pqn;
        n_pqn = t_pqn;
        if ( k == n->k )
        {
            v = GET_VALUE(n->v);
            break;
        }
    }

    rd_unlock(&n->lock, n_pqn);

    critical_exit(ptst);

    return v;
}


void _init_set_subsystem(void)
{
    gc_id = gc_add_allocator(sizeof(node_t));

    null.k = 0;
    null.v = MK_BLACK(NULL);
    null.l = NULL;
    null.r = NULL;
    null.p = NULL;
    mrsw_init(&null.lock);
}
