#ifndef _URCU_H_
#define _URCU_H_

/**
 * Copyright 2014 Maya Arbel (mayaarl [at] cs [dot] technion [dot] ac [dot] il).
 * 
 * This file is part of Citrus. 
 * 
 * Citrus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Authors Maya Arbel and Adam Morrison 
 */

#if !defined(EXTERNAL_RCU)

typedef struct rcu_node_t {
    volatile long time; 
    char p[184];
} rcu_node;

void initURCU(int num_threads);
void urcu_read_lock();
void urcu_read_unlock();
void urcu_synchronize(); 
void urcu_register(int id);
void urcu_unregister();

#else

#include "urcu.h"

static inline void initURCU(int num_threads)
{
    rcu_init();
}

static inline void urcu_register(int id)
{
    rcu_register_thread();
}

static inline void urcu_unregister()
{
    rcu_unregister_thread();
}

static inline void urcu_read_lock()
{
    rcu_read_lock();
}

static inline void urcu_read_unlock()
{
    rcu_read_unlock();
}

static inline void urcu_synchronize()
{
    synchronize_rcu();
}

#endif  /* EXTERNAL RCU */ 

#endif
