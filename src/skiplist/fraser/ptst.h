/******************************************************************************
 * ptst.h
 * 
 * Per-thread state management.
 * 
 * Copyright (c) 2002-2003, K A Fraser
 */

#ifndef __PTST_H__
#define __PTST_H__

typedef struct ptst_st ptst_t;

#include "gc.h"
#include "random.h"

struct ptst_st
{
    /* Thread id */
    unsigned int id;

    /* State management */
    ptst_t      *next;
    unsigned int count;
    /* Utility structures */
    gc_t        *gc;
    rand_t       rand;
};

extern pthread_key_t ptst_key;

/*
 * Enter/leave a critical region. A thread gets a state handle for
 * use during critical regions.
 */
ptst_t *critical_enter(void);
#define critical_exit(_p) gc_exit(_p)

/* Iterators */
extern ptst_t *ptst_list;
#define ptst_first()  (ptst_list)
#define ptst_next(_p) ((_p)->next)

/* Called once at start-of-day for entire application. */
void _init_ptst_subsystem(void);

#endif /* __PTST_H__ */
