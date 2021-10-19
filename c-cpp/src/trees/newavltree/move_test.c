/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses of a hashtable
 *
 * Copyright (c) 2009-2010.
 *
 * test.c is part of Microbench
 * 
 * Microbench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "intset.h"

/* Hashtable length (# of buckets) */
unsigned int maxhtlength;

volatile AO_t stop;
/* Hashtable seed */
#ifdef TLS
__thread unsigned int *rng_seed;
#else /* ! TLS */
pthread_key_t rng_seed_key;
#endif /* ! TLS */

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

/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
inline long rand_range(long r) {
	int m = RAND_MAX;
	long d, v = 0;
	
	do {
		d = (m > r ? r : m);
		v += 1 + (long)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
	int m = RAND_MAX;
	long d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (long)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}

typedef struct thread_data {
        val_t first;
	long range;
	int update;
	int move;
	int snapshot;
	int unit_tx;
	int alternate;
	int effective;

        unsigned long set_write_reads;
        unsigned long set_read_reads;
        unsigned long set_write_writes;
        unsigned long set_reads;
        unsigned long set_writes;
        unsigned long set_max_reads;
        unsigned long set_max_writes;

        unsigned long write_reads;
        unsigned long read_reads;
        unsigned long write_writes;
        unsigned long reads;
        unsigned long writes;
        unsigned long max_reads;
        unsigned long max_writes;
        unsigned long nb_modifications;
	unsigned long nb_add;
	unsigned long nb_added;
	unsigned long nb_remove;
	unsigned long nb_removed;
	unsigned long nb_contains;
	/* added for HashTables */
	unsigned long load_factor;
	unsigned long nb_move;
	unsigned long nb_moved;
	unsigned long nb_snapshot;
	unsigned long nb_snapshoted;
	/* end: added for HashTables */
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
	unsigned int seed;
	avl_intset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
        unsigned long nb_trans;
  int id;
} thread_data_t;


typedef struct maintenance_thread_data {
  unsigned long nb_removed;
  unsigned long nb_rotated;
  unsigned long nb_suc_rotated;
  unsigned long nb_propagated;
  unsigned long nb_suc_propagated;

  unsigned long set_write_reads;
  unsigned long set_read_reads;
  unsigned long set_write_writes;
  unsigned long set_reads;
  unsigned long set_writes;
  unsigned long set_max_reads;
  unsigned long set_max_writes;

  int id;
  int nb_maint;
  unsigned long write_reads;
  unsigned long read_reads;
  unsigned long write_writes;
  unsigned long reads;
  unsigned long writes;
  unsigned long max_reads;
  unsigned long max_writes;
  unsigned long nb_aborts;
  unsigned long nb_aborts_locked_read;
  unsigned long nb_aborts_locked_write;
  unsigned long nb_aborts_validate_read;
  unsigned long nb_aborts_validate_write;
  unsigned long nb_aborts_validate_commit;
  unsigned long nb_aborts_invalid_memory;
  unsigned long nb_aborts_double_write;
  unsigned long max_retries;
  thread_data_t *t_data;
  unsigned long *t_nb_trans;
  unsigned long *t_nb_trans_old;
  //free_list_item *free_list;
  //free_list_item **t_free_list;
  int nb_threads;
  avl_intset_t *set;
  barrier_t *barrier;
} maintenance_thread_data_t;


void valid_rec(avl_node_t *node, int *array) {
  if(node == NULL) return;
  valid_rec(node->left, array);
  valid_rec(node->right, array);

  array[node->key]++;
  if(array[node->key] > 1) {
    printf("Duplicate key in tree %d\n", node->key);
  }
}

void validate_avltree(avl_intset_t *set, int size) {
  int *array;
  int i;

  array = (int *)malloc(size * sizeof(int));
  for(i = 0; i < size; i++) {
    array[i] = 0;
  }

  valid_rec(set->root->left, array);

}



void print_rec(avl_node_t *node, int lvl, int maxlvl, long *count) {
  if(node == NULL) {
    return;
  }
  if(lvl == maxlvl) {
    //printf("Key:%d,D/R:%d%d,LRC:%d,%d,%d ",node->key, node->deleted, node->removed, node->lefth, node->righth, node->localh);
    printf("Key:%d,D/R:%d%d ", node->key, node->deleted, node->removed);
    *count = *count + 1;
    return;
  }
  lvl = lvl + 1;
  print_rec(node->left, lvl, maxlvl, count);
  print_rec(node->right, lvl, maxlvl, count);
}

void print_avltree(avl_intset_t *set) {
  long count = 1;
  int lvl = 0;
  while(count != 0) {
    count = 0;
    print_rec(set->root, 0, lvl++, &count);
    printf("\n");
  }

}



void *test(void *data) {
	int val2, numtx, r, last = -1;
	val_t val = 0;
	int unext, mnext, cnext;
	int result;
	int id;
	ulong *tloc;

	thread_data_t *d = (thread_data_t *)data;
	id = d->id;
	tloc = d->set->nb_committed;

	
	/* Create transaction */
	TM_THREAD_ENTER();
	/* Wait on barrier */
	barrier_cross(d->barrier);
	
	/* Is the first op an update, a move? */
	r = rand_range_re(&d->seed, 100) - 1;
	unext = (r < d->update);
	mnext = (r < d->move);
	cnext = (r >= d->update + d->snapshot);
	
#ifdef ICC
	while (stop == 0) {
#else
	while (AO_load_full(&stop) == 0) {
#endif /* ICC */
		
	  if (unext) { // update
	    
	    if (mnext) { // move
	      
	      if (last == -1) val = rand_range_re(&d->seed, d->range);
	      else val = last;
	      val2 = rand_range_re(&d->seed, d->range);
	      if (avl_move(d->set, val, val2, TRANSACTIONAL, id)) {
					d->nb_moved++;
					last = -1;
	      }
	      d->nb_trans++;
	      tloc[id]++;
	      d->nb_move++;
	      
	    } else if (last < 0) { // add
	      
	      val = rand_range_re(&d->seed, d->range);
	      if (avl_add(d->set, val, TRANSACTIONAL, id)) {
					d->nb_added++;
					last = val;
	      }
	      d->nb_trans++;
	      tloc[id]++; 				
	      d->nb_add++;
	      
	    } else { // remove
	      
	      if (d->alternate) { // alternate mode
		if (avl_remove(d->set, last, TRANSACTIONAL, id)) {
						d->nb_removed++;
#ifdef REMOVE_LATER
						finish_removal(d->set, id);
#endif
						last = -1;
					}
	      } else {
					/* Random computation only in non-alternated cases */
					val = rand_range_re(&d->seed, d->range);
					/* Remove one random value */
					if (avl_remove(d->set, val, TRANSACTIONAL, id)) {
						d->nb_removed++;
						/* Repeat until successful, to avoid size variations */
						last = -1;
					} 
	      }
	      d->nb_trans++;
	      tloc[id]++;
	      d->nb_remove++;
	    }
	    
	  } else { // reads
	    
	    if (cnext) { // contains (no snapshot)
				
	      if (d->alternate) {
					if (d->update == 0) {
						if (last < 0) {
							val = d->first;
							last = val;
						} else { // last >= 0
							val = rand_range_re(&d->seed, d->range);
							last = -1;
						}
					} else { // update != 0
						if (last < 0) {
							val = rand_range_re(&d->seed, d->range);
							//last = val;
						} else {
							val = last;
						}
					}
	      }	else val = rand_range_re(&d->seed, d->range);
				
	      if (avl_contains(d->set, val, TRANSACTIONAL, id)) 
					d->nb_found++;
	      d->nb_trans++;
	      tloc[id]++;
	      d->nb_contains++;
	      
	    } else { // snapshot
	      
	      if (avl_snapshot(d->set, TRANSACTIONAL, id))
		d->nb_snapshoted++;
	      d->nb_trans++;
	      tloc[id]++;
	      d->nb_snapshot++;
	      
	    }
	  }
	  
	  /* Is the next op an update, a move, a contains? */
	  if (d->effective) { // a failed remove/add is a read-only tx
	    numtx = d->nb_contains + d->nb_add + d->nb_remove + d->nb_move + d->nb_snapshot;
	    unext = ((100.0 * (d->nb_added + d->nb_removed + d->nb_moved)) < (d->update * numtx));
	    mnext = ((100.0 * d->nb_moved) < (d->move * numtx));
	    cnext = !((100.0 * d->nb_snapshoted) < (d->snapshot * numtx)); 
	  } else { // remove/add (even failed) is considered as an update
	    r = rand_range_re(&d->seed, 100) - 1;
	    unext = (r < d->update);
	    mnext = (r < d->move);
	    cnext = (r >= d->update + d->snapshot);
	  }
	  
#ifdef ICC
	}
#else
	}
#endif /* ICC */
	
	/* Free transaction */
	TM_THREAD_EXIT();
	
	return NULL;
}


void *test2(void *data)
{
	int val, newval, last, flag = 1;
	int id;
	ulong *tloc;

	thread_data_t *d = (thread_data_t *)data;
	id = d->id;
	tloc = d->set->nb_committed;
	
	/* Create transaction */
	TM_THREAD_ENTER();
	/* Wait on barrier */
	barrier_cross(d->barrier);
	
	last = 0; // to avoid warning
	while (stop == 0) {
		
	  val = rand_range_re(&d->seed, 100) - 1;
	  /* added for HashTables */
	  if (val < d->update) {
	    if (val >= d->move) { /* update without move */
	      if (flag) {
					/* Add random value */
					val = (rand_r(&d->seed) % d->range) + 1;
					if (avl_add(d->set, val, TRANSACTIONAL, id)) {
						d->nb_added++;
						last = val;
						flag = 0;
					}
					d->nb_trans++;
					tloc[id]++;
					d->nb_add++;
	      } else {
					if (d->alternate) {
						/* Remove last value */
					  if (avl_remove(d->set, last, TRANSACTIONAL, id))  
					    d->nb_removed++;
					  d->nb_trans++;
					  tloc[id]++;
					  d->nb_remove++;
					  flag = 1;
					} else {
						/* Random computation only in non-alternated cases */
						newval = rand_range_re(&d->seed, d->range);
						if (avl_remove(d->set, newval, TRANSACTIONAL, id)) {  
							d->nb_removed++;
							/* Repeat until successful, to avoid size variations */
							flag = 1;
						}
						d->nb_trans++;
						tloc[id]++;
						d->nb_remove++;
					}
	      } 
	    } else { /* move */
	      val = rand_range_re(&d->seed, d->range);
	      if (avl_move(d->set, last, val, TRANSACTIONAL, id)) {
					d->nb_moved++;
					last = val;
	      }
	      d->nb_trans++;
	      tloc[id]++;
	      d->nb_move++;
	    }
	  } else {
	    if (val >= d->update + d->snapshot) { /* read-only without snapshot */
	      /* Look for random value */
	      val = rand_range_re(&d->seed, d->range);
	      if (avl_contains(d->set, val, TRANSACTIONAL, id))
					d->nb_found++;
				d->nb_trans++;
				tloc[id]++;
				d->nb_contains++;
	    } else { /* snapshot */
	      if (avl_snapshot(d->set, TRANSACTIONAL, id))
					d->nb_snapshoted++;
	      d->nb_trans++;
	      tloc[id]++;
	      d->nb_snapshot++;
	    }
	  }
	}
	
	/* Free transaction */
	TM_THREAD_EXIT();
	return NULL;
}



void *test_maintenance(void *data) {
#ifdef TINY10B
  int done;
  int i;
  ulong *tmp;
  free_list_item *next, *tmp_item, **t_list_items;
#endif
  
  maintenance_thread_data_t *d = (maintenance_thread_data_t *)data;

#ifdef TINY10B
  t_list_items = (free_list_item **)malloc(d->nb_threads * sizeof(free_list_item *));
  for(i = 0; i < d->nb_threads; i++) {
    t_list_items[i] = d->set->t_free_list[i];
  }
#endif  

  /* Create transaction */
  TM_THREAD_ENTER();
  /* Wait on barrier */
  barrier_cross(d->barrier);
	
  /* Is the first op an update? */
  //unext = (rand_range_re(&d->seed, 100) - 1 < d->update);
  
#ifdef ICC
  while (stop == 0) {
#else
    while (AO_load_full(&stop) == 0) {
#endif /* ICC */

#ifdef TINY10B

      do_maintenance_thread(d->set, d->id, d->nb_maint);

#endif
      
#ifdef ICC
    }
#else
  }
#endif /* ICC */
  
  /* Free transaction */
  TM_THREAD_EXIT();
  
  return NULL;
}



/* void print_set(intset_t *set) { */
/* 	node_t *curr, *tmp; */
	
/* 	curr = set->head; */
/* 	tmp = curr; */
/* 	do { */
/* 		printf(" - v%d", (int) curr->val); */
/* 		tmp = curr; */
/* 		curr = tmp->next; */
/* 	} while (curr->val != VAL_MAX); */
/* 	printf(" - v%d", (int) curr->val); */
/* 	printf("\n"); */
/* } */

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
		{"move-rate",                 required_argument, NULL, 'm'},
		{"snapshot-rate",             required_argument, NULL, 'a'},
		{"elasticity",                required_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
	
	avl_intset_t *set;
	int i, j, c, size, tree_size;
	val_t last = 0; 
	val_t val = 0;
	unsigned long reads, effreads, updates, effupds, moves, moved, snapshots, 
	snapshoted, aborts, aborts_locked_read, aborts_locked_write, 
	aborts_validate_read, aborts_validate_write, aborts_validate_commit, 
	aborts_invalid_memory, aborts_double_write,
	max_retries, failures_because_contention;
	pthread_attr_t attr;
	thread_data_t *data;
	maintenance_thread_data_t *maintenance_data;
	pthread_t *threads;
	pthread_t *maintenance_threads;
	barrier_t barrier;
	struct timeval start, end;
	struct timespec timeout;
	int duration = DEFAULT_DURATION;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	int nb_maintenance_threads = DEFAULT_NB_MAINTENANCE_THREADS;
	long range = DEFAULT_RANGE;
	int seed = DEFAULT_SEED;
	int update = DEFAULT_UPDATE;
	int load_factor = DEFAULT_LOAD;
	int move = DEFAULT_MOVE;
	int snapshot = DEFAULT_SNAPSHOT;
	int unit_tx = DEFAULT_ELASTICITY;
	int alternate = DEFAULT_ALTERNATE;
	int effective = DEFAULT_EFFECTIVE;
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hAf:d:i:n:r:s:u:m:a:l:x:", long_options, &i);
		
		if(c == -1)
			break;
		
		if(c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;
		
		switch(c) {
				case 0:
					// Flag is automatically set 
					break;
				case 'h':
					printf("intset -- STM stress test "
								 "(hash table)\n"
								 "\n"
								 "Usage:\n"
								 "  intset [options...]\n"
								 "\n"
								 "Options:\n"
								 "  -h, --help\n"
								 "        Print this message\n"
								 "  -A, --Alternate\n"
								 "        Consecutive insert/remove target the same value\n"
								 "  -f, --effective <int>\n"
								 "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
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
								 "  -m , --move-rate <int>\n"
								 "        Percentage of move transactions (default=" XSTR(DEFAULT_MOVE) ")\n"
								 "  -a , --snapshot-rate <int>\n"
								 "        Percentage of snapshot transactions (default=" XSTR(DEFAULT_SNAPSHOT) ")\n"
								 "  -l , --load-factor <int>\n"
								 "        Ratio of keys over buckets (default=" XSTR(DEFAULT_LOAD) ")\n"
								 "  -x, --elasticity (default=4)\n"
								 "        Use elastic transactions\n"
								 "        0 = non-protected,\n"
								 "        1 = normal transaction,\n"
								 "        2 = read elastic-tx,\n"
								 "        3 = read/add elastic-tx,\n"
								 "        4 = read/add/rem elastic-tx,\n"
								 "        5 = elastic-tx w/ optimized move.\n"
								 );
					exit(0);
				case 'A':
					alternate = 1;
					break;
				case 'f':
					effective = atoi(optarg);
					break;
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
					range = atol(optarg);
					break;
				case 's':
					seed = atoi(optarg);
					break;
				case 'u':
					update = atoi(optarg);
					break;
				case 'm':
					move = atoi(optarg);
					break;
				case 'a':
					snapshot = atoi(optarg);
					break;
				case 'l':
					load_factor = atoi(optarg);
					break;
				case 'x':
					unit_tx = atoi(optarg);
					break;
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
	assert(move >= 0 && move <= update);
	assert(snapshot >= 0 && snapshot <= (100-update));
	assert(initial < MAXHTLENGTH);
	assert(initial >= load_factor);
	
	printf("Set type     : avl tree\n");
	printf("Duration     : %d\n", duration);
	printf("Initial size : %d\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Nb mt threads: %d\n", nb_maintenance_threads);
	printf("Value range  : %ld\n", range);
	printf("Seed         : %d\n", seed);
	printf("Update rate  : %d\n", update);
	printf("Load factor  : %d\n", load_factor);
	printf("Move rate    : %d\n", move);
	printf("Snapshot rate: %d\n", snapshot);
	printf("Elasticity   : %d\n", unit_tx);
	printf("Alternate    : %d\n", alternate);	
	printf("Effective    : %d\n", effective);
	printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
				 (int)sizeof(int),
				 (int)sizeof(long),
				 (int)sizeof(void *),
				 (int)sizeof(uintptr_t));
	
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


	if ((maintenance_data = (maintenance_thread_data_t *)malloc(nb_maintenance_threads * sizeof(maintenance_thread_data_t))) == NULL) {
		perror("malloc");
		exit(1);
	}
	if ((maintenance_threads = (pthread_t *)malloc(nb_maintenance_threads * sizeof(pthread_t))) == NULL) {
		perror("malloc");
		exit(1);
	}


	
	if (seed == 0)
		srand((int)time(0));
	else
		srand(seed);
	
	maxhtlength = (unsigned int) initial / load_factor;

	set = avl_set_new_alloc(0, nb_threads);	
	//set->stop = &stop;

	stop = 0;
	
	// Init STM 
	printf("Initializing STM\n");
	
	TM_STARTUP();
	
	// Populate set 
	printf("Adding %d entries to set\n", initial);
	i = 0;
	maxhtlength = (int) (initial / load_factor);
	while (i < initial) {
		val = rand_range(range);
		if (avl_add(set, val, 0, 0) > 0) {
		  last = val;
		  i++;			
		}
	}
	size = avl_set_size(set);
	validate_avltree(set, range);

	printf("Set size     : %d\n", size);
	tree_size = avl_tree_size(set);
	printf("Tree size     : %d\n", tree_size);
	
	// Access set from all threads 
	barrier_init(&barrier, nb_threads + nb_maintenance_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);
		data[i].id = i;
		data[i].first = last;
		data[i].range = range;
		data[i].update = update;
		data[i].load_factor = load_factor;
		data[i].move = move;
		data[i].snapshot = snapshot;
		data[i].unit_tx = unit_tx;
		data[i].alternate = alternate;
		data[i].effective = effective;
		data[i].nb_add = 0;
		data[i].nb_added = 0;
		data[i].nb_remove = 0;
		data[i].nb_removed = 0;
		data[i].nb_move = 0;
		data[i].nb_moved = 0;
		data[i].nb_snapshot = 0;
		data[i].nb_snapshoted = 0;
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
		data[i].seed = rand();
		data[i].set = set;
		data[i].barrier = &barrier;
		data[i].failures_because_contention = 0;
		if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
			fprintf(stderr, "Error creating thread\n");
			exit(1);
		}
	}

	for (i = 0; i < nb_maintenance_threads; i++) {
	        maintenance_data[i].nb_maint = nb_maintenance_threads;
		maintenance_data[i].id = i;
		maintenance_data[i].nb_removed = 0;
		maintenance_data[i].nb_rotated = 0;
		maintenance_data[i].nb_suc_rotated = 0;
		maintenance_data[i].nb_propagated = 0;
		maintenance_data[i].nb_suc_propagated = 0;
		maintenance_data[i].nb_aborts = 0;
		maintenance_data[i].nb_aborts_locked_read = 0;
		maintenance_data[i].nb_aborts_locked_write = 0;
		maintenance_data[i].nb_aborts_validate_read = 0;
		maintenance_data[i].nb_aborts_validate_write = 0;
		maintenance_data[i].nb_aborts_validate_commit = 0;
		maintenance_data[i].nb_aborts_invalid_memory = 0;
		maintenance_data[i].nb_aborts_double_write = 0;
		maintenance_data[i].max_retries = 0;
		maintenance_data[i].t_data = data;
		maintenance_data[i].nb_threads = nb_threads;

		maintenance_data[i].set = set;
		maintenance_data[i].barrier = &barrier;

		if ((maintenance_data[i].t_nb_trans = (unsigned long *)malloc(nb_threads * sizeof(unsigned long))) == NULL) {
		  perror("malloc");
		  exit(1);
		}
		for(j = 0; j < nb_threads; j++) {
		  maintenance_data[i].t_nb_trans[j] = 0;
		}
		if ((maintenance_data[i].t_nb_trans_old = (unsigned long *)malloc(nb_threads * sizeof(unsigned long))) == NULL) {
		  perror("malloc");
		  exit(1);
		}
		for(j = 0; j < nb_threads; j++) {
		  maintenance_data[i].t_nb_trans_old[j] = 0;
		}

		printf("Creating maintenance thread %d\n", i);
		if (pthread_create(&maintenance_threads[i], &attr, test_maintenance, (void *)(&maintenance_data[i])) != 0) {
			fprintf(stderr, "Error creating thread\n");
			exit(1);
		}
	}


	pthread_attr_destroy(&attr);

	
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
	
	//print_avltree(set);
	
	
	// Wait for thread completion 
	for (i = 0; i < nb_threads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for thread completion\n");
			exit(1);
		}
	}

	// Wait for maintenance thread completion 
	for (i = 0; i < nb_maintenance_threads; i++) {
		if (pthread_join(maintenance_threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for maintenance thread completion\n");
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
	effreads = 0;
	updates = 0;
	effupds = 0;
	moves = 0;
	moved = 0;
	snapshots = 0;
	snapshoted = 0;
	max_retries = 0;
	for (i = 0; i < nb_threads; i++) {
		printf("Thread %d\n", i);
		printf("  #add        : %lu\n", data[i].nb_add);
		printf("    #added    : %lu\n", data[i].nb_added);
		printf("  #remove     : %lu\n", data[i].nb_remove);
		printf("    #removed  : %lu\n", data[i].nb_removed);
		printf("  #contains   : %lu\n", data[i].nb_contains);
		printf("    #found    : %lu\n", data[i].nb_found);
		printf("  #move       : %lu\n", data[i].nb_move);
		printf("  #moved      : %lu\n", data[i].nb_moved);
		printf("  #snapshot   : %lu\n", data[i].nb_snapshot);
		printf("  #snapshoted : %lu\n", data[i].nb_snapshoted);
		printf("  #aborts     : %lu\n", data[i].nb_aborts);
		printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
		printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
		printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
		printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
		printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
		printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
		printf("    #dup-w  : %lu\n", data[i].nb_aborts_double_write);
		printf("    #failures : %lu\n", data[i].failures_because_contention);
		printf("  Max retries : %lu\n", data[i].max_retries);

		printf("  #set read trans reads: %lu\n", data[i].set_read_reads);
		printf("  #set write trans reads: %lu\n", data[i].set_write_reads);
		printf("  #set write trans writes: %lu\n", data[i].set_write_writes);
		printf("  #set trans reads: %lu\n", data[i].set_reads);
		printf("  #set trans writes: %lu\n", data[i].set_writes);
		printf("  set max reads: %lu\n", data[i].set_max_reads);
		printf("  set max writes: %lu\n", data[i].set_max_writes);

		printf("  #read trans reads: %lu\n", data[i].read_reads);
		printf("  #write trans reads: %lu\n", data[i].write_reads);
		printf("  #write trans writes: %lu\n", data[i].write_writes);
		printf("  #trans reads: %lu\n", data[i].reads);
		printf("  #trans writes: %lu\n", data[i].writes);
		printf("  max reads: %lu\n", data[i].max_reads);
		printf("  max writes: %lu\n", data[i].max_writes);
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
		effreads += data[i].nb_contains + 
		(data[i].nb_add - data[i].nb_added) + 
		(data[i].nb_remove - data[i].nb_removed) + 
		(data[i].nb_move - data[i].nb_moved) +
		data[i].nb_snapshoted;
		updates += (data[i].nb_add + data[i].nb_remove + data[i].nb_move);
		effupds += data[i].nb_removed + data[i].nb_added + data[i].nb_moved; 
		moves += data[i].nb_move;
		moved += data[i].nb_moved;
		snapshots += data[i].nb_snapshot;
		snapshoted += data[i].nb_snapshoted;
		size += data[i].nb_added - data[i].nb_removed;
		if (max_retries < data[i].max_retries)
			max_retries = data[i].max_retries;
	}



	for (i = 0; i < nb_maintenance_threads; i++) {
		printf("Maintenance thread %d\n", i);
		printf("  #removed %lu\n", set->nb_removed);
		printf("  #rotated %lu\n", set->nb_rotated);
		printf("  #rotated sucs %lu\n", set->nb_suc_rotated);
		printf("  #propogated %lu\n", set->nb_propogated);
		printf("  #propogated sucs %lu\n", set->nb_suc_propogated);
		printf("  #aborts     : %lu\n", maintenance_data[i].nb_aborts);
		printf("    #lock-r   : %lu\n", maintenance_data[i].nb_aborts_locked_read);
		printf("    #lock-w   : %lu\n", maintenance_data[i].nb_aborts_locked_write);
		printf("    #val-r    : %lu\n", maintenance_data[i].nb_aborts_validate_read);
		printf("    #val-w    : %lu\n", maintenance_data[i].nb_aborts_validate_write);
		printf("    #val-c    : %lu\n", maintenance_data[i].nb_aborts_validate_commit);
		printf("    #inv-mem  : %lu\n", maintenance_data[i].nb_aborts_invalid_memory);
		printf("    #dup-w    : %lu\n", maintenance_data[i].nb_aborts_double_write);
		//printf("    #failures : %lu\n", maintenance_data[i].failures_because_contention);
		printf("  Max retries : %lu\n", maintenance_data[i].max_retries);
	}


	printf("Set size      : %d (expected: %d)\n", avl_set_size(set), size);
	printf("Tree size      : %d\n", avl_tree_size(set));

	validate_avltree(set, range);

	printf("Duration      : %d (ms)\n", duration);
	printf("#txs          : %lu (%f / s)\n", reads + updates + snapshots, (reads + updates + snapshots) * 1000.0 / duration);
	
	printf("#read txs     : ");
	if (effective) {
		printf("%lu (%f / s)\n", effreads, effreads * 1000.0 / duration);
		printf("  #cont/snpsht: %lu (%f / s)\n", reads, reads * 1000.0 / duration);
	} else printf("%lu (%f / s)\n", reads, reads * 1000.0 / duration);
	
	printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));
	
	printf("#update txs   : ");
	if (effective) {
		printf("%lu (%f / s)\n", effupds, effupds * 1000.0 / duration);
		printf("  #upd trials : %lu (%f / s)\n", updates, updates * 1000.0 / 
					 duration);
	} else printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);
	
	printf("#move txs     : %lu (%f / s)\n", moves, moves * 1000.0 / duration);
	printf("  #moved      : %lu (%f / s)\n", moved, moved * 1000.0 / duration);
	printf("#snapshot txs : %lu (%f / s)\n", snapshots, snapshots * 1000.0 / duration);
	printf("  #snapshoted : %lu (%f / s)\n", snapshoted, snapshoted * 1000.0 / duration);
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
	
	// Delete set 
	avl_set_delete(set);
	
	// Cleanup STM 
	TM_SHUTDOWN();
	
	free(threads);
	free(data);

	free(maintenance_threads);
	free(maintenance_data);
	
	return 0;
}
