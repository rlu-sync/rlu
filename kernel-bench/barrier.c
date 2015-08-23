#include <linux/atomic.h>
#include <linux/sched.h> // schedule
#include <asm/processor.h> // cpu_relax

#include "barrier.h"

/* TODO make barrier that reinitialize itself */

void barrier_init(barrier_t *barrier, int n)
{
    atomic_set(&barrier->count, n);
    atomic_set(&barrier->crossing, 0);
}

void barrier_cross(barrier_t *barrier)
{
    int crossing;
    int count = atomic_read(&barrier->count);
    crossing = atomic_add_return(1, &barrier->crossing);
    while (crossing != count) {
        cpu_relax();
        crossing = atomic_read(&barrier->crossing);
        /* Force all threads to be scheduled (note this may delay a bit the start but no choice) */
        schedule();
    }
}

