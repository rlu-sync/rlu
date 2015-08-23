#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "new-urcu.h"

/**
 * Copyright 2014 Maya Arbel (mayaarl [at] cs [dot] technion [dot] ac [dot] il).
 * 
 * This file is part of Citrus. 
 * 
 * Citrus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Authors Maya Arbel and Adam Morrison 
 */

int threads; 
rcu_node** urcu_table;

#define MAX_SPIN_LOCKS (10000)
/*typedef struct _my_lock {
	pthread_spinlock_t lock;
	//long padding[4];	
} my_lock;*/

pthread_spinlock_t urcu_spin[MAX_SPIN_LOCKS];

void urcu_init(int num_threads){
   rcu_node** result = (rcu_node**) malloc(sizeof(rcu_node)*num_threads);
   int i;
   rcu_node* new;
   threads = num_threads; 
   for( i=0; i<threads ; i++){
        new = (rcu_node*) malloc(sizeof(rcu_node));
        new->time = 1; 
		new->f_size = 0;
        *(result + i) = new;
    }

    urcu_table =  result;

	for (i = 0; i < MAX_SPIN_LOCKS; i++) {
		pthread_spin_init(&(urcu_spin[i]), PTHREAD_PROCESS_PRIVATE);
	}	

    printf("initializing URCU finished, node_size: %zd\n", sizeof(rcu_node));
    return; 
}

__thread long* times = NULL; 
__thread int i; 

void urcu_register(int id){
    times = (long*) malloc(sizeof(long)*threads);
    i = id; 
    if (times == NULL ){
        printf("malloc failed\n");
        exit(1);
    }
}
void urcu_unregister(){
    free(times);
}

void urcu_reader_lock(){
    assert(urcu_table[i] != NULL);
    __sync_add_and_fetch(&urcu_table[i]->time, 1);
}

#if defined(__x86_64)
static inline void set_bit(int nr, volatile unsigned long *addr){
    asm("btsl %1,%0" : "+m" (*addr) : "Ir" (nr));
}
#elif defined(__PPC64__)
static __inline__ __attribute__((always_inline)) __attribute__((no_instrument_function))
void set_bits(unsigned long mask, volatile unsigned long *_p)
{
  unsigned long old;
  unsigned long *p = (unsigned long *)_p;
   __asm__ __volatile__ ( "" "1:" ".long 0x7c0000a8 | (((%0) & 0x1f) << 21) | (((0) & 0x1f) << 16) | (((%3) & 0x1f) << 11) | (((0) & 0x1) << 0)" " " "\n" "or" " " "%0,%0,%2\n" "stdcx." " " "%0,0,%3\n" "bne- 1b\n" : "=&r" (old), "+m" (*p) : "r" (mask), "r" (p) : "cc", "memory");
}

static __inline__ __attribute__((always_inline)) __attribute__((no_instrument_function))
void set_bit(int nr, volatile unsigned long *addr)
{
 set_bits((1UL << ((nr) % 64)), addr + ((nr) / 64));
}
#endif

void urcu_reader_unlock(){
    assert(urcu_table[i]!= NULL);
    set_bit(0, (unsigned long *)&urcu_table[i]->time);
}

void urcu_writer_lock(int lock_id){
	pthread_spin_lock(&(urcu_spin[lock_id]));
}

void urcu_writer_unlock(int lock_id){
	pthread_spin_unlock(&(urcu_spin[lock_id]));
}

void urcu_synchronize(){
    int i; 
    //read old counters
    for( i=0; i<threads ; i++){
        times[i] = urcu_table[i]->time;
    }
    for( i=0; i<threads ; i++){
        if (times[i] & 1) continue;
        while(1){
            unsigned long t = urcu_table[i]->time;
            if (t & 1 || t > times[i]){
                break; 
            }
        }
    }
}

void urcu_free(void *ptr) {
	int k;
	
	urcu_table[i]->free_ptrs[urcu_table[i]->f_size] = ptr;
	urcu_table[i]->f_size++;
	
	if (urcu_table[i]->f_size == URCU_MAX_FREE_PTRS) {
		
		urcu_synchronize();
		
		for (k = 0; k < urcu_table[i]->f_size; k++) {
			free(urcu_table[i]->free_ptrs[k]);
		}
		
		urcu_table[i]->f_size = 0;
	}
}
