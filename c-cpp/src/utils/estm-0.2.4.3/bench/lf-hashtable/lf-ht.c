/*
 *  lf-ht.c
 *  
 *  Lock-free Hashtable
 *  Implementation of an integer set using a stm-based/lock-free hashtable.
 *  The hashtable contains several buckets, each represented by a linked
 *  list, since hashing distinct keys may lead to the same bucket.
 *
 *  Created by Vincent Gramoli on 1/13/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "../lf-linkedlist/linkedlist.h"

#define DEFAULT_MOVE                    0
#define DEFAULT_SNAPSHOT                0
#define DEFAULT_LOAD                    1
#define MAXHTLENGTH                     65535

/* Hashtable length (# of buckets) */
static unsigned int maxhtlength;

/* Hashtable seed */
#ifdef TLS
static __thread unsigned int *rng_seed;
#else /* ! TLS */
static pthread_key_t rng_seed_key;
#endif /* ! TLS */

typedef struct ht_intset {
	intset_t *buckets[MAXHTLENGTH];
} ht_intset_t;

void ht_delete(ht_intset_t *set) {
	node_t *node, *next;
	int i;
	
	for (i=0; i < maxhtlength; i++) {
		node = set->buckets[i]->head;
		while (node != NULL) {
			next = node->next;
			free(node);
			node = next;
		}
	}
	free(set);
}

int ht_size(ht_intset_t *set) {
	int size = 0;
	node_t *node;
	int i;
	
	for (i=0; i < maxhtlength; i++) {
		//size += set_size(set->buckets[i]);
		
		node = set->buckets[i]->head->next;
		while (node->next) {
			size++;
			node = node->next;
		}
		
	}
	return size;
}

int floor_log_2(unsigned int n) {
	int pos = 0;
	printf("n result = %d\n", n);
	if (n >= 1<<16) { n >>= 16; pos += 16; }
	if (n >= 1<< 8) { n >>=  8; pos +=  8; }
	if (n >= 1<< 4) { n >>=  4; pos +=  4; }
	if (n >= 1<< 2) { n >>=  2; pos +=  2; }
	if (n >= 1<< 1) {           pos +=  1; }
	printf("floor result = %d\n", pos);
	return ((n == 0) ? (-1) : pos);
}

ht_intset_t *ht_new() {
	ht_intset_t *set;
	int i;
	
	if ((set = (ht_intset_t *)malloc(sizeof(ht_intset_t))) == NULL) {
		perror("malloc");
		exit(1);
	}  
	for (i=0; i < maxhtlength; i++) {
		set->buckets[i] = set_new();
	}
	return set;
}

int ht_contains(ht_intset_t *set, int val, int transactional) {
	int addr;
	
#ifdef DEBUG
	printf("++> ht_contains(%d)\n", (int) val);
	IO_FLUSH;
#endif
	addr = val % maxhtlength;
	if (transactional == 5)
	  return set_contains(set->buckets[addr], val, 4);
	else
	  return set_contains(set->buckets[addr], val, transactional);
}

int ht_add(ht_intset_t *set, int val, int transactional) {
	int addr;
	
#ifdef DEBUG
	printf("++> ht_add(%d)\n", (int) val);
	IO_FLUSH;
#endif
	addr = val % maxhtlength;
	if (transactional == 5)
		return set_add(set->buckets[addr], val, 4);
	else 
		return set_add(set->buckets[addr], val, transactional);
}

int ht_remove(ht_intset_t *set, int val, int transactional) {
	int addr;
    
#ifdef DEBUG
	printf("++> ht_contains(%d)\n", (int) val);
	IO_FLUSH;
#endif
	addr = val % maxhtlength;
	if (transactional == 5)
		return set_remove(set->buckets[addr], val, 4);
	else
		return set_remove(set->buckets[addr], val, transactional);
}

/* 
 * Move an element from one bucket to another.
 * It is equivalent to changing the key associated with some value.
 */
int ht_move(ht_intset_t *set, int val1, int val2, int transactional) {
	int v, addr1, addr2, result = 0;
	node_t *n, *prev, *next, *prev2, *next2;
	
	switch(transactional) {
		case 0: /* sequential code */
			addr1 = val1 % maxhtlength;
			addr2 = val2 % maxhtlength;
			result =  (set_remove(set->buckets[addr1], val1, transactional) && 
					   set_add(set->buckets[addr2], val2, transactional));
			break;
			
		case 1: /* normal transaction */
			TX_START(NL);
			addr1 = val1 % maxhtlength;
			prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
			next = (node_t *)TX_LOAD(&prev->next);
			while(1) {
				v = TX_LOAD(&next->val);
				if (v >= val1) break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			if (v == val1) {
				/* Physically removing */
				n = (node_t *)TX_LOAD(&next->next);
				TX_STORE(&prev->next, n);
				FREE(next, sizeof(node_t));
				/* Inserting */
				addr2 = val2 % maxhtlength;
				prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
				next = (node_t *)TX_LOAD(&prev->next);
				while(1) {
					v = TX_LOAD(&next->val);
					if (v >= val2) break;
					prev = next;
					next = (node_t *)TX_LOAD(&prev->next);
				}
				if (v != val2) {
					TX_STORE(&prev->next, new_node(val2, next, transactional));
				}
				/* Even if the key is already in, the operation succeeds */
				result = 1;
			} else result = 0;
			TX_END;
			break;
			
		case 2: 
		case 3: 
		case 4: /* Optimized elastic transactions */
			TX_START(EL);
			addr1 = val1 % maxhtlength;
			prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
			next = (node_t *)TX_LOAD(&prev->next);
			while(1) {
				v = TX_LOAD(&next->val);
				if (v >= val1) break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			if (v == val1) {
				addr2 = val2 % maxhtlength;
				prev2 = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
				next2 = (node_t *)TX_LOAD(&prev2->next);
				while(1) {
					v = TX_LOAD(&next2->val);
					if (v >= val2) break;
					prev2 = next2;
					next2 = (node_t *)TX_LOAD(&prev2->next);
				}
				/* Physically removing */
				n = (node_t *)TX_LOAD(&next->next);
				TX_STORE(&prev->next, n);
				//TX_STORE(next, NULL);
				FREE(next, sizeof(node_t));
				/* Inserting */
				if (v != val2) {
					TX_STORE(&prev2->next, new_node(val2, next2, transactional));
				}
				/* Even if the key is already in, the operation succeeds */
				result = 1;
			} else result = 0;
			TX_END;
			break;
			
		case 5: /* Naive elastic transactions */
			TX_START(EL);
			addr1 = val1 % maxhtlength;
			prev = (node_t *)TX_LOAD(&set->buckets[addr1]->head);
			next = (node_t *)TX_LOAD(&prev->next);
			while(1) {
				v = TX_LOAD(&next->val);
				if (v >= val1) break;
				prev = next;
				next = (node_t *)TX_LOAD(&prev->next);
			}
			if (v == val1) {
				/* Physically removing */
				n = (node_t *)TX_LOAD(&next->next);
				TX_STORE(&prev->next, n);
				FREE(next, sizeof(node_t));
				/* Inserting */
				addr2 = val2 % maxhtlength;
				prev = (node_t *)TX_LOAD(&set->buckets[addr2]->head);
				next = (node_t *)TX_LOAD(&prev->next);
				while(1) {
					v = TX_LOAD(&next->val);
					if (v >= val2) break;
					prev = next;
					next = (node_t *)TX_LOAD(&prev->next);
				}
				if (v != val2) {
					TX_STORE(&prev->next, new_node(val2, next, transactional));
				}
				/* Even if the key is already in, the operation succeeds */
				result = 1;
			} else result = 0;
			TX_END;
			break;
			
		case 6: /* No CAS-based implementation is provided */
			printf("ht_snapshot: No other implementation of atomic snapshot is available\n");
			exit(1);
			
		default:	
			result=0;
			printf("number %d do not correspond to elasticity.\n", transactional);
			exit(1);
	}
	return result;
}

/*
 * Atomic snapshot of the hash table.
 * It parses the whole hash table to sum all elements.
 *
 * Observe that this particular operation (atomic snapshot) cannot be implemented using 
 * elastic transactions in combination with the move operation, however, normal transactions
 * compose with elastic transactions.
 */
int ht_snapshot(ht_intset_t *set, int transactional) {
	int i, result, sum = 0;
	node_t *next;
	
	switch(transactional) {
		case 0: /* sequential code */
			for (i=0; i < maxhtlength; i++) {
				next = set->buckets[i]->head->next;
				while(next->next) {
					sum += next->val;
					next = next->next;
				}
			}
			result = 1;
			break;
			
		case 1: /* normal transaction */
		case 2:
		case 3:
		case 4:
		case 5:
			TX_START(NL);
			for (i=0; i < maxhtlength; i++) {
				next = (node_t *)TX_LOAD(&set->buckets[i]->head->next);
				while(next->next) {
					sum += TX_LOAD(&next->val);
					next = (node_t *)TX_LOAD(&next->next);
				}
			}
			TX_END;
			result = 1;
			break;
			
		case 6: /* No CAS-based implementation is provided */
			result = 0;
			printf("ht_snapshot: No other implementation of atomic snapshot is available\n");
			exit(1);
			
		default:
			result=0;
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
	int v = 0;
	int d;
	
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
	int move;
	int snapshot;
#ifndef USE_RBTREE
	int unit_tx;
#endif
	unsigned long nb_add;
	unsigned long nb_remove;
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
	int diff;
	unsigned int seed;
	ht_intset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;


void *test(void *data)
{
	int val, last, flag = 1;
	thread_data_t *d = (thread_data_t *)data;
	
	/* Create transaction */
	stm_init_thread();
	/* Wait on barrier */
	barrier_cross(d->barrier);
	
	last = 0; // to avoid warning
	while (stop == 0) {
		val = rand_r(&d->seed) % 100;
		/* added for HashTables */
		if (val < d->update) {
			if (val >= d->move) { /* update without move */
				if (flag) {
					/* Add random value */
					val = (rand_r(&d->seed) % d->range) + 1;
					if (ht_add(d->set, val, TRANSACTIONAL)) {
						d->diff++;
						last = val;
						flag = 0;
					}
					d->nb_add++;
				} else {
					/* Remove last value */
					if (ht_remove(d->set, last, TRANSACTIONAL))  
						d->diff--;
					d->nb_remove++;
					flag = 1;
				} 
			} else { /* move */
				val = (rand_r(&d->seed) % d->range) + 1;
				if (ht_move(d->set, last, val, TRANSACTIONAL)) {
					d->nb_moved++;
					last = val;
				}
				d->nb_move++;
			}
		} else {
			if (val >= d->update + d->snapshot) { /* read-only without snapshot */
				/* Look for random value */
				val = (rand_r(&d->seed) % d->range) + 1;
				if (ht_contains(d->set, val, TRANSACTIONAL))
					d->nb_found++;
				d->nb_contains++;
			} else { /* snapshot */
				if (ht_snapshot(d->set, TRANSACTIONAL))
					d->nb_snapshoted++;
				d->nb_snapshot++;
			}
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
	/* Free transaction */
	stm_exit_thread();
	
	return NULL;
}

void catcher(int sig)
{
	printf("CAUGHT SIGNAL %d\n", sig);
}

void print_set(intset_t *set) {
	node_t *curr, *tmp;
	
	curr = set->head;
	tmp = curr;
	do {
		printf(" - v%d", (int) curr->val);
		tmp = curr;
		curr = tmp->next;
	} while (curr->val != VAL_MAX);
	printf(" - v%d", (int) curr->val);
	printf("\n");
}

void print_ht(ht_intset_t *set) {
	int i;
	for (i=0; i < maxhtlength; i++) {
		print_set(set->buckets[i]);
	}
}

inline int rand_65535()
{
#ifdef TLS
	return rand_r(rng_seed) % 65535;
#else /* ! TLS */
	return rand_r((unsigned int *)pthread_getspecific(rng_seed_key)) % 65535;
#endif /* ! TLS */
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
		{"move-rate",                 required_argument, NULL, 'm'},
		{"snapshot-rate",             required_argument, NULL, 'a'},
		{"elasticity",                required_argument, NULL, 'x'},
		{NULL, 0, NULL, 0}
	};
	
	ht_intset_t *set;
	int i, c, val, size;
	char *s;
	unsigned long reads, updates, moves, snapshots, aborts, 
    aborts_locked_read, aborts_locked_write, aborts_validate_read, 
    aborts_validate_write, aborts_validate_commit, aborts_invalid_memory, 
	aborts_double_write,
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
	int range = DEFAULT_RANGE;
	int seed = DEFAULT_SEED;
	int update = DEFAULT_UPDATE;
	int load_factor = DEFAULT_LOAD;
	int move = DEFAULT_MOVE;
	int snapshot = DEFAULT_SNAPSHOT;
	int unit_tx = 4;
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hd:i:n:r:s:u:m:a:l:x:", long_options, &i);
		
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
					   "        5 = elastic-tx w/ naive move,\n"
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
	
	printf("Set type     : hash table\n");
	printf("Duration     : %d\n", duration);
	printf("Initial size : %d\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Value range  : %d\n", range);
	printf("Seed         : %d\n", seed);
	printf("Update rate  : %d\n", update);
	printf("Load factor  : %d\n", load_factor);
	printf("Move rate    : %d\n", move);
	printf("Snapshot rate: %d\n", snapshot);
	printf("Elasticity   : %d\n", unit_tx);	
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
	
	maxhtlength = (unsigned int) initial / load_factor;
	set = ht_new();
	
	stop = 0;
	
	// Init STM 
	printf("Initializing STM\n");
	stm_init();
	mod_mem_init();
	
	if (stm_get_parameter("compile_flags", &s))
		printf("STM flags    : %s\n", s);
	
	// Populate set 
	printf("Adding %d entries to set\n", initial);
	i = 0;
	maxhtlength = (int) (initial / load_factor);
	while (i < initial) {
		val = rand_range(range);
		if (ht_add(set, val, 0))
			i++;
	}
	size = ht_size(set);
	printf("Set size     : %d\n", size);
	printf("Bucket amount: %d\n", maxhtlength);
	printf("Load         : %d\n", load_factor);
	
	// Access set from all threads 
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);
		data[i].range = range;
		data[i].update = update;
		data[i].load_factor = load_factor;
		data[i].move = move;
		data[i].snapshot = snapshot;
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
	aborts_double_write = 0;
	failures_because_contention = 0;
	reads = 0;
	updates = 0;
	moves = 0;
	snapshots = 0;
	max_retries = 0;
	for (i = 0; i < nb_threads; i++) {
		printf("Thread %d\n", i);
		printf("  #add        : %lu\n", data[i].nb_add);
		printf("  #remove     : %lu\n", data[i].nb_remove);
		printf("  #contains   : %lu\n", data[i].nb_contains);
		printf("  #found      : %lu\n", data[i].nb_found);
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
		moves += data[i].nb_move;
		snapshots += data[i].nb_snapshot;
		size += data[i].diff;
		if (max_retries < data[i].max_retries)
			max_retries = data[i].max_retries;
	}
	printf("Set size      : %d (expected: %d)\n", ht_size(set), size);
	printf("Duration      : %d (ms)\n", duration);
	printf("#txs          : %lu (%f / s)\n", reads + updates + moves + snapshots, (reads + updates + moves + snapshots) * 1000.0 / duration);
	printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
	printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);
	printf("#move txs     : %lu (%f / s)\n", moves, moves * 1000.0 / duration);
	printf("#snapshot txs : %lu (%f / s)\n", snapshots, snapshots * 1000.0 / duration);
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
	ht_delete(set);
	
	// Cleanup STM 
	stm_exit();
	
	free(threads);
	free(data);
	
	return 0;
}
