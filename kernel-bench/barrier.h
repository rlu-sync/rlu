#ifndef _BARRIER_H
#define _BARRIER_H

//#include <linux/cache.h>     // __cacheline_aligned
#include <linux/atomic.h>

typedef struct barrier {
    atomic_t count;
    atomic_t crossing;
} barrier_t;

void barrier_init(barrier_t *barrier, int n);
void barrier_cross(barrier_t *barrier);

#endif /* _BARRIER_H */
