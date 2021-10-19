#  include "stm.h"
#  include "mod_mem.h"
#  define TX_START(type)                 { sigjmp_buf *_e = stm_get_env(); sigsetjmp(*_e, 0); stm_start(_e, 0, type)
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
#  define TM_STARTUP()                                                  \
  char *s;                                                              \
  stm_init();                                                           \
  mod_mem_init();                                                       \
  if (stm_get_parameter("compile_flags", &s))                           \
    printf("STM flags    : %s\n", s)
#  define TM_SHUTDOWN()                                                 \
  unsigned long u;                                                      \
  if (stm_get_stats("global_nb_commits", &u) != 0)                      \
    printf("#commits    : %lu\n", u);                                   \
  if (stm_get_stats("global_nb_aborts", &u) != 0)                       \
    printf("#aborts     : %lu\n", u);                                   \
  if (stm_get_stats("global_max_retries", &u) != 0)                     \
    printf("Max retries : %lu\n", u);                                   \
  stm_exit()
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
