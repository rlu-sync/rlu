
#ifndef HAZARD_PTRS_H
#define HAZARD_PTRS 1

/////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////
#ifdef __x86_64
#include <immintrin.h>
#endif

/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////
#define IS_HP_ENABLED

#define HP_MAX_RECORDS (100)

/////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////
typedef struct _hp_record_t {
	volatile int64_t *ptr;
} hp_record_t;

typedef struct _hp_thread_t { 
	long saved_n_hp_records;
	long n_hp_records;
	hp_record_t hp_records[HP_MAX_RECORDS];

} hp_thread_t ; 


/////////////////////////////////////////////////////////
// EXTERNAL FUNCTIONS
/////////////////////////////////////////////////////////
void HP_reset(hp_thread_t *self);
void HP_save(hp_thread_t *p_hp);
void HP_restore(hp_thread_t *p_hp);

hp_record_t *HP_alloc(hp_thread_t *self);
void HP_init(hp_record_t *p_hp, volatile int64_t **ptr_ptr);

#ifdef IS_HP_ENABLED

#define HP_RESET(self) HP_reset(self)
#define HP_SAVE(self) HP_save(self)
#define HP_RESTORE(self) HP_restore(self)

#define HP_ALLOC(self) HP_alloc(self)
#define HP_INIT(self, p_hp, ptr_ptr) HP_init(p_hp, (volatile int64_t **)ptr_ptr)

#else

#define HP_RESET(self)
#define HP_SAVE(self)
#define HP_RESTORE(self)

#define HP_ALLOC(self)
#define HP_INIT(self, p_hp, ptr_ptr)

#endif

#endif // HAZARD_PTRS
