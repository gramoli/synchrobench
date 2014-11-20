/*
 * interface for garbage collection
 */

#ifndef GARBAGECOLL_H_
#define GARBAGECOLL_H_

/* comment out to disable garbage collection */
#define USE_GC

#include "ptst.h"

typedef struct gc_st gc_st;

/* Initialise GC section of per-thread state */
gc_st* gc_init(void);

int gc_add_allocator(int alloc_size);
void gc_remove_allocator(int alloc_id);

void* gc_alloc(ptst_t *ptst, int alloc_id);
void gc_free(ptst_t *ptst, void *p, int alloc_id);
void gc_free_unsafe(ptst_t *ptst, void *p, int alloc_id);

/* Hook registry - allows users to hook in their own epoch-delay lists */
typedef void (*gc_hookfn)(ptst_t*, void*);
int gc_add_hook(gc_hookfn hookfn);
void gc_remove_hook(int hookid);
void gc_hooklist_addptr(ptst_t *ptst, void *ptr, int hookid);

/* Per-thread entry/exit from critical regions */
void gc_enter(ptst_t* ptst);
void gc_exit(ptst_t* ptst);

/* Initialisation of GC */
void gc_subsystem_init(void);
void gc_subsystem_destroy(void);

#endif /* GARBAGECOLL_H_ */
