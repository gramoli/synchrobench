/******************************************************************************
 * bst_lock_kung.c
 * 
 * Lock-based binary search trees (BSTs), based on:
 *  H. T. Kung and Philip L. Lehman.
 *  "Concurrent manipulation of binary search trees".
 *  ACM Tranactions on Database Systems, Vol. 5, No. 3, September 1980.
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

#define IS_BLUE(_n)      ((int)(_n)->v & 1)
#define MK_BLUE(_n)      ((_n)->v = (setval_t)((unsigned long)(_n)->v | 1))

#define GET_VALUE(_n) ((setval_t)((unsigned long)(_n)->v & ~1UL))

#define LEFT  0
#define RIGHT 1
#define FOLLOW(_n, _d) ((_d) ? (_n)->r : (_n)->l)
#define UPDATE(_n, _d, _x) ((_d) ? ((_n)->r = (_x)) : ((_n)->l = (_x)))
#define FLIP(_d) ((_d)^1)

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
};

static int gc_id;

#define LOCK(_n, _pqn)   mcs_lock(&(_n)->lock, (_pqn))
#define UNLOCK(_n, _pqn) mcs_unlock(&(_n)->lock, (_pqn))


static node_t *weak_find(node_t *n, setkey_t k)
{
    while ( n != NULL )
    {
        if ( n->k < k )
            n = n->r;
        else if ( n->k > k )
            n = n->l;
        else
            break;
    } 
    return n;
}


static node_t *find(node_t *n, setkey_t k, qnode_t *qn, int *pdir)
{
    int dir;
    node_t *f, *s;

    s = n;

    do {
        f = s;
    retry:
        if ( k < f->k )
        {
            dir = LEFT;
            s   = f->l;
        }
        else
        {
            dir = RIGHT;
            s   = f->r;
        }
    }
    while ( (s != NULL) && (s->k != k) );
        
    LOCK(f, qn);
    if ( IS_BLUE(f) )
    {
        UNLOCK(f, qn);
        f = f->p;
        goto retry;
    }
    if ( s != FOLLOW(f, dir) )
    {
        UNLOCK(f, qn);
        goto retry;
    }

    *pdir = dir;
    return f;
}


static node_t *rotate(ptst_t *ptst, node_t *a, int dir1, 
                      int dir2, node_t **pc, qnode_t *pqn[])
{
    node_t *b = FOLLOW(a, dir1), *c = FOLLOW(b, dir2);
    node_t *bp = gc_alloc(ptst, gc_id), *cp = gc_alloc(ptst, gc_id);
    qnode_t c_qn;

    LOCK(c, &c_qn);

    memcpy(bp, b, sizeof(*b));
    memcpy(cp, c, sizeof(*c));

    mcs_init(&bp->lock);
    mcs_init(&cp->lock);

    LOCK(bp, pqn[3]);
    LOCK(cp, pqn[2]);

    assert(!IS_BLUE(a));
    assert(!IS_BLUE(b));
    assert(!IS_BLUE(c));

    UPDATE(cp, FLIP(dir2), bp);
    UPDATE(bp, dir2,       FOLLOW(c, FLIP(dir2)));

    UPDATE(a, dir1, cp);
    b->p = a;
    MK_BLUE(b);
    c->p = cp;
    MK_BLUE(c);

    gc_free(ptst, b, gc_id);
    gc_free(ptst, c, gc_id);

    UNLOCK(a, pqn[0]);
    UNLOCK(b, pqn[1]);
    UNLOCK(c, &c_qn);
    
    *pc = bp;
    return cp;
}


static void _remove(ptst_t *ptst, node_t *a, int dir1, int dir2, qnode_t **pqn)
{
    node_t *b = FOLLOW(a, dir1), *c = FOLLOW(b, dir2);
    assert(FOLLOW(b, FLIP(dir2)) == NULL);
    assert(!IS_BLUE(a));
    assert(!IS_BLUE(b));
    UPDATE(a, dir1,       c);
    UPDATE(b, FLIP(dir2), c);
    b->p = a;
    MK_BLUE(b);
    gc_free(ptst, b, gc_id);
    UNLOCK(a, pqn[0]);
    UNLOCK(b, pqn[1]);
}


static void delete_by_rotation(ptst_t *ptst, node_t *f, int dir,
                               qnode_t *pqn[], int lock_idx)
{
    node_t *g, *h, *s = FOLLOW(f, dir);

    if ( s->v != NULL )
    {
        UNLOCK(f, pqn[lock_idx+0]);
        UNLOCK(s, pqn[lock_idx+1]);
        return;
    }

    if ( s->l == NULL )
        _remove(ptst, f, dir, RIGHT, pqn+lock_idx);
    else if ( s->r == NULL )
        _remove(ptst, f, dir, LEFT, pqn+lock_idx);
    else
    {
        g = rotate(ptst, f, dir, LEFT, &h, pqn+lock_idx);
        lock_idx ^= 2;
        if ( h->l == NULL )
        {
            assert(h->v == NULL);
            _remove(ptst, g, RIGHT, RIGHT, pqn+lock_idx);
        }
        else
        {
            delete_by_rotation(ptst, g, RIGHT, pqn, lock_idx);
            LOCK(f, pqn[0]);
            if ( (g != FOLLOW(f, dir)) || IS_BLUE(f) )
            {
                UNLOCK(f, pqn[0]);
            }
            else
            {
                LOCK(g, pqn[1]);
                /*
                 * XXX Check that there is a node H to be rotated up.
                 * This is missing from the original paper, and must surely
                 * be a bug (we lost all locks at previous delete_by_rotation,
                 * so we can't know the existence of G's children).
                 */
                if ( g->r != NULL )
                {
                    g = rotate(ptst, f, dir, RIGHT, &h, pqn);
                    UNLOCK(g, pqn[2]);
                    UNLOCK(h, pqn[3]);
                }
                else
                {
                    UNLOCK(f, pqn[0]);
                    UNLOCK(g, pqn[1]);
                }
            }
        }
    }
}


set_t *set_alloc(void)
{
    set_t *s;

    s = malloc(sizeof(*s));
    mcs_init(&s->root.lock);
    s->root.k = SENTINEL_KEYMIN;
    s->root.v = (setval_t)(~1UL); /* dummy root node is white. */
    s->root.l = NULL;
    s->root.r = NULL;

    return s;
}


setval_t set_update(set_t *s, setkey_t k, setval_t v, int overwrite)
{
    node_t  *f, *w;
    qnode_t  f_qn, w_qn;
    int dir;
    setval_t ov = NULL;
    ptst_t  *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

 retry:
    f = find(&s->root, k, &f_qn, &dir);
    
    if ( (w = FOLLOW(f, dir)) != NULL )
    {
        /* Protected by parent lock. */
        assert(!IS_BLUE(w));
        ov = w->v;
        if ( overwrite || (ov == NULL) ) w->v = v;
    }
    else
    {
        w = gc_alloc(ptst, gc_id);
        w->l = NULL;
        w->r = NULL;
        w->v = v;
        w->k = k;
        mcs_init(&w->lock);
        UPDATE(f, dir, w);
    }

    UNLOCK(f, &f_qn);

    critical_exit(ptst);

    return ov;
}


setval_t set_remove(set_t *s, setkey_t k)
{
    node_t *f, *w;
    qnode_t qn[4], *pqn[] = { qn+0, qn+1, qn+2, qn+3, qn+0, qn+1 };
    int dir;
    setval_t v = NULL;
    ptst_t *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    f = find(&s->root, k, pqn[0], &dir);
    if ( (w = FOLLOW(f, dir)) != NULL )
    {
        LOCK(w, pqn[1]);
        v = w->v;
        w->v = NULL;
        assert(!IS_BLUE(w));
        delete_by_rotation(ptst, f, dir, pqn, 0);
    }
    else
    {
        UNLOCK(f, pqn[0]);
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

    n = weak_find(&s->root, k);
    if ( n != NULL ) v = GET_VALUE(n);

    critical_exit(ptst);
    return v;
}


void _init_set_subsystem(void)
{
    gc_id = gc_add_allocator(sizeof(node_t));
}
