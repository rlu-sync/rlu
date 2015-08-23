
/////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////
#include <stdlib.h>
#include <stdio.h>

#include "new-urcu.h"
#include "rlu.h"
#include "hash-list.h" 

/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)

#define MEMBARSTLD() __sync_synchronize()
#define CAS(addr, expected_value, new_value) __sync_val_compare_and_swap((addr), (expected_value), (new_value))
#define HARRIS_CAS(p_addr, expected_value, new_value) (CAS((intptr_t *)p_addr, (intptr_t)expected_value, (intptr_t)new_value) == (intptr_t)expected_value) 

/////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// GLOBALS
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// FUNCTIONS
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// HARRIS LIST HELPER FUNCTIONS
/////////////////////////////////////////////////////////
inline int is_marked_ref(long i) {
	return (int) (i & (LONG_MIN+1));
}

inline long unset_mark(long i) {
	i &= LONG_MAX-1;
	return i;
}

inline long set_mark(long i) {
	i = unset_mark(i);
	i += 1;
	return i;
}

inline long get_unmarked_ref(long w) {
	return unset_mark(w);
}

inline long get_marked_ref(long w) {
	return set_mark(w);
}

/////////////////////////////////////////////////////////
// NEW NODE
/////////////////////////////////////////////////////////
node_t *pure_new_node() {
	
	node_t *p_new_node = (node_t *)malloc(sizeof(node_t));
	if (p_new_node == NULL){
		printf("out of memory\n");
		exit(1); 
	}    
	
    return p_new_node;
}

node_t *rlu_new_node() {
    
	node_t *p_new_node = (node_t *)RLU_ALLOC(sizeof(node_t));
	if (p_new_node == NULL){
		printf("out of memory\n");
		exit(1); 
	}    
	
    return p_new_node;
}

node_t *harris_new_node() {    
	return pure_new_node();
}

node_t *hp_harris_new_node() {    
	return pure_new_node();
}

node_t *rcu_new_node() {    
	return pure_new_node();
}

/////////////////////////////////////////////////////////
// FREE NODE
/////////////////////////////////////////////////////////
void pure_free_node(node_t *p_node) {
	if (p_node != NULL) {
		free(p_node);
	}
}

void harris_free_node(node_t *p_node) {
	/* leaks memory */
}

void rcu_free_node(node_t *p_node) {
	RCU_FREE(p_node);
}

/////////////////////////////////////////////////////////
// NEW LIST
/////////////////////////////////////////////////////////
list_t *pure_new_list()
{
	list_t *p_list;
	node_t *p_min_node, *p_max_node;

	p_list = (list_t *)malloc(sizeof(list_t));
	if (p_list == NULL) {
		perror("malloc");
		exit(1);
	}
	
	p_max_node = pure_new_node();
	p_max_node->val = LIST_VAL_MAX;
	p_max_node->p_next = NULL;
	
	p_min_node = pure_new_node();
	p_min_node->val = LIST_VAL_MIN;
	p_min_node->p_next = p_max_node;
	
	p_list->p_head = p_min_node;

	return p_list;
}

list_t *rlu_new_list()
{
	list_t *p_list;
	node_t *p_min_node, *p_max_node;

	p_list = (list_t *)malloc(sizeof(list_t));
	if (p_list == NULL) {
		perror("malloc");
		exit(1);
	}
	
	p_max_node = rlu_new_node();
	p_max_node->val = LIST_VAL_MAX;
	p_max_node->p_next = NULL;
	
	p_min_node = rlu_new_node();
	p_min_node->val = LIST_VAL_MIN;
	p_min_node->p_next = p_max_node;
	
	p_list->p_head = p_min_node;

	return p_list;
}

/////////////////////////////////////////////////////////
// NEW HASH LIST
/////////////////////////////////////////////////////////
hash_list_t *pure_new_hash_list(int n_buckets)
{
	int i;	
	hash_list_t *p_hash_list;
  	
	p_hash_list = (hash_list_t *)malloc(sizeof(hash_list_t));
	
	if (p_hash_list == NULL) {
	    perror("malloc");
	    exit(1);
	}
	
	p_hash_list->n_buckets = n_buckets; 
	
	for (i = 0; i < p_hash_list->n_buckets; i++) {
		p_hash_list->buckets[i] = pure_new_list();
	}
	
	return p_hash_list;
}

hash_list_t *harris_new_hash_list(int n_buckets)
{
	return pure_new_hash_list(n_buckets);
}

hash_list_t *hp_harris_new_hash_list(int n_buckets)
{
	return pure_new_hash_list(n_buckets);
}

hash_list_t *rcu_new_hash_list(int n_buckets)
{
	return pure_new_hash_list(n_buckets);
}

hash_list_t *rlu_new_hash_list(int n_buckets)
{
	int i;	
	hash_list_t *p_hash_list;
  	
	p_hash_list = (hash_list_t *)malloc(sizeof(hash_list_t));
	
	if (p_hash_list == NULL) {
	    perror("malloc");
	    exit(1);
	}
	
	p_hash_list->n_buckets = n_buckets; 
	
	for (i = 0; i < p_hash_list->n_buckets; i++) {
		p_hash_list->buckets[i] = rlu_new_list();
	}
	
	return p_hash_list;
}

/////////////////////////////////////////////////////////
// LIST SIZE
/////////////////////////////////////////////////////////
int list_size(list_t *p_list)
{
	int size = 0;
	node_t *p_node;

	/* We have at least 2 elements */
	p_node = p_list->p_head->p_next;
	while (p_node->p_next != NULL) {
		size++;
		p_node = p_node->p_next;
	}

	return size;
}

/////////////////////////////////////////////////////////
// HASH LIST SIZE
/////////////////////////////////////////////////////////
int hash_list_size(hash_list_t *p_hash_list)
{
	int i;
	int size = 0;

	for (i = 0; i < p_hash_list->n_buckets; i++) {
		size += list_size(p_hash_list->buckets[i]);
	}
	
	return size;
}

/////////////////////////////////////////////////////////
// LIST CONTAINS
/////////////////////////////////////////////////////////
int pure_list_contains(list_t *p_list, val_t val) {
	node_t *p_prev, *p_next;

	p_prev = p_list->p_head;
	p_next = p_prev->p_next;
	while (p_next->val < val) {
		p_prev = p_next;
		p_next = p_prev->p_next;
	}

	return p_next->val == val;
}

node_t *harris_list_search(list_t *p_list, val_t val, node_t **left_node) {
	node_t *left_node_next;
	node_t *right_node;

search_again:
	
	do {
		left_node_next = p_list->p_head;
		
		node_t *t = p_list->p_head;
		node_t *t_next = p_list->p_head->p_next;

		/* Find left_node and right_node */
		do {
			if (!is_marked_ref((long) t_next)) {
				(*left_node) = t;
				left_node_next = t_next;
			}
			
			t = (node_t *) get_unmarked_ref((long) t_next);
			if (!t->p_next) break;
			t_next = t->p_next;
			
		} while (is_marked_ref((long) t_next) || (t->val < val));
		
		right_node = t;

		/* Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if (right_node->p_next && is_marked_ref((long) right_node->p_next)) {
				goto search_again;
			} else {
				return right_node;
			}
		}

		/* Remove one or more marked nodes */
		if (HARRIS_CAS(&(*left_node)->p_next, left_node_next, right_node)) {
			if (right_node->p_next && is_marked_ref((long) right_node->p_next)) {
				goto search_again;
			} else { 
				return right_node;
			}
		}

	} while (1);

}

node_t *hp_harris_list_search(hp_thread_t *p_hp_td, list_t *p_list, val_t val, node_t **left_node) {
	node_t *left_node_next;
	node_t *right_node;
	node_t *t;
	node_t *my_left_node;
	
	hp_record_t *p_hp_temp;
	hp_record_t *p_hp_left_node_next;
	hp_record_t *p_hp_right_node;
	hp_record_t *p_hp_t;
	hp_record_t *p_hp_t_next;
	hp_record_t *p_hp_my_left_node;
	
	my_left_node = *left_node;
	
	HP_save(p_hp_td);

search_again:
	HP_restore(p_hp_td);
	
	p_hp_temp = HP_alloc(p_hp_td);
	p_hp_left_node_next = HP_alloc(p_hp_td);
	p_hp_right_node = HP_alloc(p_hp_td);
	p_hp_t = HP_alloc(p_hp_td);
	p_hp_t_next = HP_alloc(p_hp_td);
	p_hp_my_left_node = HP_alloc(p_hp_td);
	
	do {
		HP_INIT(p_hp_td, p_hp_left_node_next, &(p_list->p_head));
		left_node_next = p_list->p_head;
		
		HP_INIT(p_hp_td, p_hp_t, &(p_list->p_head));
		t = p_list->p_head;
		HP_INIT(p_hp_td, p_hp_t_next, &(p_list->p_head->p_next));
		node_t *t_next = p_list->p_head->p_next;

		/* Find left_node and right_node */
		do {
			if (!is_marked_ref((long) t_next)) {
				HP_INIT(p_hp_td, p_hp_my_left_node, &(t));
				my_left_node = t;
				HP_INIT(p_hp_td, p_hp_left_node_next, &(t_next));
				left_node_next = t_next;
			}
			
			t = (node_t *) get_unmarked_ref((long) t_next);
			if (!t->p_next) { 
				break; 
			}
			
			/* Swap hazard pointers */
			p_hp_temp = p_hp_t;
			p_hp_t = p_hp_t_next;
			p_hp_t_next = p_hp_temp;
			
			HP_INIT(p_hp_td, p_hp_t_next, &(t->p_next));
			t_next = t->p_next;
			
		} while (is_marked_ref((long) t_next) || (t->val < val));
		
		HP_INIT(p_hp_td, p_hp_right_node, &(t));
		right_node = t;

		/* Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if (right_node->p_next && is_marked_ref((long) right_node->p_next)) {
				goto search_again;
			} else {
				(*left_node) = my_left_node;
				return right_node;
			}
		}

		/* Remove one or more marked nodes */
		if (HARRIS_CAS(&(*left_node)->p_next, left_node_next, right_node)) {
			if (right_node->p_next && is_marked_ref((long) right_node->p_next)) {
				goto search_again;
			} else { 
				(*left_node) = my_left_node;
				return right_node;
			}
		}

	} while (1);

}

int harris_list_contains(list_t *p_list, val_t val) {
	node_t *right_node, *left_node;
	left_node = p_list->p_head;

	right_node = harris_list_search(p_list, val, &left_node);
	if ((!right_node->p_next) || right_node->val != val) {
		return 0;
	} else {
		return 1;
	}
	
}

int hp_harris_list_contains(hp_thread_t *p_hp_td, list_t *p_list, val_t val) {
	node_t *right_node;
	node_t *left_node;
	
	hp_record_t *p_hp_left_node;
	
	HP_RESET(p_hp_td);
	
	p_hp_left_node = HP_alloc(p_hp_td);
	
	HP_INIT(p_hp_td, p_hp_left_node, &(p_list->p_head));
	left_node = p_list->p_head;
	
	right_node = hp_harris_list_search(p_hp_td, p_list, val, &left_node);
	if ((!right_node->p_next) || right_node->val != val) {
		HP_RESET(p_hp_td);
		return 0;
	} else {
		HP_RESET(p_hp_td);
		return 1;
	}
	
}

int rcu_list_contains(list_t *p_list, val_t val) {
	int result;
	val_t v;
	node_t *p_prev, *p_next;
	node_t *p_node;
	
	RCU_READER_LOCK();
	
	p_prev = (node_t *)RCU_DEREF(p_list->p_head);
	p_next = (node_t *)RCU_DEREF(p_prev->p_next);
	while (1) {
		p_node = (node_t *)RCU_DEREF(p_next);
		v = p_node->val;
		
		if (v >= val) {
			break;
		}
		
		p_prev = p_next;
		p_next = (node_t *)RCU_DEREF(p_prev->p_next);
	}
	
	result = (v == val);

	RCU_READER_UNLOCK();

	return result;
}

int rlu_list_contains(rlu_thread_data_t *self, list_t *p_list, val_t val) {
	int result;
	val_t v;
	node_t *p_prev, *p_next;

	RLU_READER_LOCK(self);
	
	p_prev = (node_t *)RLU_DEREF(self, (p_list->p_head));
	p_next = (node_t *)RLU_DEREF(self, (p_prev->p_next));
	while (1) {
		//p_node = (node_t *)RLU_DEREF(self, p_next);
		v = p_next->val;
		
		if (v >= val) {
			break;
		}
		
		p_prev = p_next;
		p_next = (node_t *)RLU_DEREF(self, (p_prev->p_next));
	}
	
	result = (v == val);

	RLU_READER_UNLOCK(self);

	return result;
}

/////////////////////////////////////////////////////////
// HASH LIST CONTAINS
/////////////////////////////////////////////////////////
int pure_hash_list_contains(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return pure_list_contains(p_hash_list->buckets[hash], val);	
}

int harris_hash_list_contains(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return harris_list_contains(p_hash_list->buckets[hash], val);
}

int hp_harris_hash_list_contains(hp_thread_t *p_hp_td, hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return hp_harris_list_contains(p_hp_td, p_hash_list->buckets[hash], val);
}

int rcu_hash_list_contains(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return rcu_list_contains(p_hash_list->buckets[hash], val);	
}

int rlu_hash_list_contains(rlu_thread_data_t *self, hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return rlu_list_contains(self, p_hash_list->buckets[hash], val);	
}

/////////////////////////////////////////////////////////
// LIST ADD
/////////////////////////////////////////////////////////
int pure_list_add(list_t *p_list, val_t val)
{
	int result;
	node_t *p_prev, *p_next, *p_new_node;
	
	p_prev = p_list->p_head;
	p_next = p_prev->p_next;
	while (p_next->val < val) {
		p_prev = p_next;
		p_next = p_prev->p_next;
	}
	
	result = (p_next->val != val);
	
	if (result) {
		p_new_node = pure_new_node();
		p_new_node->val = val;
		p_new_node->p_next = p_next;
		
		p_prev->p_next = p_new_node;
	}
	
	return result;
}

int harris_list_add(list_t *p_list, val_t val) {
	node_t *new_node, *right_node, *left_node;
	left_node = p_list->p_head;

	do {
		right_node = harris_list_search(p_list, val, &left_node);
		if (right_node->val == val) {
			return 0;
		}
		new_node = harris_new_node();
		new_node->val = val;
		new_node->p_next = right_node;
		
		/* mem-bar between node creation and insertion */
		MEMBARSTLD();
		if (HARRIS_CAS(&left_node->p_next, right_node, new_node)) {
			return 1;
		}
		
	} while(1);
}

int hp_harris_list_add(hp_thread_t *p_hp_td, list_t *p_list, val_t val) {
	node_t *new_node;
	node_t *right_node;
	node_t *left_node;
	
	hp_record_t *p_hp_left_node;
		
	do {
		HP_RESET(p_hp_td);

		p_hp_left_node = HP_alloc(p_hp_td);

		HP_INIT(p_hp_td, p_hp_left_node, &(p_list->p_head));
		left_node = p_list->p_head;
		
		right_node = hp_harris_list_search(p_hp_td, p_list, val, &left_node);
		if (right_node->val == val) {
			HP_RESET(p_hp_td);
			return 0;
		}
		new_node = harris_new_node();
		new_node->val = val;
		new_node->p_next = right_node;
		
		/* mem-bar between node creation and insertion */
		MEMBARSTLD();
		if (HARRIS_CAS(&left_node->p_next, right_node, new_node)) {
			HP_RESET(p_hp_td);
			return 1;
		}
		
	} while(1);
}

int rcu_list_add(list_t *p_list, val_t val, int bucket) {
	int result;
	node_t *p_prev, *p_next;
	node_t *p_node;
	val_t v;

	RCU_WRITER_LOCK(bucket);
	
	p_prev = (node_t *)RCU_DEREF(p_list->p_head);
	p_next = (node_t *)RCU_DEREF(p_prev->p_next);
	while (1) {
		p_node = (node_t *)RCU_DEREF(p_next);
		v = p_node->val;
		
		if (v >= val) {
			break;
		}
		
		p_prev = p_next;
		p_next = (node_t *)RCU_DEREF(p_prev->p_next);
	}
	
	result = (v != val);
	
	if (result) {
		node_t *p_new_node = rcu_new_node();
		p_new_node->val = val;
		p_new_node->p_next = p_next;
		
		RCU_ASSIGN_PTR(&(p_prev->p_next), p_new_node);
	}
	
	RCU_WRITER_UNLOCK(bucket);
	
	return result;
}

int rlu_list_add(rlu_thread_data_t *self, list_t *p_list, val_t val) {
	int result;
	node_t *p_prev, *p_next;
	val_t v;

restart:
	RLU_READER_LOCK(self);
	
	p_prev = (node_t *)RLU_DEREF(self, (p_list->p_head));
	p_next = (node_t *)RLU_DEREF(self, (p_prev->p_next));
	while (1) {
		//p_node = (node_t *)RLU_DEREF(self, p_next);
		v = p_next->val;
		
		if (v >= val) {
			break;
		}
		
		p_prev = p_next;
		p_next = (node_t *)RLU_DEREF(self, (p_prev->p_next));
	}
	
	result = (v != val);
	
	if (result) {
		
		if (!RLU_TRY_LOCK(self, &p_prev)) {
			RLU_ABORT(self);
			goto restart;
		}
		
		if (!RLU_TRY_LOCK(self, &p_next)) {
			RLU_ABORT(self);
			goto restart;
		}
		
		node_t *p_new_node = rlu_new_node();
		p_new_node->val = val;
		RLU_ASSIGN_PTR(self, &(p_new_node->p_next), p_next);
		
		RLU_ASSIGN_PTR(self, &(p_prev->p_next), p_new_node);
	}
	
	RLU_READER_UNLOCK(self);
	
	return result;
}

/////////////////////////////////////////////////////////
// HASH LIST ADD
/////////////////////////////////////////////////////////
int pure_hash_list_add(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return pure_list_add(p_hash_list->buckets[hash], val);
}

int harris_hash_list_add(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return harris_list_add(p_hash_list->buckets[hash], val);
}

int hp_harris_hash_list_add(hp_thread_t *p_hp_td, hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return hp_harris_list_add(p_hp_td, p_hash_list->buckets[hash], val);
}

int rcu_hash_list_add(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return rcu_list_add(p_hash_list->buckets[hash], val, hash);
}

int rlu_hash_list_add(rlu_thread_data_t *self, hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return rlu_list_add(self, p_hash_list->buckets[hash], val);
}


/////////////////////////////////////////////////////////
// LIST REMOVE
/////////////////////////////////////////////////////////
int pure_list_remove(list_t *p_list, val_t val) {
	int result;
	node_t *p_prev, *p_next;

	p_prev = p_list->p_head;
	p_next = p_prev->p_next;
	while (p_next->val < val) {
		p_prev = p_next;
		p_next = p_prev->p_next;
	}

	result = (p_next->val == val);

	if (result) {
		p_prev->p_next = p_next->p_next;
		pure_free_node(p_next);      
	}
	
	return result;
}

int harris_list_remove(list_t *p_list, val_t val) {
	node_t *right_node, *right_node_next, *left_node;
	left_node = p_list->p_head;

	do {
		right_node = harris_list_search(p_list, val, &left_node);
		if (right_node->val != val) {
			return 0;
		}
		
		right_node_next = right_node->p_next;
		if (!is_marked_ref((long) right_node_next)) {
			if (HARRIS_CAS(&right_node->p_next, right_node_next, get_marked_ref((long) right_node_next))) {
				break;
			}
		}
		
	} while(1);
	
	if (!HARRIS_CAS(&left_node->p_next, right_node, right_node_next)) {
		right_node = harris_list_search(p_list, right_node->val, &left_node);
	}
	
	return 1;
}

int hp_harris_list_remove(hp_thread_t *p_hp_td, list_t *p_list, val_t val) {
	node_t *right_node;
	node_t *right_node_next;
	node_t *left_node;
	
	hp_record_t *p_hp_right_node_next;
	hp_record_t *p_hp_left_node;
		
	do {
		HP_RESET(p_hp_td);

		p_hp_left_node = HP_alloc(p_hp_td);
		p_hp_right_node_next = HP_alloc(p_hp_td);

		HP_INIT(p_hp_td, p_hp_left_node, &(p_list->p_head));
		left_node = p_list->p_head;
		
		right_node = hp_harris_list_search(p_hp_td, p_list, val, &left_node);
		if (right_node->val != val) {
			HP_RESET(p_hp_td);
			return 0;
		}
		
		HP_INIT(p_hp_td, p_hp_right_node_next, &(right_node->p_next));
		right_node_next = right_node->p_next;
		
		if (!is_marked_ref((long) right_node_next)) {
			if (HARRIS_CAS(&right_node->p_next, right_node_next, get_marked_ref((long) right_node_next))) {
				break;
			}
		}
		
	} while(1);
	
	if (!HARRIS_CAS(&left_node->p_next, right_node, right_node_next)) {
		right_node = harris_list_search(p_list, right_node->val, &left_node);
	}
	
	HP_RESET(p_hp_td);
	return 1;
}

int rcu_list_remove(list_t *p_list, val_t val, int bucket) {
	int result;
	node_t *p_prev, *p_next;
	node_t *p_node;
	node_t *n;
	
	RCU_WRITER_LOCK(bucket);
	
	p_prev = (node_t *)RCU_DEREF(p_list->p_head);
	p_next = (node_t *)RCU_DEREF(p_prev->p_next);
	while (1) {
		p_node = (node_t *)RCU_DEREF(p_next);
		
		if (p_node->val >= val) {
			break;
		}
		
		p_prev = p_next;
		p_next = (node_t *)RCU_DEREF(p_prev->p_next);
	}
	
	result = (p_node->val == val);
	
	if (result) {
		n = (node_t *)RCU_DEREF(p_next->p_next);
			
		RCU_ASSIGN_PTR(&(p_prev->p_next), n);
		
		RCU_WRITER_UNLOCK(bucket);

		rcu_free_node(p_next);
		
		return result;
	}
	
	RCU_WRITER_UNLOCK(bucket);
	
	return result;
}

int rlu_list_remove(rlu_thread_data_t *self, list_t *p_list, val_t val) {
	int result;
	node_t *p_prev, *p_next;
	node_t *n;
	val_t v;
	
restart:
	RLU_READER_LOCK(self);
	
	p_prev = (node_t *)RLU_DEREF(self, (p_list->p_head));
	p_next = (node_t *)RLU_DEREF(self, (p_prev->p_next));
	while (1) {
		//p_node = (node_t *)RLU_DEREF(self, p_next);
		
		v = p_next->val;
		
		if (v >= val) {
			break;
		}
		
		p_prev = p_next;
		p_next = (node_t *)RLU_DEREF(self, (p_prev->p_next));
	}
	
	result = (v == val);
	
	if (result) {
		n = (node_t *)RLU_DEREF(self, (p_next->p_next));
		
		if (!RLU_TRY_LOCK(self, &p_prev)) {
			RLU_ABORT(self);
			goto restart;
		}
		
		if (!RLU_TRY_LOCK(self, &p_next)) {
			RLU_ABORT(self);
			goto restart;
		}
		
		RLU_ASSIGN_PTR(self, &(p_prev->p_next), n);
		
		RLU_FREE(self, p_next);
		
		RLU_READER_UNLOCK(self);
		
		return result;
	}
	
	RLU_READER_UNLOCK(self);
	
	return result;
}

/////////////////////////////////////////////////////////
// HASH LIST REMOVE
/////////////////////////////////////////////////////////
int pure_hash_list_remove(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return pure_list_remove(p_hash_list->buckets[hash], val);
}

int harris_hash_list_remove(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return harris_list_remove(p_hash_list->buckets[hash], val);
}

int hp_harris_hash_list_remove(hp_thread_t *p_hp_td, hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return hp_harris_list_remove(p_hp_td, p_hash_list->buckets[hash], val);
}

int rcu_hash_list_remove(hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return rcu_list_remove(p_hash_list->buckets[hash], val, hash);
}

int rlu_hash_list_remove(rlu_thread_data_t *self, hash_list_t *p_hash_list, val_t val)
{
	int hash = HASH_VALUE(p_hash_list, val);
	
	return rlu_list_remove(self, p_hash_list->buckets[hash], val);
}

