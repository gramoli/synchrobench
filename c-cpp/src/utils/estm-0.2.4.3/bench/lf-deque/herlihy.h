/*
 *  harris.h
 *  
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "intset.h"

/* ################################################################### *
 * HARRIS' LINKED LIST
 * ################################################################### */

int is_marked_ref(long i);

long unset_mark(long i);

long set_mark(long i);

long get_unmarked_ref(long w);

long get_marked_ref(long w);

node_t *harris_search(intset_t *set, val_t val, node_t **left_node);

int harris_find(intset_t *set, val_t val);

int harris_insert(intset_t *set, val_t val);

int harris_delete(intset_t *set, val_t val);
