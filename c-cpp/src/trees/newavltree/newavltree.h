#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include <atomic_ops.h>

#include "tm.h"

#ifndef MICROBENCH

#include "types.h"

typedef struct rbtree rbtree_t;


#ifdef SEQAVL

#  include "stm.h"
#  include "mod_mem.h"
#  include "mod_stats.h"

#  define TM_ARG                        /* nothing */
#  define TM_ARG_ALONE                  /* nothing */
#  define TM_ARGDECL                    /* nothing */
#  define TM_ARGDECL_ALONE              /* nothing */
#  define TM_CALLABLE                   /* nothing */

#  define NL                             
#  define EL                            
#  define TX_START(type)                
#  define TX_END

#  define TM_SHARED_READ(addr)           (addr)
#  define TM_SHARED_WRITE(addr, val)     ((addr) = (val))


#else


#  include "stm.h"
#  include "mod_mem.h"
#  include "mod_stats.h"
#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 { sigjmp_buf *_e = stm_start(0); if (_e != NULL) sigsetjmp(*_e, 0);
#  define TX_LOAD(addr)                  stm_load((stm_word_t *)addr)
#  define TX_UNIT_LOAD(addr)             stm_unit_load((stm_word_t *)addr, NULL)
#  define UNIT_LOAD(addr)             non_stm_unit_load((stm_word_t *)addr, NULL)
#  define TX_UNIT_LOAD_TS(addr, timestamp)  stm_unit_load((stm_word_t *)addr, (stm_word_t *)timestamp)
#  define TX_STORE(addr, val)            stm_store((stm_word_t *)addr, (stm_word_t)val)
#  define TX_END stm_commit(); }
#  define FREE(addr, size)               stm_free(addr, size)
#  define MALLOC(size)                   stm_malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_THREAD_ENTER()              stm_init_thread()

#endif

#endif


#ifdef ESTM
#define TINY10B
#endif

#ifdef TINY10B

#ifdef MICROBENCH
#define SEPERATE_MAINTENANCE
#endif

#ifdef SEPERATE_MAINTENANCE
#ifndef SEQAVL
#define NON_UNIT
#endif
#endif

#endif

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#ifdef TWO_MAINTENANCE
#define DEFAULT_NB_MAINTENANCE_THREADS  2
#else
#define DEFAULT_NB_MAINTENANCE_THREADS  1
#endif
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY              4
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE               1
#define DEFAULT_MOVE                    0
#define DEFAULT_SNAPSHOT                0
#define DEFAULT_LOAD                    1

#ifndef MICROBENCH
#define KEYMAP
#define SEQUENTIAL
#endif

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define ATOMIC_CAS_MB(a, e, v)          (AO_compare_and_swap_full((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_FETCH_AND_INC_FULL(a)    (AO_fetch_and_add1_full((volatile AO_t *)(a)))

extern volatile AO_t stop;
extern unsigned int global_seed;
#ifdef TLS
extern __thread unsigned int *rng_seed;
#else /* ! TLS */
extern pthread_key_t rng_seed_key;
#endif /* ! TLS */
extern unsigned int levelmax;

#define TRANSACTIONAL                   d->unit_tx

#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX


#ifndef MICROBENCH
#define TMRBTREE_ALLOC()          TMrbtree_alloc(TM_ARG_ALONE)
#define TMRBTREE_FREE(r)          TMrbtree_free(TM_ARG  r)
#define TMRBTREE_INSERT(r, k, v)  TMrbtree_insert(TM_ARG  r, (void*)(k), (void*)(v))
#define TMRBTREE_DELETE(r, k)     TMrbtree_delete(TM_ARG  r, (void*)(k))
#define TMRBTREE_UPDATE(r, k, v)  TMrbtree_update(TM_ARG  r, (void*)(k), (void*)(v))
#define TMRBTREE_GET(r, k)        TMrbtree_get(TM_ARG  r, (void*)(k))
#define TMRBTREE_CONTAINS(r, k)   TMrbtree_contains(TM_ARG  r, (void*)(k))
#endif


typedef intptr_t val_t;
//#define val_t void*
typedef intptr_t level_t;


typedef struct avl_node {
  val_t key, val;
  intptr_t deleted;
  intptr_t removed;
  struct avl_node *left;
  struct avl_node *right;
  val_t lefth, righth, localh;
} avl_node_t;

typedef struct free_list_item_t {
  struct free_list_item_t *next;
  avl_node_t *to_free;
} free_list_item;

#ifdef REMOVE_LATER
typedef struct remove_list_item {
  struct remove_list_item *next;
  avl_node_t *parent, *item;
} remove_list_item_t;
#endif

typedef struct avl_intset {
  avl_node_t *root;
#ifdef SEPERATE_MAINTENANCE
  free_list_item **maint_list_start;
#endif
  free_list_item **t_free_list;
  free_list_item *free_list;

#ifdef REMOVE_LATER
  remove_list_item_t **to_remove_later;
#endif

  long nb_threads;
  ulong *t_nbtrans;
  ulong *t_nbtrans_old;
  volatile ulong *nb_committed;
  ulong *nb_committed_old;
  uint deleted_count;
  uint current_deleted_count;
  uint tree_size;
  uint current_tree_size;
  int active_remove;
  ulong next_maintenance;
  ulong nb_propogated, nb_suc_propogated, nb_rotated, nb_suc_rotated, nb_removed;
} avl_intset_t;


int floor_log_2(unsigned int n);

avl_node_t *avl_new_simple_node(val_t val, val_t key, int transactional);

void avl_delete_node(avl_node_t *node);
void avl_delete_node_free(avl_node_t *node, int transactional);

avl_intset_t *avl_set_new();
avl_intset_t *avl_set_new_alloc(int transactional, long nb_threads);

void avl_set_delete(avl_intset_t *set);
void avl_set_delete_node(avl_node_t *node);

int avl_set_size(avl_intset_t *set);
int avl_tree_size(avl_intset_t *set);
void avl_set_size_node(avl_node_t *node, int* size, int tree);

int avl_balance(avl_intset_t *set);
int avl_rec_depth(avl_node_t *node);



/******************************************************************************
 * The following are wrappers for STAMP
 ****************************************************************************/


#ifndef MICROBENCH

#ifdef KEYMAP

/* =============================================================================
 * rbtree_alloc
 * =============================================================================
 */
rbtree_t*
rbtree_alloc (long (*compare)(const void*, const void*), long nb_threads);


/* =============================================================================
 * TMrbtree_alloc
 * =============================================================================
 */
rbtree_t*
TMrbtree_alloc (TM_ARGDECL  long (*compare)(const void*, const void*));


/* =============================================================================
 * rbtree_free
 * =============================================================================
 */
void
rbtree_free (rbtree_t* r);


/* =============================================================================
 * TMrbtree_free
 * =============================================================================
 */
void
TMrbtree_free (TM_ARGDECL  rbtree_t* r);

#endif

#endif


/******************************************************************************
 * End STAMP wrappers
 ****************************************************************************/
