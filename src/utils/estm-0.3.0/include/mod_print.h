/*
 * File:
 *   mod_print.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Module to test callbacks.
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
 *   Module to test callbacks.  This module simply prints a message at
 *   each invocation of a callback.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 * @date
 *   2007-2009
 */

#ifndef _MOD_PRINT_H_
# define _MOD_PRINT_H_

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Initialize the module.  This function must be called once, from the
 * main thread, after initializing the STM library and before
 * performing any transactional operation.
 */
void mod_print_init();

# ifdef __cplusplus
}
# endif

#endif /* _MOD_PRINT_H_ */
