#ifndef _HASH_LIST_H_
#define _HASH_LIST_H_

/////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////
#include <linux/types.h>

/////////////////////////////////////////////////////////
// DEFINES
/////////////////////////////////////////////////////////
#define LIST_VAL_MIN (INT_MIN)
#define LIST_VAL_MAX (INT_MAX)

#define NODE_PADDING (30)
#define CACHELINE_SIZE (128)

#define MAX_BUCKETS (1000)
#define DEFAULT_BUCKETS                 1

/////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////
typedef int val_t;

typedef union node node_t;
typedef union node {
	struct {
		val_t val;
		node_t *p_next;
		struct rcu_head rcu;
	};
	char * padding[CACHELINE_SIZE];
} node_t;

typedef union list {
	struct {
		node_t *p_head;
		spinlock_t rcuspin;
	};
	char *padding[CACHELINE_SIZE];
} list_t;

typedef struct hash_list {
	int n_buckets;
	list_t *buckets[MAX_BUCKETS];
	char *padding[CACHELINE_SIZE];
} hash_list_t;

/////////////////////////////////////////////////////////
// INTERFACE
/////////////////////////////////////////////////////////
hash_list_t *rcu_new_hash_list(int n_buckets);
hash_list_t *rlu_new_hash_list(int n_buckets);

int rcu_hash_list_init(void);
int rcu_hash_list_contains(void *tl, val_t val);
int rcu_hash_list_add(void *tl, val_t val);
int rcu_hash_list_remove(void *tl, val_t val);

int rlu_hash_list_init(void);
int rlu_hash_list_contains(void *self, val_t val);
int rlu_hash_list_add(void *self, val_t val);
int rlu_hash_list_remove(void *self, val_t val);

#endif // _HASH_LIST_H_
