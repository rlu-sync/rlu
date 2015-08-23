#include <linux/slab.h>  // kmalloc
#include <linux/rcupdate.h>
#include <linux/types.h>
#include "rlu.h"


///////
///////

///////
///////
#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

///////////////////////////////
// CONFIGURATION
///////////////////////////////
#define NODE_PADDING                    (20)

#define MAX_VALUES						(0)

//#define IS_NODE_LOCAL_WORK

///////////////////////////////

#define MAX_HASH_BUCKETS                (10000)

#define MAX_FREE_NODES					(10000)

#define HASH_VALUE(hash_set, val)       (val % hash_set->hash_buckets)

#define DEFAULT_HASH_BUCKETS            (1)
#define DEFAULT_BATCH_SIZE				(1)

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#ifdef RUN_RCU

#define RCU_READ_LOCK(self)             rcu_read_lock()
#define RCU_READ_UNLOCK(self)           rcu_read_unlock()
#define RCU_SYNCHRONIZE                 synchronize_rcu()
#define RCU_ASSIGN_POINTER(self, p_ptr, p_obj)  \
										rcu_assign_pointer(p_ptr, p_obj)

#define RCU_DEREFERENCE(self, p_obj)    (p_obj)

#define RCU_LOCK(self)                  spin_lock(&rcuspin)
#define RCU_UNLOCK(self)                spin_unlock(&rcuspin)

#define RCU_INIT
#define RCU_EXIT
#define RCU_INIT_THREAD(self)
#define RCU_EXIT_THREAD(self)

#else
#ifdef RUN_RLU

#define RCU_READ_LOCK(self)             RLU_READER_LOCK(self)
#define RCU_READ_UNLOCK(self)           RLU_READER_UNLOCK(self)
#define RCU_SYNCHRONIZE                 //synchronize_rcu()

#define RLU_ALLOC(obj_size)             rlu_alloc(obj_size)

#define RLU_FORCE_REAL_REF(p_obj)       ((node_t *)FORCE_REAL_OBJ((intptr_t *)p_obj))
#define RLU_FREE(p_obj)		            (rlu_free(NULL, (intptr_t *)p_obj))
#define TH_RLU_FREE(self, p_obj)		(rlu_free(self, (intptr_t *)p_obj))

#define RCU_ASSIGN_POINTER(self, p_ptr, p_obj) \
										rlu_assign_pointer(self, (intptr_t **)p_ptr, (intptr_t *)p_obj)

#define RLU_LOCK_REF(self, p_obj)       ( rlu_obj_lock(self, (intptr_t **)p_obj, sizeof(node_t)) )
#define RCU_DEREFERENCE(self, p_obj)    ( (node_t *)RLU_DEREFERENCE(self, (intptr_t *)p_obj) )

#define RCU_LOCK(self)                  RLU_WRITER_LOCK(self)
#define RCU_UNLOCK(self)                RLU_WRITER_UNLOCK(self)

#define RCU_INIT                        rlu_init()
#define RCU_EXIT                        //pthread_mutex_destroy(&rculock)
#define RCU_INIT_THREAD(self)           rlu_thread_init(self, 64)
#define RCU_EXIT_THREAD(self)           rlu_thread_finish(self)



#else
#error define RUN_RCU or RUN_RLU
#endif
#endif

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

#ifdef RUN_RCU
__cacheline_aligned static DEFINE_SPINLOCK(rcuspin);
#endif

/* ################################################################### *
 * LINKEDLIST
 * ################################################################### */

# define INIT_SET_PARAMETERS            /* Nothing */

typedef intptr_t val_t;
# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX

typedef struct node {
	val_t val;
	struct node *next;

	long values[MAX_VALUES];
#ifdef RUN_RCU
    struct rcu_head rcu;
#endif

#ifdef NODE_PADDING
	long padding[NODE_PADDING];
#endif
} node_t;

typedef struct intset {
	node_t *head;
} intset_t;

typedef struct hash_intset {
	int hash_buckets;
	intset_t *entry[MAX_HASH_BUCKETS];
} hash_intset_t;


__cacheline_aligned static hash_intset_t * intset_set = NULL;


static void node_init_values(node_t *p_node) {
	int i;

	for (i = 0; i < MAX_VALUES; i++) {
		p_node->values[i] = i + 1;
	}
}

static long node_compute(node_t *p_node) {
#ifdef IS_NODE_LOCAL_WORK
	int i;
	long sum;

	for (i = 0; i < MAX_VALUES; i++) {
		sum += p_node->values[i];
	}

	return sum /= MAX_VALUES;
#else
    return 0;
#endif
}

static void node_free(node_t *node_to_free, rlu_thread_data_t *td) {
	if (td == NULL) {
#ifdef RUN_RLU
		RLU_FREE(node_to_free);
#else
		kfree(node_to_free);
#endif
		return;
	}

#ifdef RUN_RLU
	TH_RLU_FREE(td, node_to_free);
#else
	kfree_rcu(node_to_free, rcu);
#endif

	return;
}

static node_t *new_node(val_t val, node_t *next)
{
  node_t *node;

#ifdef RUN_RLU
  node = (node_t *)RLU_ALLOC(sizeof(node_t));
#else
  node = (node_t *)kmalloc(sizeof(node_t), GFP_KERNEL);
#endif
  if (node == NULL) {
    pr_err("%s: Cannot allocate memory", __FUNCTION__);
    return NULL;
  }

  node->val = val;
  node->next = next;

  node_init_values(node);

  return node;
}

#ifdef RUN_RLU
static node_t *rlu_new_node(void)
{
	return new_node(0, 0);
}
#endif

static intset_t *set_new(void)
{
	intset_t *set;
	node_t *min, *max;

	if ((set = (intset_t *)kmalloc(sizeof(intset_t), GFP_KERNEL)) == NULL) {
		pr_err("%s: Cannot allocate memory", __FUNCTION__);
		return NULL;
	}
	max = new_node(VAL_MAX, NULL);
	min = new_node(VAL_MIN, max);
	set->head = min;

	return set;
}

static hash_intset_t *hash_set_new(int hash_buckets)
{
	int i;

	hash_intset_t *hash_set;

	if ((hash_set = (hash_intset_t *)kmalloc(sizeof(hash_intset_t), GFP_KERNEL)) == NULL) {
		pr_err("%s: Cannot allocate memory", __FUNCTION__);
		return NULL;
	}

	hash_set->hash_buckets = hash_buckets;

	for (i = 0; i < hash_set->hash_buckets; i++) {
		hash_set->entry[i] = set_new();
	}

	return hash_set;
}

static void set_delete(intset_t *set)
{
	node_t *node, *next;

	node = set->head;
	while (node != NULL) {
		next = node->next;

		node_free(node, NULL);

		node = next;
	}

	kfree(set);
}

static void hash_set_delete(hash_intset_t *hash_set) {
	int i;

	for (i = 0; i < hash_set->hash_buckets; i++) {
		set_delete(hash_set->entry[i]);
		hash_set->entry[i] = NULL;
	}

	kfree(hash_set);
}

static int set_size(intset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}

static int hash_set_size(hash_intset_t *hash_set)
{
	int i;
	int size = 0;

	for (i = 0; i < hash_set->hash_buckets; i++) {
		size += set_size(hash_set->entry[i]);
	}

	return size;
}



static int set_contains(intset_t *set, val_t val, rlu_thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  node_t *p_node;
  val_t v;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
  } else {
    RCU_READ_LOCK(td);
    prev = (node_t *)RCU_DEREFERENCE(td, (set->head));
    next = (node_t *)RCU_DEREFERENCE(td, (prev->next));
    while (1) {
	  p_node = RCU_DEREFERENCE(td, next);
	  v = p_node->val;
	  if (node_compute(next) >= 1000) {
		pr_err("%s: Invalid sum", __FUNCTION__);
	  }
	  if (v >= val)
        break;
      prev = next;
      next = (node_t *)RCU_DEREFERENCE(td, (prev->next));
    }
    result = (v == val);
  }
  RCU_READ_UNLOCK(td);

  return result;
}

static int hash_set_contains(hash_intset_t *hash_set, val_t val, rlu_thread_data_t *td)
{
	int hash = HASH_VALUE(hash_set, val);

	return set_contains(hash_set->entry[hash], val, td);
}

static int set_add(intset_t *set, val_t val, rlu_thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  node_t *p_node;
  val_t v;

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val != val);
    if (result) {
      prev->next = new_node(val, next);
    }
  } else {
	int count = 0;
    RCU_LOCK(td);
    prev = (node_t *)RCU_DEREFERENCE(td, (set->head));
    next = (node_t *)RCU_DEREFERENCE(td, (prev->next));
    while (1) {
	  count++;
      p_node = RCU_DEREFERENCE(td, next);
      v = p_node->val;
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)RCU_DEREFERENCE(td, (prev->next));
    }
    result = (v != val);
    if (result) {
#ifdef RUN_RLU
	  node_t *p_new_node = rlu_new_node();
	  p_new_node->val = val;
	  RLU_LOCK_REF(td, &prev);
	  RCU_ASSIGN_POINTER(td, &(p_new_node->next), next);
	  RCU_ASSIGN_POINTER(td, &(prev->next), p_new_node);
#else
	  RCU_ASSIGN_POINTER(td, (prev->next), (new_node(val, next)));
#endif
    }
    RCU_UNLOCK(td);
  }

  return result;
}

static int hash_set_add(hash_intset_t *hash_set, val_t val, rlu_thread_data_t *td)
{
	int hash = HASH_VALUE(hash_set, val);

	return set_add(hash_set->entry[hash], val, td);
}

static int set_remove(intset_t *set, val_t val, rlu_thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  node_t *p_node;
  val_t v;
  node_t *n;

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
    if (result) {
      prev->next = next->next;
	  node_free(next, NULL);
    }
  } else {
    RCU_LOCK(td);
    prev = (node_t *)RCU_DEREFERENCE(td, (set->head));
    next = (node_t *)RCU_DEREFERENCE(td, (prev->next));
    while (1) {
      p_node = RCU_DEREFERENCE(td, next);
      v = p_node->val;
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)RCU_DEREFERENCE(td, (prev->next));
    }
    result = (v == val);
    if (result) {
      n = (node_t *)RCU_DEREFERENCE(td, (next->next));
#ifdef RUN_RLU
	  RLU_LOCK_REF(td, &prev);
	  RCU_ASSIGN_POINTER(td, &(prev->next), n);
	  next = RLU_FORCE_REAL_REF(next);
#else
      RCU_ASSIGN_POINTER(td, (prev->next), n);
#endif
      RCU_UNLOCK(td);

	  node_free(next, td);

    } else {
      RCU_UNLOCK(td);
    }
  }

  return result;
}

static int hash_set_remove(hash_intset_t *hash_set, val_t val, rlu_thread_data_t *td)
{
	int hash = HASH_VALUE(hash_set, val);

	return set_remove(hash_set->entry[hash], val, td);
}

#ifdef RUN_RLU
# define PREFIX(str) rlu##str
#else
# define PREFIX(str) rcu##str
#endif


/* Interface for driver */
int PREFIX(intset_init)(void)
{
	int hash_buckets = DEFAULT_HASH_BUCKETS;
	intset_set = hash_set_new(hash_buckets);
	return 0;
}

int PREFIX(intset_lookup)(void *tl, int key)
{
	rlu_thread_data_t *d = (rlu_thread_data_t *)tl;
	if (hash_set_contains(intset_set, key, d))
		return 0;
	return -ENOENT;
}

int PREFIX(intset_delete)(void *tl, int key)
{
	rlu_thread_data_t *d = (rlu_thread_data_t *)tl;
	if (hash_set_remove(intset_set, key, d))
		return 0;
	return -ENOENT;
}

int PREFIX(intset_insert)(void *tl, int key)
{
	rlu_thread_data_t *d = (rlu_thread_data_t *)tl;
	if (hash_set_add(intset_set, key, d))
		return 0;
	return -EEXIST;
}

int PREFIX(intset_test)(void* tl)
{
    rlu_thread_data_t *self = (rlu_thread_data_t *)tl;
    if (PREFIX(intset_insert)(self, 1010))
        pr_warn("intset: Element 1010 cannot be inserted");
    if (PREFIX(intset_lookup)(self, 1010))
        pr_warn("intset: Element 1010 must be here");
    if (!PREFIX(intset_lookup)(self, 1011))
        pr_warn("intset: Element 1011 must not be here");
    if (PREFIX(intset_insert)(self, 1011))
        pr_warn("intset: Element 1011 cannot be inserted");
    if (PREFIX(intset_insert)(self, 1012))
        pr_warn("intset: Element 1012 cannot be inserted");
    if (PREFIX(intset_lookup)(self, 1011))
        pr_warn("intset: Element 1011 must be here");
    if (PREFIX(intset_lookup)(self, 1012))
        pr_warn("intset: Element 1012 must be here");
    if (PREFIX(intset_lookup)(self, 1010))
        pr_warn("intset: Element 1010 must be here");
    if (PREFIX(intset_delete)(self, 1010))
        pr_warn("intset: Removal of element 1010 failed");
    if (!PREFIX(intset_lookup)(self, 1010))
        pr_warn("intset: Element 1010 must not be here");
    return 0;
}

