/*
 * Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#ifdef TINY098

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

#elif defined TINY099

#  include "stm.h"
#  include "mod_mem.h"
#  include "mod_stats.h"
#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 { sigjmp_buf *_e = stm_get_env(); sigsetjmp(*_e, 0); stm_start(_e, 0)
#  define TX_LOAD(addr)                  stm_load((stm_word_t *)addr)
#  define TX_STORE(addr, val)            stm_store((stm_word_t *)addr, (stm_word_t)val)
#  define TX_END                         stm_commit(); }
#  define FREE(addr, size)               stm_free(addr, size)
#  define MALLOC(size)                   stm_malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_THREAD_ENTER()              stm_init_thread()
#  define TM_THREAD_EXIT()                                              \
  stm_get_stats("nb_aborts", &d->nb_aborts);                            \
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);    \
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);  \
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read); \
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write); \
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit); \
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory); \
  stm_get_stats("nb_aborts_double_write", &d->nb_aborts_invalid_memory); \
  stm_get_stats("max_retries", &d->max_retries);                        \
  stm_exit_thread()
#  define TM_STARTUP()                                                  \
  if (sizeof(long) != sizeof(void *)) {                                 \
  fprintf(stderr, "Error: unsupported long and pointer sizes\n");     \
  exit(1);                                                            \
  }                                                                     \
  stm_init();                                                           \
  mod_mem_init();                                                       \
  if (getenv("STM_STATS") != NULL) {                                    \
  mod_stats_init();                                                   \
  }
#  define TM_SHUTDOWN()                                 \
  if (getenv("STM_STATS") != NULL) {                    \
  unsigned long u;                                    \
  if (stm_get_stats("global_nb_commits", &u) != 0)    \
    printf("#commits    : %lu\n", u);                 \
  if (stm_get_stats("global_nb_aborts", &u) != 0)     \
    printf("#aborts     : %lu\n", u);                 \
  if (stm_get_stats("global_max_retries", &u) != 0)   \
    printf("Max retries : %lu\n", u);                 \
  }                                                     \
  stm_exit()

#elif defined(TINY100) || defined(TINY10B)

#  include "stm.h"
#  include "mod_mem.h"
#  include "mod_stats.h"
#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 { sigjmp_buf *_e = stm_start(0); if (_e != NULL) sigsetjmp(*_e, 0);
#  define TX_LOAD(addr)                  stm_load((stm_word_t *)addr)
//#  define TX_UNIT_LOAD(addr)                  stm_load((stm_word_t *)addr)
#  define TX_UNIT_LOAD(addr)             stm_unit_load((stm_word_t *)addr, NULL)
#  define TX_UNIT_LOAD_TS(addr, timestamp)  stm_unit_load((stm_word_t *)addr, (stm_word_t *)timestamp)
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

#endif
