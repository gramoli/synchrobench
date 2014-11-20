/*
 *  recursive.c
 *  
 *  Recursive implementation of the unit-transactional linked list.
 *
 *  Created by Vincent Gramoli on 1/13/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "recursive.h"

int rec_search(stm_word_t *start_ts, node_t *prev, node_t *next, 
			   val_t val, val_t *v) {
	stm_word_t ts, curr_ts;  
	node_t *curr;
	int result=0;
	
	*v = UNIT_LOAD(&next->val, &ts);
	/* Check next ts */
	if (ts > *start_ts || !next->next) {
		return (int) ts;
	}
	
	/* Is recursion over (ie. value reached)? */
	if (*v >= val)
		return 0;
	
	/* Check next of next ts */
	curr = next;
	next = (node_t *)UNIT_LOAD(&next->next, &ts);
	if (ts > *start_ts) {
		/* Re-check next ts */ 
		UNIT_LOAD(&curr->val, &curr_ts);
		if (curr_ts > *start_ts) {
			return (int) curr_ts;
		}
		*start_ts = ts;
	}
	
	/* If further checks failed, then re-check */
restart5:
	if (!(result = rec_search(start_ts, curr, next, val, v))) return 0;
	else { 
		UNIT_LOAD(&next, &ts);
		
		if (ts > *start_ts) return 1;
		else {
			*start_ts = result;
			goto restart5;
		}
	}
}


int rec_search2(stm_word_t *start_ts, node_t **prev, node_t **next, 
				val_t val, val_t *v) {
	stm_word_t ts, curr_ts; 
	int result=0;
	
	*v = UNIT_LOAD(&(*next)->val, &ts);
	/* Check next ts */
	if (ts > *start_ts || !(*next)->next) 
		return (int) ts;
	
	/* Is recursion over (ie. value reached)? */
	if (*v >= val)
		return 0;
	
	/* Check next of next ts */
	*prev = *next;
	*next = (node_t *)UNIT_LOAD(&(*next)->next, &ts);
	if (ts > *start_ts) {
		/* Re-check next ts */ 
		UNIT_LOAD(&(*prev)->val, &curr_ts);
		if (curr_ts > *start_ts) {
			return (int) curr_ts;
		}
		*start_ts = ts;
	}
	
	/* If further checks failed, then re-check */
restart4:
	if (!(result = rec_search2(start_ts, prev, next, val, v))) return 0;
	else { 
		//UNIT_LOAD(&(*next), &ts);
		//if (ts > *start_ts) return 1;
		if (result > *start_ts) return 1;
		else {
			*start_ts = result;
			goto restart4;
		}
	}
}
