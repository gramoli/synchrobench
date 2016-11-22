/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses of the linked list
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

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>

#include "intset.h"

#if defined SEQUENTIAL
#include "sequential.h"
#elif defined HARRIS
#include "harris.h"
#elif defined VERSIONED
#include "versioned.h"
#elif defined FOMITCHEV
#include "fomitchev.h"
#elif defined LAZY
#include "lazy.h"
#elif defined COUPLING
#include "coupling.h"
#elif defined UNIVERSAL
#include "universal.h"
#elif defined SELFISH
#include "selfish.h"
#else
#error "No algorithm named"
#endif

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_BIAS_RANGE                   (-1)
#define DEFAULT_BIAS_OFFSET                  (-1)
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY              4
#define DEFAULT_LOCKTYPE                2
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE               1
#define XSTR(s)                         STR(s)
#define STR(s)                          #s

// Globals for threads to check
_Atomic(int) stop;

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
 * Returns a pseudo-random value in [1; range].
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given program options [r]ange and [i]nitial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
inline long rand_range(long r) {
	int m = 2147483647;
	long d, v = 0;
	
	do {
		d = (m > r ? r : m);
		v += 1 + (long)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
	int m = 2147483647;
	long d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (long)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}
long rand_range_re(unsigned int *seed, long r);


typedef struct thread_data {
	val_t first;
	long range;
	int bias_enabled;
	long bias_range;
	long bias_offset;
	int update;
	int unit_tx;
	int alternate;
	int effective;
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
	intset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;

void *test(void *data) {
	// Read this locally to prevent possible cache effects.
	thread_data_t d = *(thread_data_t *)data;

	// Wait for all threads to become ready.
	barrier_cross(d.barrier);

	// Last value to be inserted, or -ve if last action was remove.
	// Start -ve here so that alternate mode will not hang.
	int last = -1;

	// If we're in bias mode, should start positive so that something
	// gets removed.
	if (d.bias_enabled)
		last = d.bias_offset;

	while (atomic_load(&stop) == 0) {
		// Is the next op an update?
		int do_update;
		if (d.effective)
			do_update = (100 * (d.nb_added + d.nb_removed)) < (d.update * (d.nb_add + d.nb_remove + d.nb_contains));
		else
			do_update = rand_range_re(&d.seed, 100) - 1 < d.update;
		
		// Value on which to operate. (may be modified later,
		// if in alternate mode or bias mode)
		int value = rand_range_re(&d.seed, d.range);

		// If we're in bias mode, restrict the range, and just choose adding or removing at random
		if (d.bias_enabled) {
			value = d.bias_offset + rand_range_re(&d.seed, d.bias_range) - 1;
			last = (rand_range_re(&d.seed, 2) == 1) ? -1 : value;
		}

		if (do_update && last < 0) {
			// Add
			if (set_insert(d.set, value)) {
				d.nb_added++;
				last = value;
			}
			d.nb_add++;
		} else if (do_update && last >= 0) {
			// Remove
			
			// If in alternate mode, remove the last item added.
			if (d.alternate) {
				if (set_remove(d.set, last))
					d.nb_removed++;
				last = -1;
			} else {
				if (set_remove(d.set, value)) {
					d.nb_removed++;
					last = -1;
				}
			}
			d.nb_remove++;
		} else {
			// Read
			if (d.alternate) {
				if (d.update == 0) {
					if (last < 0)
						last = value = d.first;
					else
						last = -1;
				} else { // update != 0
					if (last >= 0)
						value = last;
				}
			}

			if (set_contains(d.set, value))
				d.nb_found++;
			d.nb_contains++;
		}
	}

	*(thread_data_t *)data = d;
	
	return NULL;
}

int main(int argc, char **argv) {
	struct option long_options[] = {
		// These options don't set a flag
		{"help",                      no_argument,       NULL, 'h'},
		{"duration",                  required_argument, NULL, 'd'},
		{"initial-size",              required_argument, NULL, 'i'},
		{"thread-num",                required_argument, NULL, 't'},
		{"range",                     required_argument, NULL, 'r'},
		{"seed",                      required_argument, NULL, 'S'},
		{"update-rate",               required_argument, NULL, 'u'},
		{"bias-range",		      required_argument, NULL, 'b'},
		{"bias-offset",               required_argument, NULL, 'u'},
		{"elasticity",                required_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
	
	intset_t *set;
	int i, c, size;
	val_t last = 0; 
	val_t val = 0;
	unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read, 
	aborts_locked_write, aborts_validate_read, aborts_validate_write, 
	aborts_validate_commit, aborts_invalid_memory, aborts_double_write, 
	max_retries, failures_because_contention;
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
	long bias_range = DEFAULT_BIAS_RANGE;
	long bias_offset = DEFAULT_BIAS_OFFSET;
	int bias_enabled = 0;
	int seed = DEFAULT_SEED;
	int update = DEFAULT_UPDATE;
	int unit_tx = DEFAULT_ELASTICITY;
	int alternate = DEFAULT_ALTERNATE;
	int effective = DEFAULT_EFFECTIVE;
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hAf:d:i:t:r:S:u:b:B:x:", long_options, &i);
		
		if(c == -1)
			break;
		
		if(c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;
		
		switch(c) {
				case 0:
					/* Flag is automatically set */
					break;
				case 'h':
					printf("intset -- STM stress test "
								 "(linked list)\n"
								 "\n"
								 "Usage:\n"
								 "  intset [options...]\n"
								 "\n"
								 "Options:\n"
								 "  -h, --help\n"
								 "        Print this message\n"
								 "  -A, --alternate (default="XSTR(DEFAULT_ALTERNATE)")\n"
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
								 "  -b, --bias-range <int>\n"
								 "        If used, updates will take place in range [B, B+b)\n"
								 "  -B, --bias-offset <int>\n"
								 "        If used, updates will take place in range [B, B+b)\n"
								 "  -x, --elasticity (default=4)\n"
								 "        Use elastic transactions\n"
								 "        0 = non-protected,\n"
								 "        1 = normal transaction,\n"
								 "        2 = read elastic-tx,\n"
								 "        3 = read/add elastic-tx,\n"
								 "        4 = read/add/rem elastic-tx,\n"
								 "        5 = all recursive elastic-tx,\n"
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
				case 'b':
					bias_range = atol(optarg);
					break;
				case 'B':
					bias_offset = atol(optarg);
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
	if (bias_range != DEFAULT_BIAS_RANGE || bias_offset != DEFAULT_BIAS_OFFSET) {
		bias_enabled = 1;
		assert(bias_range >= 0);
		assert(bias_offset > 0);
	}
	
	printf("Bench type   : " ALGONAME "\n");
	printf("Duration     : %d\n", duration);
	printf("Initial size : %d\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Value range  : %ld\n", range);
	if (bias_enabled) {
		printf("Biased range: [%ld, %ld)\n", bias_offset, bias_offset+bias_range);
	}
	printf("Seed         : %d\n", seed);
	printf("Update rate  : %d\n", update);
	printf("Elasticity   : %d\n", unit_tx);
	printf("Alternate    : %d\n", alternate);
	printf("Effective    : %d\n", effective);
	printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
				 (int)sizeof(int),
				 (int)sizeof(long),
				 (int)sizeof(void *),
				 (int)sizeof(uintptr_t));
  printf("Node size    : %d\n", (int)sizeof(node_t));
	
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
	
	set = set_new();
	stop = 0;
	
	/* Populate set */
	printf("Adding %d entries to set\n", initial);
	i = 0;
	while (i < initial) {
		val = rand_range(range);
		if (set_insert(set, val)) {
			last = val;
			i++;
		}
	}
	size = set_size(set);
	printf("Set size     : %d\n", size);
	
	/* Access set from all threads */
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);
		data[i].first = last;
		data[i].bias_enabled = bias_enabled;
		data[i].bias_range = bias_range;
		data[i].bias_offset = bias_offset;
		data[i].range = range;
		data[i].update = update;
		data[i].unit_tx = unit_tx;
		data[i].alternate = alternate;
		data[i].effective = effective;
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
	
/*
#ifdef ICC
	stop = 1;
#else	
	AO_store_full(&stop, 1);
#endif // ICC
*/
        atomic_store(&stop, 1);
	
	gettimeofday(&end, NULL);
	printf("STOPPING...\n");
	
	/* Wait for thread completion */
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
		printf("    #found    : %lu\n", data[i].nb_found);
		printf("  #aborts     : %lu\n", data[i].nb_aborts);
		printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
		printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
		printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
		printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
		printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
		printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
		printf("    #inv-mem  : %lu\n", data[i].nb_aborts_double_write);
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
		effreads += data[i].nb_contains + 
			(data[i].nb_add - data[i].nb_added) + 
			(data[i].nb_remove - data[i].nb_removed); 
		updates += (data[i].nb_add + data[i].nb_remove);
		effupds += data[i].nb_removed + data[i].nb_added; 
		size += data[i].nb_added - data[i].nb_removed;
		if (max_retries < data[i].max_retries)
			max_retries = data[i].max_retries;
	}
	printf("Set size      : %d (expected: %d)\n", set_size(set), size);
	if (set_size(set) != size) {
		printf("ERROR: Set size did not match expected.\n");
	}
	printf("Duration      : %d (ms)\n", duration);
	printf("#txs          : %lu (%f / s)\n", reads + updates, 
				 (reads + updates) * 1000.0 / duration);
	
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
	
	
	printf("#aborts       : %lu (%f / s)\n", aborts, 
				 aborts * 1000.0 / duration);
	printf("  #lock-r     : %lu (%f / s)\n", aborts_locked_read, 
				 aborts_locked_read * 1000.0 / duration);
	printf("  #lock-w     : %lu (%f / s)\n", aborts_locked_write, 
				 aborts_locked_write * 1000.0 / duration);
	printf("  #val-r      : %lu (%f / s)\n", aborts_validate_read, 
				 aborts_validate_read * 1000.0 / duration);
	printf("  #val-w      : %lu (%f / s)\n", aborts_validate_write, 
				 aborts_validate_write * 1000.0 / duration);
	printf("  #val-c      : %lu (%f / s)\n", aborts_validate_commit, 
				 aborts_validate_commit * 1000.0 / duration);
	printf("  #inv-mem    : %lu (%f / s)\n", aborts_invalid_memory, 
				 aborts_invalid_memory * 1000.0 / duration);
	printf("  #dup-w      : %lu (%f / s)\n", aborts_double_write, 
				 aborts_double_write * 1000.0 / duration);
	printf("  #failures   : %lu\n",  failures_because_contention);
	printf("Max retries   : %lu\n", max_retries);
	
	/* Delete set */
	set_delete(set);
	
	free(threads);
	free(data);
	
	return 0;
}
