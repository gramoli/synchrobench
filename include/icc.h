
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
