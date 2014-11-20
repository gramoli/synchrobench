#ifndef __PORTABLE_DEFNS_H__
#define __PORTABLE_DEFNS_H__

#define MAX_THREADS 128 /* Nobody will ever have more! */

#if defined(SPARC)
#include "sparc_defns.h"
#elif defined(INTEL)
#include "intel_defns.h"
#elif defined(PPC)
#include "ppc_defns.h"
#elif defined(IA64)
#include "ia64_defns.h"
#elif defined(MIPS)
#include "mips_defns.h"
#elif defined(ALPHA)
#include "alpha_defns.h"
#else
#error "A valid architecture has not been defined"
#endif

#include <string.h>

#ifndef MB_NEAR_CAS
#define RMB_NEAR_CAS() RMB()
#define WMB_NEAR_CAS() WMB()
#define MB_NEAR_CAS()  MB()
#endif

typedef unsigned long int_addr_t;

typedef int bool_t;
#define FALSE 0
#define TRUE  1

#define ADD_TO(_v,_x)                                                   \
do {                                                                    \
    int __val = (_v), __newval;                                         \
    while ( (__newval = CASIO(&(_v),__val,__val+(_x))) != __val )       \
        __val = __newval;                                               \
} while ( 0 )

/*
 * Allow us to efficiently align and pad structures so that shared fields
 * don't cause contention on thread-local or read-only fields.
 */
#define CACHE_PAD(_n) char __pad ## _n [CACHE_LINE_SIZE]
#define ALIGNED_ALLOC(_s)                                       \
    ((void *)(((unsigned long)malloc((_s)+CACHE_LINE_SIZE*2) +  \
        CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1)))

/*
 * Interval counting
 */

typedef unsigned int interval_t;
#define get_interval(_i)                                                 \
do {                                                                     \
    interval_t _ni = interval;                                           \
    do { _i = _ni; } while ( (_ni = CASIO(&interval, _i, _i+1)) != _i ); \
} while ( 0 )

/*
 * POINTER MARKING
 */

#define get_marked_ref(_p)      ((void *)(((unsigned long)(_p)) | 1))
#define get_unmarked_ref(_p)    ((void *)(((unsigned long)(_p)) & ~1))
#define is_marked_ref(_p)       (((unsigned long)(_p)) & 1)


/*
 * SUPPORT FOR WEAK ORDERING OF MEMORY ACCESSES
 */

#ifdef WEAK_MEM_ORDER

#define MAYBE_GARBAGE (0)

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f)                                       \
do {                                                            \
    (_x) = (_f);                                                \
    if ( (_x) == MAYBE_GARBAGE ) { RMB(); (_x) = (_f); }        \
 while ( 0 )

#define WEAK_DEP_ORDER_RMB() RMB()
#define WEAK_DEP_ORDER_WMB() WMB()
#define WEAK_DEP_ORDER_MB()  MB()

#else

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f) ((_x) = (_f))

#define WEAK_DEP_ORDER_RMB() ((void)0)
#define WEAK_DEP_ORDER_WMB() ((void)0)
#define WEAK_DEP_ORDER_MB()  ((void)0)

#endif

/*
 * Strong LL/SC operations
 */

static _u32 strong_ll(_u64 *ptr, int p)
{
    _u64 val_read;
    _u64 new_val;
    _u64 flag;

    flag = (1LL << p);

    new_val = *ptr;
    do {
        val_read = new_val;
        new_val = val_read | flag;
    } while ( ((val_read & flag) == 0) &&
              ((new_val = CAS64O(ptr, val_read, new_val)) != val_read) );

    return (_u32) (val_read >> 32);
}

static int strong_vl(_u64 *ptr, int p) 
{
    _u64 val_read;
    _u64 flag;

    flag = (1LL << p);
    val_read = *ptr;

    return (val_read & flag);
}

static int strong_sc(_u64 *ptr, int p, _u32 n) 
{
    _u64 val_read;
    _u64 new_val;
    _u64 flag;

    flag = (1LL << p);
    val_read = *ptr;

    while ( (val_read & flag) != 0 )
    {
        new_val = (((_u64)n) << 32);

        if ( (new_val = CAS64O(ptr, val_read, new_val)) == val_read )
        {
            return 1;
        }

        val_read = new_val;
    }

    return 0;
}

static void s_store(_u64 *ptr, _u32 n) 
{
    _u64 new_val;

    new_val = (((_u64)n) << 32);
    *ptr = new_val;
}

static _u32 s_load(_u64 *ptr) 
{
    _u64 val_read;

    val_read = *ptr;
    return (val_read >> 32);
}


/*
 * MCS lock
 */

typedef struct qnode_t qnode_t;

struct qnode_t {
    qnode_t *next;
    int locked;
};

typedef struct {
    qnode_t *tail;
} mcs_lock_t;

static void mcs_init(mcs_lock_t *lock)
{
    lock->tail = NULL;
}

static void mcs_lock(mcs_lock_t *lock, qnode_t *qn)
{
    qnode_t *pred;

    qn->next = NULL;
    qn->locked = 1;
    WMB_NEAR_CAS();

    pred = FASPO(&lock->tail, qn);
    if ( pred != NULL )
    {
        pred->next = qn;
        while ( qn->locked ) RMB();
    }

    MB();
}

static void mcs_unlock(mcs_lock_t *lock, qnode_t *qn)
{
    qnode_t *t = qn->next;

    MB();

    if ( t == NULL )
    {
        if ( CASPO(&lock->tail, qn, NULL) == qn ) return;
        while ( (t = qn->next) == NULL ) RMB();
        WEAK_DEP_ORDER_MB();
    }

    t->locked = 0;
}


/*
 * MCS fair MRSW lock.
 */

typedef struct mrsw_qnode_st mrsw_qnode_t;

struct mrsw_qnode_st {
#define CLS_RD 0
#define CLS_WR 1
    int class;
#define ST_NOSUCC   0
#define ST_RDSUCC   1
#define ST_WRSUCC   2
#define ST_SUCCMASK 3
#define ST_BLOCKED  4
    int state;
    mrsw_qnode_t *next;
};

typedef struct {
    mrsw_qnode_t *tail;
    mrsw_qnode_t *next_writer;
    int reader_count;
} mrsw_lock_t;


#define CLEAR_BLOCKED(_qn) ADD_TO((_qn)->state, -ST_BLOCKED)

static void mrsw_init(mrsw_lock_t *lock)
{
    memset(lock, 0, sizeof(*lock));
}

static void rd_lock(mrsw_lock_t *lock, mrsw_qnode_t *qn)
{
    mrsw_qnode_t *pred, *next;

    qn->class = CLS_RD;
    qn->next  = NULL;
    qn->state = ST_NOSUCC | ST_BLOCKED;

    WMB_NEAR_CAS();

    pred = FASPO(&lock->tail, qn);
    
    if ( pred == NULL )
    {
        ADD_TO(lock->reader_count, 1);
        CLEAR_BLOCKED(qn);
    }
    else
    {
        if ( (pred->class == CLS_WR) ||
             (CASIO(&pred->state, ST_BLOCKED|ST_NOSUCC, ST_BLOCKED|ST_RDSUCC)
              == (ST_BLOCKED|ST_NOSUCC)) )
        {
            WEAK_DEP_ORDER_WMB();
            pred->next = qn;
            while ( (qn->state & ST_BLOCKED) ) RMB();
        }
        else
        {
            ADD_TO(lock->reader_count, 1);
            pred->next = qn;
            WEAK_DEP_ORDER_WMB();
            CLEAR_BLOCKED(qn);
        }
    }

    if ( qn->state == ST_RDSUCC )
    {
        while ( (next = qn->next) == NULL ) RMB();
        ADD_TO(lock->reader_count, 1);
        WEAK_DEP_ORDER_WMB();
        CLEAR_BLOCKED(next);
    }

    RMB();
}

static void rd_unlock(mrsw_lock_t *lock, mrsw_qnode_t *qn)
{
    mrsw_qnode_t *next = qn->next;
    int c, oc;

    RMB();

    if ( (next != NULL) || (CASPO(&lock->tail, qn, NULL) != qn) )
    {
        while ( (next = qn->next) == NULL ) RMB();
        if ( (qn->state & ST_SUCCMASK) == ST_WRSUCC )
        {
            lock->next_writer = next;
            WMB_NEAR_CAS(); /* set next_writer before dec'ing refcnt */
        }
    }

    /* Bounded to maximum # readers if no native atomic_decrement */
    c = lock->reader_count;
    while ( (oc = CASIO(&lock->reader_count, c, c-1)) != c ) c = oc;
   
    if ( c == 1 )
    {
        WEAK_DEP_ORDER_MB();
        if ( (next = lock->next_writer) != NULL )
        {
            RMB();
            if ( (lock->reader_count == 0) &&
                 (CASPO(&lock->next_writer, next, NULL) == next) )
            {
                WEAK_DEP_ORDER_WMB();
                CLEAR_BLOCKED(next);
            }
        }
    }
}

static void wr_lock(mrsw_lock_t *lock, mrsw_qnode_t *qn)
{
    mrsw_qnode_t *pred;
    int os, s;

    qn->class = CLS_WR;
    qn->next  = NULL;
    qn->state = ST_NOSUCC | ST_BLOCKED;

    WMB_NEAR_CAS();
    
    pred = FASPO(&lock->tail, qn);

    if ( pred == NULL )
    {
        WEAK_DEP_ORDER_WMB();
        lock->next_writer = qn;
        MB(); /* check reader_count after setting next_writer. */
        if ( (lock->reader_count == 0) &&
             (CASPO(&lock->next_writer, qn, NULL) == qn) )
        {
            CLEAR_BLOCKED(qn);
        }
    }
    else
    {
        s = pred->state;
        /* Bounded while loop: only one other remote update may occur. */
        while ( (os = CASIO(&pred->state, s, s | ST_WRSUCC)) != s ) s = os;
        WMB();
        pred->next = qn;
    }

    while ( (qn->state & ST_BLOCKED) ) RMB();

    MB();
}

static void wr_unlock(mrsw_lock_t *lock, mrsw_qnode_t *qn)
{
    mrsw_qnode_t *next = qn->next;

    MB();

    if ( (next != NULL) || (CASPO(&lock->tail, qn, NULL) != qn) )
    {
        while ( (next = qn->next) == NULL ) RMB();
        WEAK_DEP_ORDER_MB();
        if ( next->class == CLS_RD )
        {
            ADD_TO(lock->reader_count, 1);
            WMB();
        }
        CLEAR_BLOCKED(next);
    }
}


#endif /* __PORTABLE_DEFNS_H__ */
