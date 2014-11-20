/******************************************************************************
 * rb_stm.c
 * 
 * Lock-free red-black trees, based on STM.
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
#include "stm.h"
#include "set.h"

#define IS_BLACK(_v)   ((int_addr_t)(_v)&1)
#define IS_RED(_v)     (!IS_BLACK(_v))
#define MK_BLACK(_v)   ((setval_t)((int_addr_t)(_v)|1))
#define MK_RED(_v)     ((setval_t)((int_addr_t)(_v)&~1))
#define GET_VALUE(_v)  (MK_RED(_v))
#define GET_COLOUR(_v) (IS_BLACK(_v))
#define SET_COLOUR(_v,_c) ((setval_t)((unsigned long)(_v)|(unsigned long)(_c)))

typedef struct node_st node_t;
typedef stm_blk set_t;

struct node_st
{
    setkey_t k;
    setval_t v;
    stm_blk *l, *r, *p;
};

static struct {
    CACHE_PAD(0);
    stm *memory;    /* read-only */
    stm_blk *nullb; /* read-only */
    CACHE_PAD(2);
} shared;

#define MEMORY (shared.memory)
#define NULLB  (shared.nullb)

static void left_rotate(ptst_t *ptst, stm_tx *tx, stm_blk *xb, node_t *x)
{
    stm_blk *yb, *pb;
    node_t *y, *p;

    yb = x->r;
    pb = x->p;

    y = write_stm_blk(ptst, tx, yb);
    p = write_stm_blk(ptst, tx, pb);

    if ( (x->r = y->l) != NULLB )
    {
        node_t *xr = write_stm_blk(ptst, tx, x->r);
        xr->p = xb;
    }

    x->p = yb;
    y->l = xb;
    y->p = pb;
    if ( xb == p->l ) p->l = yb; else p->r = yb;
}


static void right_rotate(ptst_t *ptst, stm_tx *tx, stm_blk *xb, node_t *x)
{
    stm_blk *yb, *pb;
    node_t *y, *p;

    yb = x->l;
    pb = x->p;

    y = write_stm_blk(ptst, tx, yb);
    p = write_stm_blk(ptst, tx, pb);

    if ( (x->l = y->r) != NULLB )
    {
        node_t *xl = write_stm_blk(ptst, tx, x->l);
        xl->p = xb;
    }

    x->p = yb;
    y->r = xb;
    y->p = pb;
    if ( xb == p->l ) p->l = yb; else p->r = yb;
}


static void delete_fixup(ptst_t *ptst, stm_tx *tx, set_t *s, 
                         stm_blk *xb, node_t *x)
{
    stm_blk *pb, *wb, *wlb, *wrb;
    node_t *p, *w, *wl, *wr;

    while ( (x->p != s) && IS_BLACK(x->v) )
    {
        pb = x->p;
        p  = write_stm_blk(ptst, tx, pb);
        
        if ( xb == p->l )
        {
            wb = p->r;
            w  = write_stm_blk(ptst, tx, wb);
            if ( IS_RED(w->v) )
            {
                w->v = MK_BLACK(w->v);
                p->v = MK_RED(p->v);
                left_rotate(ptst, tx, pb, p);
                wb = p->r;
                w  = write_stm_blk(ptst, tx, wb);
            }

            wlb = w->l;
            wl  = read_stm_blk(ptst, tx, wlb);
            wrb = w->r;
            wr  = read_stm_blk(ptst, tx, wrb);
            if ( IS_BLACK(wl->v) && IS_BLACK(wr->v) )
            {
                w->v = MK_RED(w->v);
                xb = pb;
                x  = p;
            }
            else
            {
                if ( IS_BLACK(wr->v) ) 
                {
                    wl = write_stm_blk(ptst, tx, wlb);
                    wl->v = MK_BLACK(wl->v);
                    w->v  = MK_RED(w->v);
                    right_rotate(ptst, tx, wb, w);
                    wb = p->r;
                    w  = write_stm_blk(ptst, tx, wb);
                }
                
                wrb = w->r;
                wr  = write_stm_blk(ptst, tx, wrb);
                w->v = SET_COLOUR(GET_VALUE(w->v), GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                wr->v = MK_BLACK(wr->v);
                left_rotate(ptst, tx, pb, p);
                break;
            }
        }
        else /* SYMMETRIC CASE */
        {
            wb = p->l;
            w  = write_stm_blk(ptst, tx, wb);
            if ( IS_RED(w->v) )
            {
                w->v = MK_BLACK(w->v);
                p->v = MK_RED(p->v);
                right_rotate(ptst, tx, pb, p);
                wb = p->l;
                w  = write_stm_blk(ptst, tx, wb);
            }
            
            wlb = w->l;
            wl  = read_stm_blk(ptst, tx, wlb);
            wrb = w->r;
            wr  = read_stm_blk(ptst, tx, wrb);
            if ( IS_BLACK(wl->v) && IS_BLACK(wr->v) )
            {
                w->v = MK_RED(w->v);
                xb = pb;
                x  = p;
            }
            else
            {
                if ( IS_BLACK(wl->v) ) 
                {
                    wr = write_stm_blk(ptst, tx, wrb);
                    wr->v = MK_BLACK(wr->v);
                    w->v  = MK_RED(w->v);
                    left_rotate(ptst, tx, wb, w);
                    wb = p->l;
                    w  = write_stm_blk(ptst, tx, wb);
                }
                
                wlb = w->l;
                wl  = write_stm_blk(ptst, tx, wlb);
                w->v = SET_COLOUR(GET_VALUE(w->v), GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                wl->v = MK_BLACK(wl->v);
                right_rotate(ptst, tx, pb, p);
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

    set = new_stm_blk(ptst, MEMORY);

    root = init_stm_blk(ptst, MEMORY, set);
    root->k = SENTINEL_KEYMIN;
    root->v = MK_RED(NULL);
    root->l = NULLB;
    root->r = NULLB;
    root->p = NULL;

    critical_exit(ptst);

    return set;
}


setval_t set_update(set_t *s, setkey_t k, setval_t v, int overwrite)
{
    ptst_t  *ptst;
    stm_tx  *tx;
    stm_blk *xb, *b, *pb, *gb, *yb, *newb;
    node_t  *x, *p, *g, *y, *new;
    setval_t ov;

    k = CALLER_TO_INTERNAL_KEY(k);

    newb = NULL;

    ptst = critical_enter();

    do {
        new_stm_tx(tx, ptst, MEMORY);

        b = s;
        while ( b != NULLB )
        {
            xb = b;
            x = read_stm_blk(ptst, tx, xb);
            if ( k == x->k ) break;
            b = (k < x->k) ? x->l : x->r;
        }

        x = write_stm_blk(ptst, tx, xb);

        if ( k == x->k )
        {
            ov = x->v;
            if ( overwrite ) x->v = SET_COLOUR(v, GET_COLOUR(ov));
            ov = GET_VALUE(ov);
        }
        else
        {
            ov = NULL;
            if ( newb == NULL )
            {
                newb = new_stm_blk(ptst, MEMORY);
                new = init_stm_blk(ptst, MEMORY, newb);
                new->k = k;
            }

            new->v = MK_RED(v);
            new->l = NULLB;
            new->r = NULLB;
            new->p = xb;

            if ( k < x->k ) x->l = newb; else x->r = newb;

            xb = newb;
            x  = new;

            for ( ; ; )
            {
                if ( (pb = x->p) == s )
                {
                    x->v = MK_BLACK(x->v);
                    break;
                }

                p = read_stm_blk(ptst, tx, pb);
                if ( IS_BLACK(p->v) ) break;

                gb = p->p;
                g = read_stm_blk(ptst, tx, gb);
                if ( pb == g->l )
                {
                    yb = g->r;
                    y  = read_stm_blk(ptst, tx, yb);
                    if ( IS_RED(y->v) )
                    {
                        p = write_stm_blk(ptst, tx, pb);
                        y = write_stm_blk(ptst, tx, yb);
                        g = write_stm_blk(ptst, tx, gb);
                        p->v = MK_BLACK(p->v);
                        y->v = MK_BLACK(y->v);
                        g->v = MK_RED(g->v);
                        xb = gb;
                        x  = g;
                    }
                    else
                    {
                        if ( xb == p->r )
                        {
                            xb = pb;
                            x  = write_stm_blk(ptst, tx, pb);
                            left_rotate(ptst, tx, xb, x);
                        }
                        pb = x->p;
                        p  = write_stm_blk(ptst, tx, pb);
                        gb = p->p;
                        g  = write_stm_blk(ptst, tx, gb);
                        p->v = MK_BLACK(p->v);
                        g->v = MK_RED(g->v);
                        right_rotate(ptst, tx, gb, g);
                    }
                }
                else /* SYMMETRIC CASE */
                {
                    yb = g->l;
                    y  = read_stm_blk(ptst, tx, yb);
                    if ( IS_RED(y->v) )
                    {
                        p = write_stm_blk(ptst, tx, pb);
                        y = write_stm_blk(ptst, tx, yb);
                        g = write_stm_blk(ptst, tx, gb);
                        p->v = MK_BLACK(p->v);
                        y->v = MK_BLACK(y->v);
                        g->v = MK_RED(g->v);
                        xb = gb;
                        x  = g;
                    }
                    else
                    {
                        if ( xb == p->l )
                        {
                            xb = pb;
                            x  = write_stm_blk(ptst, tx, pb);
                            right_rotate(ptst, tx, xb, x);
                        }
                        pb = x->p;
                        p  = write_stm_blk(ptst, tx, pb);
                        gb = p->p;
                        g  = write_stm_blk(ptst, tx, gb);
                        p->v = MK_BLACK(p->v);
                        g->v = MK_RED(g->v);
                        left_rotate(ptst, tx, gb, g);
                    }
                }
            }
        }

        remove_from_tx(ptst, tx, NULLB);
    }
    while ( !commit_stm_tx(ptst, tx) );

    /* Free unused new block. */
    if ( (ov != NULL) && (newb != NULL) ) free_stm_blk(ptst, MEMORY, newb);

    critical_exit(ptst);

    return ov;
}


setval_t set_remove(set_t *s, setkey_t k)
{
    ptst_t  *ptst;
    stm_tx  *tx;
    stm_blk *zb, *b, *xb, *yb;
    node_t  *z, *x, *y, *yp;
    setval_t ov;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    do {
        new_stm_tx(tx, ptst, MEMORY);
        ov = NULL;
        b  = s;
    
        while ( b != NULLB )
        {
            zb = b;
            z = read_stm_blk(ptst, tx, zb);
            if ( k == z->k )
            {
                ov = GET_VALUE(z->v);
                break;
            }
            b = (k < z->k) ? z->l : z->r;
        }
        
        if ( ov != NULL )
        {
            z = write_stm_blk(ptst, tx, zb);

            if ( (z->l != NULLB) && (z->r != NULLB) )
            {
                /* Find successor of node z, and place in (yb,y). */
                yb = z->r;
                y  = read_stm_blk(ptst, tx, yb);

                while ( y->l != NULLB )
                {
                    yb = y->l;
                    y  = read_stm_blk(ptst, tx, yb);
                }

                y = write_stm_blk(ptst, tx, yb);
            }
            else
            {
                yb = zb;
                y  = z;
            }

            xb = (y->l != NULLB) ? y->l : y->r;
            x = write_stm_blk(ptst, tx, xb);
            x->p = y->p;

            yp = write_stm_blk(ptst, tx, y->p);
            if ( yb == yp->l ) yp->l = xb; else yp->r = xb;

            if ( y != z )
            {
                z->k = y->k;
                z->v = SET_COLOUR(GET_VALUE(y->v), GET_COLOUR(z->v));
            }

            if ( IS_BLACK(y->v) ) delete_fixup(ptst, tx, s, xb, x);
        }

        remove_from_tx(ptst, tx, NULLB);
    }
    while ( !commit_stm_tx(ptst, tx) );

    /* Free a deleted block. */
    if ( ov != NULL ) free_stm_blk(ptst, MEMORY, yb);

    critical_exit(ptst);

    return ov;
}


setval_t set_lookup(set_t *s, setkey_t k)
{
    ptst_t  *ptst;
    stm_tx  *tx;
    stm_blk *nb;
    node_t  *n;
    setval_t v;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    do {
        new_stm_tx(tx, ptst, MEMORY);
        v  = NULL;
        nb = s;
    
        while ( nb != NULLB )
        {
            n = read_stm_blk(ptst, tx, nb);
            if ( k == n->k )
            {
                v = GET_VALUE(n->v);
                break;
            }
            nb = (k < n->k) ? n->l : n->r;
        }
    }
    while ( !commit_stm_tx(ptst, tx) );

    critical_exit(ptst);

    return v;
}


void _init_set_subsystem(void)
{
    node_t *null;
    ptst_t *ptst;

    ptst = critical_enter();

    _init_stm_subsystem(0);

    MEMORY = new_stm(ptst, sizeof(node_t));

    NULLB = new_stm_blk(ptst, MEMORY);
    null  = init_stm_blk(ptst, MEMORY, NULLB);
    null->k = 0;
    null->v = MK_BLACK(NULL);
    null->l = NULL;
    null->r = NULL;
    null->p = NULL;

    critical_exit(ptst);
}
