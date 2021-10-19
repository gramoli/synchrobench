/*
 * ESTM, TINYSTM, TINY098: Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */

#ifdef ESTM

#  include "stm.h"
#  include "mod_mem.h"
#  define TX_START(type)                 { sigjmp_buf *_e = stm_get_env(); sigsetjmp(*_e, 0); stm_start(_e, 0, 0)
#  define TX_LOAD(addr)                  stm_load((stm_word_t *)addr)
#  define TX_UNIT_LOAD(addr)             stm_load((stm_word_t *)addr)
#  define UNIT_LOAD(addr)             non_stm_unit_load((stm_word_t *)addr, NULL)
#  define TX_STORE(addr, val)            stm_store((stm_word_t *)addr, (stm_word_t)val)
#  define TX_END	     		 stm_commit(); }
#  define FREE(addr, size)               stm_free(addr, size)
#  define MALLOC(size)                   stm_malloc(size)	
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_STARTUP()							\
  char *s;								\
  stm_init();								\
  mod_mem_init();							\
  if (stm_get_parameter("compile_flags", &s))				\
    printf("STM flags    : %s\n", s)
#  define TM_SHUTDOWN()							\
  unsigned long u;							\
  if (stm_get_stats("global_nb_commits", &u) != 0)			\
    printf("#commits    : %lu\n", u);					\
  if (stm_get_stats("global_nb_aborts", &u) != 0)			\
    printf("#aborts     : %lu\n", u);					\
  if (stm_get_stats("global_max_retries", &u) != 0)			\
    printf("Max retries : %lu\n", u);					\
  stm_exit()
#  define TM_THREAD_ENTER()              stm_init_thread()
#  define TM_THREAD_EXIT()						\
  stm_get_stats("nb_aborts", &d->nb_aborts);				\
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);	\
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);	\
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read); \
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write); \
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit); \
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory); \
  stm_get_stats("nb_aborts_double_write", &d->nb_aborts_invalid_memory); \
  stm_get_stats("max_retries", &d->max_retries);			\
  stm_exit_thread() 

#elif defined TINY10B

#  include "stm.h"
#  include "mod_mem.h"
#  include "mod_stats.h"
#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 { sigjmp_buf *_e = stm_start(0); if (_e != NULL) sigsetjmp(*_e, 0);
/* #ifndef RW_SIZE */
#  define TX_LOAD(addr)                  stm_load((stm_word_t *)addr)
/* #else */
/* #  define TX_LOAD(addr)                  stm_load_mb((stm_word_t *)addr, id) */
/* #endif */

#ifdef NO_UNITLOADS
#  define TX_UNIT_LOAD(addr)             stm_load((stm_word_t *)addr)
#  define UNIT_LOAD(addr)             non_stm_unit_load((stm_word_t *)addr, NULL)
#  define TX_UNIT_LOAD_TS(addr, timestamp)  stm_load((stm_word_t *)addr)
#else
#ifdef ATOMIC_LOADS
#  define TX_UNIT_LOAD(addr)             stm_atomic_load((stm_word_t *)addr)
#else
#  define TX_UNIT_LOAD(addr)             stm_unit_load((stm_word_t *)addr, NULL)
#endif
#  define UNIT_LOAD(addr)             non_stm_unit_load((stm_word_t *)addr, NULL)
#  define TX_UNIT_LOAD_TS(addr, timestamp)  stm_unit_load((stm_word_t *)addr, (stm_word_t *)timestamp)
#endif
/* #ifndef RW_SIZE */
#  define TX_STORE(addr, val)            stm_store((stm_word_t *)addr, (stm_word_t)val)
/* #else */
/* #  define TX_STORE(addr, val)            stm_store_mb((stm_word_t *)addr, (stm_word_t)val, id) */
/* #endif */
#  define TX_END			 stm_commit(); }
#  define FREE(addr, size)               stm_free(addr, size)
#  define MALLOC(size)                   stm_malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_THREAD_ENTER()              stm_init_thread()
#  define TM_THREAD_EXIT()						\
  stm_get_stats("set_write_reads", &d->set_write_reads); \
  stm_get_stats("set_read_reads", &d->set_read_reads); \
  stm_get_stats("set_write_writes", &d->set_write_writes); \
  stm_get_stats("set_reads", &d->set_reads); \
  stm_get_stats("set_writes", &d->set_writes); \
  stm_get_stats("set_max_reads", &d->set_max_reads); \
  stm_get_stats("set_max_writes", &d->set_max_writes); \
  stm_get_stats("write_reads", &d->write_reads); \
  stm_get_stats("read_reads", &d->read_reads); \
  stm_get_stats("write_writes", &d->write_writes); \
  stm_get_stats("reads", &d->reads); \
  stm_get_stats("writes", &d->writes); \
  stm_get_stats("max_reads", &d->max_reads); \
  stm_get_stats("max_writes", &d->max_writes); \
  stm_get_stats("nb_aborts", &d->nb_aborts);				\
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);	\
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);	\
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read); \
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write); \
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit); \
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory); \
  stm_get_stats("nb_aborts_double_write", &d->nb_aborts_invalid_memory); \
  stm_get_stats("max_retries", &d->max_retries);			\
  stm_exit_thread()
#  define TM_STARTUP()							\
  if (sizeof(long) != sizeof(void *)) {					\
    fprintf(stderr, "Error: unsupported long and pointer sizes\n");	\
    exit(1);								\
  }									\
  stm_init();								\
  mod_mem_init(0);							\
  if (getenv("STM_STATS") != NULL) {					\
    mod_stats_init();							\
  }
#  define TM_SHUTDOWN()					\
  if (getenv("STM_STATS") != NULL) {			\
    unsigned long u;					\
    if (stm_get_stats("global_nb_commits", &u) != 0)	\
      printf("#commits    : %lu\n", u);			\
    if (stm_get_stats("global_nb_aborts", &u) != 0)	\
      printf("#aborts     : %lu\n", u);			\
    if (stm_get_stats("global_max_retries", &u) != 0)	\
      printf("Max retries : %lu\n", u);			\
  }							\
  stm_exit()



#elif defined TINY099

#  include "stm.h"
#  include "mod_mem.h"
#  include "mod_stats.h"
#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 { sigjmp_buf *_e = stm_get_env(); sigsetjmp(*_e, 0); stm_start(_e, 0)
#  define TX_LOAD(addr)                  stm_load((stm_word_t *)addr)
#  define TX_STORE(addr, val)            stm_store((stm_word_t *)addr, (stm_word_t)val)
#  define TX_END			 stm_commit(); }
#  define FREE(addr, size)               stm_free(addr, size)
#  define MALLOC(size)                   stm_malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_THREAD_ENTER()              stm_init_thread()
#  define TM_THREAD_EXIT()						\
  stm_get_stats("nb_aborts", &d->nb_aborts);				\
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);	\
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);	\
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read); \
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write); \
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit); \
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory); \
  stm_get_stats("nb_aborts_double_write", &d->nb_aborts_invalid_memory); \
  stm_get_stats("max_retries", &d->max_retries);			\
  stm_exit_thread()
#  define TM_STARTUP()							\
  if (sizeof(long) != sizeof(void *)) {					\
    fprintf(stderr, "Error: unsupported long and pointer sizes\n");	\
    exit(1);								\
  }									\
  stm_init();								\
  mod_mem_init();							\
  if (getenv("STM_STATS") != NULL) {					\
    mod_stats_init();							\
  }
#  define TM_SHUTDOWN()					\
  if (getenv("STM_STATS") != NULL) {			\
    unsigned long u;					\
    if (stm_get_stats("global_nb_commits", &u) != 0)	\
      printf("#commits    : %lu\n", u);			\
    if (stm_get_stats("global_nb_aborts", &u) != 0)	\
      printf("#aborts     : %lu\n", u);			\
    if (stm_get_stats("global_max_retries", &u) != 0)	\
      printf("Max retries : %lu\n", u);			\
  }							\
  stm_exit()

#elif defined TINY098

#  include "stm.h"
#  include "mod_mem.h"
#  include "mod_stats.h"
#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 { sigjmp_buf *_e = stm_get_env(); sigsetjmp(*_e, 0); stm_start(_e, 0)
#  define TX_LOAD(addr)                  stm_load((stm_word_t *)addr)
#  define TX_STORE(addr, val)            stm_store((stm_word_t *)addr, (stm_word_t)val)
#  define TX_END						 stm_commit(); }
#  define FREE(addr, size)               stm_free(addr, size)
#  define MALLOC(size)                   stm_malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_STARTUP(numThread)						\
  if (sizeof(long) != sizeof(void *)) {					\
    fprintf(stderr, "Error: unsupported long and pointer sizes\n");	\
    exit(1);								\
  }									\
  stm_init();								\
  mod_mem_init();							\
  mod_stats_init()
#  define TM_SHUTDOWN()							\
  unsigned long u;							\
  if (stm_get_stats("global_nb_commits", &u) != 0)			\
    printf("#commits    : %lu\n", u);					\
  if (stm_get_stats("global_nb_aborts", &u) != 0)			\
    printf("#aborts     : %lu\n", u);					\
  if (stm_get_stats("global_max_retries", &u) != 0)			\
    printf("Max retries : %lu\n", u);					\
  stm_exit()
#  define TM_THREAD_ENTER()              stm_init_thread()
#  define TM_THREAD_EXIT()						\
  stm_get_stats("nb_aborts", &d->nb_aborts);				\
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);	\
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);	\
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read); \
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write); \
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit); \
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory); \
  stm_get_stats("nb_aborts_double_write", &d->nb_aborts_invalid_memory); \
  stm_get_stats("max_retries", &d->max_retries);			\
  stm_exit_thread()

#elif defined WLPDSTM

#  include "stm.h"
#  define NL                             1
#  define EL                             0 
#  define TX_START(type)                 BEGIN_TRANSACTION 
#  define TX_LOAD(addr)                  wlpdstm_read_word((Word *)(addr)) 
#  define TX_STORE(addr, val)            wlpdstm_write_word((Word *)addr, (Word)val) 
#  define TX_END		       	 END_TRANSACTION
#  define FREE(addr, size)               wlpdstm_tx_free(addr, size) 
#  define MALLOC(size)                   wlpdstm_tx_malloc(size) 
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */ 
#  define TM_ARGDECL                     /* nothing */ 
#  define TM_ARG                         /* nothing */ 
#  define TM_ARG_LAST                    /* nothing */ 
#  define TM_ARG_ALONE                   /* nothing */ 
#  define TM_STARTUP()                   wlpdstm_global_init()
#  ifdef COLLECT_STATS
#    define TM_SHUTDOWN()                wlpdstm_print_stats()
#  else
#    define TM_SHUTDOWN()                /* nothing */
#  endif /* COLLECT_STATS */
#  define TM_THREAD_ENTER()              wlpdstm_thread_init(); \
                                         tx_desc *tx = wlpdstm_get_tx_desc() 
#  define TM_THREAD_EXIT()               /* nothing */ 

#elif defined ICC

__attribute__((tm_wrapping(malloc))) void *wlpdstm_icc_malloc(size_t size);
__attribute__((tm_wrapping(free))) void wlpdstm_icc_free(void *ptr);
#  define NL                             1
#  define EL                             0
#  define START                          __tm_atomic {
#  define START_ID(ID)                   __tm_atomic {
#  define START_RO                       __tm_atomic {
#  define START_RO_ID(ID)                __tm_atomic {
#  define LOAD(addr)                     (*(addr))
#  define STORE(addr, value)             (*(addr) = (value))
#  define COMMIT                         }
#  define MALLOC(size)                   malloc(size)
#  define FREE(addr, size)               free(addr)
#  define TM_CALLABLE                    __attribute__ ((tm_callable))
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_STARTUP()                   /* nothing */
#  define TM_SHUTDOWN()                  /* nothing */
#  define TM_THREAD_ENTER()              /* nothing */
#  define TM_THREAD_EXIT()               /* nothing */

#elif defined LOCKFREE

#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 /* nothing */
#  define TX_LOAD(addr)                  (*(addr))
#  define TX_STORE(addr, val)            (*(addr) = (val))
#  define TX_END                         /* nothing */
#  define FREE(addr, size)               free(addr)
#  define MALLOC(size)                   malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_STARTUP()                   /* nothing */
#  define TM_SHUTDOWN()                  /* nothing */
#  define TM_STARTUP()                   /* nothing */
#  define TM_SHUTDOWN()                  /* nothing */
#  define TM_THREAD_ENTER()              /* nothing */
#  define TM_THREAD_EXIT()               /* nothing */

#elif  defined TL2old

#  include <string.h>
#  include "tl2.h"
#  define NL                            1
#  define EL                            0
#define TX_START(type)                  int ro_flag = 0; sigjmp_buf buf; sigsetjmp(buf, 0); TxStart(tx, &buf, &ro_flag)
#define START_ID(ID)                    START
#define START_RO                        int ro_flag = 1; sigjmp_buf buf; sigsetjmp(buf, 0); TxStart(tx, &buf, &ro_flag)
#define START_RO_ID(ID)                 START_RO
#define LOAD(addr)                      TxLoad(tx, (volatile intptr_t*)addr)
#define STORE(addr, value)              TxStore(tx, (volatile intptr_t*)addr, (intptr_t)value)
#define TX_END                          TxCommit(tx)
#define MALLOC(size)                    TxAlloc(tx, size)
#define FREE(addr, size)                TxFree(tx, addr)
#define TM_CALLABLE                     /* nothing */
#define malloc(size)                    tmalloc_reserve(size)
#define free(ptr)                       tmalloc_release(ptr)
#define TM_ARGDECL_ALONE                Thread* tx
#define TM_ARGDECL                      Thread* tx,
#define TM_ARG                          tx, 
#define TM_ARG_LAST                     , tx
#define TM_ARG_ALONE                    tx
#define TM_STARTUP()                    TxOnce()
#define TM_SHUTDOWN()                   TxShutdown()
/* TODO: passing 0 as id seems ok unless GV6 is used, otherwise it should be generated somehow */
#define TM_THREAD_ENTER()               Thread *tx = TxNewThread(); \
TxInitThread(tx, 0)
#define TM_THREAD_EXIT()                /* nothing */

#elif defined TL2

#  include <string.h>
#  include "stm.h"
#  include "tl2.h"
//#  include "thread.h"
#  define NL                            1
#  define EL                            0
#  define TX_START(type)                STM_BEGIN_WR()
#  define TX_END                        STM_END()
#  define MALLOC(size)                  malloc(size)
#  define FREE(addr, ptr)               free(ptr)
#  define TM_MALLOC(size)               MALLOC(size)
#  define TM_FREE(ptr)                  FREE(0, ptr)
#  define TM_RESTART()                  STM_RESTART()
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARG                        STM_SELF,
#  define TM_ARG_ALONE                  STM_SELF
#  define TM_ARGDECL                    STM_THREAD_T* TM_ARG
#  define TM_ARGDECL_ALONE              STM_THREAD_T* TM_ARG_ALONE
#  define TM_STARTUP(numThread)         STM_STARTUP()
//; P_MEMORY_STARTUP(numThread); thread_startup(numThread);
#  define TM_SHUTDOWN()                 STM_SHUTDOWN()
#  define TM_THREAD_ENTER()             TM_ARGDECL_ALONE = STM_NEW_THREAD(); \
STM_INIT_THREAD(TM_ARG_ALONE, 0)
#  define TM_THREAD_EXIT()              STM_FREE_THREAD(TM_ARG_ALONE)

#  define TM_SHARED_READ(var)           STM_READ(var)
#  define TM_SHARED_READ_P(var)         STM_READ_P(var)
#  define TM_SHARED_READ_F(var)         STM_READ_F(var)

#  define TM_SHARED_WRITE(var, val)     STM_WRITE((var), val)
#  define TM_SHARED_WRITE_P(var, val)   STM_WRITE_P((var), val)
#  define TM_SHARED_WRITE_F(var, val)   STM_WRITE_F((var), val)

#  define TM_LOCAL_WRITE(var, val)      STM_LOCAL_WRITE(var, val)
#  define TM_LOCAL_WRITE_P(var, val)    STM_LOCAL_WRITE_P(var, val)
#  define TM_LOCAL_WRITE_F(var, val)    STM_LOCAL_WRITE_F(var, val)

#elif defined SEQUENTIAL

#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 /* nothing */
#  define TX_LOAD(addr)                  (*(addr))
#  define TX_STORE(addr, val)            (*(addr) = (val))
#  define TX_END						 /* nothing */
#  define FREE(addr, size)               free(addr)
#  define MALLOC(size)                   malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_STARTUP()		             /* nothing */
#  define TM_SHUTDOWN()					 /* nothing */
#  define TM_STARTUP()                   /* nothing */
#  define TM_SHUTDOWN()                  /* nothing */
#  define TM_THREAD_ENTER()              /* nothing */
#  define TM_THREAD_EXIT()               /* nothing */

#endif /* SEQUENTIAL */
