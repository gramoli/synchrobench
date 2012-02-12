/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Running deque/enqueue operations on the double-ended queue.
 * 
 * Copyright (c) 2008-2009.
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

#include "deque.h"

val_t lhint;
val_t rhint;

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
 */
inline int rand_range(int r) {
	int m = RAND_MAX;
	int d, v = 0;
	
	do {
		d = (m > r ? r : m);
		v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}

/* Re-entrant version of rand_range(r) */
inline int rand_range_re(unsigned int *seed, int r) {
	int m = RAND_MAX;
	int d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}

typedef struct thread_data {
	int range;
	int push_rate;
	int unit_tx;
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
	circarr_t *queue;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;

void *test(void *data)
{
  int val, percent, last = 0;
  thread_data_t *d = (thread_data_t *)data;
	
  /* Create transaction */
  TM_THREAD_ENTER();
  /* Wait on barrier */
  barrier_cross(d->barrier);
	
  last = -1;
	
#ifdef ICC
	while (stop == 0) {
#else
	while (AO_load_full(&stop) == 0) {
#endif /* ICC */
	
	  percent = rand_range_re(&d->seed, 100) - 1;

		if (percent < 51) {
			val = rand_range_re(&d->seed, d->range);
			/* Push a new value */
			if (percent < (d->push_rate/2)) {
				if (deque_rightpush(d->queue, val, 1)) {
					d->diff++;	
				}
			} else {	
				if (deque_leftpush(d->queue, val, 1)) {
					d->diff++;	
				}	
			}
			d->nb_add++;
		} else {
			// Pop the last pushed value 
			if (percent > (d->push_rate/2)) {
			  if (deque_leftpop(d->queue, 1)) {
					d->diff--;	
				}
			} else {	
			  if (deque_leftpop(d->queue, 1)) {
					d->diff--;	
				}
			}
			d->nb_remove++;
		}
	}
	/* Free transaction */
	TM_THREAD_EXIT();
	
	return NULL;
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
		{"elasticity",                required_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
		
	circarr_t *queue;
	int i, c, val, size;
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
	int push_rate = DEFAULT_PUSHRATE;
	int unit_tx = 4;
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hd:i:n:r:s:p:x:"
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
				printf("double-ended queue -- STM stress test\n"
					   "\n"
					   "Usage:\n"
					   "  deque [options...]\n"
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
					   "        Range of integer values inserted in queue (default=" XSTR(DEFAULT_RANGE) ")\n"
					   "  -s, --seed <int>\n"
					   "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
					   "  -p, --push-rate <int>\n"
					   "        Percentage of push over pop (default=" XSTR(DEFAULT_PUSHRATE) ")\n"
					   "  -x, --elasticity (default=4)\n"
					   "        Use elastic transactions\n"
					   "        0 = non-protected,\n"
					   "        1 = normal transaction,\n"
					   "        2 = read elastic-tx,\n"
					   "        3 = read/add elastic-tx,\n"
					   "        4 = read/add/rem elastic-tx,\n"
					   "        5 = all recursive elastic-tx,\n"
					   "        6 = herlihy obstruction-free\n"
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
			case 'p':
				push_rate = atoi(optarg);
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
	assert(push_rate >= 0 && push_rate <= 100);
	
	printf("Bench type   : double-ended queue\n");
	printf("Duration     : %d\n", duration);
	printf("Initial size : %d\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Value range  : %d\n", range);
	printf("Seed         : %d\n", seed);
	printf("Push/pop rate: %d\n", push_rate);
	printf("Elasticity   : %d\n", unit_tx);
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
	
	if (seed == 0)
		srand((int)time(0));
	else
		srand(seed);
	
	queue = queue_new();
	stop = 0;
	
	/* Init STM */
	printf("Initializing STM\n");
	 
	TM_STARTUP();
	
	/* Populate queue */
	printf("Adding %d entries to queue\n", initial);
	i = 0;
	while (i < initial) {
		val = rand_range(range);
		if (val > (range/2)) {
			if (seq_rightpush(queue, val, 0)) i++;
		} else {
			if (seq_leftpush(queue, val, 0)) i++;
		}
	}
	
	size = queue_size(queue);
	printf("Set size     : %d\n", size);
	
	/* Access queue from all threads */
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);
		data[i].range = range;
		data[i].push_rate = push_rate;
		data[i].unit_tx = unit_tx;
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
		data[i].queue = queue;
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
	printf("Set size      : %d (expected: %d)\n", queue_size(queue), size);
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
	
	/* Delete queue */
	queue_delete(queue);
	
	/* Cleanup STM */
	TM_SHUTDOWN();
	
	free(threads);
	free(data);
	
	return 0;
}
