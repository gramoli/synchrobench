#  include <string.h>
#  include "stm.h"
//#  include "tl2.h"
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
