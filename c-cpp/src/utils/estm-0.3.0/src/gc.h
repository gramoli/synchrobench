/*
 * File:
 *   gc.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Epoch-based garbage collector.
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

#ifndef _GC_H_
# define _GC_H_

# include <stdlib.h>

# ifdef __cplusplus
extern "C" {
# endif

typedef uintptr_t gc_word_t;

void gc_init(gc_word_t (*epoch)());
void gc_exit();

void gc_init_thread();
void gc_exit_thread();

void gc_set_epoch(gc_word_t epoch);

void gc_free(void *addr, gc_word_t epoch);

void gc_cleanup();

void gc_cleanup_all();

void gc_reset();

# ifdef __cplusplus
}
# endif

#endif /* _GC_H_ */
