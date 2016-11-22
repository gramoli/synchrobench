/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses to skip list integer set
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

#include <unistd.h>
#include "intset.h"

//#define THROTTLE_NUM  1000
//#define THROTTLE_TIME 10000
//#define THROTTLE_MAINTENANCE

volatile AO_t stop;
unsigned int global_seed;
#ifdef TLS
__thread unsigned int *rng_seed;
#else /* ! TLS */
pthread_key_t rng_seed_key;
#endif /* ! TLS */
unsigned int levelmax;

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
	int d, v = 0;

/* #ifdef BIAS_RANGE */
/* 	if(rand() < RAND_MAX / 10000) { */
/* 	  if(last < r || last > r * 10) { */
/* 	    last = r; */
/* 	  } */
/* 	  return last++; */
/* 	} */
/* #endif */
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
	int m = RAND_MAX;
	int d, v = 0;

/* #ifdef BIAS_RANGE */
/* 	if(rand_r(seed) < RAND_MAX / 10000) { */
/* 	  if(last < r || last > r * 10) { */
/* 	    last = r; */
/* 	  } */
/* 	  return last++; */
/* 	} */
/* #endif	 */
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}
long rand_range_re(unsigned int *seed, long r);


typedef struct thread_data {
        int id;
	val_t first;
	long range;
	int update;
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
  //free_list_item *free_list;
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

  unsigned long write_reads;
  unsigned long read_reads;
  unsigned long write_writes;
  unsigned long reads;
  unsigned long writes;

  int id;
  int nb_maint;
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
	int unext, last = -1; 
	val_t val = 0;
	int result;
	int id;
	ulong *tloc;
#ifdef BIAS_RANGE
	val_t increase;
#endif

	thread_data_t *d = (thread_data_t *)data;
	id = d->id;
	tloc = d->set->nb_committed;
	
#ifdef BIAS_RANGE
	increase = d->range;
#endif

	/* Create transaction */
	TM_THREAD_ENTER();
	/* Wait on barrier */
	barrier_cross(d->barrier);
	
	/* Is the first op an update? */
	unext = (rand_range_re(&d->seed, 100) - 1 < d->update);
	
#ifdef ICC
	while (stop == 0) {
#else
	while (AO_load_full(&stop) == 0) {
#endif /* ICC */
		
		if (unext) { // update
			
			if (last < 0) { // add
				
				val = rand_range_re(&d->seed, d->range);
#ifdef BIAS_RANGE
				if(rand_range_re(&d->seed, 1000) < 50) {
				  increase += rand_range_re(&d->seed, 10);
				  if(increase > d->range * 20) {
				    increase = d->range;
				  }
				  val = increase;
				}
#endif
				if ((result = avl_add(d->set, val, TRANSACTIONAL, id)) > 0) {
					d->nb_added++;
					if(result > 1) {
					  d->nb_modifications++;
					}
					last = val;
				}
				d->nb_trans++;
				tloc[id]++;
				d->nb_add++;
				
			} else { // remove
				
				if (d->alternate) { // alternate mode (default)
#ifdef TINY10B
				  if ((result = avl_remove(d->set, last, TRANSACTIONAL, id)) > 0) {
#else
				    if ((result = avl_remove(d->set, last, TRANSACTIONAL, 0)) > 0) {
#endif
						d->nb_removed++;
#ifdef REMOVE_LATER

						finish_removal(d->set, id);
#endif
					        if(result > 1) {
					           d->nb_modifications++;
					         }
					}
					last = -1;
				} else {
					/* Random computation only in non-alternated cases */
					val = rand_range_re(&d->seed, d->range);
					/* Remove one random value */
#ifdef BIAS_RANGE
					if(rand_range_re(&d->seed, 1000) < 300) {
					  //val = d->range + rand_range_re(&d->seed, increase - d->range);
					  val = increase - rand_range_re(&d->seed, 10);
					}
#endif
#ifdef TINY10B
					if ((result = avl_remove(d->set, val, TRANSACTIONAL, id)) > 0) {
#else
					  if ((result = avl_remove(d->set, val, TRANSACTIONAL, 0)) > 0) {
#endif
						d->nb_removed++;
#ifdef REMOVE_LATER

						finish_removal(d->set, id);
#endif
					        if(result > 1) {
					          d->nb_modifications++;
					        }
						/* Repeat until successful, to avoid size variations */
						last = -1;
					} 
				}
				d->nb_trans++;
				tloc[id]++;
				d->nb_remove++;
			}
			
		} else { // read
			
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
			
#ifdef BIAS_RANGE
			if(rand_range_re(&d->seed, 1000) < 100) {
			  val = increase;
			}
#endif
			if (avl_contains(d->set, val, TRANSACTIONAL, id)) 
				d->nb_found++;
			d->nb_trans++;
			tloc[id]++;
			d->nb_contains++;
			
		}
		
		/* Is the next op an update? */
		if (d->effective) { // a failed remove/add is a read-only tx
			unext = ((100 * (d->nb_added + d->nb_removed))
							 < (d->update * (d->nb_add + d->nb_remove + d->nb_contains)));
		} else { // remove/add (even failed) is considered as an update
			unext = (rand_range_re(&d->seed, 100) - 1 < d->update);
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









void *test_maintenance(void *data) {
#ifdef TINY10B
  int i;
  free_list_item **t_list_items;
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
		{"thread-num",                required_argument, NULL, 't'},
		{"range",                     required_argument, NULL, 'r'},
		{"seed",                      required_argument, NULL, 'S'},
		{"update-rate",               required_argument, NULL, 'u'},
		{"elasticity",                required_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
	
	avl_intset_t *set;
	int i, j, c, size, tree_size;
	val_t last = 0; 
	val_t val = 0;
	unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read, aborts_locked_write,
	aborts_validate_read, aborts_validate_write, aborts_validate_commit,
	aborts_invalid_memory, aborts_double_write, max_retries, failures_because_contention;
	thread_data_t *data;
	maintenance_thread_data_t *maintenance_data;
	pthread_t *threads;
	pthread_t *maintenance_threads;
	pthread_attr_t attr;
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
	int unit_tx = DEFAULT_ELASTICITY;
	int alternate = DEFAULT_ALTERNATE;
	int effective = DEFAULT_EFFECTIVE;
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hAf:d:i:t:r:S:u:x:"
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
								 "(avltree)\n"
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
								 "  -t, --thread-num <int>\n"
								 "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
								 "  -r, --range <int>\n"
								 "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
								 "  -S, --seed <int>\n"
								 "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
								 "  -u, --update-rate <int>\n"
								 "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
								 "  -x, --elasticity (default=4)\n"
								 "        Use elastic transactions\n"
								 "        0 = non-protected,\n"
								 "        1 = normal transaction,\n"
								 "        2 = read elastic-tx,\n"
								 "        3 = read/add elastic-tx,\n"
								 "        4 = read/add/rem elastic-tx,\n"
								 "        5 = fraser lock-free\n"
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
				case 't':
					nb_threads = atoi(optarg);
					break;
				case 'r':
					range = atol(optarg);
					break;
				case 'S':
					seed = atoi(optarg);
					break;
				case 'u':
					update = atoi(optarg);
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
	
	printf("Set type     : avltree\n");
	printf("Duration     : %d\n", duration);
	printf("Initial size : %u\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Nb mt threads: %d\n", nb_maintenance_threads);
	printf("Value range  : %ld\n", range);
	printf("Seed         : %d\n", seed);
	printf("Update rate  : %d\n", update);
	printf("Elasticity   : %d\n", unit_tx);
	printf("Alternate    : %d\n", alternate);
	printf("Efffective   : %d\n", effective);
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
	
	//levelmax = floor_log_2((unsigned int) initial);

	//set = avl_set_new();
	set = avl_set_new_alloc(0, nb_threads);
	//set->stop = &stop;
	//#endif
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
	
	TM_STARTUP();
	
	// Populate set 
	printf("Adding %d entries to set\n", initial);
	i = 0;
	
	while (i < initial) {
		val = rand_range_re(&global_seed, range);
		//printf("Adding %d\n", val);
		if (avl_add(set, val, 0, 0) > 0) {
		  //printf("Added %d\n", val);
		  //print_avltree(set);

			last = val;
			i++;
		}
	}
	size = avl_set_size(set);
	tree_size = avl_tree_size(set);
	printf("Set size     : %d\n", size);
	printf("Tree size     : %d\n", tree_size);
	//printf("Level max    : %d\n", levelmax);
	
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
		data[i].unit_tx = unit_tx;
		data[i].alternate = alternate;
		data[i].effective = effective;
		data[i].nb_modifications = 0;
		data[i].nb_add = 0;
		data[i].nb_added = 0;
		data[i].nb_remove = 0;
		data[i].nb_removed = 0;
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
	
#ifdef ICC
	stop = 1;
#else	
	AO_store_full(&stop, 1);
#endif /* ICC */
	
	gettimeofday(&end, NULL);
	printf("STOPPING...\n");
	
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
	max_retries = 0;
	for (i = 0; i < nb_threads; i++) {
		printf("Thread %d\n", i);
		printf("  #add        : %lu\n", data[i].nb_add);
		printf("    #added    : %lu\n", data[i].nb_added);
		printf("  #remove     : %lu\n", data[i].nb_remove);
		printf("    #removed  : %lu\n", data[i].nb_removed);
		printf("  #contains   : %lu\n", data[i].nb_contains);
		printf("  #found      : %lu\n", data[i].nb_found);
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
		(data[i].nb_remove - data[i].nb_removed); 
		updates += (data[i].nb_add + data[i].nb_remove);
		effupds += data[i].nb_removed + data[i].nb_added; 
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
	printf("Duration      : %d (ms)\n", duration);
	printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
	
	printf("#read txs     : ");
	if (effective) {
		printf("%lu (%f / s)\n", effreads, effreads * 1000.0 / duration);
		printf("  #contains   : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
	} else printf("%lu (%f / s)\n", reads, reads * 1000.0 / duration);
	
	printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));
	
	printf("#update txs   : ");
	if (effective) {
		printf("%lu (%f / s)\n", effupds, effupds * 1000.0 / duration);
		printf("  #upd trials : %lu (%f / s)\n", updates, updates * 1000.0 / 
					 duration);
	} else printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);
	
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
	

	//print_avltree(set);
	// Delete set 
        avl_set_delete(set);
	
	// Cleanup STM 
	TM_SHUTDOWN();
	
#ifndef TLS
	pthread_key_delete(rng_seed_key);
#endif /* ! TLS */
	
	free(threads);
	free(data);

	free(maintenance_threads);
	free(maintenance_data);
	
	return 0;
}

