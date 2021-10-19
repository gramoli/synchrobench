
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
