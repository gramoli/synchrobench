#ifndef ATOMIC_H
#define ATOMIC_H 

#include <atomic_ops.h>

#define CAS(_m, _o, _n) \
    AO_compare_and_swap_full(((volatile AO_t*) _m), ((AO_t) _o), ((AO_t) _n))

#define FAI(a) AO_fetch_and_add_full((volatile AO_t*) (a), 1)
#define FAD(a) AO_fetch_and_add_full((volatile AO_t*) (a), -1)


#endif
