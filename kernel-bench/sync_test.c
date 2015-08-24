#include <linux/module.h>
#include <linux/moduleparam.h>  /* module_param */
#include <linux/kernel.h>       /* pr_info/printk */
#include <linux/errno.h>        /* errors code */
#include <linux/sched.h>        /* current global variable */
//#include <linux/rcupdate.h>     /* rcu */
#include <linux/init.h>         /* __init __exit macros */
#include <linux/kthread.h>      /* kthreads */
#include <linux/completion.h>   /* complete/wait_for_completion */
#include <linux/delay.h>        /* msleep */
#include <linux/slab.h>         /* kmalloc/kzalloc */
#include <linux/random.h>       /* rnd_state/ */
#include <asm/timex.h>          /* get_cycles (same as rdtscll) */

#include "sync_test.h"
#include "barrier.h"
#include "rlu.h"
#include "hash-list.h"
#include "intset.h"

#define MODULE_NAME    "sync_test"
#define MAX_BENCHMARKS (16)
#ifndef RLU_DEFER_WS
# define RLU_DEFER_WS  (1)
#endif
#define FORCE_SCHED    (1) /* Enable this define to allow long execution */

/* Bench configuration */
static char *benchmark = "rcuhashlist";
module_param(benchmark, charp, 0000);
MODULE_PARM_DESC(benchmark, "Benchmark name");
static int threads_nb = 1;
module_param(threads_nb, int, 0000);
MODULE_PARM_DESC(threads_nb, "Number of threads");
static int duration = 100;
module_param(duration, int, 0000);
MODULE_PARM_DESC(duration, "Duration of the benchmark in ms");
static int update = 0;
module_param(update, int, 0000);
MODULE_PARM_DESC(update, "Probability for update operations. No floating-point in kernel so 10000 = 100%, 1 = 0.01%");
static int range = 1024;
module_param(range, int, 0000);
MODULE_PARM_DESC(range, "Key range. Initial set size is half the key range.");

typedef struct benchmark {
    char name[32];
    int (*init)(void);
    int (*lookup)(void *tl, int key);
    int (*insert)(void *tl, int key);
    int (*delete)(void *tl, int key);
    unsigned long nb_lookup;
    unsigned long nb_insert;
    unsigned long nb_delete;
} benchmark_t;

static benchmark_t benchmarks[MAX_BENCHMARKS] = {
    {
        .name = "rcuhashlist",
        .init = &rcu_hash_list_init,
        .lookup = &rcu_hash_list_contains,
        .insert = &rcu_hash_list_add,
        .delete = &rcu_hash_list_remove,
    },
    {
        .name = "rluhashlist",
        .init = &rlu_hash_list_init,
        .lookup = &rlu_hash_list_contains,
        .insert = &rlu_hash_list_add,
        .delete = &rlu_hash_list_remove,
    },
    /*
    {
        .name = "rluintset",
        .init = &rluintset_init,
        .lookup = &rluintset_lookup,
        .insert = &rluintset_insert,
        .delete = &rluintset_delete,
    },*/
    {
        .name = "rcuintset",
        .init = &rcuintset_init,
        .lookup = &rcuintset_lookup,
        .insert = &rcuintset_insert,
        .delete = &rcuintset_delete,
    }
};

typedef struct benchmark_thread {
    benchmark_t *benchmark;
    unsigned int id;
    rlu_thread_data_t *rlu;
    struct rnd_state rnd;
    struct {
        unsigned long nb_lookup;
        unsigned long nb_insert;
        unsigned long nb_delete;
    } ops;
    /* TODO to complete */

} benchmark_thread_t;

benchmark_thread_t* benchmark_threads[RLU_MAX_THREADS];

struct completion sync_test_working;

__cacheline_aligned static barrier_t sync_test_barrier;

/* Generate a pseudo random in range [0:n[ */
static inline int rand_range(int n, struct rnd_state *seed)
{
    return prandom_u32_state(seed) % n;
}

static int sync_test_thread(void* data)
{
    benchmark_thread_t *bench = (benchmark_thread_t *)data;
    unsigned long duration_ms;
    struct timespec start, end;
    unsigned long long tsc_start, tsc_end;
    rlu_thread_data_t *self = bench->rlu;

    /* Wait on barrier */
    barrier_cross(&sync_test_barrier);

    tsc_start = get_cycles();
    start = current_kernel_time();

    /* Thread main */
    do {
        int op = rand_range(10000, &bench->rnd);
        int val = rand_range(range, &bench->rnd);
        if (op < update) {
            /* Update */
            op = rand_range(2, &bench->rnd);
            if ((op & 1) == 0) {
                bench->benchmark->insert(self, val);
                bench->ops.nb_insert++;
            } else {
                bench->benchmark->delete(self, val);
                bench->ops.nb_delete++;
            }
        } else {
            /* Lookup */
            bench->benchmark->lookup(self, val);
            bench->ops.nb_lookup++;
        }
        end = current_kernel_time();
        duration_ms = (end.tv_sec * 1000 + end.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);
#ifdef FORCE_SCHED
        /* No need to force schedule(), time bound. */
        cond_resched();
#endif /* FORCE_SCHED */
    } while (duration_ms < duration);

    tsc_end = get_cycles();
    pr_info(MODULE_NAME "(%i:%i) time: %lu ms (%llu cycles)\n", current->pid, bench->id, duration_ms, tsc_end - tsc_start);

    /* TODO probably better to do the ops/sec here. */

    /* Thread finishing */
    complete(&sync_test_working);
    rlu_thread_finish(bench->rlu);

    return 0;
}

static int __init sync_test_init(void)
{
    benchmark_t *bench = NULL;
    int i;

    /* Benchmark to run */
    for (i = 0; i < MAX_BENCHMARKS; i++) {
        if (benchmarks[i].name && !strcmp(benchmarks[i].name, benchmark)) {
            bench = &benchmarks[i];
            break;
        }
    }
    if (!bench) {
        pr_err(MODULE_NAME ": Unknown benchmark %s\n", benchmark);
        return -EPERM;
    }
    if (bench->lookup == NULL || bench->insert == NULL || bench->delete == NULL) {
        pr_err(MODULE_NAME ": Benchmark %s has a NULL function defined\n", benchmark);
        return -EPERM;
    }
    /* TODO display all user parameters */
    pr_notice(MODULE_NAME ": Running benchmark %s with %i threads\n", benchmark, threads_nb);

    if (threads_nb > num_online_cpus()) {
        pr_err(MODULE_NAME ": Invalid number of threads %d (MAX %d)\n", threads_nb, num_online_cpus());
        return -EPERM;
    }

    /* Initialization */
    init_completion(&sync_test_working);
    barrier_init(&sync_test_barrier, threads_nb);
    rlu_init(RLU_TYPE_FINE_GRAINED, RLU_DEFER_WS);
    bench->init();
    for (i = 0; i < threads_nb; i++) {
        benchmark_threads[i] = kzalloc(sizeof(*benchmark_threads[i]), GFP_KERNEL);
        benchmark_threads[i]->id = i;
        prandom_seed_state(&benchmark_threads[i]->rnd, i + 1);
        benchmark_threads[i]->benchmark = bench;
        /* Other fields are set to 0 with kzalloc */
        /* RLU Thread initialization */
        benchmark_threads[i]->rlu = kmalloc(sizeof(rlu_thread_data_t), GFP_KERNEL);
        rlu_thread_init(benchmark_threads[i]->rlu);
    }

    /* Half fill the set */
    for (i = 0; i < range / 2; i++) {
        /* Ensure the success of insertion */
        while (bench->insert(benchmark_threads[0]->rlu, get_random_int() % range));
    }

    /* Start N-1 threads */
    for (i = 1; i < threads_nb; i++) {
        struct task_struct *t;
        /* kthread_run(...) can be also used to avoid wake_up_process */
        t = kthread_create(sync_test_thread, benchmark_threads[i], "sync_test_thread");
        if (t) {
            pr_notice(MODULE_NAME ": pid: %d (created from %d)\n", t->pid, current->pid);
            wake_up_process(t);
            /* kthread_bind(threads[cpu], cpu); */
        }
    }

    /* Main thread is also doing work. */
    sync_test_thread(benchmark_threads[0]);

    /* Wait for the threads to finish */
    for (i = 0; i < threads_nb; i++) {
        pr_debug(MODULE_NAME ": Waiting still %d threads to finish\n", i);
        wait_for_completion(&sync_test_working);
    }

    /* Reinitialize one thread to cleanup */
    rlu_thread_init(benchmark_threads[0]->rlu);

    /* Statistics output */
    for (i = 0; i < threads_nb; i++) {
        bench->nb_lookup += benchmark_threads[i]->ops.nb_lookup;
        bench->nb_insert += benchmark_threads[i]->ops.nb_insert;
        bench->nb_delete += benchmark_threads[i]->ops.nb_delete;
    }
    pr_info(MODULE_NAME ": #lookup: %lu / s\n", bench->nb_lookup * 1000 / duration);
    pr_info(MODULE_NAME ": #insert: %lu / s\n", bench->nb_insert * 1000 / duration);
    pr_info(MODULE_NAME ": #delete: %lu / s\n", bench->nb_delete * 1000 / duration);

    /* Empty the set using all possible keys */
    for (i = 0; i < range; i++) {
        bench->delete(benchmark_threads[0]->rlu, i);
    }

    /* Cleaning and free */
    rlu_thread_finish(benchmark_threads[0]->rlu);
    for (i = 0; i < threads_nb; i++) {
        kfree(benchmark_threads[i]->rlu);
    }

    rlu_finish();

    /* When the benchmark is done, the module is loaded. Maybe we can fail anyway to avoid empty unload. */
    pr_info(MODULE_NAME ": Done\n");
    return 0;
}

static void __exit sync_test_exit(void)
{
    pr_info(MODULE_NAME ": Unloaded\n");
}

module_init(sync_test_init);
module_exit(sync_test_exit);

MODULE_LICENSE("GPL");
