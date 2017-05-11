/*
 * File:
 *   test.c
 * Author(s):
 *   Tyler Crain <tyler.crain@irisa.fr>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses to the fast lock-free unbalanced binary search tree
 *
 * Copyright (c) 2009-2010.
 *
 * test.c is part of Synchrobench
 * 
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>

#include "wfrbt.h"
#include "operations.h"

#define DEFAULT_DURATION                1000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY              4
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE               1

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

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


void *test3(void *data) {
	
  thread_data_t *d = (thread_data_t *)data;
	
  /* Wait on barrier */
  barrier_cross(d->barrier);
	
  while (stop == 0) {;}
  return NULL;
}

void *test(void *data) {
  val_t last = -1;
  val_t val = 0;
  int unext; 

  thread_data_t *d = (thread_data_t *)data;

  /* Wait on barrier */
  barrier_cross(d->barrier);
	
  /* Is the first op an update? */
  unext = (rand_range_re(&d->seed, 100) - 1 < d->update);

  //#ifdef ICC
  while (stop == 0) {
    //#else
    //while (AO_load_full(&stop) == 0) {
    //#endif /* ICC */
		
    if (unext) { // update
			
      if (last < 0) { // add
				
	val = rand_range_re(&d->seed, d->range);
	assert(val > 0);
	if (insert(d,val)) {
	  last = val;
	} 				
	d->nb_add++;
				
      } else { // remove
				
	if (d->alternate) { // alternate mode (default)
					
	  delete_node(d, last);
	  
	  last = -1;
					
	} else {
					
	  // Random computation only in non-alternated cases 
	  val = rand_range_re(&d->seed, d->range);
	  // Remove one random value 
	  if (delete_node(d, val)) {
	    // Repeat until successful, to avoid size variations 
	    last = -1;
	  } 
					
	}
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
			
      /*if (d->effective && last)
	val = last;
	else 
	val = rand_range_re(&d->seed, d->range);*/
			
      if (search(d, val)) 
	      d->nb_found++;
      d->nb_contains++;
			
    }
		
    /* Is the next op an update? */
    if (d->effective) { // a failed remove/add is a read-only tx
      unext = ((100 * (d->nb_added + d->nb_removed))
	       < (d->update * (d->nb_add + d->nb_remove + d->nb_contains)));
    } else { // remove/add (even failed) is considered as an update
      unext = ((rand_range_re(&d->seed, 100) - 1) < d->update);
    }
		
    //#ifdef ICC
  }
  //#else
  //	}
  //#endif /* ICC */
	
  return NULL;
}


void *test2(void *data)
{
  val_t val, newval, last = 0;
  thread_data_t *d = (thread_data_t *)data;
	
#ifdef TLS
  rng_seed = &d->seed;
#else /* ! TLS */
  pthread_setspecific(rng_seed_key, &d->seed);
#endif /* ! TLS */
	
  /* Wait on barrier */
  barrier_cross(d->barrier);
	
  last = -1;
	
#ifdef ICC
  while (stop == 0) {
#else
    while (AO_load_full(&stop) == 0) {
#endif /* ICC */
			
      val = rand_range_re(&d->seed, 100) - 1;
      if (val < d->update) {
	if (last < 0) {
	  /* Add random value */
	  val = rand_range_re(&d->seed, d->range);
	  if (insert(d, val)) {
	    last = val;
	  }
	  d->nb_add++;
	} else {
	  if (d->alternate) {
	    /* Remove last value */
	    if (delete_node(d, last)) {
	      last = -1; 
	    }
	    d->nb_remove++;
	  } else {
	    /* Random computation only in non-alternated cases */
	    newval = rand_range_re(&d->seed, d->range);
	    /* Remove one random value */
	    if (delete_node(d, newval)) {
	      /* Repeat until successful, to avoid size variations */
	      last = -1;
	    }
	    d->nb_remove++;
	  }
	}
      } else {
	/* Look for random value */
	val = rand_range_re(&d->seed, d->range);
	if (search(d, val))
	  d->nb_found++;
	d->nb_contains++;
      }
			
    }
		
    return NULL;
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
      {"unit-tx",                   required_argument, NULL, 'x'},
      {NULL, 0, NULL, 0}
    };

    node_t *set;		
    //sl_intset_t *set;
    int i, c, size;
    val_t last = 0; 
    val_t val = 0;
    unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read, 
      aborts_locked_write, aborts_validate_read, aborts_validate_write, 
      aborts_validate_commit, aborts_invalid_memory, max_retries;
    thread_data_t *data;
    pthread_t *threads;
    pthread_attr_t attr;
    barrier_t barrier;
    struct timeval start, end;
    struct timespec timeout;
    int duration = DEFAULT_DURATION;
    int initial = DEFAULT_INITIAL;
    int nb_threads = DEFAULT_NB_THREADS;
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
	/* Flag is automatically set */
	break;
      case 'h':
	printf("Lock-Free BST stress test "
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
	       "  -x, --unit-tx (default=1)\n"
	       "        Use unit transactions\n"
	       "        0 = non-protected,\n"
	       "        1 = normal transaction,\n"
	       "        2 = read unit-tx,\n"
	       "        3 = read/add unit-tx,\n"
	       "        4 = read/add/rem unit-tx,\n"
	       "        5 = all recursive unit-tx,\n"
	       "        6 = harris lock-free\n"
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
		
    printf("Set type     : BST\n");
    printf("Duration     : %d\n", duration);
    printf("Initial size : %d\n", initial);
    printf("Nb threads   : %d\n", nb_threads);
    printf("Value range  : %ld\n", range);
    printf("Seed         : %d\n", seed);
    printf("Update rate  : %d\n", update);
    printf("Lock alg.    : %d\n", unit_tx);
    printf("Alternate    : %d\n", alternate);
    printf("Effective    : %d\n", effective);
    printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
	   (int)sizeof(int),
	   (int)sizeof(long),
	   (int)sizeof(void *),
	   (int)sizeof(uintptr_t));
		
    timeout.tv_sec = duration / 1000;
    timeout.tv_nsec = (duration % 1000) * 1000000;
		
    data = (thread_data_t *)xmalloc(nb_threads * sizeof(thread_data_t));
    threads = (pthread_t *)xmalloc(nb_threads * sizeof(pthread_t));
		
    if (seed == 0)
      srand((int)time(0));
    else
      srand(seed);
		
    node_t * newRT = (node_t*)xmalloc(sizeof(node_t));
 node_t * newLC = (node_t*)xmalloc(sizeof(node_t));
 node_t * newRC = (node_t*)xmalloc(sizeof(node_t));
 
 /// Sentinel keys are larger than all other keys in the tree
 newRT->key = range+2;
 newLC->key = range+1;
 newRC->key = range+2;
 
 
 newRT->child.AO_val1 = create_child_word(newLC,UNMARK, UNFLAG);
 newRT->child.AO_val2 = create_child_word(newRC,UNMARK, UNFLAG);
		
		  i = 0;
		  data[i].first = last;
      data[i].range = range;
      data[i].update = update;
      data[i].alternate = alternate;
      data[i].effective = effective;
      data[i].nb_add = 0;
      data[i].nb_added = 0;
      data[i].nb_remove = 0;
      data[i].nb_removed = 0;
      data[i].nb_contains = 0;
      data[i].nb_found = 0;
      data[i].barrier = &barrier;
      data[i].rootOfTree = newRT;
      data[i].id = i;
		  data[i].recycledNodes.reserve(RECYCLED_VECTOR_RESERVE);
      data[i].sr = new seekRecord_t;
      data[i].ssr = new seekRecord_t;
  
    /* Populate set */
    printf("Adding %d entries to set\n",initial);
    i = 0;
    while (i < initial) {
      val = rand_range_re(&global_seed, range);
      if (insert(&data[0], val)) {
	last = val;
	
	i++;
      }
    }
    
    size = data[0].nb_added + 2; /// Add 2 for the 2 sentinel keys
    //size = sl_set_size(set);
    //printf("Set size     : %d\n", size);
    printf("Set size (TENTATIVE) : %d\n", initial);
    printf("Level max    : %d\n", levelmax);
		
    /* Access set from all threads */
    barrier_init(&barrier, nb_threads + 1);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (i = 0; i < nb_threads; i++) {
      printf("Creating thread %d\n", i);
      data[i].first = last;
      data[i].range = range;
      data[i].update = update;
      data[i].alternate = alternate;
      data[i].effective = effective;
      data[i].nb_add = 0;
      data[i].nb_added = 0;
      data[i].nb_remove = 0;
      data[i].nb_removed = 0;
      data[i].nb_contains = 0;
      data[i].nb_found = 0;
      data[i].barrier = &barrier;
      data[i].rootOfTree = newRT;
      data[i].id = i;
      data[i].recycledNodes.reserve(RECYCLED_VECTOR_RESERVE);
      data[i].sr = new seekRecord_t;
      data[i].ssr = new seekRecord_t;
      if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
	fprintf(stderr, "Error creating thread\n");
	exit(1);
      }
    }
    pthread_attr_destroy(&attr);
		
    /* Start threads */
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
		
    /* Wait for thread completion */
    for (i = 0; i < nb_threads; i++) {
      if (pthread_join(threads[i], NULL) != 0) {
	fprintf(stderr, "Error waiting for thread completion\n");
	exit(1);
      }
    }
		
    duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - 
      (start.tv_sec * 1000 + start.tv_usec / 1000);
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
      reads += data[i].nb_contains;
      effreads += data[i].nb_contains + 
	(data[i].nb_add - data[i].nb_added) + 
	(data[i].nb_remove - data[i].nb_removed); 
      updates += (data[i].nb_add + data[i].nb_remove);
      effupds += data[i].nb_removed + data[i].nb_added; 
      size += data[i].nb_added - data[i].nb_removed;
      
    }
    
    /// Sanity check
    in_order_visit((newRT));
    
    //printf("Set size      : %d (expected: %d)\n", sl_set_size(set), size);
    printf("Duration      : %d (ms)\n", duration);
    printf("#txs          : %lu (%f / s)\n", reads + updates, 
	   (reads + updates) * 1000.0 / duration);
		
    printf("#read txs     : ");
    if (effective) {
      printf("%lu (%f / s)\n", effreads, effreads * 1000.0 / duration);
      printf("  #contains   : %lu (%f / s)\n", reads, reads * 1000.0 / 
	     duration);
    } else printf("%lu (%f / s)\n", reads, reads * 1000.0 / duration);
		
    printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));
		
    printf("#update txs   : ");
    if (effective) {
      printf("%lu (%f / s)\n", effupds, effupds * 1000.0 / duration);
      printf("  #upd trials : %lu (%f / s)\n", updates, updates * 1000.0 / 
	     duration);
    } else printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);
		
		
    /* Delete set */
    //sl_set_delete(set);
		
#ifndef TLS
    pthread_key_delete(rng_seed_key);
#endif /* ! TLS */
		
    free(threads);
    free(data);
		
    return 0;
  }

