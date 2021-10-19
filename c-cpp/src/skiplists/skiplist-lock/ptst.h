/*
 * the per-thread state interface
 */
#ifndef PTST_H_
#define PTST_H_

#include <pthread.h>

typedef struct sl_ptst ptst_t;

#include "garbagecoll.h"

struct sl_ptst {
        /* thread id */
        unsigned int id;

        /* state management */
        struct sl_ptst *next;
        unsigned int   count;

        /* utility structures */
        gc_st *gc;
        unsigned long rand;
};

extern pthread_key_t ptst_key;

/*
 * enter/leave a critical section - a thread gets a state handle for
 * use during critical regions
 */
ptst_t* ptst_critical_enter(void);
#define ptst_critical_exit(_p) gc_exit(_p);

/* Iterators */
extern ptst_t *ptst_list;
#define ptst_first() (ptst_list)
#define ptst_next(_p) ((_p)->next)

/* Called once at the beginning of the application */
void ptst_subsystem_init(void);

#endif /* PTST_H_ */
