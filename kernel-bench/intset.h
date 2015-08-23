#ifndef _INTSET_H
#define _INTSET_H

int rluintset_init(void);
int rluintset_lookup(void *tl, int key);
int rluintset_delete(void *tl, int key);
int rluintset_insert(void *tl, int key);
int rluintset_test(void *arg);
int rcuintset_init(void);
int rcuintset_lookup(void *tl, int key);
int rcuintset_delete(void *tl, int key);
int rcuintset_insert(void *tl, int key);
int rcuintset_test(void *arg);

#endif /* _INTSET_H */

