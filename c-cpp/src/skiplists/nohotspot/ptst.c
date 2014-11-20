/*
 * ptst.c: per-thread state for threads operating on the skip list
 *
 * Author: Ian Dick, 2013.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "skiplist.h"
#include "ptst.h"
#include "garbagecoll.h"

/* - Globals - */
pthread_key_t   ptst_key;
ptst_t  *ptst_list;
static unsigned int next_id;

/* - Private function declarations - */
static void ptst_destructor(ptst_t *ptst);

/* - Private function definitions - */

/**
 * ptst_destructor - reclaim a recently released per-thread state
 * @ptst: the per-thread state to reclaim
 */
static void ptst_destructor(ptst_t *ptst)
{
        ptst->count = 0;
}

/* - Public ptst functions - */

/**
 * ptst_critical_enter - enter/leave a critical section
 *
 * Returns a ptst handle for use by the calling thread during the 
 * critical section.
 */
ptst_t* ptst_critical_enter(void)
{
        ptst_t *ptst, *next;
        unsigned int id;

        ptst = (ptst_t*) pthread_getspecific(ptst_key);
        if (NULL == ptst) {
                ptst = ptst_first();
                for ( ; NULL != ptst; ptst = ptst_next(ptst)) {
                        if ((0 == ptst->count) && CAS(&ptst->count, 0, 1))
                                break;
                }

                if (NULL == ptst) {
                        ptst = ALIGNED_ALLOC(sizeof(ptst_t));
                        if (!ptst) {
                                perror("malloc: sl_ptst_critial_enter\n");
                                exit(1);
                        }
                        memset(ptst, 0, sizeof(*ptst));
                        ptst->gc = gc_init();
                        ptst->count = 1;
                        id = next_id;
                        while ((!CAS(&next_id, id, id+1)))
                                id = next_id;
                        ptst->id = id;
                        do {
                                next = ptst_list;
                                ptst->next = next;
                        } while (!CAS(&ptst_list, next, ptst));
                }

                pthread_setspecific(ptst_key, ptst);
        }

        gc_enter(ptst);

        return ptst;
}

/**
 * ptst_subsystem_init - initialise the ptst subsystem
 *
 * Note: This should only happen once at the start of the application.
 */
void ptst_subsystem_init(void)
{
        ptst_list = NULL;
        next_id      = 0;
        BARRIER();
        if (pthread_key_create(&ptst_key, (void (*)(void *))ptst_destructor)) {
                perror("pthread_key_create: ptst_subsystem_init\n");
                exit(1);
        }
}
