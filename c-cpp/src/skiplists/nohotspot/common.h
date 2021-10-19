/*
 * common definitions shared by all modules
 */
#ifndef COMMON_H_
#define COMMON_H_

#include <atomic_ops.h>

#define VOLATILE /* volatile */
#define BARRIER() asm volatile("" ::: "memory");

#define CAS(_m, _o, _n) \
    AO_compare_and_swap_full(((volatile AO_t*) _m), ((AO_t) _o), ((AO_t) _n))

#define FAI(a) AO_fetch_and_add_full((volatile AO_t*) (a), 1)
#define FAD(a) AO_fetch_and_add_full((volatile AO_t*) (a), -1)

/*
 * Allow us to efficiently align and pad structures so that shared fields
 * don't cause contention on thread-local or read-only fields.
 */
#define CACHE_PAD(_n) char __pad ## _n [CACHE_LINE_SIZE]
#define ALIGNED_ALLOC(_s)                                       \
    ((void *)(((unsigned long)malloc((_s)+CACHE_LINE_SIZE*2) +  \
        CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1)))

#define CACHE_LINE_SIZE 64



#endif /* COMMON_H_ */
