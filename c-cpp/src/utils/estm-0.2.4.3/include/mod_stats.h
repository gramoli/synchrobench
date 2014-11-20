/*
 * File:
 *   mod_stats.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Module for gathering statistics about transactions.
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

/**
 * @file
 *   Module for gathering statistics about transactions.  This module
 *   maintain both aggregate statistics about all threads (aggregates
 *   are updated upon thread cleanup) and per-thread statistics.  The
 *   built-in statistics of the core STM library are more efficient and
 *   detailed but this module is useful in case the library is compiled
 *   without support for statistics.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 * @date
 *   2007-2009
 */

#ifndef _MOD_STATS_H_
# define _MOD_STATS_H_

# include "stm.h"

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Get various statistics about the transactions of all threads.  See
 * the source code (mod_stats.c) for a list of supported statistics.
 *
 * @param name
 *   Name of the statistics.
 * @param val
 *   Pointer to the variable that should hold the value of the
 *   statistics.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_get_global_stats(const char *name, void *val);

/**
 * Get various statistics about the transactions of the current thread.
 * See the source code (mod_stats.c) for a list of supported statistics.
 *
 * @param name
 *   Name of the statistics.
 * @param val
 *   Pointer to the variable that should hold the value of the
 *   statistics.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_get_local_stats(TXPARAMS const char *name, void *val);

/**
 * Initialize the module.  This function must be called once, from the
 * main thread, after initializing the STM library and before
 * performing any transactional operation.
 */
void mod_stats_init();

# ifdef __cplusplus
}
# endif

#endif /* _MOD_STATS_H_ */
