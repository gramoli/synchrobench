/*
 * File:
 *   skiplist.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip List Integer Set
 *
 * Copyright (c) 2008-2009.
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

#include "intset.c"
#define ATOMIC_LOAD_MB(a)               (AO_load((volatile AO_t *)(a)))
#define MAXLEVEL						32

/* ################################################################### *
 * FRASER SKIPLIST
 * ################################################################### */

inline int is_marked(uintptr_t i) {
  return (int)(i & (uintptr_t)0x01);
}

inline uintptr_t unset_mark(uintptr_t i) {
  return (i & ~(uintptr_t)0x01);
}

inline uintptr_t set_mark(uintptr_t i) {
  return (i | (uintptr_t)0x01);
}

inline val_t get_val(sl_node_t *n) {
  return ((sl_node_t *)unset_mark((uintptr_t)n))->val;
}

void fraser_search(sl_intset_t *set, val_t val, sl_node_t **left_list, sl_node_t **right_list) {
  int i;
  sl_node_t *left, *left_next, *right, *right_next;

#ifdef DEBUG
  printf("++> fraser_search\n");
  IO_FLUSH;
#endif

retry:
  left = set->head;
  for (i = levelmax - 1; i >= 0; i--) {
    left_next = left->next[i];
    if (is_marked((uintptr_t)left_next))
      goto retry;
    /* Find unmarked node pair at this level */
    for (right = left_next; ; right = right_next) {
      /* Skip a sequence of marked nodes */
      while(1) {
        right_next = right->next[i];
        if (!is_marked((uintptr_t)right_next))
          break;
        right = (sl_node_t*)unset_mark((uintptr_t)right_next);
      }
      if (right->val >= val)
        break;
      left = right; 
      left_next = right_next;
    }
    /* Ensure left and right nodes are adjacent */
    if ((left_next != right) && 
        (!ATOMIC_CAS_MB(&left->next[i], left_next, right)))
      goto retry;
    if (left_list != NULL)
      left_list[i] = left;
    if (right_list != NULL)	
      right_list[i] = right;
  }

#ifdef DEBUG
  printf("++> fraser_search ends\n");
  IO_FLUSH;
#endif
}

int fraser_find(sl_intset_t *set, val_t val) {
  sl_node_t **succs;
  int result;

#ifdef DEBUG
  printf("++> fraser_find\n");
  IO_FLUSH;
#endif

  succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
  fraser_search(set, val, NULL, succs);
  result = (succs[0]->val == val && !succs[0]->deleted);
  free(succs);
  return result;
}

void mark_node_ptrs(sl_node_t *n) {
  int i;
  sl_node_t *n_next;
	
  for (i=n->toplevel-1; i>=0; i--) {
    do {
      n_next = n->next[i];
      if (is_marked((uintptr_t)n_next))
        break;
    } while (!ATOMIC_CAS_MB(&n->next[i], n_next, set_mark((uintptr_t)n_next)));
  }
}

int fraser_remove(sl_intset_t *set, val_t val) {
  sl_node_t **succs;
  int result;

#ifdef DEBUG
  printf("++> fraser_remove\n");
  IO_FLUSH;
#endif

  succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
  fraser_search(set, val, NULL, succs);
  result = (succs[0]->val == val);
  if (result == 0)
    goto end;
  /* 1. Node is logically deleted when the deleted field is not 0 */
  if (succs[0]->deleted) {
#ifdef DEBUG
    printf("++> fraser_remove ends\n");
    IO_FLUSH;
#endif
    result = 0;
    goto end;
  }
  ATOMIC_FETCH_AND_INC_FULL(&succs[0]->deleted);
  /* 2. Mark forward pointers, then search will remove the node */
  mark_node_ptrs(succs[0]);
  fraser_search(set, val, NULL, NULL);    
end:
  free(succs);
#ifdef DEBUG
  printf("++> fraser_remove ends\n");
  IO_FLUSH;
#endif

  return result;
}

int fraser_insert(sl_intset_t *set, val_t v) {
  sl_node_t *new, *new_next, *pred, *succ, **succs, **preds;
  int i;
  int result;

#ifdef DEBUG
  printf("++> fraser_insert\n");
  IO_FLUSH;
#endif

  new = sl_new_simple_node(v, get_rand_level(), 6);
  preds = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
  succs = (sl_node_t **)malloc(levelmax * sizeof(sl_node_t *));
retry: 	
  fraser_search(set, v, preds, succs);
  /* Update the value field of an existing node */
  if (succs[0]->val == v) {
    /* Value already in list */
    if (succs[0]->deleted) {
      /* Value is deleted: remove it and retry */
      mark_node_ptrs(succs[0]);
      goto retry;
    }
    result = 0;
    sl_delete_node(new);
    goto end;
  }
  for (i = 0; i < new->toplevel; i++)
    new->next[i] = succs[i];
  /* Node is visible once inserted at lowest level */
  if (!ATOMIC_CAS_MB(&preds[0]->next[0], succs[0], new)) 
    goto retry;
  for (i = 1; i < new->toplevel; i++) {
    while (1) {
      pred = preds[i];
      succ = succs[i];
      /* Update the forward pointer if it is stale */
      new_next = new->next[i];
      if ((new_next != succ) && 
          (!ATOMIC_CAS_MB(&new->next[i], unset_mark((uintptr_t)new_next), succ)))
        break; /* Give up if pointer is marked */
      /* Check for old reference to a k node */
      if (succ->val == v)
        succ = (sl_node_t *)unset_mark((uintptr_t)succ->next);
      /* We retry the search if the CAS fails */
      if (ATOMIC_CAS_MB(&pred->next[i], succ, new))
        break;
      fraser_search(set, v, preds, succs);
    }
  }
  result = 1;
end:
  free(preds);
  free(succs);

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
	val_t v = VAL_MIN;
	
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
		    TX_START(NL);
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)TX_LOAD(&node->next[i]);
				while ((v = TX_LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)TX_LOAD(&node->next[i]);
				}
			}
			node = (sl_node_t *)TX_LOAD(&node->next[0]);
			result = (v == val);
			TX_END;
			break;
			
		case 2:
		case 3:
		case 4: /* Elastic transaction */
		    TX_START(EL);
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)TX_LOAD(&node->next[i]);
				while ((v = TX_LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)TX_LOAD(&node->next[i]);
				}
			}
			node = (sl_node_t *)TX_LOAD(&node->next[0]);
			result = (v == val);
			TX_END;
			break;
			
		case 5: /* fraser lock-free */
			result = fraser_find(set, val);
			break;
			
		default:
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);

	}
	return result;
}

int sl_add(sl_intset_t *set, val_t val, int transactional)
{
	int result = 0;
	int i, l;
	sl_node_t *node, *next;
	sl_node_t *preds[MAXLEVEL], *succs[MAXLEVEL];
	val_t v;  
	
#ifdef DEBUG
	printf("++> sl_add(%d)\n", (int) val);
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
			if ((result = (node->val != val)) == 1) {
				l = get_rand_level();
				node = sl_new_simple_node(val, l, transactional);
				for (i = 0; i < l; i++) {
					node->next[i] = succs[i];
					preds[i]->next[i] = node;
				}
			}
			break;
			
		case 1:
		case 2: /* Normal transaction */
			TX_START(NL);
			v = VAL_MIN;
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)TX_LOAD(&node->next[i]);
				while ((v = TX_LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)TX_LOAD(&node->next[i]);
				}
				preds[i] = node;
				succs[i] = next;
			}
			if ((result = (v != val)) == 1) {
				l = get_rand_level();
				node = sl_new_simple_node(val, l, transactional);
				for (i = 0; i < l; i++) {
					node->next[i] = (sl_node_t *)TX_LOAD(&succs[i]);	
					TX_STORE(&preds[i]->next[i], node);
				}
			}
			TX_END;
			break;
			
		case 3:
		case 4:
			TX_START(EL);
			v = VAL_MIN;
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)TX_LOAD(&node->next[i]);
				while ((v = TX_LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)TX_LOAD(&node->next[i]);
				}
				preds[i] = node;
				succs[i] = next;
			}
			if ((result = (v != val)) == 1) {
				l = get_rand_level();
				node = sl_new_simple_node(val, l, transactional);
				for (i = 0; i < l; i++) {
					node->next[i] = (sl_node_t *)TX_LOAD(&succs[i]);	
					TX_STORE(&preds[i]->next[i], node);
				}
			}
			TX_END;
			break;
		
		case 5: /* fraser lock-free */
			result = fraser_insert(set, val);
			break;
			
		default:
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);

	}
	
	return result;
}

int sl_remove(sl_intset_t *set, val_t val, int transactional)
{
	int result = 0;
	int i;
	sl_node_t *node, *next = NULL;
	sl_node_t *preds[MAXLEVEL], *succs[MAXLEVEL];
	val_t v;  
	
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
			if ((result = (next->val == val)) == 1) {
				for (i = 0; i < set->head->toplevel; i++) 
					if (succs[i]->val == val)
						preds[i]->next[i] = succs[i]->next[i];
			}
			break;
			
		case 1:
		case 2: 
		case 3: /* Normal transaction */
			TX_START(NL);
			v = VAL_MIN;
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)TX_LOAD(&node->next[i]);
				while ((v = TX_LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)TX_LOAD(&node->next[i]);
				}
				// unnecessary duplicate assignation
				preds[i] = node;
				succs[i] = next;
			}
			if ((result = (next->val == val))) {
				for (i = 0; i < set->head->toplevel; i++) 
					if (succs[i]->val == val)
						TX_STORE(&preds[i]->next[i], (sl_node_t *)TX_LOAD(&succs[i]->next[i]));
				//FREE(node->next, (node->toplevel-1) * sizeof(sl_node_t *));
				FREE(next, sizeof(sl_node_t));
			}
			TX_END;
			break;
			
		case 4: /* Elastic transaction */ 
			TX_START(EL);
			v = VAL_MIN;
			node = set->head;
			for (i = node->toplevel-1; i >= 0; i--) {
				next = (sl_node_t *)TX_LOAD(&node->next[i]);
				while ((v = TX_LOAD(&next->val)) < val) {
					node = next;
					next = (sl_node_t *)TX_LOAD(&node->next[i]);
				}
				// unnecessary duplicate assignation
				preds[i] = node;
				succs[i] = next;
			}
			if ((result = (next->val == val))) {
				for (i = 0; i < set->head->toplevel; i++) 
					if (succs[i]->val == val) 
						TX_STORE(&preds[i]->next[i], (sl_node_t *)TX_LOAD(&succs[i]->next[i]));
				/* Free memory (delayed until commit) */
				//FREE(node->next, (node->toplevel) * sizeof(sl_node_t *));
				FREE(next, sizeof(sl_node_t));	
				//TX_STORE(next, NULL);
			}
			TX_END;
			break;
			
		case 5: /* fraser lock-free */
			result = fraser_remove(set, val);
			break;
			
		default:
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);
	}
	
	return result;
}


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

/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 */
inline int rand_range(int r) {
	int m = RAND_MAX;
	int d;
	int v = 0;
	
	do {
		d = (m > r ? r : m);
		v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}

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
	unsigned long nb_aborts_double_write;
	unsigned long max_retries;
	int diff;
	unsigned int seed;
	sl_intset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;


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
			//printf("val %d add\n", val);
        }
        d->nb_add++;
      } else {
        /* Remove last value */
		  if (sl_remove(d->set, last, TRANSACTIONAL)) {
			d->diff--;
			//printf("val %d rem\n", last);
			last = -1;
		}
        d->nb_remove++;
      }
    } else {
      /* Look for random value */
      val = (rand_r(&d->seed) % d->range) + 1;
      if (sl_contains(d->set, val, TRANSACTIONAL))
        d->nb_found++;
      d->nb_contains++;
    }
  }
  stm_get_stats("nb_aborts", &d->nb_aborts);
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read);
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write);
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit);
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory);
  stm_get_stats("nb_aborts_double_write", &d->nb_aborts_double_write);
  stm_get_stats("max_retries", &d->max_retries);
  /* free transaction */
  stm_exit_thread();
	
  return NULL;
}

void catcher(int sig)
{
	printf("CAUGHT SIGNAL %d\n", sig);
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
		{"elasticity",                required_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
	
	sl_intset_t *set;
	int i, c, val, size;
	char *s;
	unsigned long reads, updates, aborts, aborts_locked_read, aborts_locked_write,
    aborts_validate_read, aborts_validate_write, aborts_validate_commit,
    aborts_invalid_memory, aborts_double_write, max_retries, failures_because_contention;
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
	int unit_tx = 4;
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
					   "  -x, --elasticity (default=4)\n"
					   "        Use elastic transactions\n"
					   "        0 = non-protected,\n"
					   "        1 = normal transaction,\n"
					   "        2 = read elastic-tx,\n"
					   "        3 = read/add elastic-tx,\n"
					   "        4 = read/add/rem elastic-tx,\n"
					   "        5 = fraser lock-free\n"
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
	printf("Elasticity   : %d\n", unit_tx);
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
		//val = (rand() % range) + 1;
		val = rand_range(range);
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
		data[i].nb_aborts_double_write = 0;
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
	    //signal(SIGINT, catcher) == SIG_ERR ||
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
	aborts_double_write = 0;
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
		printf("  #diff       : %d\n", data[i].diff);
		printf("  #aborts     : %lu\n", data[i].nb_aborts);
		printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
		printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
		printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
		printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
		printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
		printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
		printf("    #dup-w    : %lu\n", data[i].nb_aborts_double_write);
		printf("    #failures : %lu\n", data[i].failures_because_contention);
		printf("  Max retries : %lu\n", data[i].max_retries);
		aborts += data[i].nb_aborts;
		aborts_locked_read += data[i].nb_aborts_locked_read;
		aborts_locked_write += data[i].nb_aborts_locked_write;
		aborts_validate_read += data[i].nb_aborts_validate_read;
		aborts_validate_write += data[i].nb_aborts_validate_write;
		aborts_validate_commit += data[i].nb_aborts_validate_commit;
		aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
		aborts_double_write += data[i].nb_aborts_double_write;
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
	printf("  #dup-w      : %lu (%f / s)\n", aborts_double_write, aborts_double_write * 1000.0 / duration);
	printf("  #failures   : %lu\n",  failures_because_contention);
	printf("Max retries   : %lu\n", max_retries);
	
	
	//print_skiplist(set);
	
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

