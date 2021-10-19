/*
 *  main.c
 *  
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "linkedlist.h"

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
	intset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;



void *test(void *data)
{
	int val, last = 0;
	thread_data_t *d = (thread_data_t *)data;
	
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
				if (set_add(d->set, val, TRANSACTIONAL)) {
					d->diff++;
					last = val;
				}
				d->nb_add++;
			} else {
				/* Remove last value */
				if (set_remove(d->set, last, TRANSACTIONAL))
					d->diff--;
				d->nb_remove++;
				last = -1;
			}
		} else {
			/* Look for random value */
			val = (rand_r(&d->seed) % d->range) + 1;
			if (set_contains(d->set, val, TRANSACTIONAL))
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
	
	intset_t *set;
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
				/* Flag is automatically set */
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
					   "        5 = all recursive elastic-tx,\n"
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
	printf("Set type     : linked list\n");
#endif
	printf("Duration     : %d\n", duration);
	printf("Initial size : %d\n", initial);
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
	
	set = set_new();
	
	stop = 0;
	
	/* Init STM */
	printf("Initializing STM\n");
	stm_init();
	mod_mem_init();
	
	if (stm_get_parameter("compile_flags", &s))
		printf("STM flags    : %s\n", s);
	
	/* Populate set */
	printf("Adding %d entries to set\n", initial);
	i = 0;
	while (i < initial) {
		val = (rand() % range) + 1;
		if (set_add(set, val, 0))
			i++;
	}
	size = set_size(set);
	printf("Set size     : %d\n", size);
	
	/* Access set from all threads */
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
	
	/* Catch some signals */
	if (signal(SIGHUP, catcher) == SIG_ERR ||
		signal(SIGINT, catcher) == SIG_ERR ||
		signal(SIGTERM, catcher) == SIG_ERR) {
		perror("signal");
		exit(1);
	}
	
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
	AO_store_full(&stop, 1);
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
	printf("Set size      : %d (expected: %d)\n", set_size(set), size);
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
	
	/* Delete set */
	set_delete(set);
	
	/* Cleanup STM */
	stm_exit();
	
	free(threads);
	free(data);
	
	return 0;
}
