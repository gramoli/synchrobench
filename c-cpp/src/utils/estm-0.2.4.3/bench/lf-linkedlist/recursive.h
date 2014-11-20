/*
 *  recursive.h
 *  
 *
 *  Created by Vincent Gramoli on 1/13/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "harris.h"

/* Search the linked list recursively */

int rec_search(stm_word_t *start_ts, node_t *prev, node_t *next, 
			   val_t val, val_t *v);
int rec_search2(stm_word_t *start_ts, node_t **prev, node_t **next, 
				val_t val, val_t *v);
