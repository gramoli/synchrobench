/*
 *  lf-sl.h
 *  
 *
 *  Created by Vincent Gramoli on 1/23/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */


/* ################################################################### *
 * HARRIS' LINKED LIST
 * ################################################################### */

int is_marked_ref(long i) {
	// 1 means true, 0 false
	return (int) (i &= LONG_MIN+1);
}

long unset_mark(long i) {
	i &= LONG_MAX-1;
	return i;
}

long set_mark(long i) {
	i = unset_mark(i);
	i += 1;
	return i;
}

long get_unmarked_ref(long w) {
	return unset_mark(w);
}

long get_marked_ref(long w) {
	return set_mark(w);
}

node_t *harris_search(intset_t *set, val_t val, node_t **left_node) {
	node_t *left_node_next, *right_node;
	left_node_next = set->head;
	
search_again:
	do {
		node_t *t = set->head;
		node_t *t_next = set->head->next;
		
		/* Find left_node and right_node */
		do {
			if (!is_marked_ref((long) t_next)) {
				(*left_node) = t;
				left_node_next = t_next;
			}
			t = (node_t *) get_unmarked_ref((long) t_next);
			if (!t->next) break;
			t_next = t->next;
		} while (is_marked_ref((long) t_next) || (t->val < val));
		right_node = t;
		
		/* Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if (right_node->next && is_marked_ref((long) right_node->next))
				goto search_again;
			else return right_node;
		}
		
		/* Remove one or more marked nodes */
		if (ATOMIC_CAS_MB(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if (right_node->next && is_marked_ref((long) right_node->next))
				goto search_again;
			else return right_node;
		} 
		
	} while (1);
}

int harris_find(intset_t *set, val_t val) {
	node_t *right_node, *left_node;
	left_node = set->head;
	
	right_node = harris_search(set, val, &left_node);
	if ((!right_node->next) || right_node->val != val)
		return 0;
	else 
		return 1;
}

int harris_insert(intset_t *set, val_t val) {
	node_t *newnode, *right_node, *left_node;
	left_node = set->head;
	
	do {
		right_node = harris_search(set, val, &left_node);
		if (right_node->val == val)
			return 0;
		newnode = new_node(val, right_node, 0);
		/* mem-bar between node creation and insertion */
		AO_nop_full(); 
		if (ATOMIC_CAS_MB(&left_node->next, right_node, newnode))
			return 1;
	} while(1);
}

int harris_delete(intset_t *set, val_t val) {
	node_t *right_node, *right_node_next, *left_node;
	left_node = set->head;
	
	do {
		right_node = harris_search(set, val, &left_node);
		if (right_node->val != val)
			return 0;
		right_node_next = right_node->next;
		if (!is_marked_ref((long) right_node_next))
			if (ATOMIC_CAS_MB(&right_node->next, 
							  right_node_next, 
							  get_marked_ref((long) right_node_next)))
				break;
	} while(1);
	if (!ATOMIC_CAS_MB(&left_node->next, right_node, right_node_next))
		right_node = harris_search(set, right_node->val, &left_node);
	return 1;
}

/* ################################################################### *
 * FRASER SKIPLIST
 * ################################################################### */

sl_node_t **fraser_search(sl_intset_t *set, val_t val, sl_node_t ***left_list) {
	int i;
	sl_node_t *left, *left_next, *right, *right_next, **right_list;
	
	*left_list = (sl_node_t **)malloc((levelmax+1) * sizeof(sl_node_t));
	right_list = (sl_node_t **)malloc((levelmax+1) * sizeof(sl_node_t));
	
#ifdef DEBUG
	printf("++> fraser_search\n");
	IO_FLUSH;
#endif
	
	//printf("entering search...\n");
retry:
	left = set->head; 
	for (i=left->toplevel-1; i>=0; i--){
		left_next = left->next[i];
		if (is_marked_ref((long) left_next)) goto retry;
		/* Find unmarked node pair at this level */
		for (right = left_next; ; right = right_next) {
			/* Skip a sequence of marked nodes */
			while(1) {
				right_next = right->next[i];
				if (!is_marked_ref((long) right_next)) break;
				right = (sl_node_t*) unset_mark((long) right_next);
			}
			if (right->val >= val) break;
			left = right; 
			left_next = right_next;
			//printf("leaving search loop\n");
		}
		/* Ensure left and right nodes are adjacent */
		if ((left_next != right) && 
			(!ATOMIC_CAS_MB(&left->next[i], left_next, right)))
			goto retry;
		(*left_list)[i] = left;
		right_list[i] = right;
	}
	//printf("leaving search...\n");
	return right_list;
}

int fraser_find(sl_intset_t *set, val_t val) {
	sl_node_t **left_list, **succs;
	
#ifdef DEBUG
	printf("++> fraser_find\n");
	IO_FLUSH;
#endif
	
	succs = fraser_search(set, val, &left_list);
	return (succs[0]->val == val);
}

/* loop that mark a logically-deleted node is placed in a separate 
 * function so that it can be used by fraser_update.
 */
void mark_node_ptrs(sl_node_t *n) {
	int i;
	sl_node_t *n_next;
	
	for (i=n->toplevel-1; i>=0; i--) {
		do {
			n_next = n->next[i];
			if (is_marked_ref((long) n_next)) break;
		} while (!ATOMIC_CAS_MB(&n->next[i], n_next, set_mark((long) n_next)));
		/* mark in order */
		AO_nop_full();
	}
}

int fraser_remove(sl_intset_t *set, val_t val) {
	sl_node_t **left_list, **succs;
	
#ifdef DEBUG
	printf("++> fraser_remove\n");
	IO_FLUSH;
#endif
	
	//printf("entering remove...\n");
	succs = fraser_search(set, val, &left_list);
	if (succs[0]->val != val) return 0;
	/* 1. Node is logically deleted when the value field is NULL */
	do {
		//if (!(val = succs[0]->val)) return 0;
		if (is_marked_ref((long)succs[0])) return 0;
		AO_nop_full();
	} while (!ATOMIC_CAS_MB(&succs[0]->val, val, NULL));    
	/* enforce above as linearization point */
	AO_nop_full();
	/* 2. Mark forward pointers, then search will remove the node */
	mark_node_ptrs(succs[0]);
	fraser_search(set, val, &left_list);    
	AO_nop_full();
	//printf("leaving remove...\n");
	return 1;
}

int fraser_update(sl_intset_t *set, val_t v1, val_t v2) {
	sl_node_t *new, *new_next, *pred, *succ, **succs, **preds;
	val_t old_v;
	int i;
	
#ifdef DEBUG
	printf("++> fraser_update\n");
	IO_FLUSH;
#endif
	
	succs = fraser_search(set, v1, &preds);
retry:
	/* Update the value field of an existing node */
	if (succs[0]->val == v1) {
		do {
			if (!(old_v = succs[0]->val)) {
				mark_node_ptrs(succs[0]);
				AO_nop_full();
				goto retry;
			}
		} while(!ATOMIC_CAS_MB(&succs[0]->val, old_v, v2));
		return 1;
	}
	new = sl_new_node(v2, NULL, get_rand_level(), 0);
	for (i=0; i<new->toplevel; i++) new->next[i] = succs[i];
	/* mem-bar between node creation and insertion */
	AO_nop_full(); 
	
	/* Node is visible once inserted at lowest level */
	if (!ATOMIC_CAS_MB(&preds[0]->next[0], succs[0], new)) 
		goto retry;
	for (i=1; i < new->toplevel; i++)
		while (1) {
			pred = preds[i];
			succ = succs[i];
			/* Update the forward pointer if it is stale */
			new_next  = new->next[i];
			AO_nop_full();
			if ((new_next != succ) && 
				(!ATOMIC_CAS_MB(&new->next[i], get_unmarked_ref((long) new_next), succ)))
				break; /* Give up if pointer is marked */
			/* Check for old reference to a k node */
			if (succ->val == v1) {
				/* get update view of the world */
				AO_nop_full();
				succ = (sl_node_t *) get_unmarked_ref((long) succ->next);
			}
			AO_nop_full();
			/* We retry the search if the CAS fails */
			if (ATOMIC_CAS_MB(&pred->next[i], succ, new)) break;
			/* ensure node is visible at all levels before punting deletion */
			AO_nop_full();
			succs = fraser_search(set, v1, &preds);
			AO_nop_full();
		}
	return 0; /* No existing value was replaced */
}

int fraser_insert(sl_intset_t *set, val_t val) {
	sl_node_t *new, *new_next, *pred, *succ, **succs, **preds;
	int i, level;
	
#ifdef DEBUG
	printf("++> fraser_insert\n");
	IO_FLUSH;
#endif
	
	if ((preds = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t))) == NULL || 
		(succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t))) == NULL) {
		perror("malloc");
		exit(1);
	}
	
	//printf("entering insert...\n");
	succs = fraser_search(set, val, &preds);
retry:
	/* Update the value field of an existing node */
	if (succs[0]->val == val) return 0;
	level = get_rand_level();
	new = sl_new_node(val, NULL, level, 0);
	/* mem-bar between node creation and insertion */
	AO_nop_full(); 
	for (i=0; i<new->toplevel; i++) new->next[i] = succs[i];
	AO_nop_full();
	
	/* Node is visible once inserted at lowest level */
	if (!ATOMIC_CAS_MB(&preds[0]->next[0], succs[0], new)) 
		goto retry;
	for (i=1; i < new->toplevel; i++)
		while (1) {
			pred = preds[i];
			succ = succs[i];
			/* Update the forward pointer if it is stale */
			new_next = new->next[i];
			AO_nop_full();
			if ((new_next != succ) && 
				(!ATOMIC_CAS_MB(&new->next[i], get_unmarked_ref((long) new_next), succ)))
				break; /* Give up if pointer is marked */
			/* Check for old reference to a k node */
			if (succ->val == val) {
				/* get update view of the world */
				AO_nop_full();
				succ = (sl_node_t *) get_unmarked_ref((long) succ->next);
				AO_nop_full();
			}
			/* We retry the search if the CAS fails */
			if (ATOMIC_CAS_MB(&pred->next[i], succ, new)) break;
			/* ensure node is visible at all levels before punting deletion */
			AO_nop_full();
			succs = fraser_search(set, val, &preds);
			AO_nop_full();
		}
	//printf("leaving insert...\n");
	return 1; /* No existing value was replaced */
}

/* ################################################################### *
 * RECURSIVE UNIT TX
 * ################################################################### */

/* Search the linked list recursively */

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

/* ################################################################### *
 * INTSET OPS
 * ################################################################### */

int set_contains(intset_t *set, val_t val, int transactional)
{
	int result;
	node_t *prev, *next;
	val_t v;
	stm_word_t ts, start_ts, prev_ts;
	
#ifdef DEBUG
	printf("++> set_contains(%d)\n", (int) val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
		case 0: /* Unprotected */
			prev = set->head;
			next = prev->next;
			while (next->val < val) {
				prev = next;
				next = prev->next;
			}
			result = (next->val == val);
			break;
			
		case 1: /* Normal transaction */
			START_RO;
			prev = (node_t *)LOAD(&set->head);
			next = (node_t *)LOAD(&prev->next);
			while (1) {
				v = LOAD(&next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)LOAD(&prev->next);
			}
			result = (v == val);
			COMMIT;
			break;
			
		case 2:
		case 3:
		case 4: /* Unit transaction */
		restart:
			start_ts = stm_get_clock();
			/* Head node is never removed */
			prev = (node_t *)UNIT_LOAD(&set->head, &ts);
			next = (node_t *)UNIT_LOAD(&prev->next, &ts);
			if (ts > start_ts)
				start_ts = ts;
			while (1) {
				v = UNIT_LOAD(&next->val, &ts);
				if (ts > start_ts) {
					/* Restart traversal */
					goto restart;
				}
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)UNIT_LOAD(&prev->next, &ts);
				if (ts > start_ts) {
					/* Verify that node has not been modified 
					 (value and pointer are updated together) */
					UNIT_LOAD(&prev->val, &prev_ts);
					if (prev_ts > start_ts) {
						/* Restart traversal */
						goto restart;
					}
					start_ts = ts;
				}
			}
			result = (v == val);
			break;
			
		case 5:
			do {
				start_ts = stm_get_clock();
				prev = (node_t *)UNIT_LOAD(&set->head, &ts); 
				next = (node_t *)UNIT_LOAD(&prev->next, &ts);
				if (ts > start_ts) start_ts = ts;
			} while (rec_search2(&start_ts, &prev, &next, val, &v));
			result = (v==val);
			break;
			
		default: /* harris lock-free */
			result = harris_find(set, val);
	}
	
	return result;
}

int set_add(intset_t *set, val_t val, int transactional)
{
	int result;
	node_t *prev, *next;
	val_t v;
	stm_word_t ts, start_ts, prev_ts;
	
#ifdef DEBUG
	printf("++> set_add(%d)\n", (int) val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
		case 0: /* Unprotected */
			prev = set->head;
			next = prev->next;
			while (next->val < val) {
				prev = next;
				next = prev->next;
			}
			result = (next->val != val);
			if (result) {
				prev->next = new_node(val, next, transactional);
			}
			break;
			
		case 1:
		case 2: /* Normal transaction */
			START;
			prev = (node_t *)LOAD(&set->head);
			next = (node_t *)LOAD(&prev->next);
			while (1) {
				v = LOAD(&next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)LOAD(&prev->next);
			}
			result = (v != val);
			if (result) {
				STORE(&prev->next, new_node(val, next, transactional));
			}
			COMMIT;
			break;
			
		case 3:
		case 4: /* Unit transaction */
		restart:
			start_ts = stm_get_clock();
			/* Head node is never removed */
			prev = (node_t *)UNIT_LOAD(&set->head, &ts);
			next = (node_t *)UNIT_LOAD(&prev->next, &ts);
			if (ts > start_ts)
				start_ts = ts;
			while (1) {
				v = UNIT_LOAD(&next->val, &ts);
				if (ts > start_ts) {
					/* Restart traversal */
					goto restart;
				}
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)UNIT_LOAD(&prev->next, &ts);
				if (ts > start_ts) {
					/* Verify that node has not been modified 
					 (value and pointer are updated together) */
					UNIT_LOAD(&prev->val, &prev_ts);
					if (prev_ts > start_ts) {
						/* Restart traversal */
						goto restart;
					}
					start_ts = ts;
				}
			}
			result = (v != val);	
			if (result) {
				/* Make sure that the transaction does not access 
				 versions more recent than start_ts */
				START_TS(start_ts, restart);
				STORE(&prev->next, new_node(val, next, transactional));
				COMMIT;
			}
			break;
			
		case 5:
		restart2:
			do {
				start_ts = stm_get_clock();
				prev = (node_t *)UNIT_LOAD(&set->head, &ts);
				next = (node_t *)UNIT_LOAD(&prev->next, &ts);
				if (ts > start_ts) start_ts = ts;
			} while (rec_search2(&start_ts, &prev, &next, val, &v));
			result = (v != val);
			/*if (result) {
			 START_TS(start_ts, restart2);
			 STORE(&prev->next, new_node(val, next, transactional));
			 COMMIT;
			 }    */
			if (result) {
				START_TS(start_ts, restart2);
				STORE(&prev->next, new_node(val, next, transactional));
				COMMIT;
			}    
			break;
			
		default: /* harris lock-free */
			result = harris_insert(set, val);
	}
	
	return result;
}

int set_remove(intset_t *set, val_t val, int transactional)
{
	int result;
	node_t *prev, *next;
	val_t v;
	node_t *n;
	stm_word_t ts, start_ts, prev_ts;
	
#ifdef DEBUG
	printf("++> set_remove(%d)\n", (int) val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
		case 0: /* Unprotected */
			prev = set->head;
			next = prev->next;
			while (next->val < val) {
				prev = next;
				next = prev->next;
			}
			result = (next->val == val);
			if (result) {
				prev->next = next->next;
				free(next);
			}
			break;
			
		case 1:
		case 2:
		case 3: /* Normal transaction */
			START;
			prev = (node_t *)LOAD(&set->head);
			next = (node_t *)LOAD(&prev->next);
			while (1) {
				v = LOAD(&next->val);
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)LOAD(&prev->next);
			}
			result = (v == val);
			if (result) {
				n = (node_t *)LOAD(&next->next);
				STORE(&prev->next, n);
				/* Free memory (delayed until commit) */
				FREE(next, sizeof(node_t));
			}
			COMMIT;
			break;
			
		case 4: /* Unit transaction */
		restart:
			start_ts = stm_get_clock();
			/* Head node is never removed */
			prev = (node_t *)UNIT_LOAD(&set->head, &ts);
			next = (node_t *)UNIT_LOAD(&prev->next, &ts);
			if (ts > start_ts)
				start_ts = ts;
			while (1) {
				v = UNIT_LOAD(&next->val, &ts);
				if (ts > start_ts) {
					/* Restart traversal */
					goto restart;
				}
				if (v >= val)
					break;
				prev = next;
				next = (node_t *)UNIT_LOAD(&prev->next, &ts);
				if (ts > start_ts) {
					/* Verify that node has not been modified 
					 (value and pointer are updated together) */
					UNIT_LOAD(&prev->val, &prev_ts);
					if (prev_ts > start_ts) {
						/* Restart traversal */
						goto restart;
					}
					start_ts = ts;
				}
			}
			result = (v == val);
			if (result) {
				/* Make sure that the transaction does not access 
				 versions more recent than start_ts */
				START_TS(start_ts, restart);
				n = (node_t *)LOAD(&next->next);
				STORE(&prev->next, n);
				/* Free memory (delayed until commit) */
				FREE(next, sizeof(node_t));
				COMMIT;
			}
			break;
			
		case 5:
		restart3:
			do {
				start_ts = stm_get_clock();
				prev = (node_t *)UNIT_LOAD(&set->head, &ts);
				next = (node_t *)UNIT_LOAD(&prev->next, &ts);
				if (ts > start_ts) start_ts = ts;
			} while (rec_search2(&start_ts, &prev, &next, val, &v));
			result = (v == val);
			if (result) {
				START_TS(start_ts, restart3);
				n = (node_t *)LOAD(&next->next);
				STORE(&prev->next, n);
				/* Check if there is a way to free memory here */
				FREE(next, sizeof(node_t));
				COMMIT;
			}
			break;
			
		default: /* harris lock-free */
			result = harris_delete(set, val);
	}
	
	return result;
}

/* ################################################################### *
 * SKIPLIST OPS
 * ################################################################### */

int sl_contains(sl_intset_t *set, val_t val, int transactional)
{
	int result = 0;
	int i;
	sl_node_t *node, *next;
	val_t v;
	stm_word_t ts, start_ts, prev_ts;
	
#ifdef DEBUG
	printf("++> sl_contains(%d)\n", (int) val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
			
		case 0: /* Unprotected */
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = node->next[i];
				while (next->val < val) {
					node = next;
					next = node->next[i];
				}
			}
			node = node->next[0];
			result = (node->val == val);
			break;
			
		case 1: /* Normal transaction */
			START_RO;
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)LOAD(&node->next[i]);
				while ((v = LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)LOAD(&node->next[i]);
				}
			}
			node = (sl_node_t *)LOAD(&node->next[0]);
			result = (v == val);
			COMMIT;
			break;
			
		case 2:
		case 3:
		case 4: /* Unit transaction */
			v = VAL_MIN;
		restart:
			start_ts = stm_get_clock();
			node = set->head; // remove the unit-load of head in linked-list 
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)UNIT_LOAD(&node->next[i], &ts);
				while (1) { 
					v = UNIT_LOAD(&next->val, &ts); 
					if (ts > start_ts) {
						// Restart traversal 
						goto restart;
					}
					if (v >= val) break;
					node = next;
					next = (sl_node_t *)UNIT_LOAD(&node->next[i], &ts);
					if (ts > start_ts) {
						// Verify that node has not been modified 
						// (value and pointer are updated together)
						UNIT_LOAD(&node->val, &prev_ts);
						if (prev_ts > start_ts) {
							// Restart traversal 
							goto restart;
						}
						start_ts = ts;
					}
				}
			}
			result = (v == val);
			break;
			
		case 5:
			break;
			
		case 6: /* fraser lock-free */
			result = fraser_find(set, val);
			break;
			
		default:
			printf("number %d do not correspond to any unit-tx mode.\n", transactional);
	}
	return result;
}

int sl_add(sl_intset_t *set, val_t val, int transactional)
{
	int result = 0;
	int i, l;
	sl_node_t *node, *next; //**preds, **succs;
	sl_node_t *preds[16], *succs[16];
	val_t v;  
	stm_word_t ts, start_ts, prev_ts;
	
#ifdef DEBUG
	printf("++> sl_add(%d)\n", (int) val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
			
		case 0: /* Unprotected */
			/*if ((preds = (sl_node_t **)malloc((levelmax+1) * sizeof(sl_node_t))) == NULL 
			 || (succs = (sl_node_t **)malloc((levelmax+1) * sizeof(sl_node_t))) == NULL) 
			 {
			 perror("malloc");
			 exit(1);
			 }*/
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = node->next[i];
				while (next->val < val) {
					node = next;
					next = node->next[i];
				}
				preds[i] = node;
				succs[i] = node->next[i];
			}
			node = node->next[0];
			if (node->val == val) {
				result = 0;
			} else {
				l = get_rand_level();
				node = sl_new_simple_node(val, l, transactional);
				for (i = 0; i < l; i++) {
					node->next[i] = succs[i];
					preds[i]->next[i] = node;
				}
				result = 1;
			}
			break;
			
		case 1:
		case 2: /* Normal transaction */
			START;
			v = VAL_MIN;
			/*if ((preds = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t))) == NULL ||
			 (succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t))) == NULL) {
			 perror("malloc");
			 exit(1);
			 }*/
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)LOAD(&node->next[i]);
				while ((v = LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)LOAD(&node->next[i]);
				}
				preds[i] = node;
				succs[i] = next;
			}
			if (v == val) {
				result = 0;
			} else {
				l = get_rand_level();
				node = sl_new_simple_node(val, l, transactional);
				for (i = 0; i < l; i++) {
					node->next[i] = (sl_node_t *)LOAD(&succs[i]);	
					STORE(&preds[i]->next[i], node);
				}
				result = 1;
			}
			COMMIT;
			break;
			
		case 3:
		case 4:  /* Unit transaction */
			v = VAL_MIN;
		restart:
			start_ts = stm_get_clock();
			node = set->head; // remove the unit-load of head in linked-list 
			//preds[set->head->toplevel-1] = set->head;
			//if (ts > start_ts) start_ts = ts;
			for (i = node->toplevel-1; i >= 0; i--) { 
				//for (i = set->head->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)UNIT_LOAD(&node->next[i], &ts);
				//succs[i] = (sl_node_t *)UNIT_LOAD(&preds[i]->next[i], &ts);
				while (1) { 
					v = UNIT_LOAD(&next->val, &ts); 
					//v = UNIT_LOAD(&succs[i]->val, &ts);
					if (ts > start_ts) goto restart;
					if (v >= val) break;
					node = next;
					//preds[i] = succs[i];
					next = (sl_node_t *)UNIT_LOAD(&node->next[i], &ts);
					//succs[i] = (sl_node_t *)UNIT_LOAD(&preds[i]->next[i], &ts);
					if (ts > start_ts) {
						//UNIT_LOAD(&succs[i]->val, &prev_ts);
						UNIT_LOAD(&node->val, &prev_ts);
						if (prev_ts > start_ts) goto restart;
						start_ts = ts;
					}
				}
				preds[i] = node;
				succs[i] = next;
			}
			
			if (v == val) {
				result = 0;
			} else {
				l = get_rand_level();
				node = sl_new_simple_node(val, l, transactional);
				START_TS(start_ts, restart);
				// double loop for rapid inlined memory accesses
				for (i = 0; i < l; i++) {	
					node->next[i] = (sl_node_t *)LOAD(&succs[i]);
				}
				for (i = 0; i < l; i++) {	
					STORE(&preds[i]->next[i], node);
				}
				COMMIT;
				result = 1;
			}
			break;
			
		case 5: /* Recursive unit-tx */
			break;
			
		case 6: /* fraser lock-free */
			result = fraser_insert(set, val);
			break;
			
		default:
			printf("number %d do not correspond to any unit-tx mode.\n", transactional);
	}
	
	return result;
}

int sl_remove(sl_intset_t *set, val_t val, int transactional)
{
	int result = 0;
	int i;
	sl_node_t *node, *next;
	sl_node_t *preds[16], *succs[16];
	val_t v;  
	stm_word_t ts, start_ts, prev_ts;
	
#ifdef DEBUG
	printf("++> sl_remove(%d)\n", (int) val);
	IO_FLUSH;
#endif
	
	switch(transactional) {
			
		case 0: /* Unprotected */
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = node->next[i];
				while (next->val < val) {
					node = next;
					next = node->next[i];
				}
				preds[i] = node;
				succs[i] = node->next[i];
			}
			node = node->next[0];
			
			if (node->val != val) {
				result = 0;
			} else {
				for (i = 0; i < set->head->toplevel; i++) 
					preds[i]->next[i] = succs[i];
				result = 1;
			}
			break;
			
		case 1:
		case 2: 
		case 3: /* Normal transaction */
			START;
			v = VAL_MIN;
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)LOAD(&node->next[i]);
				while ((v = LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)LOAD(&node->next[i]);
				}
				// unnecessary duplicate assignation
				preds[i] = node;
				succs[i] = next;
			}
			node = (sl_node_t *)LOAD(&node->next[0]);
			
			if (v != val) {
				result = 0;
			} else {
				for (i = 0; i < set->head->toplevel; i++) 
					//STORE(&preds[i]->next[i], (sl_node_t *)LOAD(&node->next[i]));
					STORE(&preds[i]->next[i], (sl_node_t *)LOAD(&succs[i]));
				result = 1;
			}
			COMMIT;
			break;
			
		case 4: /* Unit transaction */ 
			v = VAL_MIN;
		restart:
			start_ts = stm_get_clock();
			node = set->head; /* remove the unit-load of head in linked-list */
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)UNIT_LOAD(&node->next[i], &ts);
				if (ts > start_ts) start_ts = ts;
				while (1) { 
					v = UNIT_LOAD(&next->val, &ts); 
					if (ts > start_ts) goto restart;
					if (v >= val) break;
					node = next;
					next = (sl_node_t *)UNIT_LOAD(&node->next[i], &ts);
					if (ts > start_ts) {
						UNIT_LOAD(&node->val, &prev_ts);
						if (prev_ts > start_ts) goto restart;
						start_ts = ts;
					}
				}
				preds[i] = node;
				succs[i] = next;
			}
			
			if (node->val != val) {
				result = 0;
			} else {
				START_TS(start_ts, restart);
				for (i = 0; i < set->head->toplevel; i++) 
					STORE(&preds[i]->next[i], (sl_node_t *)LOAD(&succs[i]));
				COMMIT;
				result = 1;
			}
			break;
			
		case 5: /* Recursive unit-tx */
			break;
			
		case 6: /* fraser lock-free */
			result = fraser_remove(set, val);
			break;
			
		default:
			printf("number %d do not correspond to any unit-tx mode.\n", transactional);
	}
	
	return result;
}


#endif /* USE_RBTREE */

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct barrier {
	pthread_cond_t complete;
	pthread_mutex_t mutex;
	int count;
	int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n)
{
	pthread_cond_init(&b->complete, NULL);
	pthread_mutex_init(&b->mutex, NULL);
	b->count = n;
	b->crossing = 0;
}

void barrier_cross(barrier_t *b)
{
	pthread_mutex_lock(&b->mutex);
	/* One more thread through */
	b->crossing++;
	/* If not all here, wait */
	if (b->crossing < b->count) {
		pthread_cond_wait(&b->complete, &b->mutex);
	} else {
		pthread_cond_broadcast(&b->complete);
		/* Reset for next time */
		b->crossing = 0;
	}
	pthread_mutex_unlock(&b->mutex);
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

typedef struct thread_data {
	int range;
	int update;
#ifndef USE_RBTREE
	int unit_tx;
#endif
	unsigned long nb_add;
	unsigned long nb_remove;
	unsigned long nb_contains;
	unsigned long nb_found;
	unsigned long nb_aborts;
	unsigned long nb_aborts_locked_read;
	unsigned long nb_aborts_locked_write;
	unsigned long nb_aborts_validate_read;
	unsigned long nb_aborts_validate_write;
	unsigned long nb_aborts_validate_commit;
	unsigned long nb_aborts_invalid_memory;
	unsigned long max_retries;
	int diff;
	unsigned int seed;
	sl_intset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;

void *test(void *data)
{
	int val, last = 0;
	thread_data_t *d = (thread_data_t *)data;
	
#ifdef TLS
	rng_seed = &d->seed;
#else /* ! TLS */
	pthread_setspecific(rng_seed_key, &d->seed);
#endif /* ! TLS */
	
	/* Create transaction */
	stm_init_thread();
	/* Wait on barrier */
	barrier_cross(d->barrier);
	
	last = -1;
	while (stop == 0) {
		val = rand_r(&d->seed) % 100;
		if (val < d->update) {
			if (last < 0) {
				/* Add random value */
				val = (rand_r(&d->seed) % d->range) + 1;
				if (sl_add(d->set, val, TRANSACTIONAL)) {
					d->diff++;
					last = val;
				}
				d->nb_add++;
			} else {
				/* Remove last value */
				if (sl_remove(d->set, last, TRANSACTIONAL))
					d->diff--;
				d->nb_remove++;
				last = -1;
			}
		} else {
			/* Look for random value */
			val = (rand_r(&d->seed) % d->range) + 1;
			if (sl_contains(d->set, val, TRANSACTIONAL))
				d->nb_found++;
			d->nb_contains++;
		}
	}
	stm_get_parameter("nb_aborts", &d->nb_aborts);
	stm_get_parameter("nb_aborts_locked_read", &d->nb_aborts_locked_read);
	stm_get_parameter("nb_aborts_locked_write", &d->nb_aborts_locked_write);
	stm_get_parameter("nb_aborts_validate_read", &d->nb_aborts_validate_read);
	stm_get_parameter("nb_aborts_validate_write", &d->nb_aborts_validate_write);
	stm_get_parameter("nb_aborts_validate_commit", &d->nb_aborts_validate_commit);
	stm_get_parameter("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory);
	stm_get_parameter("max_retries", &d->max_retries);
	stm_get_parameter("failures_because_contention", &d->failures_because_contention);
	/* Free transaction */
	stm_exit_thread();
	
	return NULL;
}

void catcher(int sig)
{
	printf("CAUGHT SIGNAL %d\n", sig);
}

void print_skiplist(sl_intset_t *set) {
	sl_node_t *curr;
	int i, j;
	int arr[levelmax];
	
	for (i=0; i< sizeof arr/sizeof arr[0]; i++) arr[i] = 0;
	
	curr = set->head;
	do {
		printf("%d", (int) curr->val);
		for (i=0; i<curr->toplevel; i++) {
			printf("-*");
		}
		arr[curr->toplevel-1]++;
		printf("\n");
		curr = curr->next[0];
	} while (curr); 
	for (j=0; j<levelmax; j++)
		printf("%d nodes of level %d\n", arr[j], j);
}

int main(int argc, char **argv)
{
	struct option long_options[] = {
		// These options don't set a flag
		{"help",                      no_argument,       NULL, 'h'},
		{"duration",                  required_argument, NULL, 'd'},
		{"initial-size",              required_argument, NULL, 'i'},
		{"num-threads",               required_argument, NULL, 'n'},
		{"range",                     required_argument, NULL, 'r'},
		{"seed",                      required_argument, NULL, 's'},
		{"update-rate",               required_argument, NULL, 'u'},
		{"unit-tx",                   required_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
	
	sl_intset_t *set;
	int i, c, val, size;
	char *s;
	unsigned long reads, updates, aborts, aborts_locked_read, aborts_locked_write,
    aborts_validate_read, aborts_validate_write, aborts_validate_commit,
    aborts_invalid_memory, max_retries, failures_because_contention;
	thread_data_t *data;
	pthread_t *threads;
	pthread_attr_t attr;
	barrier_t barrier;
	struct timeval start, end;
	struct timespec timeout;
	int duration = DEFAULT_DURATION;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	int range = DEFAULT_RANGE;
	int seed = DEFAULT_SEED;
	int update = DEFAULT_UPDATE;
#ifndef USE_RBTREE
	int unit_tx = 1;
#endif
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hd:i:n:r:s:u:"
#ifndef USE_RBTREE
						"x:"
#endif
						, long_options, &i);
		
		if(c == -1)
			break;
		
		if(c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;
		
		switch(c) {
			case 0:
				break;
			case 'h':
				printf("intset -- STM stress test "
#ifdef USE_RBTREE
					   "(red-black tree)\n"
#else
					   "(linked list)\n"
#endif
					   "\n"
					   "Usage:\n"
					   "  intset [options...]\n"
					   "\n"
					   "Options:\n"
					   "  -h, --help\n"
					   "        Print this message\n"
					   "  -d, --duration <int>\n"
					   "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
					   "  -i, --initial-size <int>\n"
					   "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
					   "  -n, --num-threads <int>\n"
					   "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
					   "  -r, --range <int>\n"
					   "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
					   "  -s, --seed <int>\n"
					   "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
					   "  -u, --update-rate <int>\n"
					   "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
#ifndef USE_RBTREE
					   "  -x, --unit-tx (default=1)\n"
					   "        Use unit transactions\n"
					   "        0 = non-protected,\n"
					   "        1 = normal transaction,\n"
					   "        2 = read unit-tx,\n"
					   "        3 = read/add unit-tx,\n"
					   "        4 = read/add/rem unit-tx,\n"
					   "        5 = all recursive unit-tx,\n"
					   "        6 = harris lock-free\n"
#endif
					   );
				exit(0);
			case 'd':
				duration = atoi(optarg);
				break;
			case 'i':
				initial = atoi(optarg);
				break;
			case 'n':
				nb_threads = atoi(optarg);
				break;
			case 'r':
				range = atoi(optarg);
				break;
			case 's':
				seed = atoi(optarg);
				break;
			case 'u':
				update = atoi(optarg);
				break;
#ifndef USE_RBTREE
			case 'x':
				unit_tx = atoi(optarg);
				break;
#endif
			case '?':
				printf("Use -h or --help for help\n");
				exit(0);
			default:
				exit(1);
		}
	}
	
	assert(duration >= 0);
	assert(initial >= 0);
	assert(nb_threads > 0);
	assert(range > 0 && range >= initial);
	assert(update >= 0 && update <= 100);
	
#ifdef USE_RBTREE
	printf("Set type     : red-black tree\n");
#else
	printf("Set type     : skip list\n");
#endif
	printf("Duration     : %d\n", duration);
	printf("Initial size : %u\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Value range  : %d\n", range);
	printf("Seed         : %d\n", seed);
	printf("Update rate  : %d\n", update);
#ifndef USE_RBTREE
	printf("Unit tx      : %d\n", unit_tx);
#endif
	printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
		   (int)sizeof(int),
		   (int)sizeof(long),
		   (int)sizeof(void *),
		   (int)sizeof(stm_word_t));
	
	timeout.tv_sec = duration / 1000;
	timeout.tv_nsec = (duration % 1000) * 1000000;
	
	if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
		perror("malloc");
		exit(1);
	}
	if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
		perror("malloc");
		exit(1);
	}
	
	if (seed == 0)
		srand((int)time(0));
	else
		srand(seed);
	
	levelmax = floor_log_2((unsigned int) initial);
	set = sl_set_new();
	stop = 0;
	
	global_seed = rand();
#ifdef TLS
	rng_seed = &global_seed;
#else /* ! TLS */
	if (pthread_key_create(&rng_seed_key, NULL) != 0) {
		fprintf(stderr, "Error creating thread local\n");
		exit(1);
	}
	pthread_setspecific(rng_seed_key, &global_seed);
#endif /* ! TLS */
	
	// Init STM 
	printf("Initializing STM\n");
	stm_init();
	mod_mem_init();
	
	if (stm_get_parameter("compile_flags", &s))
		printf("STM flags    : %s\n", s);
	
	// Populate set 
	printf("Adding %d entries to set\n", initial);
	i = 0;
	while (i < initial) {
		val = (rand() % range) + 1;
		if (sl_add(set, val, 0))
			i++;
	}
	size = sl_set_size(set);
	printf("Set size     : %d\n", size);
	printf("Level max    : %d\n", levelmax);
	
	// Access set from all threads 
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);
		data[i].range = range;
		data[i].update = update;
#ifndef USE_RBTREE
		data[i].unit_tx = unit_tx;
#endif
		data[i].nb_add = 0;
		data[i].nb_remove = 0;
		data[i].nb_contains = 0;
		data[i].nb_found = 0;
		data[i].nb_aborts = 0;
		data[i].nb_aborts_locked_read = 0;
		data[i].nb_aborts_locked_write = 0;
		data[i].nb_aborts_validate_read = 0;
		data[i].nb_aborts_validate_write = 0;
		data[i].nb_aborts_validate_commit = 0;
		data[i].nb_aborts_invalid_memory = 0;
		data[i].max_retries = 0;
		data[i].diff = 0;
		data[i].seed = rand();
		data[i].set = set;
		data[i].barrier = &barrier;
		data[i].failures_because_contention = 0;
		if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
			fprintf(stderr, "Error creating thread\n");
			exit(1);
		}
	}
	pthread_attr_destroy(&attr);
	
	// Catch some signals 
	if (signal(SIGHUP, catcher) == SIG_ERR ||
		signal(SIGINT, catcher) == SIG_ERR ||
		signal(SIGTERM, catcher) == SIG_ERR) {
		perror("signal");
		exit(1);
	}
	
	// Start threads 
	barrier_cross(&barrier);
	
	printf("STARTING...\n");
	gettimeofday(&start, NULL);
	if (duration > 0) {
		nanosleep(&timeout, NULL);
	} else {
		sigemptyset(&block_set);
		sigsuspend(&block_set);
	}
	AO_store_full(&stop, 1);
	gettimeofday(&end, NULL);
	printf("STOPPING...\n");
	
	// Wait for thread completion 
	for (i = 0; i < nb_threads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for thread completion\n");
			exit(1);
		}
	}
	
	duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
	aborts = 0;
	aborts_locked_read = 0;
	aborts_locked_write = 0;
	aborts_validate_read = 0;
	aborts_validate_write = 0;
	aborts_validate_commit = 0;
	aborts_invalid_memory = 0;
	failures_because_contention = 0;
	reads = 0;
	updates = 0;
	max_retries = 0;
	for (i = 0; i < nb_threads; i++) {
		printf("Thread %d\n", i);
		printf("  #add        : %lu\n", data[i].nb_add);
		printf("  #remove     : %lu\n", data[i].nb_remove);
		printf("  #contains   : %lu\n", data[i].nb_contains);
		printf("  #found      : %lu\n", data[i].nb_found);
		printf("  #aborts     : %lu\n", data[i].nb_aborts);
		printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
		printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
		printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
		printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
		printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
		printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
		printf("    #failures : %lu\n", data[i].failures_because_contention);
		printf("  Max retries : %lu\n", data[i].max_retries);
		aborts += data[i].nb_aborts;
		aborts_locked_read += data[i].nb_aborts_locked_read;
		aborts_locked_write += data[i].nb_aborts_locked_write;
		aborts_validate_read += data[i].nb_aborts_validate_read;
		aborts_validate_write += data[i].nb_aborts_validate_write;
		aborts_validate_commit += data[i].nb_aborts_validate_commit;
		aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
		failures_because_contention += data[i].failures_because_contention;
		reads += data[i].nb_contains;
		updates += (data[i].nb_add + data[i].nb_remove);
		size += data[i].diff;
		if (max_retries < data[i].max_retries)
			max_retries = data[i].max_retries;
	}
	printf("Set size      : %d (expected: %d)\n", sl_set_size(set), size);
	printf("Duration      : %d (ms)\n", duration);
	printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
	printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
	printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);
	printf("#aborts       : %lu (%f / s)\n", aborts, aborts * 1000.0 / duration);
	printf("  #lock-r     : %lu (%f / s)\n", aborts_locked_read, aborts_locked_read * 1000.0 / duration);
	printf("  #lock-w     : %lu (%f / s)\n", aborts_locked_write, aborts_locked_write * 1000.0 / duration);
	printf("  #val-r      : %lu (%f / s)\n", aborts_validate_read, aborts_validate_read * 1000.0 / duration);
	printf("  #val-w      : %lu (%f / s)\n", aborts_validate_write, aborts_validate_write * 1000.0 / duration);
	printf("  #val-c      : %lu (%f / s)\n", aborts_validate_commit, aborts_validate_commit * 1000.0 / duration);
	printf("  #inv-mem    : %lu (%f / s)\n", aborts_invalid_memory, aborts_invalid_memory * 1000.0 / duration);
	printf("  #failures   : %lu\n",  failures_because_contention);
	printf("Max retries   : %lu\n", max_retries);
	
	// Delete set 
	sl_set_delete(set);
	
	// Cleanup STM 
	stm_exit();
	
#ifndef TLS
	pthread_key_delete(rng_seed_key);
#endif /* ! TLS */
	
	free(threads);
	free(data);
	
	return 0;
}

/*
 int main(int argc, char **argv)
 {
 sl_intset_t *set;
 int i;
 int size = 256;
 
 int j = rand();
 
 #ifdef TLS
 rng_seed = &j;
 #else // ! TLS 
 pthread_setspecific(rng_seed_key, &j);
 #endif // ! TLS 
 
 // initialize skip list set
 printf("Initializing with %d elts...\n", size);
 levelmax = 16; //floor_log_2((unsigned int) size);
 set = sl_set_new();
 print_skiplist(set);
 fraser_insert(set,1);
 printf("levelmax:%d\n", levelmax);
 print_skiplist(set);
 if (fraser_find(set, 1))
 printf("1 found\n");
 else 
 printf("1 not found\n");
 fraser_remove(set, 1);
 print_skiplist(set);
 if (fraser_find(set, 1))
 printf("1 found\n");
 else 
 printf("1 not found\n");
 
 for (i=0; i<size; i++) {
 fraser_insert(set, i);
 }
 
 if (fraser_find(set, 0))
 printf("0 found\n");
 else 
 printf("0 not found\n");
 // check pointer size
 if (sizeof(set->head) != sizeof(long)) {
 printf("We may have a problem here due to pointer size\n");
 exit(1);
 }
 
 print_skiplist(set);
 
 // testing delete and contains...
 for (i=0; i<256; i++) {
 if (!fraser_remove(set, i)) printf("impossible to delete %d\n",i);
 if (!fraser_insert(set, i)) printf("impossible to insert %d\n",i);
 }
 printf("Is 53 in it? %d\n", fraser_find(set, 53));
 if (!fraser_insert(set, 53)) printf("impossible to insert 53\n");
 printf("Delete 53.\n");
 fraser_remove(set, 53);
 printf("Is 53 in it? %d\n", fraser_find(set, 53));
 for (i=0; i<size; i++) {
 if (!fraser_remove(set, i)) printf("impossible to delete %d\n",i);
 }
 
 print_skiplist(set);
 
 return 0;
 }
 */
