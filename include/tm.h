
#ifdef ESTM
#  include "estm.h"
#elif defined(TINY100) || defined(TINY10B) || defined(TINY099) || defined(TINY098)
#  include "tinystm.h"
#elif defined WPLDSTM
#  include "wlpdstm.h"
#elif defined LOCKFREE
#  include "lockfree.h"
#elif defined TL2
#  include "tl2-mbench.h"
#elif defined ICC
#  include "icc.h"
#elif defined SEQUENTIAL
#  include "sequential.h"
#endif
