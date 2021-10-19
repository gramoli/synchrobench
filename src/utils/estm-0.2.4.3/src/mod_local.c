/*
 * File:
 *   mod_local.c
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "mod_local.h"

#include "stm.h"

#ifndef LW_SET_SIZE
# define LW_SET_SIZE                    1024
#endif /* ! LW_SET_SIZE */

/* ################################################################### *
 * TYPES
 * ################################################################### */

enum {
  TYPE_WORD,
  TYPE_CHAR,
  TYPE_UCHAR,
  TYPE_SHORT,
  TYPE_USHORT,
  TYPE_INT,
  TYPE_UINT,
  TYPE_LONG,
  TYPE_ULONG,
  TYPE_FLOAT,
  TYPE_DOUBLE
};

typedef struct w_entry {                /* Write set entry */
  int type;                             /* Data type */
  union {                               /* Address written and old value */
    struct { volatile char *a; char v; } c;
    struct { volatile unsigned char *a; unsigned char v; } uc;
    struct { volatile short *a; short v; } s;
    struct { volatile unsigned short *a; unsigned short v; } us;
    struct { volatile int *a; int v; } i;
    struct { volatile unsigned int *a; unsigned int v; } ui;
    struct { volatile long *a; long v; } l;
    struct { volatile unsigned long *a; unsigned long v; } ul;
    struct { volatile float *a; float v; } f;
    struct { volatile double *a; double v; } d;
    struct { volatile stm_word_t *a; stm_word_t v; } w;
  } data;
} w_entry_t;

typedef struct w_set {                  /* Write set */
  w_entry_t *entries;                   /* Array of entries */
  int nb_entries;                       /* Number of entries */
  int size;                             /* Size of array */
} w_set_t;

static int key;
static int initialized = 0;

/* ################################################################### *
 * STATIC
 * ################################################################### */

/*
 * Called by the CURRENT thread to write to local memory.
 */
static inline w_entry_t *get_entry(TXPARAM)
{
  w_set_t *ws;

  if (!initialized) {
    fprintf(stderr, "Module mod_local not initialized\n");
    exit(1);
  }

  /* Store in undo log */
  ws = (w_set_t *)stm_get_specific(TXARGS key);
  assert(ws != NULL);

  if (ws->nb_entries == ws->size) {
    /* Extend read set */
    ws->size = (ws->size < LW_SET_SIZE ? LW_SET_SIZE : ws->size * 2);
    if ((ws->entries = (w_entry_t *)realloc(ws->entries, ws->size * sizeof(w_entry_t))) == NULL) {
      perror("realloc");
      exit(1);
    }
  }

  return &ws->entries[ws->nb_entries++];
}

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

void stm_store_local(TXPARAMS stm_word_t *addr, stm_word_t value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_WORD;
  w->data.w.a = addr;
  w->data.w.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_char(TXPARAMS char *addr, char value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_CHAR;
  w->data.c.a = addr;
  w->data.c.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_uchar(TXPARAMS unsigned char *addr, unsigned char value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_UCHAR;
  w->data.uc.a = addr;
  w->data.uc.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_short(TXPARAMS short *addr, short value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_SHORT;
  w->data.s.a = addr;
  w->data.s.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_ushort(TXPARAMS unsigned short *addr, unsigned short value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_USHORT;
  w->data.us.a = addr;
  w->data.us.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_int(TXPARAMS int *addr, int value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_INT;
  w->data.i.a = addr;
  w->data.i.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_uint(TXPARAMS unsigned int *addr, unsigned int value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_UINT;
  w->data.ui.a = addr;
  w->data.ui.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_long(TXPARAMS long *addr, long value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_LONG;
  w->data.l.a = addr;
  w->data.l.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_ulong(TXPARAMS unsigned long *addr, unsigned long value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_ULONG;
  w->data.ul.a = addr;
  w->data.ul.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_float(TXPARAMS float *addr, float value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_FLOAT;
  w->data.f.a = addr;
  w->data.f.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_double(TXPARAMS double *addr, double value)
{
  w_entry_t *w = get_entry(TXARG);

  w->type = TYPE_DOUBLE;
  w->data.d.a = addr;
  w->data.d.v = *addr;

  /* Write to memory */
  *addr = value;
}

void stm_store_local_ptr(TXPARAMS void **addr, void *value)
{
  union { stm_word_t w; void *v; } convert;
  convert.v = value;
  stm_store_local(TXARGS (stm_word_t *)addr, convert.w);
}

/*
 * Called upon thread creation.
 */
static void on_thread_init(TXPARAMS void *arg)
{
  w_set_t *ws;

  if ((ws = (w_set_t *)malloc(sizeof(w_set_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  ws->entries = NULL;
  ws->nb_entries = ws->size = 0;

  stm_set_specific(TXARGS key, ws);
}

/*
 * Called upon thread deletion.
 */
static void on_thread_exit(TXPARAMS void *arg)
{
  w_set_t *ws;

  ws = (w_set_t *)stm_get_specific(TXARGS key);
  assert(ws != NULL);

  free(ws->entries);
  free(ws);
}

/*
 * Called upon transaction commit.
 */
static void on_commit(TXPARAMS void *arg)
{
  w_set_t *ws;

  ws = (w_set_t *)stm_get_specific(TXARGS key);
  assert(ws != NULL);

  /* Erase undo log */
  ws->nb_entries = 0;
}

/*
 * Called upon transaction abort.
 */
static void on_abort(TXPARAMS void *arg)
{
  w_set_t *ws;
  int i;

  ws = (w_set_t *)stm_get_specific(TXARGS key);
  assert(ws != NULL);

  /* Apply undo log */
  for (i = ws->nb_entries - 1; i >= 0; i--) {
    switch (ws->entries[i].type) {
     case TYPE_WORD:
       *ws->entries[i].data.w.a = ws->entries[i].data.w.v;
       break;
     case TYPE_CHAR:
       *ws->entries[i].data.c.a = ws->entries[i].data.c.v;
       break;
     case TYPE_UCHAR:
       *ws->entries[i].data.uc.a = ws->entries[i].data.uc.v;
       break;
     case TYPE_SHORT:
       *ws->entries[i].data.s.a = ws->entries[i].data.s.v;
       break;
     case TYPE_USHORT:
       *ws->entries[i].data.us.a = ws->entries[i].data.us.v;
       break;
     case TYPE_INT:
       *ws->entries[i].data.i.a = ws->entries[i].data.i.v;
       break;
     case TYPE_UINT:
       *ws->entries[i].data.ui.a = ws->entries[i].data.ui.v;
       break;
     case TYPE_LONG:
       *ws->entries[i].data.l.a = ws->entries[i].data.l.v;
       break;
     case TYPE_ULONG:
       *ws->entries[i].data.ul.a = ws->entries[i].data.ul.v;
       break;
     case TYPE_FLOAT:
       *ws->entries[i].data.f.a = ws->entries[i].data.f.v;
       break;
     case TYPE_DOUBLE:
       *ws->entries[i].data.d.a = ws->entries[i].data.d.v;
       break;
     default:
       fprintf(stderr, "Unexpected entry in undo log\n");
       abort();
       exit(1);
    }
  }
}

/*
 * Initialize module.
 */
void mod_local_init()
{
  if (initialized)
    return;

  stm_register(on_thread_init, on_thread_exit, NULL, on_commit, on_abort, NULL);
  key = stm_create_specific();
  if (key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  initialized = 1;
}
