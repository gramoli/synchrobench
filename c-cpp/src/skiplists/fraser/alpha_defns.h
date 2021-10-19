#ifndef __ALPHA_DEFNS_H__
#define __ALPHA_DEFNS_H__

#include <c_asm.h>
#include <alpha/builtins.h>
#include <pthread.h>

#ifndef ALPHA
#define ALPHA
#endif

#define CACHE_LINE_SIZE 64


/*
 * I. Compare-and-swap, fetch-and-store.
 */

#define FAS32(_x,_n)  asm ( \
                "1:     ldl_l   %v0, 0(%a0);" \
                "       bis     %a1, 0, %t0;" \
                "       stl_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;", (_x), (_n))
#define FAS64(_x,_n)  asm ( \
                "1:     ldq_l   %v0, 0(%a0);" \
                "       bis     %a1, 0, %t0;" \
                "       stq_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;", (_x), (_n))
#define CAS32(_x,_o,_n)  asm ( \
                "1:     ldl_l   %v0, 0(%a0);" \
                "       cmpeq   %v0, %a1, %t0;" \
                "       beq     %t0, 3f;" \
                "       bis     %a2, 0, %t0;" \
                "       stl_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;" \
                "3:", (_x), (_o), (_n))
#define CAS64(_x,_o,_n)  asm ( \
                "1:     ldq_l   %v0, 0(%a0);" \
                "       cmpeq   %v0, %a1, %t0;" \
                "       beq     %t0, 3f;" \
                "       bis     %a2, 0, %t0;" \
                "       stq_c   %t0, 0(%a0);" \
                "       beq     %t0, 1b;" \
                "3:", (_x), (_o), (_n))
#define CAS(_x,_o,_n) ((sizeof (*_x) == 4)?CAS32(_x,_o,_n):CAS64(_x,_o,_n))
#define FAS(_x,_n)    ((sizeof (*_x) == 4)?FAS32(_x,_n)   :FAS64(_x,_n))
/* Update Integer location, return Old value. */
#define CASIO(_x,_o,_n) CAS(_x,_o,_n)
#define FASIO(_x,_n)    FAS(_x,_n)
/* Update Pointer location, return Old value. */
#define CASPO(_x,_o,_n) (void*)CAS((_x),(void*)(_o),(void*)(_n))
#define FASPO(_x,_n)    (void*)FAS((_x),(void*)(_n))
#define CAS32O CAS32
#define CAS64O CAS64

/*
 * II. Memory barriers. 
 *  WMB(): All preceding write operations must commit before any later writes.
 *  RMB(): All preceding read operations must commit before any later reads.
 *  MB():  All preceding memory accesses must commit before any later accesses.
 * 
 *  If the compiler does not observe these barriers (but any sane compiler
 *  will!), then VOLATILE should be defined as 'volatile'.
 */

#define MB()  asm("mb")
#define WMB() asm("wmb")
#define RMB() (MB())
#define VOLATILE /*volatile*/


/*
 * III. Cycle counter access.
 */

#include <sys/time.h>
typedef unsigned long tick_t;
#define RDTICK() asm("rpcc %v0")


/*
 * IV. Types.
 */

typedef unsigned char  _u8;
typedef unsigned short _u16;
typedef unsigned int   _u32;
typedef unsigned long  _u64;

#endif /* __ALPHA_DEFNS_H__ */
