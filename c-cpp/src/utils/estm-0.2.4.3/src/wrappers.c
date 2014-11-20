/*
 * File:
 *   wrappers.c
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

#include <assert.h>

#include "wrappers.h"

#define COMPILE_TIME_ASSERT(pred)       switch (0) { case 0: case pred: ; }

typedef union convert_64 {
  uint64_t u64;
  uint32_t u32[2];
  uint16_t u16[4];
  uint8_t u8[8];
  int64_t s64;
  double d;
} convert_64_t;

typedef union convert_32 {
  uint32_t u32;
  uint16_t u16[2];
  uint8_t u8[4];
  int32_t s32;
  float f;
} convert_32_t;

typedef union convert_16 {
  uint16_t u16;
  int16_t s16;
} convert_16_t;

typedef union convert_8 {
  uint8_t u8;
  int8_t s8;
} convert_8_t;

typedef union convert {
  stm_word_t w;
  uint8_t b[sizeof(stm_word_t)];
} convert_t;

static void sanity_checks()
{
  COMPILE_TIME_ASSERT(sizeof(convert_64_t) == 8);
  COMPILE_TIME_ASSERT(sizeof(convert_32_t) == 4);
  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == 4 || sizeof(stm_word_t) == 8);
  COMPILE_TIME_ASSERT(sizeof(char) == 1);
  COMPILE_TIME_ASSERT(sizeof(short) == 2);
  COMPILE_TIME_ASSERT(sizeof(int) == 4);
  COMPILE_TIME_ASSERT(sizeof(long) == 4 || sizeof(long) == 8);
  COMPILE_TIME_ASSERT(sizeof(float) == 4);
  COMPILE_TIME_ASSERT(sizeof(double) == 8);
}

/* ################################################################### *
 * LOADS
 * ################################################################### */

uint8_t stm_load8(TXPARAMS volatile uint8_t *addr)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val;
    val.u32 = (uint32_t)stm_load(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03));
    return val.u8[(uintptr_t)addr & 0x03];
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)stm_load(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u8[(uintptr_t)addr & 0x07];
  }
}

uint16_t stm_load16(TXPARAMS volatile uint16_t *addr)
{
  if (((uintptr_t)addr & 0x01) != 0) {
    uint16_t val;
    stm_load_bytes(TXARGS (volatile uint8_t *)addr, (uint8_t *)&val, sizeof(uint16_t));
    return val;
  } else if (sizeof(stm_word_t) == 4) {
    convert_32_t val;
    val.u32 = (uint32_t)stm_load(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03));
    return val.u16[((uintptr_t)addr & 0x03) >> 1];
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)stm_load(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u16[((uintptr_t)addr & 0x07) >> 1];
  }
}

uint32_t stm_load32(TXPARAMS volatile uint32_t *addr)
{
  if (((uintptr_t)addr & 0x03) != 0) {
    uint32_t val;
    stm_load_bytes(TXARGS (volatile uint8_t *)addr, (uint8_t *)&val, sizeof(uint32_t));
    return val;
  } else if (sizeof(stm_word_t) == 4) {
    return (uint32_t)stm_load(TXARGS (volatile stm_word_t *)addr);
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)stm_load(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u32[((uintptr_t)addr & 0x07) >> 2];
  }
}

uint64_t stm_load64(TXPARAMS volatile uint64_t *addr)
{
  if (((uintptr_t)addr & 0x07) != 0) {
    uint64_t val;
    stm_load_bytes(TXARGS (volatile uint8_t *)addr, (uint8_t *)&val, sizeof(uint64_t));
    return val;
  } else if (sizeof(stm_word_t) == 4) {
    convert_64_t val;
    val.u32[0] = (uint32_t)stm_load(TXARGS (volatile stm_word_t *)addr);
    val.u32[1] = (uint32_t)stm_load(TXARGS (volatile stm_word_t *)addr + 1);
    return val.u64;
  } else {
    return (uint64_t)stm_load(TXARGS (volatile stm_word_t *)addr);
  }
}

char stm_load_char(TXPARAMS volatile char *addr)
{
  convert_8_t val;
  val.u8 = stm_load8(TXARGS (volatile uint8_t *)addr);
  return val.s8;
}

unsigned char stm_load_uchar(TXPARAMS volatile unsigned char *addr)
{
  return (unsigned char)stm_load8(TXARGS (volatile uint8_t *)addr);
}

short stm_load_short(TXPARAMS volatile short *addr)
{
  convert_16_t val;
  val.u16 = stm_load16(TXARGS (volatile uint16_t *)addr);
  return val.s16;
}

unsigned short stm_load_ushort(TXPARAMS volatile unsigned short *addr)
{
  return (unsigned short)stm_load16(TXARGS (volatile uint16_t *)addr);
}

int stm_load_int(TXPARAMS volatile int *addr)
{
  convert_32_t val;
  val.u32 = stm_load32(TXARGS (volatile uint32_t *)addr);
  return val.s32;
}

unsigned int stm_load_uint(TXPARAMS volatile unsigned int *addr)
{
  return (unsigned int)stm_load32(TXARGS (volatile uint32_t *)addr);
}

long stm_load_long(TXPARAMS volatile long *addr)
{
  if (sizeof(long) == 4) {
    convert_32_t val;
    val.u32 = stm_load32(TXARGS (volatile uint32_t *)addr);
    return val.s32;
  } else {
    convert_64_t val;
    val.u64 = stm_load64(TXARGS (volatile uint64_t *)addr);
    return val.s64;
  }
}

unsigned long stm_load_ulong(TXPARAMS volatile unsigned long *addr)
{
  if (sizeof(long) == 4) {
    return (unsigned long)stm_load32(TXARGS (volatile uint32_t *)addr);
  } else {
    return (unsigned long)stm_load64(TXARGS (volatile uint64_t *)addr);
  }
}

float stm_load_float(TXPARAMS volatile float *addr)
{
  convert_32_t val;
  val.u32 = stm_load32(TXARGS (volatile uint32_t *)addr);
  return val.f;
}

double stm_load_double(TXPARAMS volatile double *addr)
{
  convert_64_t val;
  val.u64 = stm_load64(TXARGS (volatile uint64_t *)addr);
  return val.d;
}

void *stm_load_ptr(TXPARAMS volatile void **addr)
{
  union { stm_word_t w; void *v; } convert;
  convert.w = stm_load(TXARGS (stm_word_t *)addr);
  return convert.v;
}

void stm_load_bytes(TXPARAMS volatile uint8_t *addr, uint8_t *buf, size_t size)
{
  convert_t val;
  unsigned int i;
  stm_word_t *a;

  if (size == 0)
    return;
  i = (uintptr_t)addr & (sizeof(stm_word_t) - 1);
  if (i != 0) {
    /* First bytes */
    a = (stm_word_t *)((uintptr_t)addr & ~(uintptr_t)(sizeof(stm_word_t) - 1));
    val.w = stm_load(TXARGS a++);
    for (; i < sizeof(stm_word_t) && size > 0; i++, size--)
      *buf++ = val.b[i];
  } else
    a = (stm_word_t *)addr;
  /* Full words */
  while (size >= sizeof(stm_word_t)) {
#ifdef ALLOW_MISALIGNED_ACCESSES
    *((stm_word_t *)buf) = stm_load(TXARGS a++);
    buf += sizeof(stm_word_t);
#else /* ! ALLOW_MISALIGNED_ACCESSES */
    val.w = stm_load(TXARGS a++);
    for (i = 0; i < sizeof(stm_word_t); i++)
      *buf++ = val.b[i];
#endif /* ! ALLOW_MISALIGNED_ACCESSES */
    size -= sizeof(stm_word_t);
  }
  if (size > 0) {
    /* Last bytes */
    val.w = stm_load(TXARGS a);
    i = 0;
    for (i = 0; size > 0; i++, size--)
      *buf++ = val.b[i];
  }
}

/* ################################################################### *
 * STORES
 * ################################################################### */

void stm_store8(TXPARAMS volatile uint8_t *addr, uint8_t value)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val, mask;
    val.u8[(uintptr_t)addr & 0x03] = value;
    mask.u32 = 0;
    mask.u8[(uintptr_t)addr & 0x03] = ~(uint8_t)0;
    stm_store2(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03), (stm_word_t)val.u32, (stm_word_t)mask.u32);
  } else {
    convert_64_t val, mask;
    val.u8[(uintptr_t)addr & 0x07] = value;
    mask.u64 = 0;
    mask.u8[(uintptr_t)addr & 0x07] = ~(uint8_t)0;
    stm_store2(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

void stm_store16(TXPARAMS volatile uint16_t *addr, uint16_t value)
{
  if (((uintptr_t)addr & 0x01) != 0) {
    stm_store_bytes(TXARGS (volatile uint8_t *)addr, (uint8_t *)&value, sizeof(uint16_t));
  } else if (sizeof(stm_word_t) == 4) {
    convert_32_t val, mask;
    val.u16[((uintptr_t)addr & 0x03) >> 1] = value;
    mask.u32 = 0;
    mask.u16[((uintptr_t)addr & 0x03) >> 1] = ~(uint16_t)0;
    stm_store2(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03), (stm_word_t)val.u32, (stm_word_t)mask.u32);
  } else {
    convert_64_t val, mask;
    val.u16[((uintptr_t)addr & 0x07) >> 1] = value;
    mask.u64 = 0;
    mask.u16[((uintptr_t)addr & 0x07) >> 1] = ~(uint16_t)0;
    stm_store2(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

void stm_store32(TXPARAMS volatile uint32_t *addr, uint32_t value)
{
  if (((uintptr_t)addr & 0x03) != 0) {
    stm_store_bytes(TXARGS (volatile uint8_t *)addr, (uint8_t *)&value, sizeof(uint32_t));
  } else if (sizeof(stm_word_t) == 4) {
    stm_store(TXARGS (volatile stm_word_t *)addr, (stm_word_t)value);
  } else {
    convert_64_t val, mask;
    val.u32[((uintptr_t)addr & 0x07) >> 2] = value;
    mask.u64 = 0;
    mask.u32[((uintptr_t)addr & 0x07) >> 2] = ~(uint32_t)0;
    stm_store2(TXARGS (volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

void stm_store64(TXPARAMS volatile uint64_t *addr, uint64_t value)
{
  if (((uintptr_t)addr & 0x07) != 0) {
    stm_store_bytes(TXARGS (volatile uint8_t *)addr, (uint8_t *)&value, sizeof(uint64_t));
  } else if (sizeof(stm_word_t) == 4) {
    convert_64_t val;
    val.u64 = value;
    stm_store(TXARGS (volatile stm_word_t *)addr, (stm_word_t)val.u32[0]);
    stm_store(TXARGS (volatile stm_word_t *)addr + 1, (stm_word_t)val.u32[1]);
  } else {
    return stm_store(TXARGS (volatile stm_word_t *)addr, (stm_word_t)value);
  }
}

void stm_store_char(TXPARAMS volatile char *addr, char value)
{
  convert_8_t val;
  val.s8 = value;
  stm_store8(TXARGS (volatile uint8_t *)addr, val.u8);
}

void stm_store_uchar(TXPARAMS volatile unsigned char *addr, unsigned char value)
{
  stm_store8(TXARGS (volatile uint8_t *)addr, (uint8_t)value);
}

void stm_store_short(TXPARAMS volatile short *addr, short value)
{
  convert_16_t val;
  val.s16 = value;
  stm_store16(TXARGS (volatile uint16_t *)addr, val.u16);
}

void stm_store_ushort(TXPARAMS volatile unsigned short *addr, unsigned short value)
{
  stm_store16(TXARGS (volatile uint16_t *)addr, (uint16_t)value);
}

void stm_store_int(TXPARAMS volatile int *addr, int value)
{
  convert_32_t val;
  val.s32 = value;
  stm_store32(TXARGS (volatile uint32_t *)addr, val.u32);
}

void stm_store_uint(TXPARAMS volatile unsigned int *addr, unsigned int value)
{
  stm_store32(TXARGS (volatile uint32_t *)addr, (uint32_t)value);
}

void stm_store_long(TXPARAMS volatile long *addr, long value)
{
  if (sizeof(long) == 4) {
    convert_32_t val;
    val.s32 = value;
    stm_store32(TXARGS (volatile uint32_t *)addr, val.u32);
  } else {
    convert_64_t val;
    val.s64 = value;
    stm_store64(TXARGS (volatile uint64_t *)addr, val.u64);
  }
}

void stm_store_ulong(TXPARAMS volatile unsigned long *addr, unsigned long value)
{
  if (sizeof(long) == 4) {
    stm_store32(TXARGS (volatile uint32_t *)addr, (uint32_t)value);
  } else {
    stm_store64(TXARGS (volatile uint64_t *)addr, (uint64_t)value);
  }
}

void stm_store_float(TXPARAMS volatile float *addr, float value)
{
  convert_32_t val;
  val.f = value;
  stm_store32(TXARGS (volatile uint32_t *)addr, val.u32);
}

void stm_store_double(TXPARAMS volatile double *addr, double value)
{
  convert_64_t val;
  val.d = value;
  stm_store64(TXARGS (volatile uint64_t *)addr, val.u64);
}

void stm_store_ptr(TXPARAMS volatile void **addr, void *value)
{
  union { stm_word_t w; void *v; } convert;
  convert.v = value;
  stm_store(TXARGS (stm_word_t *)addr, convert.w);
}

void stm_store_bytes(TXPARAMS volatile uint8_t *addr, uint8_t *buf, size_t size)
{
  convert_t val, mask;
  unsigned int i;
  stm_word_t *a;

  if (size == 0)
    return;
  i = (uintptr_t)addr & (sizeof(stm_word_t) - 1);
  if (i != 0) {
    /* First bytes */
    a = (stm_word_t *)((uintptr_t)addr & ~(uintptr_t)(sizeof(stm_word_t) - 1));
    val.w = mask.w = 0;
    for (; i < sizeof(stm_word_t) && size > 0; i++, size--) {
      mask.b[i] = 0xFF;
      val.b[i] = *buf++;
    }
    stm_store2(TXARGS a++, val.w, mask.w);
  } else
    a = (stm_word_t *)addr;
  /* Full words */
  while (size >= sizeof(stm_word_t)) {
#ifdef ALLOW_MISALIGNED_ACCESSES
    stm_store(TXARGS a++, *((stm_word_t *)buf));
    buf += sizeof(stm_word_t);
#else /* ! ALLOW_MISALIGNED_ACCESSES */
    for (i = 0; i < sizeof(stm_word_t); i++)
      val.b[i] = *buf++;
    stm_store(TXARGS a++, val.w);
#endif /* ! ALLOW_MISALIGNED_ACCESSES */
    size -= sizeof(stm_word_t);
  }
  if (size > 0) {
    /* Last bytes */
    val.w = mask.w = 0;
    for (i = 0; size > 0; i++, size--) {
      mask.b[i] = 0xFF;
      val.b[i] = *buf++;
    }
    stm_store2(TXARGS a, val.w, mask.w);
  }
}
