/*
 * File:
 *   mod_local.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   Module for local memory accesses.
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
 *   Module for local memory accesses.  Data is both written to memory
 *   and stored in an undo log.  Upon abort, modifications are reverted.
 *   Note that this module should not be used for updating shared data
 *   as there are no mechanisms to deal with concurrent accesses.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 * @date
 *   2007-2009
 */

#ifndef _MOD_LOCAL_H_
# define _MOD_LOCAL_H_

# include "stm.h"

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Transaction-local store of a word-sized value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local(TXPARAMS stm_word_t *addr, stm_word_t value);

/**
 * Transaction-local store of a char value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_char(TXPARAMS char *addr, char value);

/**
 * Transaction-local store of an unsigned char value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_uchar(TXPARAMS unsigned char *addr, unsigned char value);

/**
 * Transaction-local store of a short value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_short(TXPARAMS short *addr, short value);

/**
 * Transaction-local store of an unsigned short value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_ushort(TXPARAMS unsigned short *addr, unsigned short value);

/**
 * Transaction-local store of an int value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_int(TXPARAMS int *addr, int value);

/**
 * Transaction-local store of an unsigned int value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_uint(TXPARAMS unsigned int *addr, unsigned int value);

/**
 * Transaction-local store of a long value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_long(TXPARAMS long *addr, long value);

/**
 * Transaction-local store of an unsigned long value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_ulong(TXPARAMS unsigned long *addr, unsigned long value);

/**
 * Transaction-local store of a float value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_float(TXPARAMS float *addr, float value);

/**
 * Transaction-local store of a double value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_double(TXPARAMS double *addr, double value);

/**
 * Transaction-local store of a pointer value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_local_ptr(TXPARAMS void **addr, void *value);

/**
 * Initialize the module.  This function must be called once, from the
 * main thread, after initializing the STM library and before
 * performing any transactional operation.
 */
void mod_local_init();

# ifdef __cplusplus
}
# endif

#endif /* _MOD_LOCAL_H_ */
