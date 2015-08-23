
/////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "hash-list.h"

/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////

#define RO                              1
#define RW                              0

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define DEFAULT_BUCKETS                 1
#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define DEFAULT_SEED                    0
#define DEFAULT_RLU_MAX_WS              1
#define DEFAULT_UPDATE                  200

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////
typedef struct thread_data {
	long uniq_id;
	hash_list_t *p_hash_list;
	struct barrier *barrier;
	unsigned long nb_add;
	unsigned long nb_remove;
	unsigned long nb_contains;
	unsigned long nb_found;
	unsigned short seed[3];
	int initial;
	int diff;
	int range;
	int update;
	int alternate;	
	rlu_thread_data_t *p_rlu_td;
	rlu_thread_data_t rlu_td;
	hp_thread_t *p_hp_td;
	hp_thread_t hp_td;
	char padding[64];
} thread_data_t;

typedef struct barrier {
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

/////////////////////////////////////////////////////////
// GLOBALS
/////////////////////////////////////////////////////////
static volatile long padding[50];

static volatile int stop;
static unsigned short main_seed[3];

/////////////////////////////////////////////////////////
// HELPER FUNCTIONS
/////////////////////////////////////////////////////////
static inline int MarsagliaXORV (int x) { 
  if (x == 0) x = 1 ; 
  x ^= x << 6;
  x ^= ((unsigned)x) >> 21;
  x ^= x << 7 ; 
  return x ;        // use either x or x & 0x7FFFFFFF
}

static inline int MarsagliaXOR (int * seed) {
  int x = MarsagliaXORV(*seed);
  *seed = x ; 
  return x & 0x7FFFFFFF;
}

static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)rand();
  seed[1] = (unsigned short)rand();
  seed[2] = (unsigned short)rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  /* Return a random number in range [0;n) */
  
  /*int v = (int)(erand48(seed) * n);
  assert (v >= 0 && v < n);*/
  
  int v = MarsagliaXOR((int *)seed) % n;
  return v;
}

static void barrier_init(barrier_t *b, int n)
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

static void barrier_cross(barrier_t *b)
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

/////////////////////////////////////////////////////////
// FUNCTIONS
/////////////////////////////////////////////////////////
static void global_init(int n_threads, int rlu_max_ws) {
	RLU_INIT(RLU_TYPE_FINE_GRAINED, rlu_max_ws);
	RCU_INIT(n_threads);
}

static void thread_init(thread_data_t *d) {
	RLU_THREAD_INIT(d->p_rlu_td);
	RCU_THREAD_INIT(d->uniq_id);	
}

static void thread_finish(thread_data_t *d) {
	RLU_THREAD_FINISH(d->p_rlu_td);
	RCU_THREAD_FINISH();
}

static void print_stats() {
	RLU_PRINT_STATS();
	RCU_PRINT_STATS();
}

static void hash_list_init(hash_list_t **pp, int n_buckets) {
#ifdef IS_RLU
	*pp = rlu_new_hash_list(n_buckets);
#else
#ifdef IS_RCU
	*pp = rcu_new_hash_list(n_buckets);
#else
#ifdef IS_HARRIS
	*pp = harris_new_hash_list(n_buckets);
#else
#ifdef IS_HAZARD_PTRS_HARRIS
	*pp = hp_harris_new_hash_list(n_buckets);
#else
	printf("ERROR: benchmark not defined!\n");
	abort();
#endif
#endif
#endif
#endif
}

static int hash_list_contains(thread_data_t *d, int key) {
#ifdef IS_RLU
	return rlu_hash_list_contains(d->p_rlu_td, d->p_hash_list, key);
#else
#ifdef IS_RCU
	return rcu_hash_list_contains(d->p_hash_list, key);
#else
#ifdef IS_HARRIS
	return harris_hash_list_contains(d->p_hash_list, key);
#else
#ifdef IS_HAZARD_PTRS_HARRIS
	return hp_harris_hash_list_contains(d->p_hp_td, d->p_hash_list, key);
#else
	printf("ERROR: benchmark not defined!\n");
	abort();
#endif
#endif
#endif
#endif
}

static int hash_list_add(thread_data_t *d, int key) {
#ifdef IS_RLU
	return rlu_hash_list_add(d->p_rlu_td, d->p_hash_list, key);
#else
#ifdef IS_RCU
	return rcu_hash_list_add(d->p_hash_list, key);
#else
#ifdef IS_HARRIS
	return harris_hash_list_add(d->p_hash_list, key);
#else
#ifdef IS_HAZARD_PTRS_HARRIS
	return hp_harris_hash_list_add(d->p_hp_td, d->p_hash_list, key);
#else
	printf("ERROR: benchmark not defined!\n");
	abort();
#endif
#endif
#endif
#endif
}

static int hash_list_remove(thread_data_t *d, int key) {
#ifdef IS_RLU
	return rlu_hash_list_remove(d->p_rlu_td, d->p_hash_list, key);
#else
#ifdef IS_RCU
	return rcu_hash_list_remove(d->p_hash_list, key);
#else
#ifdef IS_HARRIS
	return harris_hash_list_remove(d->p_hash_list, key);
#else
#ifdef IS_HAZARD_PTRS_HARRIS
	return hp_harris_hash_list_remove(d->p_hp_td, d->p_hash_list, key);
#else
	printf("ERROR: benchmark not defined!\n");
	abort();
#endif
#endif
#endif
#endif
}

static void *test(void *data)
{
	int op, key, last = -1;
	thread_data_t *d = (thread_data_t *)data;
	
	thread_init(d);
	
	if (d->uniq_id == 0) {
		/* Populate set */
		printf("[%ld] Initializing\n", d->uniq_id);
		printf("[%ld] Adding %d entries to set\n", d->uniq_id, d->initial);
		d->p_rlu_td->is_no_quiescence = 1;
		int i = 0;
		while (i < d->initial) {
			key = rand_range(d->range, d->seed) + 1;
			
			if (hash_list_add(d, key)) {
				i++;
			}
		}
		printf("[%ld] Adding done\n", d->uniq_id);
		int size = hash_list_size(d->p_hash_list);
		printf("Hash-list size     : %d\n", size);	
		
		d->p_rlu_td->is_no_quiescence = 0;
	}
	
	/* Wait on barrier */
	barrier_cross(d->barrier);

	while (stop == 0) {
		op = rand_range(1000, d->seed);
		if (op < d->update) {
			if (d->alternate) {
				/* Alternate insertions and removals */
				if (last < 0) {
					/* Add random value */
					key = rand_range(d->range, d->seed) + 1;
					if (hash_list_add(d, key)) {
						d->diff++;
						last = key;
					}
					d->nb_add++;
				} else {
					/* Remove last value */
					if (hash_list_remove(d, last)) {
						d->diff--;
					}
					d->nb_remove++;
					last = -1;
				}
			} else {
				/* Randomly perform insertions and removals */
				key = rand_range(d->range, d->seed) + 1;
				if ((op & 0x01) == 0) {
					/* Add random value */
					if (hash_list_add(d, key)) {
						d->diff++;
					}
					d->nb_add++;
				} else {
					/* Remove random value */
					if (hash_list_remove(d, key)) {
						d->diff--;
					}
					d->nb_remove++;
				}
			}
		} else {
			/* Look for random value */
			key = rand_range(d->range, d->seed) + 1;
			if (hash_list_contains(d, key)) {
				d->nb_found++;
			}
			d->nb_contains++;
		}
	}
	
	thread_finish(d);
	
	return NULL;
}

int main(int argc, char **argv)
{
	struct option long_options[] = {
	// These options don't set a flag
			{"help",                      no_argument,       NULL, 'h'},
			{"do-not-alternate",          no_argument,       NULL, 'a'},
			{"buckets",                   required_argument, NULL, 'b'},
			{"duration",                  required_argument, NULL, 'd'},
			{"initial-size",              required_argument, NULL, 'i'},
			{"num-threads",               required_argument, NULL, 'n'},
			{"range",                     required_argument, NULL, 'r'},
			{"seed",                      required_argument, NULL, 's'},
			{"rlu-max-ws",                required_argument, NULL, 'w'},
			{"update-rate",               required_argument, NULL, 'u'},
			{NULL, 0, NULL, 0}
	};

	hash_list_t *p_hash_list;
	int i, c, size;
	unsigned long reads, updates;
	thread_data_t *data;
	pthread_t *threads;
	pthread_attr_t attr;
	barrier_t barrier;
	struct timeval start, end;
	struct timespec timeout;
	int n_buckets = DEFAULT_BUCKETS;
	int duration = DEFAULT_DURATION;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	int range = DEFAULT_RANGE;
	int seed = DEFAULT_SEED;
	int rlu_max_ws = DEFAULT_RLU_MAX_WS;
	int update = DEFAULT_UPDATE;
	int alternate = 1;
	sigset_t block_set;

	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hab:d:i:n:r:s:w:u:", long_options, &i);

		if(c == -1)
			break;

		if(c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;

		switch(c) {
			case 0:
	/* Flag is automatically set */
			break;
			case 'h':
			printf("intset "
				"(linked list)\n"
				"\n"
				"Usage:\n"
				"  intset [options...]\n"
				"\n"
				"Options:\n"
				"  -h, --help\n"
				"        Print this message\n"
				"  -a, --do-not-alternate\n"
				"        Do not alternate insertions and removals\n"
				"  -b, --buckets <int>\n"
				"        Number of buckets (default=" XSTR(DEFAULT_BUCKETS) ")\n"
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
				"        Percentage of update transactions (1000 = 100 percent) (default=" XSTR(DEFAULT_UPDATE) ")\n"				
				);
			exit(0);
			case 'a':
			alternate = 0;
			break;
			case 'b':
			n_buckets = atoi(optarg);
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
			range = atoi(optarg);
			break;
			case 's':
			seed = atoi(optarg);
			break;
			case 'w':
			rlu_max_ws = atoi(optarg);
			break;
			case 'u':
			update = atoi(optarg);
			break;
			case '?':
			printf("Use -h or --help for help\n");
			exit(0);
			default:
			exit(1);
		}
	}

	assert(n_buckets >= 1);
	assert(duration >= 0);
	assert(initial >= 0);
	assert(nb_threads > 0);
	assert(range > 0 && range >= initial);
	assert(rlu_max_ws >= 1 && rlu_max_ws <= 100 && update >= 0 && update <= 1000);

	printf("Set type     : hash-list\n");
	printf("Buckets      : %d\n", n_buckets);
	printf("Duration     : %d\n", duration);
	printf("Initial size : %d\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Value range  : %d\n", range);
	printf("Seed         : %d\n", seed);
	printf("rlu-max-ws   : %d\n", rlu_max_ws);	
	printf("Update rate  : %d\n", update);
	printf("Alternate    : %d\n", alternate);
	printf("Node size    : %lu\n", sizeof(node_t));
	printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
		(int)sizeof(int),
		(int)sizeof(long),
		(int)sizeof(void *),
		(int)sizeof(size_t));

	timeout.tv_sec = duration / 1000;
	timeout.tv_nsec = (duration % 1000) * 1000000;

	if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
		perror("malloc");
		exit(1);
	}

	memset(data, 0, nb_threads * sizeof(thread_data_t));

	if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
		perror("malloc");
		exit(1);
	}

	if (seed == 0)
		srand((int)time(NULL));
	else
		srand(seed);
	
	global_init(nb_threads, rlu_max_ws);
	
	hash_list_init(&p_hash_list, n_buckets);

	size = initial;
	
	stop = 0;

	/* Thread-local seed for main thread */
	rand_init(main_seed);

	if (alternate == 0 && range != initial * 2) {
		printf("ERROR: range is not twice the initial set size\n");
		exit(1);
	}

	/* Access set from all threads */
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);

		data[i].uniq_id = i;
		data[i].range = range;
		data[i].update = update;
		data[i].alternate = alternate;
		data[i].nb_add = 0;
		data[i].nb_remove = 0;
		data[i].nb_contains = 0;
		data[i].nb_found = 0;
		data[i].initial = initial;
		data[i].diff = 0;
		rand_init(data[i].seed);
		data[i].p_hash_list = p_hash_list;
		data[i].barrier = &barrier;
		data[i].p_rlu_td = &(data[i].rlu_td);
		data[i].p_hp_td = &(data[i].hp_td);
		if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
			fprintf(stderr, "Error creating thread\n");
			exit(1);
		}
	}
	pthread_attr_destroy(&attr);

	/* Start threads */
	barrier_cross(&barrier);

	printf("STARTING THREADS...\n");
	gettimeofday(&start, NULL);
	if (duration > 0) {
		nanosleep(&timeout, NULL);
	} else {
		sigemptyset(&block_set);
		sigsuspend(&block_set);
	}
	stop = 1;
	gettimeofday(&end, NULL);
	printf("STOPPING THREADS...\n");

	/* Wait for thread completion */
	for (i = 0; i < nb_threads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for thread completion\n");
			exit(1);
		}
	}

	duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
	reads = 0;
	updates = 0;
	for (i = 0; i < nb_threads; i++) {
		printf("Thread %d\n", i);
		printf("  #add        : %lu\n", data[i].nb_add);
		printf("  #remove     : %lu\n", data[i].nb_remove);
		printf("  #contains   : %lu\n", data[i].nb_contains);
		printf("  #found      : %lu\n", data[i].nb_found);
		reads += data[i].nb_contains;
		updates += (data[i].nb_add + data[i].nb_remove);
		size += data[i].diff;
	}
	printf("Set size      : %d (expected: %d)\n", hash_list_size(p_hash_list), size);
	printf("Duration      : %d (ms)\n", duration);
	printf("#ops          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
	printf("#read ops     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
	printf("#update ops   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);

	print_stats();

	/* Cleanup */
	free(threads);
	free(data);

	return 0;
}
