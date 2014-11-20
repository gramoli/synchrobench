/******************************************************************************
 * skip_mcas.c
 * 
 * Skip lists, allowing concurrent update by use of MCAS primitive.
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "portable_defns.h"
#include "ptst.h"
#include "set.h"

#define MCAS_MARK(_v) ((unsigned long)(_v) & 3)

#define PROCESS(_v, _pv)                        \
  while ( MCAS_MARK(_v) ) {                     \
    mcas_fixup((void **)(_pv), _v);             \
    (_v) = *(_pv);                              \
  }

#define WALK_THRU(_v, _pv)                      \
  if ( MCAS_MARK(_v) ) (_v) = read_barrier_lite((void **)(_pv));

/* Pull in the MCAS implementation. */
#include "mcas.c"

/*
 * SKIP LIST
 */

typedef struct node_st node_t;
typedef struct set_st set_t;
typedef VOLATILE node_t *sh_node_pt;

struct node_st
{
    int        level;
    setkey_t  k;
    setval_t  v;
    sh_node_pt next[1];
};

struct set_st
{
    node_t head;
};

static int gc_id[NUM_LEVELS];

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
    return(n);
}


/* Free a node to the garbage collector. */
static void free_node(ptst_t *ptst, sh_node_pt n)
{
    gc_free(ptst, (void *)n, gc_id[n->level - 1]);
}


/*
 * Search for first non-deleted node, N, with key >= @k at each level in @l.
 * RETURN VALUES:
 *  Array @pa: @pa[i] is non-deleted predecessor of N at level i
 *  Array @na: @na[i] is N itself, which should be pointed at by @pa[i]
 *  MAIN RETURN VALUE: same as @na[0].
 */
static sh_node_pt search_predecessors(
    set_t *l, setkey_t k, sh_node_pt *pa, sh_node_pt *na)
{
    sh_node_pt x, x_next;
    setkey_t  x_next_k;
    int        i;

    RMB();

    x = &l->head;
    for ( i = NUM_LEVELS - 1; i >= 0; i-- )
    {
        for ( ; ; )
        {
            READ_FIELD(x_next, x->next[i]);
            WALK_THRU(x_next, &x->next[i]);

            READ_FIELD(x_next_k, x_next->k);
            if ( x_next_k >= k ) break;

            x = x_next;
        }

        if ( pa ) pa[i] = x;
        if ( na ) na[i] = x_next;
    }

    return(x_next);
}

static setval_t finish_delete(sh_node_pt x, sh_node_pt *preds)
{
    per_thread_state_t *mcas_ptst = get_ptst();
    CasDescriptor_t *cd;
    int level, i, ret = FALSE;
    sh_node_pt x_next;
    setkey_t x_next_k;
    setval_t v;

    READ_FIELD(level, x->level);

    cd = new_descriptor(mcas_ptst, (level << 1) + 1);
    cd->status = STATUS_IN_PROGRESS;
    cd->length = (level << 1) + 1;
    
    /* First, the deleted node's value field. */
    READ_FIELD(v, x->v);
    PROCESS(v, &x->v);
    if ( v == NULL ) goto fail;
    cd->entries[0].ptr = (void **)&x->v;
    cd->entries[0].old = v;
    cd->entries[0].new = NULL;

    for ( i = 0; i < level; i++ )
    {
        READ_FIELD(x_next, x->next[i]);
        PROCESS(x_next, &x->next[i]);
        READ_FIELD(x_next_k, x_next->k);
        if ( x->k > x_next_k ) { v = NULL; goto fail; }
        cd->entries[i      +1].ptr = (void **)&x->next[i];
        cd->entries[i      +1].old = x_next;
        cd->entries[i      +1].new = preds[i];
        cd->entries[i+level+1].ptr = (void **)&preds[i]->next[i];
        cd->entries[i+level+1].old = x;
        cd->entries[i+level+1].new = x_next;
    }
    
    ret = mcas0(mcas_ptst, cd);
    if ( ret == 0 ) v = NULL;

 fail:
    rc_down_descriptor(cd);
    return v;
}


/*
 * PUBLIC FUNCTIONS
 */

set_t *set_alloc(void)
{
    set_t *l;
    node_t *n;
    int i;

    static int mcas_inited = 0;
    if ( !CASIO(&mcas_inited, 0, 1) ) mcas_init();

    n = malloc(sizeof(*n) + (NUM_LEVELS-1)*sizeof(node_t *));
    memset(n, 0, sizeof(*n) + (NUM_LEVELS-1)*sizeof(node_t *));
    n->k = SENTINEL_KEYMAX;

    /*
     * Set the forward pointers of final node to other than NULL,
     * otherwise READ_FIELD() will continually execute costly barriers.
     * Note use of 0xfc -- that doesn't look like a marked value!
     */
    memset(n->next, 0xfc, NUM_LEVELS*sizeof(node_t *));

    l = malloc(sizeof(*l) + (NUM_LEVELS-1)*sizeof(node_t *));
    l->head.k = SENTINEL_KEYMIN;
    l->head.level = NUM_LEVELS;
    for ( i = 0; i < NUM_LEVELS; i++ )
    {
        l->head.next[i] = n;
    }

    return(l);
}


setval_t set_update(set_t *l, setkey_t k, setval_t v, int overwrite)
{
    setval_t  ov, new_ov;
    ptst_t    *ptst;
    sh_node_pt preds[NUM_LEVELS], succs[NUM_LEVELS];
    sh_node_pt succ, new = NULL;
    int        i, ret;
    per_thread_state_t *mcas_ptst = NULL;
    CasDescriptor_t *cd;

    k = CALLER_TO_INTERNAL_KEY(k);

    ptst = critical_enter();

    do {
    retry:
        ov = NULL;

        succ = search_predecessors(l, k, preds, succs);
    
        if ( succ->k == k )
        {
            /* Already a @k node in the list: update its mapping. */
            READ_FIELD(new_ov, succ->v);
            do {
                ov = new_ov;
                PROCESS(ov, &succ->v);
                if ( ov == NULL ) goto retry;
            }
            while ( overwrite && ((new_ov = CASPO(&succ->v, ov, v)) != ov) );

            if ( new != NULL ) free_node(ptst, new);
            goto out;
        }

#ifdef WEAK_MEM_ORDER
        /* Free node from previous attempt, if this is a retry. */
        if ( new != NULL ) 
        { 
            free_node(ptst, new);
            new = NULL;
        }
#endif

        /* Not in the list, so initialise a new node for insertion. */
        if ( new == NULL )
        {
            new    = alloc_node(ptst);
            new->k = k;
            new->v = v;
        }

        for ( i = 0; i < new->level; i++ )
        {
            new->next[i] = succs[i];
        }

        if ( !mcas_ptst ) mcas_ptst = get_ptst();
        cd = new_descriptor(mcas_ptst, new->level);
        cd->status = STATUS_IN_PROGRESS;
        cd->length = new->level;
        for ( i = 0; i < new->level; i++ )
        {
            cd->entries[i].ptr = (void **)&preds[i]->next[i];
            cd->entries[i].old = succs[i];
            cd->entries[i].new = new;
        }
        ret = mcas0(mcas_ptst, cd);
        rc_down_descriptor(cd);
    }
    while ( !ret );

 out:
    critical_exit(ptst);
    return(ov);
}


setval_t set_remove(set_t *l, setkey_t k)
{
    setval_t  v = NULL;
    ptst_t    *ptst;
    sh_node_pt preds[NUM_LEVELS], x;

    k = CALLER_TO_INTERNAL_KEY(k);
  
    ptst = critical_enter();
    
    do {
        x = search_predecessors(l, k, preds, NULL);
        if ( x->k > k ) goto out;
    } while ( (v = finish_delete(x, preds)) == NULL );

    free_node(ptst, x);

 out:
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

    x = search_predecessors(l, k, NULL, NULL);
    if ( x->k == k ) 
    {
        READ_FIELD(v, x->v);
        WALK_THRU(v, &x->v);
    }

    critical_exit(ptst);
    return(v);
}


void _init_set_subsystem(void)
{
    int i;

    for ( i = 0; i < NUM_LEVELS; i++ )
    {
        gc_id[i] = gc_add_allocator(sizeof(node_t) + i*sizeof(node_t *));
    }
    
}
