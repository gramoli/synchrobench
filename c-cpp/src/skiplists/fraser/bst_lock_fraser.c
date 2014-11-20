/******************************************************************************
 * bst_lock_fraser.c
 * 
 * Lock-free binary serach trees (BSTs), based on per-node spinlocks.
 * Uses threaded tree presentation as described in my PhD dissertation:
 *  "Practical Lock-Freedom", University of Cambridge, 2003.
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

#define MARK_THREAD      1
#define THREAD(_p)     ((node_t *)((int_addr_t)(_p)|(MARK_THREAD)))
#define UNTHREAD(_p) ((node_t *)((int_addr_t)(_p)&~MARK_THREAD))
#define IS_THREAD(_p)       ((int)((int_addr_t)(_p)&MARK_THREAD))

#define IS_GARBAGE(_n)      ((_n)->v == NULL)

typedef struct node_st node_t;
typedef struct set_st set_t;

struct node_st
{
    setkey_t k;
    setval_t v;
    node_t *l, *r;
    mcs_lock_t lock;
};

struct set_st
{
    node_t root;
    node_t sentinel;
};

static int gc_id;

/* We use these flags to determine whch nodes are currently locked. */
#define P_LOCKED    0x01
#define N_LOCKED    0x02
#define PAL_LOCKED  0x04
#define PAR_LOCKED  0x08
#define AL_LOCKED   0x10
#define AR_LOCKED   0x20

#define LOCK(_n, _qn, _flag)                                  \
    do {                                                      \
        mcs_lock(&(_n)->lock, &(_qn));                        \
        if ( IS_GARBAGE(_n) ) {                               \
            mcs_unlock(&(_n)->lock, &(_qn));                  \
            goto retry;                                       \
        }                                                     \
        lock_flags |= (_flag);                                \
    } while ( 0 )

#define UNLOCK(_n, _qn, _flag)                                \
    do {                                                      \
        if ( (lock_flags & (_flag)) )                         \
             mcs_unlock(&(_n)->lock, &(_qn));                 \
    } while ( 0 )


/*
 * Search for node with key == k. Return NULL if none such, else ptr to node.
 * @ppn is filled in with parent node, or closest leaf if no match.
 * p and n will both be unmarked and adjacent on return.
 */
static node_t *search(set_t *s, setkey_t k, node_t **ppn)
{
    node_t *p, *n, *c;

 retry:
    p = &s->root;
    n = p->r;

    while ( !IS_THREAD(n) )
    {
        if ( k < n->k ) {
            c = n->l;
            assert(UNTHREAD(c)->k < n->k);
        } else if ( k > n->k ) {
            c = n->r;
            assert(UNTHREAD(c)->k > n->k);
        } else /* k == n->k */
            goto found;

        p = n; n = c;
    }

    /* Follow final thread, just in case. */
    c = UNTHREAD(n);
    if ( k == c->k ) goto followed_thread;

 found:
    if ( ppn ) *ppn = p;
    return n;

 followed_thread:
    if ( ppn ) { RMB(); goto retry; }
    return c;
}


set_t *set_alloc(void)
{
    set_t *s;

    s = malloc(sizeof(*s));
    mcs_init(&s->root.lock);
    s->root.k = SENTINEL_KEYMIN;
    s->root.v = (setval_t)(~0UL);
    s->root.l = THREAD(&s->root);
    s->root.r = THREAD(&s->sentinel);

    mcs_init(&s->sentinel.lock);
    s->sentinel.k = SENTINEL_KEYMAX;

    return s;
}


setval_t set_update(set_t *s, setkey_t k, setval_t v, int overwrite)
{
    setval_t ov;
    node_t *p, *n, *new = NULL;
    qnode_t qp, qn;
    ptst_t *ptst;
    int lock_flags, r = 0;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    do {
        ov = NULL;
        lock_flags = 0;

        n = search(s, k, &p);

        if ( !IS_THREAD(n) )
        {
            LOCK(n, qn, N_LOCKED);
            ov = n->v;
            if ( overwrite ) n->v = v;
        }
        else
        {
            if ( new == NULL )
            {
                new = gc_alloc(ptst, gc_id);
                mcs_init(&new->lock);
                new->k = k;
                new->v = v;
            }
            
            LOCK(p, qp, P_LOCKED);
            
            if ( p->k < k )
            {
                if ( (p->r != n) || (UNTHREAD(n)->k < k) ) goto retry;
                new->l = THREAD(p);
                new->r = n;
                WMB();
                p->r = new;
            }
            else
            {
                if ( (p->l != n) || (UNTHREAD(n)->k > k) ) goto retry;
                new->l = n;
                new->r = THREAD(p);
                WMB();
                p->l = new;
            }

            new = NULL; /* node is now in tree */
        }

        r = 1; /* success */

    retry:
        UNLOCK(p, qp, P_LOCKED);
        UNLOCK(n, qn, N_LOCKED);
    } 
    while ( !r );

    if ( new ) gc_free(ptst, new, gc_id);
    critical_exit(ptst);
    return ov;
}


#define FIND_HELPER(_d1, _d2, _n, _ap, _a)                               \
{                                                                        \
    node_t *ac;                                                          \
    (_ap) = NULL;                                                        \
    (_a)  = (_n);                                                        \
    ac    = (_a)->_d1;                                                   \
    while ( !IS_THREAD(ac) )                                             \
    {                                                                    \
        (_ap) = (_a);                                                    \
        (_a)  = ac;                                                      \
        ac    = (_a)->_d2;                                               \
    }                                                                    \
}


/*
 * Order of first two cases does matter! If @n is the left-link of @p, then
 * we use DELETE_HELPER(l, r). What matters is what we do when @n is a leaf.
 * In this case we end up choosing n->l to propagate to p->l -- this
 * happens to be the correct choice :-)
 *
 * NB. Note symmetric deletion cases dependent on parameter @dir. We
 * could simplify the algorithm by always following one direction. In fact,
 * that is slightly worse, or much worse, depending on the chosen case
 * (hint: works best with dir hardwired to zero :-)....
 */
#define dir 0
#define DELETE_HELPER(_d1, _d2)                                              \
    FIND_HELPER(_d1, _d2, n, pal, al);                                       \
    FIND_HELPER(_d2, _d1, n, par, ar);                                       \
    if ( IS_THREAD(n ## _d2) )                                               \
    {                                                                        \
        if ( IS_THREAD(n ## _d1) )                                           \
        {                                                                    \
            *p_pc = n ## _d1;                                                \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            LOCK(al, qal, AL_LOCKED);                                        \
            if ( al->_d2 != THREAD(n) ) goto retry;                          \
            *p_pc = n ## _d1;                                                \
            al->_d2 = n ## _d2;                                              \
        }                                                                    \
    }                                                                        \
    else if ( IS_THREAD(n ## _d1) )                                          \
    {                                                                        \
        LOCK(ar, qar, AR_LOCKED);                                            \
        if ( ar->_d1 != THREAD(n) ) goto retry;                              \
        *p_pc = n ## _d2;                                                    \
        ar->_d1 = n ## _d1;                                                  \
    }                                                                        \
    else if ( dir )                                                          \
    {                                                                        \
        if ( par != n )                                                      \
        {                                                                    \
            LOCK(par, qpar, PAR_LOCKED);                                     \
            if ( par->_d1 != ar ) goto retry;                                \
        }                                                                    \
        LOCK(al, qal, AL_LOCKED);                                            \
        LOCK(ar, qar, AR_LOCKED);                                            \
        if ( (al->_d2 != THREAD(n)) || (ar->_d1 != THREAD(n)) ) goto retry;  \
        al->_d2 = THREAD(ar);                                                \
        ar->_d1 = n ## _d1;                                                  \
        if ( par != n )                                                      \
        {                                                                    \
            ac = ar->_d2;                                                    \
            ar->_d2 = n ## _d2;                                              \
            par->_d1 = IS_THREAD(ac) ? THREAD(ar) : ac;                      \
        }                                                                    \
        WMB(); /* New links in AR must appear before it is raised. */        \
        *p_pc = ar;                                                          \
    }                                                                        \
    else                                                                     \
    {                                                                        \
        if ( pal != n )                                                      \
        {                                                                    \
            LOCK(pal, qpal, PAL_LOCKED);                                     \
            if ( pal->_d2 != al ) goto retry;                                \
        }                                                                    \
        LOCK(al, qal, AL_LOCKED);                                            \
        LOCK(ar, qar, AR_LOCKED);                                            \
        if ( (al->_d2 != THREAD(n)) || (ar->_d1 != THREAD(n)) ) goto retry;  \
        al->_d2 = n ## _d2;                                                  \
        ar->_d1 = THREAD(al);                                                \
        if ( pal != n )                                                      \
        {                                                                    \
            ac = al->_d1;                                                    \
            al->_d1 = n ## _d1;                                              \
            pal->_d2 = IS_THREAD(ac) ? THREAD(al) : ac;                      \
        }                                                                    \
        WMB(); /* New links in AL must appear before it is raised. */        \
        *p_pc = al;                                                          \
    }


/* @k: key of node to be deleted */
setval_t set_remove(set_t *s, setkey_t k)
{
    node_t *p, *n, *nl, *nr, *al, *ar, *pal, *par, *ac, **p_pc;
    qnode_t qp, qn, qal, qar, qpal, qpar;
    int r = 0, lock_flags;
    setval_t v;
    ptst_t *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    do {
        v = NULL;
        lock_flags = 0;

        n = search(s, k, &p);
        if ( IS_THREAD(n) ) goto out;

        LOCK(p, qp, P_LOCKED);
        p_pc = (p->k > n->k) ? &p->l : &p->r;
        if ( *p_pc != n ) goto retry;

        LOCK(n, qn, N_LOCKED);

        nl = n->l;
        nr = n->r;

        if ( p->k > n->k )
        {
            /* @n is leftwards link from @p. */
            DELETE_HELPER(l, r);
        }
        else
        {
            /* @n is rightwards link from @p. */
            DELETE_HELPER(r, l);
        }
        
        r = 1;
        v = n->v;
        n->v = NULL;

    retry:
        UNLOCK(p, qp, P_LOCKED);
        UNLOCK(n, qn, N_LOCKED);
        UNLOCK(pal, qpal, PAL_LOCKED);
        UNLOCK(par, qpar, PAR_LOCKED);
        UNLOCK(al, qal, AL_LOCKED);
        UNLOCK(ar, qar, AR_LOCKED);
    }
    while ( !r );

    gc_free(ptst, n, gc_id);

 out:
    critical_exit(ptst);
    return v;
}


setval_t set_lookup(set_t *s, setkey_t k)
{
    node_t *n;
    setval_t v;
    ptst_t *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    n = search(s, k, NULL);
    v = (!IS_THREAD(n)) ? n->v : NULL;

    critical_exit(ptst);
    return v;
}


void _init_set_subsystem(void)
{
    gc_id = gc_add_allocator(sizeof(node_t));
}
