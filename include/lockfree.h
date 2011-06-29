
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
