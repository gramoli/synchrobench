#ifndef __SPARC_DEFNS_H__
#define __SPARC_DEFNS_H__

#ifndef SPARC
#define SPARC
#endif

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sched.h>
#include <alloca.h>

#define CACHE_LINE_SIZE 64

#if 1
#include <thread.h>
#define pthread_mutex_t mutex_t
#define pthread_cond_t  cond_t
#define pthread_t       thread_t
#define pthread_key_t   thread_key_t
#define pthread_create(_a,_b,_c,_d) thr_create(NULL,0,_c,_d,THR_BOUND|THR_NEW_LWP,_a)
#define pthread_join(_a,_b) thr_join(_a,NULL,NULL)
#define pthread_key_create(_a,_b) thr_keycreate(_a,_b)
#define pthread_setspecific(_a,_b) thr_setspecific(_a,_b)
static void *pthread_getspecific(pthread_key_t _a)
{
    void *__x;
    thr_getspecific(_a,&__x);
    return __x;
}
#define pthread_setconcurrency(_x) thr_setconcurrency(_x)
#define pthread_mutex_init(_a,_b) mutex_init(_a,USYNC_THREAD,NULL)
#define pthread_mutex_lock(_a) mutex_lock(_a)
#define pthread_mutex_unlock(_a) mutex_unlock(_a)
#define pthread_cond_init(_a,_b) cond_init(_a,USYNC_THREAD,NULL)
#define pthread_cond_wait(_a,_b) cond_wait(_a,_b)
#define pthread_cond_broadcast(_a) cond_broadcast(_a)
#else
#include <pthread.h>
#endif


/*
 * I. Compare-and-swap.
 */

typedef unsigned long long _u64;

extern int CASIO_internal(int *, int, int);
extern void * CASPO_internal(void *, void *, void *);
extern _u64 CAS64O_internal(_u64 *, _u64, _u64);
#define CASIO(_a,_o,_n) (CASIO_internal((int*)(_a),(int)(_o),(int)(_n)))
#define CASPO(_a,_o,_n) (CASPO_internal((void*)(_a),(void*)(_o),(void*)(_n)))
#define CAS32O(_a,_o,_n) (_u32)(CASIO_internal((int *)_a,(int)_o,(int)_n))
#define CAS64O(_a,_o,_n) (CAS64O_internal((_u64 *)_a,(_u64)_o,(_u64)_n))

static int FASIO(int *a, int n)
{
    int no, o = *a;
    while ( (no = CASIO(a, o, n)) != o ) o = no;
    return o;
}

static void *FASPO(void *a, void *n)
{
    void *no, *o = *(void **)a;
    while ( (no = CASPO(a, o, n)) != o ) o = no;
    return o;
}


/*
 * II. Memory barriers. 
 *  WMB(): All preceding write operations must commit before any later writes.
 *  RMB(): All preceding read operations must commit before any later reads.
 *  MB():  All preceding memory accesses must commit before any later accesses.
 * 
 *  If the compiler does not observe these barriers (but any sane compiler
 *  will!), then VOLATILE should be defined as 'volatile'.
 */

extern void MEMBAR_ALL(void);
extern void MEMBAR_STORESTORE(void);
extern void MEMBAR_LOADLOAD(void);
#define MB()  MEMBAR_ALL()
#define WMB() MEMBAR_STORESTORE()
#define RMB() MEMBAR_LOADLOAD()
#define VOLATILE /*volatile*/


/*
 * III. Cycle counter access.
 */

typedef unsigned long tick_t;
extern tick_t RDTICK(void);


/*
 * IV. Types.
 */

typedef unsigned char      _u8;
typedef unsigned short     _u16;
typedef unsigned int       _u32;

#endif /* __SPARC_DEFNS_H__ */
