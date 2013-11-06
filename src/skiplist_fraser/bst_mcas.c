/******************************************************************************
 * bst_mcas.c
 * 
 * Lock-free binary search trees (BSTs), based on MCAS.
 * Uses a threaded representation to synchronise searches with deletions.
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
#include <stdlib.h>
#include <unistd.h>
#include "portable_defns.h"
#include "gc.h"
#include "set.h"

/* Allow MCAS marks to be detected using a single bitop (see IS_MCAS_OWNED). */
#define MARK_IN_PROGRESS 2
#define MARK_PTR_TO_CD   3

#define MARK_THREAD      1
#define MARK_GARBAGE     4

#define THREAD(_p)     ((node_t *)((int_addr_t)(_p)|(MARK_THREAD)))
#define GARBAGE(_p)    ((node_t *)((int_addr_t)(_p)|(MARK_GARBAGE)))
#define UNTHREAD(_p) ((node_t *)((int_addr_t)(_p)&~MARK_THREAD))
#define UNGARBAGE(_p) ((node_t *)((int_addr_t)(_p)&~MARK_GARBAGE))
/* Following only matches 2 and 3 (mod 4). Those happen to be MCAS marks :) */
#define IS_MCAS_OWNED(_p)   ((int)((int_addr_t)(_p)&2))
/* Matches 1 and 3 (mod 4). So only use if the ref is *not* owned by MCAS!! */
#define IS_THREAD(_p)       ((int)((int_addr_t)(_p)&MARK_THREAD))
/* Only use if the ref is *not* owned by MCAS (which may use bit 2)!! */
#define IS_GARBAGE(_p)      ((int)((int_addr_t)(_p)&MARK_GARBAGE))

#include "mcas.c"

typedef struct node_st node_t;
typedef struct set_st set_t;

struct node_st
{
    setkey_t k;
    setval_t v;
    node_t *l, *r;
};

struct set_st
{
    node_t root;
    node_t sentinel;
};

static int gc_id;

#define READ_LINK(_var, _link)                              \
    do {                                                    \
        (_var) = (_link);                                   \
        if ( !IS_MCAS_OWNED(_var) ) break;                  \
        mcas_fixup((void **)&(_link), (_var));              \
    } while ( 1 )

#define WEAK_READ_LINK(_var, _link)                         \
    do {                                                    \
        READ_LINK(_var, _link);                             \
        (_var) = UNGARBAGE(_var);                           \
    } while ( 0 )

#define STRONG_READ_LINK(_var, _link)                       \
    do {                                                    \
        READ_LINK(_var, _link);                             \
        if ( IS_GARBAGE(_var) ) goto retry;                 \
    } while ( 0 )

#define PROCESS_VAL(_v,_pv)                                 \
    do {                                                    \
        while ( IS_MCAS_OWNED(_v) )                         \
        {                                                   \
            mcas_fixup((void **)(_pv), (_v));               \
            (_v) = *(_pv);                                  \
        }                                                   \
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
    WEAK_READ_LINK(n, p->r);

    while ( !IS_THREAD(n) )
    {
        if ( k < n->k ) {
            WEAK_READ_LINK(c, n->l);
            assert(UNTHREAD(c)->k < n->k);
        } else if ( k > n->k ) {
            WEAK_READ_LINK(c, n->r);
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

    static int mcas_inited = 0;
    if ( !CASIO(&mcas_inited, 0, 1) )
    {
        if ( (sizeof(node_t) % 8) != 0 )
        {
            fprintf(stderr, "FATAL: node_t must be multiple of 8 bytes\n");
            *((int*)0)=0;
        }
        mcas_init();
    }

    s = malloc(sizeof(*s));
    s->root.k = SENTINEL_KEYMIN;
    s->root.v = NULL;
    s->root.l = THREAD(&s->root);
    s->root.r = THREAD(&s->sentinel);

    s->sentinel.k = SENTINEL_KEYMAX;

    return s;
}


setval_t set_update(set_t *s, setkey_t k, setval_t v, int overwrite)
{
    setval_t ov, nov;
    node_t *p, *n, *new = NULL, **ppc;
    ptst_t *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    do {
    retry:
        ov = NULL;

        n = search(s, k, &p);
        if ( !IS_THREAD(n) )
        {
            /* Already a @k node in the set: update its mapping. */
            nov = n->v;
            do {
                ov = nov;
                PROCESS_VAL(ov, &n->v);
                if ( ov == NULL ) goto retry;
            }
            while ( overwrite && ((nov = CASPO(&n->v, ov, v)) != ov) );

            goto out;            
        }

        if ( new == NULL )
        {
            new = gc_alloc(ptst, gc_id);
            new->k = k;
            new->v = v;
        }

        if ( p->k < k )
        {
            /* Ensure we insert in the correct interval. */
            if ( UNTHREAD(n)->k < k ) goto retry;
            new->l = THREAD(p);
            new->r = n;
            ppc = &p->r;
        }
        else
        {
            if ( UNTHREAD(n)->k > k ) goto retry;
            new->l = n;
            new->r = THREAD(p);
            ppc = &p->l;
        }

        WMB_NEAR_CAS();
    } 
    while ( CASPO(ppc, n, new) != n );

    new = NULL;

 out:
    if ( new ) gc_free(ptst, new, gc_id);
    critical_exit(ptst);
    return ov;
}


#define FIND_HELPER(_d1, _d2, _n, _ap, _a)                               \
{                                                                        \
    node_t *ac;                                                          \
    (_ap) = NULL;                                                        \
    (_a)  = (_n);                                                        \
    WEAK_READ_LINK(ac, (_a)->_d1);                                       \
    while ( !IS_THREAD(ac) )                                             \
    {                                                                    \
        (_ap) = (_a);                                                    \
        (_a)  = ac;                                                      \
        WEAK_READ_LINK(ac, (_a)->_d2);                                   \
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
            r = mcas(4,                                                      \
                     (void **)&n->v, v,  NULL,                               \
                     (void **)&n->l, nl, GARBAGE(nl),                        \
                     (void **)&n->r, nr, GARBAGE(nr),                        \
                     (void **)p_pc,  n,  n ## _d1);                          \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            if ( al == n ) goto retry;                                       \
            r = mcas(5,                                                      \
                     (void **)&n->v, v,  NULL,                               \
                     (void **)&n->l, nl, GARBAGE(nl),                        \
                     (void **)&n->r, nr, GARBAGE(nr),                        \
                     (void **)p_pc,  n,  n ## _d1,                           \
                     (void **)&al->_d2, THREAD(n), n ## _d2);                \
        }                                                                    \
    }                                                                        \
    else if ( IS_THREAD(n ## _d1) )                                          \
    {                                                                        \
        if ( ar == n ) goto retry;                                           \
        r = mcas(5,                                                          \
                 (void **)&n->v, v,  NULL,                                   \
                 (void **)&n->l, nl, GARBAGE(nl),                            \
                 (void **)&n->r, nr, GARBAGE(nr),                            \
                 (void **)p_pc,  n,  n ## _d2,                               \
                 (void **)&ar->_d1, THREAD(n), n ## _d1);                    \
    }                                                                        \
    else if ( dir )                                                          \
    {                                                                        \
        if ( (al == n) || (ar == n) ) goto retry;                            \
        if ( par == n )                                                      \
        {                                                                    \
            r = mcas(6,                                                      \
                     (void **)&n->v, v,  NULL,                               \
                     (void **)&ar->_d1, THREAD(n), n ## _d1,                 \
                     (void **)&al->_d2, THREAD(n), THREAD(ar),               \
                     (void **)&n->l,    nl, GARBAGE(nl),                     \
                     (void **)&n->r,    nr, GARBAGE(nr),                     \
                     (void **)p_pc,     n,  ar);                             \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            STRONG_READ_LINK(ac, ar->_d2);                                   \
            r = mcas(8,                                                      \
                     (void **)&n->v, v,  NULL,                               \
                     (void **)&par->_d1, ar,                                 \
                                         (IS_THREAD(ac) ? THREAD(ar) : ac),  \
                     (void **)&ar->_d2, ac, n ## _d2,                        \
                     (void **)&ar->_d1, THREAD(n), n ## _d1,                 \
                     (void **)&al->_d2, THREAD(n), THREAD(ar),               \
                     (void **)&n->l,    nl, GARBAGE(nl),                     \
                     (void **)&n->r,    nr, GARBAGE(nr),                     \
                     (void **)p_pc,     n,  ar);                             \
        }                                                                    \
    }                                                                        \
    else                                                                     \
    {                                                                        \
        if ( (al == n) || (ar == n) ) goto retry;                            \
        if ( pal == n )                                                      \
        {                                                                    \
            r = mcas(6,                                                      \
                     (void **)&n->v, v,  NULL,                               \
                     (void **)&al->_d2, THREAD(n), n ## _d2,                 \
                     (void **)&ar->_d1, THREAD(n), THREAD(al),               \
                     (void **)&n->l,    nl, GARBAGE(nl),                     \
                     (void **)&n->r,    nr, GARBAGE(nr),                     \
                     (void **)p_pc,     n,  al);                             \
        }                                                                    \
        else                                                                 \
        {                                                                    \
            STRONG_READ_LINK(ac, al->_d1);                                   \
            r = mcas(8,                                                      \
                     (void **)&n->v, v,  NULL,                               \
                     (void **)&pal->_d2, al,                                 \
                                         (IS_THREAD(ac) ? THREAD(al) : ac),  \
                     (void **)&al->_d1, ac, n ## _d1,                        \
                     (void **)&al->_d2, THREAD(n), n ## _d2,                 \
                     (void **)&ar->_d1, THREAD(n), THREAD(al),               \
                     (void **)&n->l,    nl, GARBAGE(nl),                     \
                     (void **)&n->r,    nr, GARBAGE(nr),                     \
                     (void **)p_pc,     n,  al);                             \
        }                                                                    \
    }


/* @k: key of node to be deleted */
setval_t set_remove(set_t *s, setkey_t k)
{
    node_t *p, *n, *nl, *nr, *al, *ar, *pal, *par, *ac, **p_pc;
    int r = 0;
    setval_t v;
    ptst_t *ptst;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    do
    {
    retry:
        v = NULL;

        /* Node present? */
        n = search(s, k, &p);
        if ( IS_THREAD(n) ) goto out;

        /* Already deleted? */
        v = n->v;
        PROCESS_VAL(v, &n->v);
        if ( v == NULL ) goto out;

        STRONG_READ_LINK(nl, n->l);
        STRONG_READ_LINK(nr, n->r);
        p_pc = (p->k > n->k) ? &p->l : &p->r;

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
    } while ( !r );

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
    PROCESS_VAL(v, &n->v);

    critical_exit(ptst);
    return v;
}


void _init_set_subsystem(void)
{
    gc_id = gc_add_allocator(sizeof(node_t));
}
