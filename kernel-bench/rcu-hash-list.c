#include <linux/slab.h>  // kmalloc
#include <linux/rcupdate.h>
#include <linux/types.h>


#include "hash-list.h"


/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////
#define HASH_VALUE(p_hash_list, val)    (val % p_hash_list->n_buckets)

#define RCU_READER_LOCK()               rcu_read_lock()
#define RCU_READER_UNLOCK()             rcu_read_unlock()
#define RCU_SYNCHRONIZE()               synchronize_rcu()
#define RCU_ASSIGN_PTR(p_ptr, p_obj)    rcu_assign_pointer(p_ptr, p_obj)

#define RCU_DEREF(p_obj)                (p_obj)

#define RCU_WRITER_LOCK(lock)           spin_lock(&lock)
#define RCU_WRITER_UNLOCK(lock)         spin_unlock(&lock)
#define RCU_FREE(ptr)                   kfree_rcu(ptr, rcu);

/////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// GLOBALS
/////////////////////////////////////////////////////////
// This global lock has been changed for a per-bucket lock
//__cacheline_aligned static DEFINE_SPINLOCK(rcuspin);

// FIXME Change function for no parameter and init func
__cacheline_aligned static hash_list_t *g_hash_list;

/////////////////////////////////////////////////////////
// FUNCTIONS
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
// NEW NODE
/////////////////////////////////////////////////////////
node_t *rcu_new_node(void) {

	node_t *p_new_node = (node_t *)kmalloc(sizeof(node_t), GFP_KERNEL);
	if (p_new_node == NULL){
		pr_err("%s: out of memory\n", __FUNCTION__);
	}

	return p_new_node;
}

/////////////////////////////////////////////////////////
// FREE NODE
/////////////////////////////////////////////////////////

void rcu_free_node(node_t *p_node) {
	RCU_FREE(p_node);
}

/////////////////////////////////////////////////////////
// NEW LIST
/////////////////////////////////////////////////////////
list_t *rcu_new_list(void)
{
	list_t *p_list;
	node_t *p_min_node, *p_max_node;

	p_list = (list_t *)kmalloc(sizeof(list_t), GFP_KERNEL);
	if (p_list == NULL) {
		pr_err("%s: out of memory\n", __FUNCTION__);
	}

	p_max_node = rcu_new_node();
	p_max_node->val = LIST_VAL_MAX;
	p_max_node->p_next = NULL;

	p_min_node = rcu_new_node();
	p_min_node->val = LIST_VAL_MIN;
	p_min_node->p_next = p_max_node;

	p_list->p_head = p_min_node;
	p_list->rcuspin = __SPIN_LOCK_UNLOCKED(p_list->rcuspin);

	return p_list;
}

/////////////////////////////////////////////////////////
// NEW HASH LIST
/////////////////////////////////////////////////////////
hash_list_t *rcu_new_hash_list(int n_buckets)
{
	int i;
	hash_list_t *p_hash_list;

	p_hash_list = (hash_list_t *)kmalloc(sizeof(hash_list_t), GFP_KERNEL);

	if (p_hash_list == NULL) {
		pr_err("%s: out of memory\n", __FUNCTION__);
	}

	p_hash_list->n_buckets = n_buckets;

	for (i = 0; i < p_hash_list->n_buckets; i++) {
		p_hash_list->buckets[i] = rcu_new_list();
	}

	return p_hash_list;
}

int rcu_hash_list_init(void)
{
	g_hash_list = rcu_new_hash_list(DEFAULT_BUCKETS);
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

/////////////////////////////////////////////////////////
// HASH LIST CONTAINS
/////////////////////////////////////////////////////////
int rcu_hash_list_contains(void *tl, val_t val)
{
	int hash = HASH_VALUE(g_hash_list, val);

	return rcu_list_contains(g_hash_list->buckets[hash], val) ? 0 : -ENOENT;
}

/////////////////////////////////////////////////////////
// LIST ADD
/////////////////////////////////////////////////////////
int rcu_list_add(list_t *p_list, val_t val) {
	int result;
	node_t *p_prev, *p_next;
	node_t *p_node;
	val_t v;

	RCU_WRITER_LOCK(p_list->rcuspin);

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

		RCU_ASSIGN_PTR((p_prev->p_next), p_new_node);
	}

	RCU_WRITER_UNLOCK(p_list->rcuspin);

	return result;
}


/////////////////////////////////////////////////////////
// HASH LIST ADD
/////////////////////////////////////////////////////////
int rcu_hash_list_add(void *tl, val_t val)
{
	int hash = HASH_VALUE(g_hash_list, val);

	return rcu_list_add(g_hash_list->buckets[hash], val) ? 0 : -EEXIST;
}

/////////////////////////////////////////////////////////
// LIST REMOVE
/////////////////////////////////////////////////////////
int rcu_list_remove(list_t *p_list, val_t val) {
	int result;
	node_t *p_prev, *p_next;
	node_t *p_node;
	node_t *n;

	RCU_WRITER_LOCK(p_list->rcuspin);

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

		RCU_ASSIGN_PTR((p_prev->p_next), n);

		RCU_WRITER_UNLOCK(p_list->rcuspin);

		rcu_free_node(p_next);

		return result;
	}

	RCU_WRITER_UNLOCK(p_list->rcuspin);

	return result;
}

/////////////////////////////////////////////////////////
// HASH LIST REMOVE
/////////////////////////////////////////////////////////
int rcu_hash_list_remove(void *tl, val_t val)
{
	int hash = HASH_VALUE(g_hash_list, val);

	return rcu_list_remove(g_hash_list->buckets[hash], val) ? 0 : -ENOENT;
}
