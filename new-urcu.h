#ifndef _NEW_URCU_H_
#define _NEW_URCU_H_

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

#define URCU_MAX_FREE_PTRS (1000)

#if !defined(EXTERNAL_RCU)

typedef struct rcu_node_t {
    volatile long time; 
	int f_size;
	void *free_ptrs[URCU_MAX_FREE_PTRS];
	char p[184];
} rcu_node;

void urcu_init(int num_threads);
void urcu_reader_lock();
void urcu_reader_unlock();
void urcu_writer_lock(int lock_id);
void urcu_writer_unlock(int lock_id);
void urcu_synchronize(); 
void urcu_register(int id);
void urcu_unregister();
void urcu_free(void *ptr);

#else

#include <urcu.h>

static inline void initURCU(int num_threads)
{
    rcu_init();
}

static inline void urcu_register(int id)
{
    rcu_register_thread();
}

static inline void urcu_unregister()
{
    rcu_unregister_thread();
}

static inline void urcu_read_lock()
{
    rcu_read_lock();
}

static inline void urcu_read_unlock()
{
    rcu_read_unlock();
}

static inline void urcu_synchronize()
{
    synchronize_rcu();
}

#endif  /* EXTERNAL RCU */ 

//////////

#define RCU_PRINT_STATS()
#define RCU_INIT(n_threads)      urcu_init(n_threads)
#define RCU_THREAD_INIT(th_id)   urcu_register(th_id)
#define RCU_THREAD_FINISH()      /* */
#define RCU_READER_LOCK()        urcu_reader_lock()
#define RCU_READER_UNLOCK()      urcu_reader_unlock()
#define RCU_WRITER_LOCK(lock_id)        urcu_writer_lock(lock_id)
#define RCU_WRITER_UNLOCK(lock_id)      urcu_writer_unlock(lock_id)
#define RCU_SYNCHRONIZE()        urcu_synchronize()
#define RCU_FREE(p_obj)          urcu_free(p_obj)

#define RCU_ASSIGN_PTR(p_ptr, p_obj) (*p_ptr) = p_obj									
#define RCU_DEREF(p_obj) (p_obj)


//////////

#endif /* _NEW_URCU_H_ */