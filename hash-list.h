#ifndef _HASH_LIST_H_
#define _HASH_LIST_H_

/////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////
#include "hazard_ptrs.h"
#include "new-urcu.h"
#include "rlu.h"

/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////
#define LIST_VAL_MIN (INT_MIN)
#define LIST_VAL_MAX (INT_MAX)

#define NODE_PADDING (16)

#define MAX_BUCKETS (20000)

/////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////
typedef intptr_t val_t;

typedef struct node node_t; 
typedef struct node {
	val_t val;
	node_t *p_next;

	long padding[NODE_PADDING];
} node_t;

typedef struct list {
	node_t *p_head;
} list_t;

typedef struct hash_list {
	int n_buckets;
	list_t *buckets[MAX_BUCKETS];  
} hash_list_t;

/////////////////////////////////////////////////////////
// INTERFACE
/////////////////////////////////////////////////////////
hash_list_t *pure_new_hash_list(int n_buckets);
hash_list_t *harris_new_hash_list(int n_buckets);
hash_list_t *hp_harris_new_hash_list(int n_buckets);
hash_list_t *rcu_new_hash_list(int n_buckets);
hash_list_t *rlu_new_hash_list(int n_buckets);

int hash_list_size(hash_list_t *p_hash_list);

int pure_hash_list_contains(hash_list_t *p_hash_list, val_t val);
int pure_hash_list_add(hash_list_t *p_hash_list, val_t val);
int pure_hash_list_remove(hash_list_t *p_hash_list, val_t val);

int harris_hash_list_contains(hash_list_t *p_hash_list, val_t val);
int harris_hash_list_add(hash_list_t *p_hash_list, val_t val);
int harris_hash_list_remove(hash_list_t *p_hash_list, val_t val);

int hp_harris_hash_list_contains(hp_thread_t *p_hp_td, hash_list_t *p_hash_list, val_t val);
int hp_harris_hash_list_add(hp_thread_t *p_hp_td, hash_list_t *p_hash_list, val_t val);
int hp_harris_hash_list_remove(hp_thread_t *p_hp_td, hash_list_t *p_hash_list, val_t val);

int rcu_hash_list_contains(hash_list_t *p_hash_list, val_t val);
int rcu_hash_list_add(hash_list_t *p_hash_list, val_t val);
int rcu_hash_list_remove(hash_list_t *p_hash_list, val_t val);

int rlu_hash_list_contains(rlu_thread_data_t *self, hash_list_t *p_hash_list, val_t val);
int rlu_hash_list_add(rlu_thread_data_t *self, hash_list_t *p_hash_list, val_t val);
int rlu_hash_list_remove(rlu_thread_data_t *self, hash_list_t *p_hash_list, val_t val);

#endif // _HASH_LIST_H_