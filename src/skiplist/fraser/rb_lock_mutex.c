/******************************************************************************
 * rb_lock_mutex.c
 * 
 * Lock-based red-black trees, based on Hanke's relaxed balancing operations.
 * 
 * For more details on the local tree restructuring operations used here:
 *  S. Hanke, T. Ottmann, and E. Soisalon-Soininen.
 *  "Relaxed balanced red-black trees".
 *  3rd Italian Conference on Algorithms and Complexity, pages 193-204.
 * 
 * Rather than issuing up-in and up-out requests to a balancing process,
 * each operation is directly responsible for local rebalancing. However,
 * this process can be split into a number of individual restructuring
 * operations, and locks can be released between each operation. Between
 * operations, we mark the node concerned as UNBALANCED -- contending
 * updates will then wait for this mark to be removed before continuing.
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

#define BLACK_MARK      0
#define RED_MARK        1
#define UNBALANCED_MARK 2

#define SET_VALUE(_v,_n)  \
    ((_v) = ((setval_t)(((unsigned long)(_v)&3)|((unsigned long)(_n)))))
#define GET_VALUE(_v)     ((setval_t)((int_addr_t)(_v) & ~3UL))
#define GET_COLOUR(_v)    ((int_addr_t)(_v) & 1)
#define SET_COLOUR(_v,_c) \
    ((setval_t)(((unsigned long)(_v)&~1UL)|(unsigned long)(_c)))

#define IS_BLACK(_v)      (GET_COLOUR(_v) == 0)
#define IS_RED(_v)        (GET_COLOUR(_v) == 1)
#define IS_UNBALANCED(_v) (((int_addr_t)(_v) & 2) == 2)

#define MK_BLACK(_v)      ((setval_t)(((int_addr_t)(_v)&~1UL) | 0))
#define MK_RED(_v)        ((setval_t)(((int_addr_t)(_v)&~1UL) | 1))
#define MK_BALANCED(_v)   ((setval_t)(((int_addr_t)(_v)&~2UL) | 0))
#define MK_UNBALANCED(_v) ((setval_t)(((int_addr_t)(_v)&~2UL) | 2))

#define GARBAGE_VALUE     ((setval_t)4)
#define IS_GARBAGE(_n)    (GET_VALUE((_n)->v) == GARBAGE_VALUE)
#define MK_GARBAGE(_n)    (SET_VALUE((_n)->v, GARBAGE_VALUE))

#define INTERNAL_VALUE ((void *)0xdeadbee0)

#define IS_ROOT(_n) ((_n)->p->k == 0)
#define IS_LEAF(_n) ((_n)->l == NULL)

/* TRUE if node X is a child of P. */
#define ADJACENT(_p,_x) (((_p)->l==(_x))||((_p)->r==(_x)))

typedef struct node_st node_t;
typedef struct set_st set_t;

struct node_st
{
    setkey_t k;
    setval_t v;
    node_t *l, *r, *p;
    mcs_lock_t lock;
};

struct set_st
{
    node_t root;
    node_t null;
    node_t dummy_g, dummy_gg;
};

static int gc_id;

/* Nodes p, x, y must be locked. */
static void left_rotate(ptst_t *ptst, node_t *x)
{
    node_t *y = x->r, *p = x->p, *nx;

    nx    = gc_alloc(ptst, gc_id);
    nx->p = y;
    nx->l = x->l;
    nx->r = y->l;
    nx->k = x->k;
    nx->v = x->v;
    mcs_init(&nx->lock);

    WMB();

    y->p    = p;
    x->l->p = nx;
    y->l->p = nx;
    y->l    = nx;
    if ( x == p->l ) p->l = y; else p->r = y;

    MK_GARBAGE(x);
    gc_free(ptst, x, gc_id);
}


/* Nodes p, x, y must be locked. */
static void right_rotate(ptst_t *ptst, node_t *x)
{
    node_t *y = x->l, *p = x->p, *nx;

    nx    = gc_alloc(ptst, gc_id);
    nx->p = y;
    nx->l = y->r;
    nx->r = x->r;
    nx->k = x->k;
    nx->v = x->v;
    mcs_init(&nx->lock);

    WMB();

    y->p    = p;
    x->r->p = nx;
    y->r->p = nx;
    y->r    = nx;
    if ( x == p->l ) p->l = y; else p->r = y;

    MK_GARBAGE(x);
    gc_free(ptst, x, gc_id);
}


static void fix_unbalance_up(ptst_t *ptst, node_t *x)
{
    qnode_t x_qn, g_qn, p_qn, w_qn, gg_qn;
    node_t *g, *p, *w, *gg;
    int done = 0;

    do {
        assert(IS_UNBALANCED(x->v));
        if ( IS_GARBAGE(x) ) return;

        p  = x->p;
        g  = p->p;
        gg = g->p;

        mcs_lock(&gg->lock, &gg_qn);
        if ( !ADJACENT(gg, g) || IS_UNBALANCED(gg->v) || IS_GARBAGE(gg) ) 
            goto unlock_gg;

        mcs_lock(&g->lock, &g_qn);
        if ( !ADJACENT(g, p) || IS_UNBALANCED(g->v) ) goto unlock_ggg;

        mcs_lock(&p->lock, &p_qn);
        if ( !ADJACENT(p, x) || IS_UNBALANCED(p->v) ) goto unlock_pggg;

        mcs_lock(&x->lock, &x_qn);
        
        assert(IS_RED(x->v));
        assert(IS_UNBALANCED(x->v));

        if ( IS_BLACK(p->v) )
        {
            /* Case 1. Nothing to do. */
            x->v = MK_BALANCED(x->v);
            done = 1;
            goto unlock_xpggg;
        }

        if ( IS_ROOT(x) )
        {
            /* Case 2. */
            x->v = MK_BLACK(MK_BALANCED(x->v));
            done = 1;
            goto unlock_xpggg;
        }

        if ( IS_ROOT(p) )
        {
            /* Case 2. */
            p->v = MK_BLACK(p->v);
            x->v = MK_BALANCED(x->v);
            done = 1;
            goto unlock_xpggg;
        }

        if ( g->l == p ) w = g->r; else w = g->l;
        mcs_lock(&w->lock, &w_qn);

        if ( IS_RED(w->v) )
        {
            /* Case 5. */
            /* In all other cases, doesn't change colour or subtrees. */
            if ( IS_UNBALANCED(w->v) ) goto unlock_wxpggg;
            g->v = MK_UNBALANCED(MK_RED(g->v));
            p->v = MK_BLACK(p->v);
            w->v = MK_BLACK(w->v);
            x->v = MK_BALANCED(x->v);
            done = 2;
            goto unlock_wxpggg;
        }

        /* Cases 3 & 4. Both of these need the great-grandfather locked. */
        if ( p == g->l )
        {
            if ( x == p->l )
            {
                /* Case 3. Single rotation. */
                x->v = MK_BALANCED(x->v);
                p->v = MK_BLACK(p->v);
                g->v = MK_RED(g->v);
                right_rotate(ptst, g);
            }
            else
            {
                /* Case 4. Double rotation. */
                x->v = MK_BALANCED(MK_BLACK(x->v));
                g->v = MK_RED(g->v);
                left_rotate(ptst, p);
                right_rotate(ptst, g);                
            }
        }
        else /* SYMMETRIC CASE */
        {
            if ( x == p->r )
            {
                /* Case 3. Single rotation. */
                x->v = MK_BALANCED(x->v);
                p->v = MK_BLACK(p->v);
                g->v = MK_RED(g->v);
                left_rotate(ptst, g);
            }
            else
            {
                /* Case 4. Double rotation. */
                x->v = MK_BALANCED(MK_BLACK(x->v));
                g->v = MK_RED(g->v);
                right_rotate(ptst, p);
                left_rotate(ptst, g);                
            }
        }

        done = 1;

    unlock_wxpggg:
        mcs_unlock(&w->lock, &w_qn);
    unlock_xpggg:
        mcs_unlock(&x->lock, &x_qn);
    unlock_pggg:
        mcs_unlock(&p->lock, &p_qn);
    unlock_ggg:
        mcs_unlock(&g->lock, &g_qn);
    unlock_gg:
        mcs_unlock(&gg->lock, &gg_qn);
        
        if ( done == 2 )
        {
            x = g;
            done = 0;
        }
    }
    while ( !done );
}


static void fix_unbalance_down(ptst_t *ptst, node_t *x)
{
    /* WN == W_NEAR, WF == W_FAR (W_FAR is further, in key space, from X). */
    qnode_t x_qn, w_qn, p_qn, g_qn, wn_qn, wf_qn;
    node_t *w, *p, *g, *wn, *wf;
    int done = 0;

    do {
        if ( !IS_UNBALANCED(x->v) || IS_GARBAGE(x) ) return;

        p = x->p;
        g = p->p;

        mcs_lock(&g->lock, &g_qn);
        if ( !ADJACENT(g, p) || IS_UNBALANCED(g->v) || IS_GARBAGE(g) )
            goto unlock_g;

        mcs_lock(&p->lock, &p_qn);
        if ( !ADJACENT(p, x) || IS_UNBALANCED(p->v) ) goto unlock_pg;

        mcs_lock(&x->lock, &x_qn);

        if ( !IS_BLACK(x->v) || !IS_UNBALANCED(x->v) )
        {
            done = 1;
            goto unlock_xpg;
        }

        if ( IS_ROOT(x) )
        {
            x->v = MK_BALANCED(x->v);
            done = 1;
            goto unlock_xpg;
        }

        w = (x == p->l) ? p->r : p->l;
        mcs_lock(&w->lock, &w_qn);
        if ( IS_UNBALANCED(w->v) )
        {
            if ( IS_BLACK(w->v) )
            {
                /* Funky relaxed rules to the rescue. */
                x->v = MK_BALANCED(x->v);
                w->v = MK_BALANCED(w->v);
                if ( IS_BLACK(p->v) )
                {
                    p->v = MK_UNBALANCED(p->v);
                    done = 2;
                }
                else
                {
                    p->v = MK_BLACK(p->v);
                    done = 1;
                }
            }
            goto unlock_wxpg;
        }

        assert(!IS_LEAF(w));
        
        if ( x == p->l )
        {
            wn = w->l;
            wf = w->r;
        }
        else
        {
            wn = w->r;
            wf = w->l;
        }

        mcs_lock(&wn->lock, &wn_qn);
        /* Hanke has an extra relaxed transform here. It's not needed. */
        if ( IS_UNBALANCED(wn->v) ) goto unlock_wnwxpg;

        mcs_lock(&wf->lock, &wf_qn);
        if ( IS_UNBALANCED(wf->v) ) goto unlock_wfwnwxpg;

        if ( IS_RED(w->v) )
        {
            /* Case 1. Rotate at parent. */
            assert(IS_BLACK(p->v) && IS_BLACK(wn->v) && IS_BLACK(wf->v));
            w->v = MK_BLACK(w->v);
            p->v = MK_RED(p->v);
            if ( x == p->l ) left_rotate(ptst, p); else right_rotate(ptst, p);
            goto unlock_wfwnwxpg;
        }

        if ( IS_BLACK(wn->v) && IS_BLACK(wf->v) )
        {
            if ( IS_RED(p->v) )
            {
                /* Case 2. Simple recolouring. */
                p->v = MK_BLACK(p->v);
                done = 1;
            }
            else
            {
                /* Case 5. Simple recolouring. */
                p->v = MK_UNBALANCED(p->v);
                done = 2;
            }
            w->v = MK_RED(w->v);
            x->v = MK_BALANCED(x->v);
            goto unlock_wfwnwxpg;
        }

        if ( x == p->l )
        {
            if ( IS_RED(wf->v) )
            {
                /* Case 3. Single rotation. */
                wf->v = MK_BLACK(wf->v);
                w->v = SET_COLOUR(w->v, GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                x->v = MK_BALANCED(x->v);
                left_rotate(ptst, p);
            }
            else
            {
                /* Case 4. Double rotation. */
                assert(IS_RED(wn->v));
                wn->v = SET_COLOUR(wn->v, GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                x->v = MK_BALANCED(x->v);
                right_rotate(ptst, w);
                left_rotate(ptst, p);
            }
        }
        else /* SYMMETRIC CASE: X == P->R  */
        {
            if ( IS_RED(wf->v) )
            {
                /* Case 3. Single rotation. */
                wf->v = MK_BLACK(wf->v);
                w->v = SET_COLOUR(w->v, GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                x->v = MK_BALANCED(x->v);
                right_rotate(ptst, p);
            }
            else
            {
                /* Case 4. Double rotation. */
                assert(IS_RED(wn->v));
                wn->v = SET_COLOUR(wn->v, GET_COLOUR(p->v));
                p->v = MK_BLACK(p->v);
                x->v = MK_BALANCED(x->v);
                left_rotate(ptst, w);
                right_rotate(ptst, p);
            }
        }

        done = 1;

    unlock_wfwnwxpg:
        mcs_unlock(&wf->lock, &wf_qn);
    unlock_wnwxpg:
        mcs_unlock(&wn->lock, &wn_qn);
    unlock_wxpg:
        mcs_unlock(&w->lock, &w_qn);
    unlock_xpg:
        mcs_unlock(&x->lock, &x_qn);
    unlock_pg:
        mcs_unlock(&p->lock, &p_qn);
    unlock_g:
        mcs_unlock(&g->lock, &g_qn);
        
        if ( done == 2 )
        {
            x = p;
            done = 0;
        }
    }
    while ( !done );
}


static void delete_finish(ptst_t *ptst, node_t *x)
{
    qnode_t g_qn, p_qn, w_qn, x_qn;
    node_t *g, *p, *w;
    int done = 0;

    do {
        if ( IS_GARBAGE(x) ) return;

        p = x->p;
        g = p->p;

        mcs_lock(&g->lock, &g_qn);
        if ( !ADJACENT(g, p) || IS_UNBALANCED(g->v) || IS_GARBAGE(g) ) 
            goto unlock_g;
        
        mcs_lock(&p->lock, &p_qn);
        /* Removing unbalanced red nodes is okay. */
        if ( !ADJACENT(p, x) || (IS_UNBALANCED(p->v) && IS_BLACK(p->v)) )
            goto unlock_pg;

        mcs_lock(&x->lock, &x_qn);
        if ( IS_UNBALANCED(x->v) ) goto unlock_xpg;        
        if ( GET_VALUE(x->v) != NULL )
        {
            done = 1;
            goto unlock_xpg;
        }

        if ( p->l == x ) w = p->r; else w = p->l;
        assert(w != x);
        mcs_lock(&w->lock, &w_qn);
        if ( IS_UNBALANCED(w->v) ) goto unlock_wxpg;

        if ( g->l == p ) g->l = w; else g->r = w; 
        MK_GARBAGE(p); gc_free(ptst, p, gc_id);
        MK_GARBAGE(x); gc_free(ptst, x, gc_id);
        w->p = g;
        if ( IS_BLACK(p->v) && IS_BLACK(w->v) )
        {
            w->v = MK_UNBALANCED(w->v);
            done = 2;
        }
        else
        {
            w->v = MK_BLACK(w->v);
            done = 1;
        }

    unlock_wxpg:
        mcs_unlock(&w->lock, &w_qn);
    unlock_xpg:
        mcs_unlock(&x->lock, &x_qn);
    unlock_pg:
        mcs_unlock(&p->lock, &p_qn);
    unlock_g:
        mcs_unlock(&g->lock, &g_qn);
    }
    while ( !done );

    if ( done == 2 ) fix_unbalance_down(ptst, w);
}


set_t *set_alloc(void)
{
    ptst_t *ptst;
    set_t  *set;
    node_t *root, *null;

    ptst = critical_enter();

    set = (set_t *)malloc(sizeof(*set));
    memset(set, 0, sizeof(*set));

    root = &set->root;
    null = &set->null;

    root->k = 0;
    root->v = MK_RED(INTERNAL_VALUE);
    root->l = NULL;
    root->r = null;
    root->p = NULL;
    mcs_init(&root->lock);

    null->k = SENTINEL_KEYMIN;
    null->v = MK_BLACK(INTERNAL_VALUE);
    null->l = NULL;
    null->r = NULL;
    null->p = root;
    mcs_init(&null->lock);

    set->dummy_gg.l = &set->dummy_g;
    set->dummy_g.p  = &set->dummy_gg;
    set->dummy_g.l  = &set->root;
    set->root.p     = &set->dummy_g;

    critical_exit(ptst);

    return set;
}


setval_t set_update(set_t *s, setkey_t k, setval_t v, int overwrite)
{
    ptst_t  *ptst;
    qnode_t  y_qn, z_qn;
    node_t  *y, *z, *new_internal, *new_leaf;
    int      fix_up = 0;
    setval_t ov = NULL;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

 retry:
    z = &s->root;
    while ( (y = (k <= z->k) ? z->l : z->r) != NULL )
        z = y;
    
    y = z->p;
    mcs_lock(&y->lock, &y_qn);
    if ( (((k <= y->k) ? y->l : y->r) != z) || IS_GARBAGE(y) )
    {
        mcs_unlock(&y->lock, &y_qn);
        goto retry;
    }

    mcs_lock(&z->lock, &z_qn);
    assert(!IS_GARBAGE(z) && IS_LEAF(z));

    if ( z->k == k )
    {
        ov = GET_VALUE(z->v);
        if ( overwrite || (ov == NULL) )
            SET_VALUE(z->v, v);
    }
    else
    {
        new_leaf     = gc_alloc(ptst, gc_id);
        new_internal = gc_alloc(ptst, gc_id);
        new_leaf->k = k;
        new_leaf->v = MK_BLACK(v);
        new_leaf->l = NULL;
        new_leaf->r = NULL;

        new_leaf->p = new_internal;
        mcs_init(&new_leaf->lock);
        if ( z->k < k )
        {
            new_internal->k = z->k;
            new_internal->l = z;
            new_internal->r = new_leaf;
        }
        else
        {
            new_internal->k = k;
            new_internal->l = new_leaf;
            new_internal->r = z;
        }
        new_internal->p = y;
        mcs_init(&new_internal->lock);

        if ( IS_UNBALANCED(z->v) )
        {
            z->v = MK_BALANCED(z->v);
            new_internal->v = MK_BLACK(INTERNAL_VALUE);
        }
        else if ( IS_RED(y->v) )
        {
            new_internal->v = MK_UNBALANCED(MK_RED(INTERNAL_VALUE));
            fix_up = 1;
        }
        else
        {
            new_internal->v = MK_RED(INTERNAL_VALUE);
        }

        WMB();

        z->p = new_internal;
        if ( y->l == z ) y->l = new_internal; else y->r = new_internal;
    }

    mcs_unlock(&y->lock, &y_qn);
    mcs_unlock(&z->lock, &z_qn);

    if ( fix_up ) 
        fix_unbalance_up(ptst, new_internal);

 out:
    critical_exit(ptst);

    return ov;
}


setval_t set_remove(set_t *s, setkey_t k)
{
    ptst_t  *ptst;
    node_t  *y, *z;
    qnode_t  z_qn;
    setval_t ov = NULL;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    z = &s->root;
    while ( (y = (k <= z->k) ? z->l : z->r) != NULL )
        z = y;

    if ( z->k == k ) 
    {
        mcs_lock(&z->lock, &z_qn);
        if ( !IS_GARBAGE(z) )
        {
            ov = GET_VALUE(z->v);

            SET_VALUE(z->v, NULL);
        }
        mcs_unlock(&z->lock, &z_qn);
    }

    if ( ov != NULL ) 
        delete_finish(ptst, z);

    critical_exit(ptst);

    return ov;
}


setval_t set_lookup(set_t *s, setkey_t k)
{
    ptst_t  *ptst;
    node_t  *m, *n;
    setval_t v;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    n = &s->root;
    while ( (m = (k <= n->k) ? n->l : n->r) != NULL )
        n = m;

    v = (k == n->k) ? GET_VALUE(n->v) : NULL;
    if ( v == GARBAGE_VALUE ) v = NULL;

    critical_exit(ptst);

    return v;
}


void _init_set_subsystem(void)
{
    gc_id = gc_add_allocator(sizeof(node_t));
}

#if 0
static int valll=0, bug=0, nrb=-1;
static void __traverse(node_t *n, int d, int _nrb)
{
    int i;
    if ( n == NULL ) 
    {
        if ( nrb == -1 ) nrb = _nrb;
        if ( nrb != _nrb )
            printf("Imbalance at depth %d (%d,%d)\n", d, nrb, _nrb);
        return;
    }
    if ( IS_LEAF(n) && (n->k != 0) )
    {
        assert(n->l == NULL);
        assert(n->r == NULL);
        assert(IS_BLACK(n->v));
    }
    if ( !IS_LEAF(n) && IS_RED(n->v) )
    {
        assert(IS_BLACK(n->l->v));
        assert(IS_BLACK(n->r->v));
    }
    if ( IS_BLACK(n->v) ) _nrb++;
    __traverse(n->l, d+1, _nrb);
    if ( valll > n->k ) bug=1;
#if 0
    for ( i = 0; i < d; i++ ) printf("  ");
    printf("%c%p K: %5d  V: %p  P: %p  L: %p  R: %p  depth: %d\n",
           IS_BLACK(n->v) ? 'B' : 'R', n, n->k, n->v, n->p, n->l, n->r, d);
#endif
    valll = n->k;
    __traverse(n->r, d+1, _nrb);
}
void check_tree(set_t *s)
{
    __traverse(s->root.r, 0, 0);    
    if ( bug )
        printf("***********************************************************************************************\n");
}
#endif
