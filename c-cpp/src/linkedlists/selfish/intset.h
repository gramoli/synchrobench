#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

typedef int val_t;
#define VAL_MIN INT_MIN
#define VAL_MAX INT_MAX

// These structs should be defined in <algo>.h
typedef struct node node_t;
typedef struct intset intset_t;

// These live in mixin.c, or <algo>.c if special initialisation/destruction
// is required.
intset_t *set_new(void);
void set_delete(intset_t *set);
int set_size(intset_t *set);
node_t *new_node(int val, node_t *next);
void set_print(intset_t *set);

// These live in <algo>.c
int set_contains(intset_t *set, int val);
int set_insert(intset_t *set, int val);
int set_remove(intset_t *set, int val);

// Locked operations
#ifdef MUTEX
typedef pthread_mutex_t ptlock_t;
#  define INIT_LOCK(lock)				pthread_mutex_init((pthread_mutex_t *) lock, NULL);
#  define DESTROY_LOCK(lock)			pthread_mutex_destroy((pthread_mutex_t *) lock)
#  define LOCK(lock)					pthread_mutex_lock((pthread_mutex_t *) lock)
#  define UNLOCK(lock)					pthread_mutex_unlock((pthread_mutex_t *) lock)
#else
typedef pthread_spinlock_t ptlock_t;
#  define INIT_LOCK(lock)				pthread_spin_init((pthread_spinlock_t *) lock, PTHREAD_PROCESS_PRIVATE);
#  define DESTROY_LOCK(lock)			pthread_spin_destroy((pthread_spinlock_t *) lock)
#  define LOCK(lock)					pthread_spin_lock((pthread_spinlock_t *) lock)
#  define UNLOCK(lock)					pthread_spin_unlock((pthread_spinlock_t *) lock)
#endif
