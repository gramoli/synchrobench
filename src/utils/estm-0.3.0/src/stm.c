/*
 * File:
 *   stm.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   STM functions.
 *
 * Copyright (c) 2007-2009.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sched.h>

#include "stm.h"
#include "atomic.h"
#include "gc.h"

/* ################################################################### *
 * DEFINES
 * ################################################################### */

#define COMPILE_TIME_ASSERT(pred)       switch (0) { case 0: case pred: ; }

/* Designs */
#define WRITE_BACK_ETL                  0
#define WRITE_BACK_CTL                  1
#define WRITE_THROUGH                   2

#define EL				0
#define NL				1

#ifdef EXPLICIT_TX_PARAMETER
# define TX_RETURN                      return tx
# define TX_GET                         /* Nothing */
#else /* ! EXPLICIT_TX_PARAMETER */
# define TX_RETURN                      /* Nothing */
# define TX_GET                         stm_tx_t *tx = stm_get_tx()
#endif /* ! EXPLICIT_TX_PARAMETER */

# define IO_FLUSH
# define PRINT_DEBUG(...)
# define PRINT_DEBUG2(...)

#ifndef RW_SET_SIZE
# define RW_SET_SIZE                    4096                /* Initial size of read/write sets */
#endif /* ! RW_SET_SIZE */

#ifndef LOCK_ARRAY_LOG_SIZE
# define LOCK_ARRAY_LOG_SIZE            20                  /* Size of lock array: 2^20 = 1M */
#endif /* LOCK_ARRAY_LOG_SIZE */

#ifndef LOCK_SHIFT_EXTRA
# define LOCK_SHIFT_EXTRA               2                   /* 2 extra shift */
#endif /* LOCK_SHIFT_EXTRA */

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * TYPES
 * ################################################################### */

enum {                                  /* Transaction status */
  TX_IDLE = 0,
  TX_ACTIVE = 1,
  TX_COMMITTED = 2,
  TX_ABORTED = 3
};

typedef struct r_entry {                /* Read set entry */
  stm_word_t version;                   /* Version read */
  volatile stm_word_t *lock;            /* Pointer to lock (for fast access) */
} r_entry_t;

typedef struct r_set {                  /* Read set */
  r_entry_t *entries;                   /* Array of entries */
  int nb_entries;                       /* Number of entries */
  int size;                             /* Size of array */
} r_set_t;

typedef struct w_entry {                /* Write set entry */
  union {                               /* For padding... */
    struct {
      volatile stm_word_t *addr;        /* Address written */
      stm_word_t value;                 /* New (write-back) or old (write-through) value */
      stm_word_t mask;                  /* Write mask */
      stm_word_t version;               /* Version overwritten */
      volatile stm_word_t *lock;        /* Pointer to lock (for fast access) */
      struct w_entry *next;             /* Next address covered by same lock (if any) */
    };
  };
} w_entry_t;

typedef struct w_set {                  /* Write set */
  w_entry_t *entries;                   /* Array of entries */
  int nb_entries;                       /* Number of entries */
  int size;                             /* Size of array */
  int reallocate;                       /* Reallocate on next start */
} w_set_t;

#define ELASTICITY                      2
#ifndef MAX_SPECIFIC
# define MAX_SPECIFIC                   16
#endif /* MAX_SPECIFIC */

typedef struct stm_tx {                 /* Transaction descriptor */
  stm_tx_attr_t *attr;                  /* Transaction attributes (user-specified) */
  stm_word_t status;                    /* Transaction status (not read by other threads) */
  stm_word_t start;                     /* Start timestamp */
  stm_word_t end;                       /* End timestamp (validity range) */
  r_set_t r_set;                        /* Read set */
  w_set_t w_set;                        /* Write set */
  sigjmp_buf env;                       /* Environment for setjmp/longjmp */
  sigjmp_buf *jmp;                      /* Pointer to environment (NULL when not using setjmp/longjmp) */
  int nesting;                          /* Nesting level */
  int ro;                               /* Is this execution read-only? */
  int can_extend;                       /* Can this transaction be extended? */
  void *data[MAX_SPECIFIC];             /* Transaction-specific data (fixed-size array for better speed) */
  unsigned long retries;                /* Number of consecutive aborts (retries) */
  unsigned long aborts;                 /* Total number of aborts (cumulative) */
  unsigned long aborts_ro;              /* Aborts due to wrong read-only specification (cumulative) */
  unsigned long aborts_locked_read;     /* Aborts due to trying to read when locked (cumulative) */
  unsigned long aborts_locked_write;    /* Aborts due to trying to write when locked (cumulative) */
  unsigned long aborts_validate_read;   /* Aborts due to failed validation upon read (cumulative) */
  unsigned long aborts_validate_write;  /* Aborts due to failed validation upon write (cumulative) */
  unsigned long aborts_validate_commit; /* Aborts due to failed validation upon commit (cumulative) */
  unsigned long aborts_invalid_memory;  /* Aborts due to invalid memory access (cumulative) */
  unsigned long aborts_double_write;    /* Aborts due to impossible cut of elastic tx */ 
  unsigned long aborts_reallocate;      /* Aborts due to write set reallocation (cumulative) */
  unsigned long aborts_rollover;        /* Aborts due to clock rolling over (cumulative) */
  unsigned long max_retries;            /* Maximum number of consecutive aborts (retries) */
  int type;				/* Is this transaction normal (NL=1) or elastic (EL=0)? */
  stm_word_t *lastraddr[ELASTICITY];	/* Elastic rotating buffer, keep track of the last read values (if elastic) */
  int marker;                           /* Marker for the elastic rotating buffer */
} stm_tx_t;

static int nb_specific = 0;             /* Number of specific slots used (<= MAX_SPECIFIC) */

/*
 * Transaction nesting is supported in a minimalist way (flat nesting):
 * - When a transaction is started in the context of another
 *   transaction, we simply increment a nesting counter but do not
 *   actually start a new transaction.
 * - The environment to be used for setjmp/longjmp is only returned when
 *   no transaction is active so that it is not overwritten by nested
 *   transactions. This allows for composability as the caller does not
 *   need to know whether it executes inside another transaction.
 * - The commit of a nested transaction simply decrements the nesting
 *   counter. Only the commit of the top-level transaction will actually
 *   carry through updates to shared memory.
 * - An abort of a nested transaction will rollback the top-level
 *   transaction and reset the nesting counter. The call to longjmp will
 *   restart execution before the top-level transaction.
 * Using nested transactions without setjmp/longjmp is not recommended
 * as one would need to explicitly jump back outside of the top-level
 * transaction upon abort of a nested transaction. This breaks
 * composability.
 */

/*
 * Reading from the previous version of locked addresses is implemented
 * by peeking into the write set of the transaction that owns the
 * lock. Each transaction has a unique identifier, updated even upon
 * retry. A special "commit" bit of this identifier is set upon commit,
 * right before writing the values from the redo log to shared memory. A
 * transaction can read a locked address if the identifier of the owner
 * does not change between before and after reading the value and
 * version, and it does not have the commit bit set.
 */

/* ################################################################### *
 * CALLBACKS
 * ################################################################### */

typedef struct cb_entry {               /* Callback entry */
  void (*f)(TXPARAMS void *);           /* Function */
  void *arg;                            /* Argument to be passed to function */
} cb_entry_t;

#define MAX_CB                          16

/* Declare as static arrays (vs. lists) to improve cache locality */
static cb_entry_t init_cb[MAX_CB];      /* Init thread callbacks */
static cb_entry_t exit_cb[MAX_CB];      /* Exit thread callbacks */
static cb_entry_t start_cb[MAX_CB];     /* Start callbacks */
static cb_entry_t commit_cb[MAX_CB];    /* Commit callbacks */
static cb_entry_t abort_cb[MAX_CB];     /* Abort callbacks */

static int nb_init_cb = 0;
static int nb_exit_cb = 0;
static int nb_start_cb = 0;
static int nb_commit_cb = 0;
static int nb_abort_cb = 0;

/* ################################################################### *
 * THREAD-LOCAL
 * ################################################################### */

#ifdef TLS
static __thread stm_tx_t* thread_tx;
#else /* ! TLS */
static pthread_key_t thread_tx;
#endif /* ! TLS */

/* ################################################################### *
 * LOCKS
 * ################################################################### */

/*
 * A lock is a unsigned int of the size of a pointer.
 * The LSB is the lock bit. If it is set, this means:
 * - At least some covered memory addresses is being written.
 * - Write-back (ETL): all bits of the lock apart from the lock bit form
 *   a pointer that points to the write log entry holding the new
 *   value. Multiple values covered by the same log entry and orginized
 *   in a linked list in the write log.
 * - Write-through and write-back (CTL): all bits of the lock apart from
 *   the lock bit form a pointer that points to the transaction
 *   descriptor containing the write-set.
 * If the lock bit is not set, then:
 * - All covered memory addresses contain consistent values.
 * - Write-back (ETL and CTL): all bits of the lock besides the lock bit
 *   contain a version number (timestamp).
 * - Write-through: all bits of the lock besides the lock bit contain a
 *   version number.
 *   - The high order bits contain the commit time.
 *   - The low order bits contain an incarnation number (incremented
 *     upon abort while writing the covered memory addresses).
 * When using the PRIORITY contention manager, the format of locks is
 * slightly different. It is documented elsewhere.
 */

#define OWNED_MASK                      0x01                /* 1 bit */
#define VERSION_MAX                    (~(stm_word_t)0 >> 1)

#define LOCK_GET_OWNED(l)               (l & OWNED_MASK)
#define LOCK_SET_ADDR(a)               (a | OWNED_MASK)    /* OWNED bit set */
#define LOCK_GET_ADDR(l)               (l & ~(stm_word_t)OWNED_MASK)
#define LOCK_GET_TIMESTAMP(l)          (l >> 1)            /* Logical shift (unsigned) */
#define LOCK_SET_TIMESTAMP(t)          (t << 1)            /* OWNED bit not set */
#define LOCK_UNIT                       (~(stm_word_t)0)

/*
 * We use the very same hash functions as TL2 for degenerate Bloom
 * filters on 32 bits.
 */

/*
 * We use an array of locks and hash the address to find the location of the lock.
 * We try to avoid collisions as much as possible (two addresses covered by the same lock).
 */
#define LOCK_ARRAY_SIZE                 (1 << LOCK_ARRAY_LOG_SIZE)
#define LOCK_MASK                       (LOCK_ARRAY_SIZE - 1)
#define LOCK_SHIFT                      (((sizeof(stm_word_t) == 4) ? 2 : 3) + LOCK_SHIFT_EXTRA)
#define LOCK_IDX(a)                     (((stm_word_t)(a) >> LOCK_SHIFT) & LOCK_MASK)
#define GET_LOCK(a)                    (locks + LOCK_IDX(a))

static volatile stm_word_t locks[LOCK_ARRAY_SIZE];

/* ################################################################### *
 * CLOCK
 * ################################################################### */

/* At least twice a cache line (512 bytes to be on the safe side) */
static volatile stm_word_t gclock[1024 / sizeof(stm_word_t)];
#define CLOCK                          (gclock[512 / sizeof(stm_word_t)])

#define GET_CLOCK                       (ATOMIC_LOAD_ACQ(&CLOCK))
#define FETCH_INC_CLOCK                 (ATOMIC_FETCH_INC_FULL(&CLOCK))

/* ################################################################### *
 * STATIC
 * ################################################################### */

/*
 * Returns the transaction descriptor for the CURRENT thread.
 */
static inline stm_tx_t *stm_get_tx()
{
#ifdef TLS
  return thread_tx;
#else /* ! TLS */
  return (stm_tx_t *)pthread_getspecific(thread_tx);
#endif /* ! TLS */
}


/*
 * We use a simple approach for clock roll-over:
 * - We maintain the count of (active) transactions using a counter
 *   protected by a mutex. This approach is not very efficient but the
 *   cost is quickly amortized because we only modify the counter when
 *   creating and deleting a transaction descriptor, which typically
 *   happens much less often than starting and committing a transaction.
 * - We detect overflows when reading the clock or when incrementing it.
 *   Upon overflow, we wait until all threads have blocked on a barrier.
 * - Threads can block on the barrier upon overflow when they (1) start
 *   a transaction, or (2) delete a transaction. This means that threads
 *   must ensure that they properly delete their transaction descriptor
 *   before performing any blocking operation outside of a transaction
 *   in order to guarantee liveness (our model prohibits blocking
 *   inside a transaction).
 */

pthread_mutex_t tx_count_mutex;
pthread_cond_t tx_reset;
int tx_count;
int tx_overflow;

/*
 * Enter new transactional thread.
 */
static inline void stm_rollover_enter(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_rollover_enter(%p)\n", tx);

  pthread_mutex_lock(&tx_count_mutex);
  while (tx_overflow != 0)
    pthread_cond_wait(&tx_reset, &tx_count_mutex);
  /* One more (active) transaction */
  tx_count++;
  pthread_mutex_unlock(&tx_count_mutex);
}

/*
 * Exit transactional thread.
 */
static inline void stm_rollover_exit(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_rollover_exit(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  pthread_mutex_lock(&tx_count_mutex);
  /* One less (active) transaction */
  tx_count--;
  assert(tx_count >= 0);
  /* Are all transactions stopped? */
  if (tx_overflow != 0 && tx_count == 0) {
    /* Yes: reset clock */
    memset((void *)locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));
    CLOCK = 0;
    tx_overflow = 0;
    /* Reset GC */
    gc_reset();
    /* Wake up all thread */
    pthread_cond_broadcast(&tx_reset);
  }
  pthread_mutex_unlock(&tx_count_mutex);
}

/*
 * Clock overflow.
 */
static inline void stm_overflow(stm_tx_t *tx)
{
  PRINT_DEBUG("==> stm_overflow(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  pthread_mutex_lock(&tx_count_mutex);
  /* Set overflow flag (might already be set) */
  tx_overflow = 1;
  /* One less (active) transaction */
  tx_count--;
  assert(tx_count >= 0);
  /* Are all transactions stopped? */
  if (tx_count == 0) {
    /* Yes: reset clock */
    memset((void *)locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));
    CLOCK = 0;
    tx_overflow = 0;
    /* Reset GC */
    gc_reset();
    /* Wake up all thread */
    pthread_cond_broadcast(&tx_reset);
  } else {
    /* No: wait for other transactions to stop */
    pthread_cond_wait(&tx_reset, &tx_count_mutex);
  }
  /* One more (active) transaction */
  tx_count++;
  pthread_mutex_unlock(&tx_count_mutex);
}

/*
 * Check if stripe has been read previously.
 */
static inline r_entry_t *stm_has_read(stm_tx_t *tx, volatile stm_word_t *lock)
{
  r_entry_t *r;
  int i;

  PRINT_DEBUG("==> stm_has_read(%p[%lu-%lu],%p)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, lock);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Look for read */
  r = tx->r_set.entries;
  for (i = tx->r_set.nb_entries; i > 0; i--, r++) {
    if (r->lock == lock) {
      /* Return first match*/
      return r;
    }
  }
  return NULL;
}

/*
 * (Re)allocate read set entries.
 */
static inline void stm_allocate_rs_entries(stm_tx_t *tx, int extend)
{
  if (extend) {
    /* Extend read set */
    tx->r_set.size *= 2;
    PRINT_DEBUG2("==> reallocate read set (%p[%lu-%lu],%d)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, tx->r_set.size);
    if ((tx->r_set.entries = (r_entry_t *)realloc(tx->r_set.entries, tx->r_set.size * sizeof(r_entry_t))) == NULL) {
      perror("realloc");
      exit(1);
    }
  } else {
    /* Allocate read set */
    if ((tx->r_set.entries = (r_entry_t *)malloc(tx->r_set.size * sizeof(r_entry_t))) == NULL) {
      perror("malloc");
      exit(1);
    }
  }
}

/*
 * (Re)allocate write set entries.
 */
static inline void stm_allocate_ws_entries(stm_tx_t *tx, int extend)
{

  if (extend) {
    /* Extend write set */
    tx->w_set.size *= 2;
    PRINT_DEBUG("==> reallocate write set (%p[%lu-%lu],%d)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, tx->w_set.size);
    if ((tx->w_set.entries = (w_entry_t *)realloc(tx->w_set.entries, tx->w_set.size * sizeof(w_entry_t))) == NULL) {
      perror("realloc");
      exit(1);
    }
  } else {
    /* Allocate write set */
    if ((tx->w_set.entries = (w_entry_t *)malloc(tx->w_set.size * sizeof(w_entry_t))) == NULL) {
      perror("malloc");
      exit(1);
    }
  }

}

/*
 * Validate read set (check if all read addresses are still valid now).
 */
static inline int stm_validate(stm_tx_t *tx)
{
  r_entry_t *r;
  int i;
  stm_word_t l;

  PRINT_DEBUG("==> stm_validate(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Validate reads */
  r = tx->r_set.entries;
  for (i = tx->r_set.nb_entries; i > 0; i--, r++) {
    /* Read lock */
    l = ATOMIC_LOAD(r->lock);
    /* Unlocked and still the same version? */
    if (LOCK_GET_OWNED(l)) {
      /* Do we own the lock? */
      w_entry_t *w = (w_entry_t *)LOCK_GET_ADDR(l);
      /* Simply check if address falls inside our write set (avoids non-faulting load) */
      if (!(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries))
	{
	  /* Locked by another transaction: cannot validate */
	  return 0;
	}
      /* We own the lock: OK */
    } else {
      if (LOCK_GET_TIMESTAMP(l) != r->version) {
        /* Other version: cannot validate */
        return 0;
      }
      /* Same version: OK */
    }
  }
  return 1;
}

/*
 * Extend snapshot range.
 */
static inline int stm_extend(stm_tx_t *tx)
{
  stm_word_t now;

  PRINT_DEBUG("==> stm_extend(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Get current time */
  now = GET_CLOCK;
  if (now >= VERSION_MAX) {
    /* Clock overflow */
    return 0;
  }
  /* Try to validate read set */
  if (stm_validate(tx)) {
    /* It works: we can extend until now */
    tx->end = now;
    return 1;
  }
  return 0;
}

/*
 * Rollback transaction.
 */
static inline void stm_rollback(stm_tx_t *tx)
{
  w_entry_t *w;
  int i;

  PRINT_DEBUG("==> stm_rollback(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Drop locks */
  i = tx->w_set.nb_entries;
  if (i > 0) {
    w = tx->w_set.entries;
    for (; i > 0; i--, w++) {
      if (w->next == NULL) {
        /* Only drop lock for last covered address in write set */
        ATOMIC_STORE(w->lock, LOCK_SET_TIMESTAMP(w->version));
      }
      PRINT_DEBUG2("==> discard(t=%p[%lu-%lu],a=%p,d=%p-%lu,v=%lu)\n",
                   tx, (unsigned long)tx->start, (unsigned long)tx->end, w->addr, (void *)w->value, (unsigned long)w->value, (unsigned long)w->version);
    }
    /* Make sure that all lock releases become visible */
    ATOMIC_MB_WRITE;
  }

  tx->retries++;
  tx->aborts++;
  if (tx->max_retries < tx->retries)
    tx->max_retries = tx->retries;

  /* Callbacks */
  if (nb_abort_cb != 0) {
    int cb;
    for (cb = 0; cb < nb_abort_cb; cb++)
      abort_cb[cb].f(TXARGS abort_cb[cb].arg);
  }

  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_ABORTED;

  /* Reset nesting level */
  tx->nesting = 0;


  /* Jump back to transaction start */
  if (tx->jmp != NULL)
    siglongjmp(*tx->jmp, 1);
}

/*
 * Store a word-sized value (return write set entry or NULL).
 */
static inline w_entry_t *stm_write(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  volatile stm_word_t *lock;
  stm_word_t l, version;
  w_entry_t *w;
  w_entry_t *prev = NULL;

  PRINT_DEBUG2("==> stm_write(t=%p[%lu-%lu],a=%p,d=%p-%lu,m=0x%lx)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, (void *)value, (unsigned long)value, (unsigned long)mask);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  if (tx->ro) {
    /* Disable read-only and abort */
    assert(tx->attr != NULL);
    tx->attr->ro = 0;
    tx->aborts_ro++;
    stm_rollback(tx);
    return NULL;
  }

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Try to acquire lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
  if (LOCK_GET_OWNED(l)) {
    /* Locked */
    /* Do we own the lock? */
    w = (w_entry_t *)LOCK_GET_ADDR(l);
    /* Simply check if address falls inside our write set (avoids non-faulting load) */
    if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries) {
      /* Yes */
      prev = w;
      /* Did we previously write the same address? */
      while (1) {
        if (addr == prev->addr) {
          if (mask == 0)
            return prev;
          /* No need to add to write set */
          PRINT_DEBUG2("==> stm_write(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu,m=0x%lx)\n",
                       tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value, (unsigned long)mask);
          if (mask != ~(stm_word_t)0) {
            if (prev->mask == 0)
              prev->value = ATOMIC_LOAD(addr);
            value = (prev->value & ~mask) | (value & mask);
          }
          prev->value = value;
          prev->mask |= mask;
          return prev;
        }
        if (prev->next == NULL) {
          /* Remember last entry in linked list (for adding new entry) */
          break;
        }
        prev = prev->next;
      }
      /* Get version from previous write set entry (all entries in linked list have same version) */
      version = prev->version;
      /* Must add to write set */
      if (tx->w_set.nb_entries == tx->w_set.size) {
        /* Extend write set (invalidate pointers to write set entries => abort and reallocate) */
        tx->w_set.size *= 2;
        tx->w_set.reallocate = 1;
        tx->aborts_reallocate++;
        stm_rollback(tx);
        return NULL;
      }
      w = &tx->w_set.entries[tx->w_set.nb_entries];
      goto do_write;
    }
    /* Abort */
    tx->aborts_locked_write++;
    stm_rollback(tx);
    return NULL;
  } else {
    /* Not locked */
    /* Handle write after reads (before CAS) */
    version = LOCK_GET_TIMESTAMP(l);
    if (version > tx->end) {
      /* We might have read an older version previously */
      if (!tx->can_extend || stm_has_read(tx, lock) != NULL) {
        /* Read version must be older (otherwise, tx->end >= version) */
        /* Not much we can do: abort */
        tx->aborts_validate_write++;
        stm_rollback(tx);
        return NULL;
      }
    }
    /* Acquire lock (ETL) */
    if (tx->w_set.nb_entries == tx->w_set.size) {
      /* Extend write set (invalidate pointers to write set entries => abort and reallocate) */
      tx->w_set.size *= 2;
      tx->w_set.reallocate = 1;
      tx->aborts_reallocate++;
      stm_rollback(tx);
      return NULL;
    }
    w = &tx->w_set.entries[tx->w_set.nb_entries];
    if (ATOMIC_CAS_FULL(lock, l, LOCK_SET_ADDR((stm_word_t)w)) == 0)
      goto restart;
  }
  /* We own the lock here (ETL) */
 do_write:
  PRINT_DEBUG2("==> stm_write(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu,m=0x%lx)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value, (unsigned long)mask);

  /* Add address to write set */
  w->addr = addr;
  w->mask = mask;
  w->lock = lock;
  if (mask == 0) {
    /* Do not write anything */
#ifndef NDEBUG
    w->value = 0;
#endif /* ! NDEBUG */
  } else
    {
      /* Remember new value */
      if (mask != ~(stm_word_t)0)
	value = (ATOMIC_LOAD(addr) & ~mask) | (value & mask);
      w->value = value;
    }
  w->version = version;
  w->next = NULL;
  if (prev != NULL) {
    /* Link new entry in list */
    prev->next = w;
  }
  tx->w_set.nb_entries++;

  return w;
}



/*
 * Catch signal (to emulate non-faulting load).
 */
static void signal_catcher(int sig)
{
  stm_tx_t *tx = stm_get_tx();

  /* A fault might only occur upon a load concurrent with a free (read-after-free) */
  PRINT_DEBUG("Caught signal: %d\n", sig);

  if (tx == NULL || tx->jmp == NULL) {
    /* There is not much we can do: execution will restart at faulty load */
    fprintf(stderr, "Error: invalid memory accessed and no longjmp destination\n");
    exit(1);
  }

  tx->aborts_invalid_memory++;
  /* Will cause a longjmp */
  stm_rollback(tx);
}

/* ################################################################### *
 * STM FUNCTIONS
 * ################################################################### */

/*
 * Called once (from main) to initialize STM infrastructure.
 */
void stm_init()
{
  struct sigaction act;

  PRINT_DEBUG("==> stm_init()\n");

  PRINT_DEBUG("\tsizeof(word)=%d\n", (int)sizeof(stm_word_t));

  PRINT_DEBUG("\tVERSION_MAX=0x%lx\n", (unsigned long)VERSION_MAX);

  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(void *));
  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(atomic_t));

  gc_init(stm_get_clock);

  memset((void *)locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));


  CLOCK = 0;
  if (pthread_mutex_init(&tx_count_mutex, NULL) != 0) {
    fprintf(stderr, "Error creating mutex\n");
    exit(1);
  }
  if (pthread_cond_init(&tx_reset, NULL) != 0) {
    fprintf(stderr, "Error creating condition variable\n");
    exit(1);
  }
  tx_count = 0;
  tx_overflow = 0;

#ifndef TLS
  if (pthread_key_create(&thread_tx, NULL) != 0) {
    fprintf(stderr, "Error creating thread local\n");
    exit(1);
  }
#endif /* ! TLS */

  /* Catch signals for non-faulting load */
  act.sa_handler = signal_catcher;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGBUS, &act, NULL) < 0 || sigaction(SIGSEGV, &act, NULL) < 0) {
    perror("sigaction");
    exit(1);
  }

}

/*
 * Called once (from main) to clean up STM infrastructure.
 */
void stm_exit()
{
  PRINT_DEBUG("==> stm_exit()\n");

#ifndef TLS
  pthread_key_delete(thread_tx);
#endif /* ! TLS */
  pthread_cond_destroy(&tx_reset);
  pthread_mutex_destroy(&tx_count_mutex);

  gc_exit();
}

/*
 * Called by the CURRENT thread to initialize thread-local STM data.
 */
TXTYPE stm_init_thread()
{
  stm_tx_t *tx;

  PRINT_DEBUG("==> stm_init_thread()\n");

  gc_init_thread();

  /* Allocate descriptor */
  if ((tx = (stm_tx_t *)malloc(sizeof(stm_tx_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_IDLE;
  /* Read set */
  tx->r_set.nb_entries = 0;
  tx->r_set.size = RW_SET_SIZE;
  stm_allocate_rs_entries(tx, 0);
  /* Write set */
  tx->w_set.nb_entries = 0;
  tx->w_set.size = RW_SET_SIZE;
  tx->w_set.reallocate = 0;
  stm_allocate_ws_entries(tx, 0);
  /* Nesting level */
  tx->nesting = 0;
  /* Transaction-specific data */
  memset(tx->data, 0, MAX_SPECIFIC * sizeof(void *));
  tx->retries = 0;
  /* Statistics */
  tx->aborts = 0;
  tx->aborts_ro = 0;
  tx->aborts_locked_read = 0;
  tx->aborts_locked_write = 0;
  tx->aborts_validate_read = 0;
  tx->aborts_validate_write = 0;
  tx->aborts_validate_commit = 0;
  tx->aborts_invalid_memory = 0;
  tx->aborts_reallocate = 0;
  tx->aborts_rollover = 0;
  tx->max_retries = 0;
  /* Store as thread-local data */
#ifdef TLS
  thread_tx = tx;
#else /* ! TLS */
  pthread_setspecific(thread_tx, tx);
#endif /* ! TLS */
  stm_rollover_enter(tx);

  /* Callbacks */
  if (nb_init_cb != 0) {
    int cb;
    for (cb = 0; cb < nb_init_cb; cb++)
      init_cb[cb].f(TXARGS init_cb[cb].arg);
  }

  PRINT_DEBUG("==> %p\n", tx);

  TX_RETURN;
}

/*
 * Called by the CURRENT thread to cleanup thread-local STM data.
 */
void stm_exit_thread(TXPARAM)
{
  stm_word_t t;
  TX_GET;

  PRINT_DEBUG("==> stm_exit_thread(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Callbacks */
  if (nb_exit_cb != 0) {
    int cb;
    for (cb = 0; cb < nb_exit_cb; cb++)
      exit_cb[cb].f(TXARGS exit_cb[cb].arg);
  }

  stm_rollover_exit(tx);

  t = GET_CLOCK;
  gc_free(tx->r_set.entries, t);
  gc_free(tx->w_set.entries, t);
  gc_free(tx, t);
  gc_exit_thread();
}

/*
 * Called by the CURRENT thread to start an elastic transaction.
 */
static inline void stm_elastic_start(TXPARAMS sigjmp_buf *env, stm_tx_attr_t *attr)
{
  TX_GET;
  tx->type = EL;
  
  memset(tx->lastraddr, 0, ELASTICITY * sizeof(void *));
  tx->marker = 0;
  	
  PRINT_DEBUG("==> stm_elastic_start(%p)\n", tx);
	
  /* Increment nesting level */
  if (tx->nesting++ > 0)
    return;
	
  /* Use setjmp/longjmp? */
  tx->jmp = env;
  /* Attributes */
  tx->attr = attr;
  tx->ro = (attr == NULL ? 0 : attr->ro);
  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_ACTIVE;
 start:
  /* Start timestamp */
  tx->start = tx->end = GET_CLOCK; /* OPT: Could be delayed until first read/write */
  /* Disallow extensions in elastic transactions */
  tx->can_extend = 0;
  if (tx->start >= VERSION_MAX) {
    /* Overflow: we must reset clock */
    stm_overflow(tx);
    goto start;
  }
  /* Read/write set */
  if (tx->w_set.reallocate) {
    /* Don't need to copy the content from the previous write set */
    gc_free(tx->w_set.entries, tx->start);
    stm_allocate_ws_entries(tx, 0);
    tx->w_set.reallocate = 0;
  }
  tx->w_set.nb_entries = 0;
  tx->r_set.nb_entries = 0;
	
  gc_set_epoch(tx->start);
	
  /* Callbacks */
  if (nb_start_cb != 0) {
    int cb;
    for (cb = 0; cb < nb_start_cb; cb++)
      start_cb[cb].f(TXARGS start_cb[cb].arg);
  }
}

/*
 * Called by the CURRENT thread to start a transaction.
 */
static inline void stm_normal_start(TXPARAMS sigjmp_buf *env, stm_tx_attr_t *attr)
{
  TX_GET;
  tx->type = NL;

  PRINT_DEBUG("==> stm_normal_start(%p)\n", tx);

  /* Increment nesting level */
  if (tx->nesting++ > 0)
    return;

  /* Use setjmp/longjmp? */
  tx->jmp = env;
  /* Attributes */
  tx->attr = attr;
  tx->ro = (attr == NULL ? 0 : attr->ro);
  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_ACTIVE;
 start:
  /* Start timestamp */
  tx->start = tx->end = GET_CLOCK; /* OPT: Could be delayed until first read/write */
  /* Allow extensions */
  tx->can_extend = 1;
  if (tx->start >= VERSION_MAX) {
    /* Overflow: we must reset clock */
    stm_overflow(tx);
    goto start;
  }
  /* Read/write set */
  if (tx->w_set.reallocate) {
    /* Don't need to copy the content from the previous write set */
    gc_free(tx->w_set.entries, tx->start);
    stm_allocate_ws_entries(tx, 0);
    tx->w_set.reallocate = 0;
  }
  tx->w_set.nb_entries = 0;
  tx->r_set.nb_entries = 0;

  gc_set_epoch(tx->start);

  /* Callbacks */
  if (nb_start_cb != 0) {
    int cb;
    for (cb = 0; cb < nb_start_cb; cb++)
      start_cb[cb].f(TXARGS start_cb[cb].arg);
  }
}

void stm_start(TXPARAMS sigjmp_buf *env, stm_tx_attr_t *attr, int type) {
  if (type == NL) stm_normal_start(env, attr);
  else stm_elastic_start(env, attr);
}

/*
 * Called by the CURRENT thread to commit an elastic transaction.
 */
static inline int stm_elastic_commit(TXPARAM)
{
  TX_GET;
	
  PRINT_DEBUG("==> stm_elastic_commit(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);
	
  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Decrement nesting level */
  if (--tx->nesting > 0)
    return 1;
	
  tx->retries = 0;
		
  /* Callbacks */
  if (nb_commit_cb != 0) {
    int cb;
    for (cb = 0; cb < nb_commit_cb; cb++)
      commit_cb[cb].f(TXARGS commit_cb[cb].arg);
  }
	
  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_COMMITTED;
	
  return 1;
}

/*
 * Called by the CURRENT thread to commit a transaction.
 */
static inline int stm_normal_commit(TXPARAM)
{
  w_entry_t *w;
  stm_word_t t;
  int i;
  TX_GET;

  PRINT_DEBUG("==> stm_normal_commit(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Decrement nesting level */
  if (--tx->nesting > 0)
    return 1;

  if (tx->w_set.nb_entries > 0) {
    /* Update transaction */


    /* Get commit timestamp */
    t = FETCH_INC_CLOCK + 1;
    if (t >= VERSION_MAX) {
      /* Abort: will reset the clock on next transaction start or delete */
      tx->aborts_rollover++;
      stm_rollback(tx);
      return 0;
    }

    /* Try to validate (only if a concurrent transaction has committed since tx->start) */
    if (tx->start != t - 1 && !stm_validate(tx)) {
      /* Cannot commit */
      tx->aborts_validate_commit++;
      stm_rollback(tx);
      return 0;
    }

    /* Install new versions, drop locks and set new timestamp */
    w = tx->w_set.entries;
    for (i = tx->w_set.nb_entries; i > 0; i--, w++) {
      if (w->mask != 0)
        ATOMIC_STORE(w->addr, w->value);
      /* Only drop lock for last covered address in write set */
      if (w->next == NULL)
        ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(t));

      PRINT_DEBUG2("==> write(t=%p[%lu-%lu],a=%p,d=%p-%d,v=%d)\n",
                   tx, (unsigned long)tx->start, (unsigned long)tx->end, w->addr, (void *)w->value, (int)w->value, (int)w->version);
    }
  }

  tx->retries = 0;



  /* Callbacks */
  if (nb_commit_cb != 0) {
    int cb;
    for (cb = 0; cb < nb_commit_cb; cb++)
      commit_cb[cb].f(TXARGS commit_cb[cb].arg);
  }

  /* Set status (no need for CAS or atomic op) */
  tx->status = TX_COMMITTED;

  return 1;
}

int stm_commit(TXPARAM) {
  TX_GET;
  if (tx->type == NL) return stm_normal_commit();
  else return stm_elastic_commit();
}

/*
 * Called by the CURRENT thread to abort a transaction.
 */
void stm_abort(TXPARAM)
{
  TX_GET;
  stm_rollback(tx);
}

/* 
 * Read version value and same version to make sure the value corresponds to the version.
 */
static inline stm_word_t stm_vervalver(volatile stm_word_t *addr, stm_word_t *timestamp)
{
  volatile stm_word_t *lock;
  stm_word_t l, l2, value;
  w_entry_t *w;
	
  TX_GET;
		
  PRINT_DEBUG2("==> stm_vervalver(a=%p)\n", addr);
	
  /* Get reference to lock */
  lock = GET_LOCK(addr);
	
  /* Read lock, value, lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (LOCK_GET_OWNED(l)) {
		
    if ((stm_tx_t *)LOCK_GET_ADDR(l) == tx) {
      /* If the current tx owns the lock */
      w = (w_entry_t *)LOCK_GET_ADDR(l);
      /* Simply check if address falls inside our write set (avoids non-faulting load) */
      if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries) {
	/* Yes: did we previously write the same address? */
	while (1) {
	  if (addr == w->addr) {
	    /* Yes: get value from write set (or from memory if mask was empty) */
	    value = (w->mask == 0 ? ATOMIC_LOAD(addr) : w->value);
	    break;
	  }
	  if (w->next == NULL) {
	    /* No: get value from memory */
	    value = ATOMIC_LOAD(addr);
	    break;
	  }
	  w = w->next;
	}
	/* No need to add to read set (will remain valid) */
	PRINT_DEBUG2("==> stm_normal_load(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu)\n",
		     tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value);
	return value;
      }
    }
		
    /* Locked: wait until lock is free */
    goto restart;
  }
  /* Not locked */
  value = ATOMIC_LOAD_ACQ(addr);
  l2 = ATOMIC_LOAD_ACQ(lock);
  if (l != l2) {
    l = l2;
    goto restart_no_load;
  }
	
  *timestamp = LOCK_GET_TIMESTAMP(l);
	
  PRINT_DEBUG2("==> stm_vervalver(a=%p,l=%p,*l=%lu,d=%p-%lu)\n",
	       addr, lock, (unsigned long)l, (void *)value, (unsigned long)value);
	
  return value;
}

/*
 * Called by the CURRENT thread in an elastic transaction to 
 * load a word-sized value.
 */
static inline stm_word_t stm_elastic_load(TXPARAMS volatile stm_word_t *x)
{
  int i;
  stm_word_t v_x;
  stm_word_t *y;
  stm_word_t ts_x = 0;
  stm_word_t ts_y = 0;

  TX_GET;
	
  /* Check status */
  assert(tx->status == TX_ACTIVE);
	
  PRINT_DEBUG2("==> stm_elastic_load(t=%p[%lu-%lu],a=%p)\n", tx, 
	       (unsigned long)tx->start, (unsigned long)tx->end, addr);
	
  v_x = stm_vervalver(x, &ts_x);
  
  if (ts_x > tx->start) {
    /* Check consistency of the rotating buffer read addresses */ 
    for (i=0; i<ELASTICITY; i++) {
      if ((y=tx->lastraddr[i])) {
	stm_vervalver(y, &ts_y);
	if (ts_y > tx->start) {
	  tx->aborts_ro++;
	  stm_rollback(tx);
	  return 0;
	}
      }
    }
    tx->start = ts_x;
  }

  /* Fill the elastic rotating buffer with the address read */ 
  tx->lastraddr[tx->marker] = (stm_word_t *)x;
  /* Increment the marker of the elastic rotating buffer */
  tx->marker = ++tx->marker % ELASTICITY;
  
  return v_x;
}

/*
 * Called by the CURRENT thread in a normal transaction to 
 * load a word-sized value.
 */
static inline stm_word_t stm_normal_load(TXPARAMS volatile stm_word_t *addr)
{
  volatile stm_word_t *lock;
  stm_word_t l, l2, value, version;
  r_entry_t *r;
  w_entry_t *w;
  TX_GET;

  PRINT_DEBUG2("==> stm_normal_load(t=%p[%lu-%lu],a=%p)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, addr);

  /* Check status */
  assert(tx->status == TX_ACTIVE);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Note: we could check for duplicate reads and get value from read set */

  /* Read lock, value, lock */
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (LOCK_GET_OWNED(l)) {
    /* Locked */
    /* Do we own the lock? */
    w = (w_entry_t *)LOCK_GET_ADDR(l);
    /* Simply check if address falls inside our write set (avoids non-faulting load) */
    if (tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries) {
      /* Yes: did we previously write the same address? */
      while (1) {
        if (addr == w->addr) {
          /* Yes: get value from write set (or from memory if mask was empty) */
          value = (w->mask == 0 ? ATOMIC_LOAD(addr) : w->value);
          break;
        }
        if (w->next == NULL) {
          /* No: get value from memory */
          value = ATOMIC_LOAD(addr);
          break;
        }
        w = w->next;
      }
      /* No need to add to read set (will remain valid) */
      PRINT_DEBUG2("==> stm_normal_load(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu)\n",
                   tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value);
      return value;
    }
    /* Abort */
    tx->aborts_locked_read++;
    stm_rollback(tx);
    return 0;
  } else {
    /* Not locked */
    value = ATOMIC_LOAD_ACQ(addr);
    l2 = ATOMIC_LOAD_ACQ(lock);
    if (l != l2) {
      l = l2;
      goto restart_no_load;
    }
    /* Check timestamp */
    version = LOCK_GET_TIMESTAMP(l);
    /* Valid version? */
    if (version > tx->end) {
      /* No: try to extend first (except for read-only transactions: no read set) */
      if (tx->ro || !tx->can_extend || !stm_extend(tx)) {
        /* Not much we can do: abort */
        tx->aborts_validate_read++;
        stm_rollback(tx);
        return 0;
      }
      /* Verify that version has not been overwritten (read value has not
       * yet been added to read set and may have not been checked during
       * extend) */
      l = ATOMIC_LOAD_ACQ(lock);
      if (l != l2) {
        l = l2;
        goto restart_no_load;
      }
      /* Worked: we now have a good version (version <= tx->end) */
    }
  }
  /* We have a good version: add to read set (update transactions) and return value */

  if (!tx->ro) {
    /* Add address and version to read set */
    if (tx->r_set.nb_entries == tx->r_set.size)
      stm_allocate_rs_entries(tx, 1);
    r = &tx->r_set.entries[tx->r_set.nb_entries++];
    r->version = version;
    r->lock = lock;
  }

  PRINT_DEBUG2("==> stm_normal_load(t=%p[%lu-%lu],a=%p,l=%p,*l=%lu,d=%p-%lu,v=%lu)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, lock, (unsigned long)l, (void *)value, (unsigned long)value, (unsigned long)version);

  return value;
}

/*
 * Called by the CURRENT thread in an elastic transaction to 
 * load a word-sized value.
 */
stm_word_t stm_load(TXPARAMS volatile stm_word_t *addr) {
  TX_GET;
  if (tx->type == NL) return stm_normal_load(addr);
  else return stm_elastic_load(addr);
}

/*
 * Called by the CURRENT thread in an elastic transaction to store 
 * a word-sized value.
 */
static inline int stm_elastic_store(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  int i;
  stm_word_t ts_x;
  volatile stm_word_t *lock;
  stm_word_t l;
  w_entry_t *w;  
	
  TX_GET;
	
  PRINT_DEBUG2("==> stm_elastic_store(t=%p[%lu-%lu],a=%p)\n", tx, 
	       (unsigned long)tx->start, (unsigned long)tx->end, addr);
	
  tx->type = NL;
  stm_write(tx, addr, value, mask);

  /* Make sure last read values are unchanged */
  for (i=0; i<ELASTICITY; i++) {
    /* All addresses of the elastic rotating buffer must be consistent */
    if (tx->lastraddr[i]) {
      lock = GET_LOCK(tx->lastraddr[i]);
      l = ATOMIC_LOAD_ACQ(lock);
      if (LOCK_GET_OWNED(l)) {
	/* Is the buffer read address, the one we write? */
	w = (w_entry_t *)LOCK_GET_ADDR(l);                                                                                                                         
	/* Simply check if address falls inside our write set (avoids non-faulting load) */                                                                        
	if ((tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries))
	  return 1;  
	/* It is locked by another tx */			
	tx->aborts_locked_write++;
	stm_rollback(tx);
	return 0;
      }
      /* The read address of the rotating buffer has been seen unlocked */
      ts_x = (stm_word_t)LOCK_GET_TIMESTAMP(l);
      if (ts_x > tx->start) {
	/* The read address of the rotating buffer has changed */
	tx->aborts_validate_write++;
	stm_rollback(tx);
	return 0;
      }
    }
   
    /* Do not recheck the elastic rotating buffer in further stores */
    memset(tx->lastraddr, 0, ELASTICITY * sizeof(void *));
  }	
  return 1;
}

/*
 * CURRENT thread in a normal transaction to store 
 * a word-sized value.
 */
static inline void stm_normal_store(volatile stm_word_t *addr, stm_word_t value)
{	
  TX_GET;
  stm_write(tx, addr, value, ~(stm_word_t)0);
}

/*
 * Called by the CURRENT thread in a transaction to store a 
 * word-sized value.
 */
void stm_store(TXPARAMS volatile stm_word_t *addr, stm_word_t value)
{
  TX_GET;
  if (tx->type == EL) stm_elastic_store(addr, value, ~(stm_word_t)0);
  else stm_write(tx, addr, value, ~(stm_word_t)0);
}

/*
 * Called by the CURRENT thread in a normal transaction to store part
 * a word-sized value.
 */
void stm_store2(TXPARAMS volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  TX_GET;
  if (tx->type == EL) stm_elastic_store(addr, value, mask);
  else stm_write(tx, addr, value, mask);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
int stm_active(TXPARAM)
{
  TX_GET;

  return (tx->status == TX_ACTIVE);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
int stm_aborted(TXPARAM)
{
  TX_GET;

  return (tx->status == TX_ABORTED);
}

/*
 * Called by the CURRENT thread to obtain an environment for setjmp/longjmp.
 */
sigjmp_buf *stm_get_env(TXPARAM)
{
  TX_GET;

  /* Only return environment for top-level transaction */
  return tx->nesting == 0 ? &tx->env : NULL;
}

/*
 * Get transaction attributes.
 */
stm_tx_attr_t *stm_get_attributes(TXPARAM)
{
  TX_GET;

  return tx->attr;
}

/*
 * Return statistics about a thread/transaction.
 */
int stm_get_stats(TXPARAMS const char *name, void *val)
{
  TX_GET;

  if (strcmp("read_set_size", name) == 0) {
    *(unsigned int *)val = tx->r_set.size;
    return 1;
  }
  if (strcmp("write_set_size", name) == 0) {
    *(unsigned int *)val = tx->w_set.size;
    return 1;
  }
  if (strcmp("read_set_nb_entries", name) == 0) {
    *(unsigned int *)val = tx->r_set.nb_entries;
    return 1;
  }
  if (strcmp("write_set_nb_entries", name) == 0) {
    *(unsigned int *)val = tx->w_set.nb_entries;
    return 1;
  }
  if (strcmp("read_only", name) == 0) {
    *(unsigned int *)val = tx->ro;
    return 1;
  }
  if (strcmp("nb_aborts", name) == 0) {
    *(unsigned long *)val = tx->aborts;
    return 1;
  }
  if (strcmp("nb_aborts_ro", name) == 0) {
    *(unsigned long *)val = tx->aborts_ro;
    return 1;
  }
  if (strcmp("nb_aborts_locked_read", name) == 0) {
    *(unsigned long *)val = tx->aborts_locked_read;
    return 1;
  }
  if (strcmp("nb_aborts_locked_write", name) == 0) {
    *(unsigned long *)val = tx->aborts_locked_write;
    return 1;
  }
  if (strcmp("nb_aborts_validate_read", name) == 0) {
    *(unsigned long *)val = tx->aborts_validate_read;
    return 1;
  }
  if (strcmp("nb_aborts_validate_write", name) == 0) {
    *(unsigned long *)val = tx->aborts_validate_write;
    return 1;
  }
  if (strcmp("nb_aborts_validate_commit", name) == 0) {
    *(unsigned long *)val = tx->aborts_validate_commit;
    return 1;
  }
  if (strcmp("nb_aborts_invalid_memory", name) == 0) {
    *(unsigned long *)val = tx->aborts_invalid_memory;
    return 1;
  }
  if (strcmp("nb_aborts_double_write", name) == 0) {
    *(unsigned long *)val = tx->aborts_double_write;
    return 1;
  }
  if (strcmp("nb_aborts_reallocate", name) == 0) {
    *(unsigned long *)val = tx->aborts_reallocate;
    return 1;
  } 
  if (strcmp("nb_aborts_rollover", name) == 0) {
    *(unsigned long *)val = tx->aborts_rollover;
    return 1;
  }
  if (strcmp("max_retries", name) == 0) {
    *(unsigned long *)val = tx->max_retries;
    return 1;
  }
  return 0;
}

/*
 * Return STM parameters.
 */
int stm_get_parameter(const char *name, void *val)
{
  if (strcmp("contention_manager", name) == 0) {
    *(const char **)val = 0;
    return 1;
  }
  if (strcmp("design", name) == 0) {
    *(const char **)val = 0;
    return 1;
  }
  if (strcmp("initial_rw_set_size", name) == 0) {
    *(int *)val = RW_SET_SIZE;
    return 1;
  }
#ifdef COMPILE_FLAGS
  if (strcmp("compile_flags", name) == 0) {
    *(const char **)val = XSTR(COMPILE_FLAGS);
    return 1;
  }
#endif /* COMPILE_FLAGS */
  return 0;
}

/*
 * Set STM parameters.
 */
int stm_set_parameter(const char *name, void *val)
{
  return 0;
}

/*
 * Create transaction-specific data (return -1 on error).
 */
int stm_create_specific()
{
  if (nb_specific >= MAX_SPECIFIC) {
    fprintf(stderr, "Error: maximum number of specific slots reached\n");
    return -1;
  }
  return nb_specific++;
}

/*
 * Store transaction-specific data.
 */
void stm_set_specific(TXPARAMS int key, void *data)
{
  TX_GET;

  assert (key >= 0 && key < nb_specific);
  tx->data[key] = data;
}

/*
 * Fetch transaction-specific data.
 */
void *stm_get_specific(TXPARAMS int key)
{
  TX_GET;

  assert (key >= 0 && key < nb_specific);
  return tx->data[key];
}

/*
 * Register callbacks for an external module (must be called before creating transactions).
 */
int stm_register(void (*on_thread_init)(TXPARAMS void *arg),
                 void (*on_thread_exit)(TXPARAMS void *arg),
                 void (*on_start)(TXPARAMS void *arg),
                 void (*on_commit)(TXPARAMS void *arg),
                 void (*on_abort)(TXPARAMS void *arg),
                 void *arg)
{
  if ((on_thread_init != NULL && nb_init_cb >= MAX_CB) ||
      (on_thread_exit != NULL && nb_exit_cb >= MAX_CB) ||
      (on_start != NULL && nb_start_cb >= MAX_CB) ||
      (on_commit != NULL && nb_commit_cb >= MAX_CB) ||
      (on_abort != NULL && nb_abort_cb >= MAX_CB)) {
    fprintf(stderr, "Error: maximum number of modules reached\n");
    return 0;
  }
  /* New callback */
  if (on_thread_init != NULL) {
    init_cb[nb_init_cb].f = on_thread_init;
    init_cb[nb_init_cb++].arg = arg;
  }
  /* Delete callback */
  if (on_thread_exit != NULL) {
    exit_cb[nb_exit_cb].f = on_thread_exit;
    exit_cb[nb_exit_cb++].arg = arg;
  }
  /* Start callback */
  if (on_start != NULL) {
    start_cb[nb_start_cb].f = on_start;
    start_cb[nb_start_cb++].arg = arg;
  }
  /* Commit callback */
  if (on_commit != NULL) {
    commit_cb[nb_commit_cb].f = on_commit;
    commit_cb[nb_commit_cb++].arg = arg;
  }
  /* Abort callback */
  if (on_abort != NULL) {
    abort_cb[nb_abort_cb].f = on_abort;
    abort_cb[nb_abort_cb++].arg = arg;
  }

  return 1;
}

/*
 * Get curent value of global clock.
 */
stm_word_t stm_get_clock()
{
  return GET_CLOCK;
}

/*
 * Get current transaction descriptor.
 */
stm_tx_t *stm_current_tx()
{
  return stm_get_tx();
}
