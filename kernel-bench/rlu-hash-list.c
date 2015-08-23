#include <linux/slab.h>  // kmalloc
#include <linux/rcupdate.h>
#include <linux/types.h>

#include "rlu.h"
#include "hash-list.h" 

/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)

/////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// GLOBALS
/////////////////////////////////////////////////////////
__cacheline_aligned static hash_list_t *g_hash_list;

/////////////////////////////////////////////////////////
// FUNCTIONS
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// NEW NODE
/////////////////////////////////////////////////////////
node_t *rlu_new_node(void) {
	node_t *p_new_node = (node_t *)RLU_ALLOC(sizeof(node_t));
	if (p_new_node == NULL){
		pr_err("out of memory\n");
	}    
	
    return p_new_node;
}

/////////////////////////////////////////////////////////
// NEW LIST
/////////////////////////////////////////////////////////
list_t *rlu_new_list(void)
{
	list_t *p_list;
	node_t *p_min_node, *p_max_node;

	p_list = (list_t *)kmalloc(sizeof(list_t), GFP_KERNEL);
	if (p_list == NULL) {
		pr_err("malloc");
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
hash_list_t *rlu_new_hash_list(int n_buckets)
{
	int i;	
	hash_list_t *p_hash_list;
  	
	p_hash_list = (hash_list_t *)kmalloc(sizeof(hash_list_t), GFP_KERNEL);
	
	if (p_hash_list == NULL) {
	    pr_err("malloc");
	}
	
	p_hash_list->n_buckets = n_buckets; 
	
	for (i = 0; i < p_hash_list->n_buckets; i++) {
		p_hash_list->buckets[i] = rlu_new_list();
	}
	
	return p_hash_list;
}

int rlu_hash_list_init(void)
{
    g_hash_list = rlu_new_hash_list(DEFAULT_BUCKETS);
    return 0;
}

/////////////////////////////////////////////////////////
// LIST SIZE
/////////////////////////////////////////////////////////
static int list_size(list_t *p_list)
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
static int hash_list_size(hash_list_t *p_hash_list)
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
int rlu_hash_list_contains(void *tl, val_t val)
{
	rlu_thread_data_t *self = (rlu_thread_data_t *)tl;
	int hash = HASH_VALUE(g_hash_list, val);
	
	int ret = rlu_list_contains(self, g_hash_list->buckets[hash], val);	
    if (ret)
        return 0;
    return -ENOENT;
}

/////////////////////////////////////////////////////////
// LIST ADD
/////////////////////////////////////////////////////////
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
		node_t *p_new_node;

		if (!RLU_TRY_LOCK(self, &p_prev)) {
			RLU_ABORT(self);
			goto restart;
		}
		
		if (!RLU_TRY_LOCK(self, &p_next)) {
			RLU_ABORT(self);
			goto restart;
		}
		
		p_new_node = rlu_new_node();
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
int rlu_hash_list_add(void *tl, val_t val)
{
	rlu_thread_data_t *self = (rlu_thread_data_t *)tl;
	int hash = HASH_VALUE(g_hash_list, val);
	
	int ret = rlu_list_add(self, g_hash_list->buckets[hash], val);
    if (ret)
        return 0;
    return -EEXIST;
}


/////////////////////////////////////////////////////////
// LIST REMOVE
/////////////////////////////////////////////////////////
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
int rlu_hash_list_remove(void *tl, val_t val)
{
	rlu_thread_data_t *self = (rlu_thread_data_t *)tl;
	int hash = HASH_VALUE(g_hash_list, val);
	
    int ret = rlu_list_remove(self, g_hash_list->buckets[hash], val);
    if (ret)
        return 0;
    return -ENOENT;
}

