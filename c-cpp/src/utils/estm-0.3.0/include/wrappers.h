/*
 * File:
 *   wrappers.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 * Description:
 *   STM wrapper functions for different data types.
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
 *   STM wrapper functions for different data types.  This library
 *   defines transactional loads/store functions for unsigned data types
 *   of various sizes and for basic C data types.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 * @date
 *   2007-2009
 */

#ifndef _WRAPPERS_H_
# define _WRAPPERS_H_

# include <stdint.h>

# include "stm.h"

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Transactional load of an unsigned 8-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
uint8_t stm_load8(TXPARAMS volatile uint8_t *addr);

/**
 * Transactional load of an unsigned 16-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
uint16_t stm_load16(TXPARAMS volatile uint16_t *addr);

/**
 * Transactional load of an unsigned 32-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
uint32_t stm_load32(TXPARAMS volatile uint32_t *addr);

/**
 * Transactional load of an unsigned 64-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
uint64_t stm_load64(TXPARAMS volatile uint64_t *addr);

/**
 * Transactional load of a char value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
char stm_load_char(TXPARAMS volatile char *addr);

/**
 * Transactional load of an unsigned char value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
unsigned char stm_load_uchar(TXPARAMS volatile unsigned char *addr);

/**
 * Transactional load of a short value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
short stm_load_short(TXPARAMS volatile short *addr);

/**
 * Transactional load of an unsigned short value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
unsigned short stm_load_ushort(TXPARAMS volatile unsigned short *addr);

/**
 * Transactional load of an int value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
int stm_load_int(TXPARAMS volatile int *addr);

/**
 * Transactional load of an unsigned int value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
unsigned int stm_load_uint(TXPARAMS volatile unsigned int *addr);

/**
 * Transactional load of a long value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
long stm_load_long(TXPARAMS volatile long *addr);

/**
 * Transactional load of an unsigned long value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
unsigned long stm_load_ulong(TXPARAMS volatile unsigned long *addr);

/**
 * Transactional load of a float value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
float stm_load_float(TXPARAMS volatile float *addr);

/**
 * Transactional load of a double value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
double stm_load_double(TXPARAMS volatile double *addr);

/**
 * Transactional load of a pointer value.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
void *stm_load_ptr(TXPARAMS volatile void **addr);

/**
 * Transactional load of a memory region.  The address of the region
 * does not need to be word aligned and its size may be longer than a
 * word.  The values are copied into the provided buffer, which must be
 * allocated by the caller.
 *
 * @param addr
 *   Address of the memory location.
 * @param buf
 *   Buffer for storing the read bytes.
 * @param size
 *   Number of bytes to read.
 */
void stm_load_bytes(TXPARAMS volatile uint8_t *addr, uint8_t *buf, size_t size);

/**
 * Transactional store of an unsigned 8-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store8(TXPARAMS volatile uint8_t *addr, uint8_t value);

/**
 * Transactional store of an unsigned 16-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store16(TXPARAMS volatile uint16_t *addr, uint16_t value);

/**
 * Transactional store of an unsigned 32-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store32(TXPARAMS volatile uint32_t *addr, uint32_t value);

/**
 * Transactional store of an unsigned 64-bit value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store64(TXPARAMS volatile uint64_t *addr, uint64_t value);

/**
 * Transactional store of a char value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_char(TXPARAMS volatile char *addr, char value);

/**
 * Transactional store of an unsigned char value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_uchar(TXPARAMS volatile unsigned char *addr, unsigned char value);

/**
 * Transactional store of a short value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_short(TXPARAMS volatile short *addr, short value);

/**
 * Transactional store of an unsigned short value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_ushort(TXPARAMS volatile unsigned short *addr, unsigned short value);

/**
 * Transactional store of an int value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_int(TXPARAMS volatile int *addr, int value);

/**
 * Transactional store of an unsigned int value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_uint(TXPARAMS volatile unsigned int *addr, unsigned int value);

/**
 * Transactional store of a long value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_long(TXPARAMS volatile long *addr, long value);

/**
 * Transactional store of an unsigned long value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_ulong(TXPARAMS volatile unsigned long *addr, unsigned long value);

/**
 * Transactional store of a float value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_float(TXPARAMS volatile float *addr, float value);

/**
 * Transactional store of a double value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_double(TXPARAMS volatile double *addr, double value);

/**
 * Transactional store of a pointer value.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store_ptr(TXPARAMS volatile void **addr, void *value);

/**
 * Transactional store of a memory region.  The address of the region
 * does not need to be word aligned and its size may be longer than a
 * word.  The values are copied from the provided buffer.
 *
 * @param addr
 *   Address of the memory location.
 * @param buf
 *   Buffer with the bytes to write.
 * @param size
 *   Number of bytes to write.
 */
void stm_store_bytes(TXPARAMS volatile uint8_t *addr, uint8_t *buf, size_t size);

# ifdef __cplusplus
}
# endif

#endif /* _WRAPPERS_H_ */
