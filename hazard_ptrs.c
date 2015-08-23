
#include <stdio.h>
#include <string.h>
#include "hazard_ptrs.h"

/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////
#define CPU_RELAX() asm volatile("pause\n": : :"memory");
#define MEMBARSTLD() __sync_synchronize()

/////////////////////////////////////////////////////////
// EXTERNAL FUNCTIONS
/////////////////////////////////////////////////////////
void HP_reset(hp_thread_t *self) {	
	self->n_hp_records = 0;	
}

void HP_save(hp_thread_t *self) {
	self->saved_n_hp_records = self->n_hp_records;
}

void HP_restore(hp_thread_t *self) {
	self->n_hp_records = self->saved_n_hp_records;
}

hp_record_t *HP_alloc(hp_thread_t *self) {	
	hp_record_t *p_hp;
	
	p_hp = &(self->hp_records[self->n_hp_records]);
	self->n_hp_records++;
	
	if (self->n_hp_records >= HP_MAX_RECORDS) {
		abort();
	}
	
	return p_hp;
	
}

void HP_init(hp_record_t *p_hp, volatile int64_t **ptr_ptr) {
	
	while (1) { 
		p_hp->ptr = *ptr_ptr;
		MEMBARSTLD();	
	
		if (p_hp->ptr == *ptr_ptr) {
			return;
		}
		
		CPU_RELAX();
	}
	
}
