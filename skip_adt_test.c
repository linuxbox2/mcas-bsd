#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>

/* for mutex testing */
#include <pthread.h>

#include "osi_mcas_obj_cache.h"
#include "set_queue_adt.h"

#define CREATE_N 1000000

gc_global_t *gc_global;

typedef struct {
    unsigned long key;
    unsigned long secret_key;
    unsigned long xxx[10];
    int traversal;
    pthread_mutex_t lock;
    void *gc_id;
} harness_ulong_t;

static struct {
    CACHE_PAD(0);
    osi_set_t *set;
      CACHE_PAD(1);
    osi_set_t *set2;
      CACHE_PAD(2);
} shared;

/* lock-free object cache */

/* Key data type and comparison function */
int
harness_ulong_comp(const void *lhs, const void *rhs)
{
    harness_ulong_t *l, *r;

    l = (harness_ulong_t *) lhs;
    r = (harness_ulong_t *) rhs;

#if 0
    /* todo:  move to wrapper macro outside
     * cmpf */

    if (lhs == SENTINEL_KEYMAX)
	return (1);
    if (rhs == SENTINEL_KEYMAX)
	return (-1);

    if (lhs == SENTINEL_KEYMIN)
	return (-1);
    if (rhs == SENTINEL_KEYMIN)
	return (1);
    /* end todo */
#endif

    if (l->key == r->key)
	return (0);
    if (l->key > r->key)
	return (1);

    return (-1);
}

void
test_each_func(osi_set_t * l, setval_t k, setval_t v, void *arg)
{

    harness_ulong_t *z;
    osi_mcas_obj_cache_t gc_id;
    int traversal;

    traversal = *(int *)arg;
    z = (harness_ulong_t *) k;
    gc_id = z->gc_id;

    if (z->traversal == traversal) {
	// should never appear
	printf("YELLOWJACKET key %d: secret key: %d (t: %d) \n",
	       z->key, z->secret_key, z->traversal);
	goto done;
    }

    if (z->key % 10 == 0) {
	printf("SPONGEBOB would delete key %d: secret key: %d (t: %d) \n",
	       z->key, z->secret_key, z->traversal);

	osi_cas_skip_remove(gc_global, l, z);
	osi_mcas_obj_cache_free(gc_global, gc_id, z);
    } else {
	printf("SQUAREPANTS still here key %d: secret key: %d (t: %d) \n",
	       z->key, z->secret_key, z->traversal);
    }

  done:
    z->traversal = traversal;
}

void
test_each_func_2(osi_set_t * l, setval_t k, setval_t v, void *arg)
{

    int traversal;
    harness_ulong_t *z;

    traversal = *(int *)arg;
    z = (harness_ulong_t *) k;

    if (z->traversal == traversal) {
	// should never appear
	printf("ORANGECAKE key %d: secret key: %d (t: %d) \n",
	       z->key, z->secret_key, z->traversal);
	goto done;
    }

    printf("SQUAREPANTS still here key %d: secret key: %d\n",
	   z->key, z->secret_key);
  done:
    z->traversal = traversal;
}

void
thread_do_test(void *arg)
{
    int ix, ix2, six, eix, sv;
    harness_ulong_t *node, *node2, *snode;

    sv = *((int *)arg);

    six = CREATE_N / 4 * sv;
    eix = six + CREATE_N / 4;

    printf("Starting thread with %d %d %d\n", sv, six, eix);

    /* Allocate nodes and insert in skip queue */

    for (; six < eix; ++six) {

	/* set 1 */
	node = (harness_ulong_t *) malloc(sizeof(harness_ulong_t));
	memset(node, 0, sizeof(harness_ulong_t));
	node->key = six;
	node->secret_key = 2 * six;
	printf("thread %d insert: %d key: %d secret: %d\n", sv, node,
	       node->key, node->secret_key);
	osi_cas_skip_update(gc_global, shared.set, node, node, 1);

#if 0				/* reuse package test */
	/* set 2 */
	node2 = (harness_ulong_t *) malloc(sizeof(harness_ulong_t));
	memset(node2, 0, sizeof(harness_ulong_t));
	node2->key = six;
	node2->secret_key = 3 * six;
	printf("thread %d insert: %d key: %d secret: %d\n", sv, node2,
	       node2->key, node2->secret_key);
	osi_cas_skip_update(gc_global, shared.set2, node2, node2, 1);
#endif
    }

    snode = (harness_ulong_t *) malloc(sizeof(harness_ulong_t));
    goto tdone;

    ix2 = 0;
    do {
	for (ix = 0; ix < CREATE_N; ix += 1) {
	    snode->key = ix;
	    node = osi_cas_skip_lookup(gc_global, shared.set, snode);
	    if (node) {
		printf("thread %d searched set(1) for and found: key: "
		       "%d secret_key: %d\n",
		       sv, node->key, node->secret_key);
	    } else {
		printf("thread %d searched set(1) for and didn't find: %d\n",
		       sv, snode->key);
	    }
#if 0				/* set2 */
	    node = osi_cas_skip_lookup(gc_global, shared.set2, snode);
	    if (node) {
		printf("thread %d searched set(2) for and found: key: "
		       "%d secret_key: %d\n",
		       sv, node->key, node->secret_key);
	    } else {
		printf("thread %d searched set(2) for and didn't find: %d\n",
		       sv, snode->key);
	    }
#endif
	}
	ix2++;
    } while (ix2 < 10000);

    /* dump queue */
  tdone:
    ix2++;

}

/* help test atomic inc, dec */

typedef long osi_atomic_t;

#define osi_atomic_inc(x) \
do { \
    RMB(); \
    FASPO(&x, x+1);	\
} while(0);

#define osi_atomic_dec(x) \
do { \
    RMB(); \
    FASPO(&x, x-1);	\
} while(0);

int
main(int argc, char **argv)
{

    int ix, traversal;
    harness_ulong_t *node, *node2, *snode;
    osi_mcas_obj_cache_t gc_id, gc2_id;

    printf("Starting ADT Test\n");

    /* do this once, 1st thread */
    gc_global = _init_gc_subsystem();
    _init_osi_cas_skip_subsystem(gc_global);

    osi_mcas_obj_cache_create(gc_global, &gc_id, sizeof(harness_ulong_t), "1");
    osi_mcas_obj_cache_create(gc_global, &gc2_id, sizeof(harness_ulong_t), "2");

    shared.set = osi_cas_skip_alloc(&harness_ulong_comp);
    shared.set2 = osi_cas_skip_alloc(&harness_ulong_comp);

    /* just insert and iterate */

    for (ix = 0; ix < CREATE_N; ++ix) {

	/* set 1 */
	node = (harness_ulong_t *) osi_mcas_obj_cache_alloc(gc_global, gc_id);
	pthread_mutex_init(&node->lock, NULL);

	/* and pound on collector 2 */
	node2 = (harness_ulong_t *) osi_mcas_obj_cache_alloc(gc_global, gc2_id);

	node->gc_id = gc_id;
	node->key = ix;
	node->secret_key = 2 * ix;
	printf("insert: %d key: %d secret: %d\n", node,
	       node->key, node->secret_key);
	osi_cas_skip_update(gc_global, shared.set, node, node, 1);
    }

    snode = (harness_ulong_t *) osi_mcas_obj_cache_alloc(gc_global, gc_id);
    snode->gc_id = gc_id;
    snode->key = 5;
    node = osi_cas_skip_lookup(gc_global, shared.set, snode);
    if (node) {
	printf("searched set(1) for and found: key: "
	       "%d secret_key: %d\n", node->key, node->secret_key);
    } else {
	printf("searched set(1) for and didn't find: %d\n", snode->key);
    }

    sleep(1);

    /* traversal is used to avoid acting on duplicate references (which
     * are an artifact of skip list implementation;  assumes only one
     * thread may be in set_for_each, as implemented */

    printf("now... \n");
    traversal = 1;
    osi_cas_skip_for_each(gc_global, shared.set, &test_each_func, &traversal);

    sleep(1);

    printf("now... \n");
    traversal = 2;
    osi_cas_skip_for_each(gc_global, shared.set, &test_each_func_2, &traversal);

    /* test osi_atomic_inc */

    {
	int a_ix;
	osi_atomic_t ai;

	for (ai = 0, a_ix = 0; a_ix < 10; ++a_ix) {
	    osi_atomic_inc(ai);
	}
	osi_atomic_dec(ai);

	printf("ai is: %d\n", ai);
    }

    osi_mcas_obj_cache_free(gc_global, gc_id, node);

    return 0;
}
