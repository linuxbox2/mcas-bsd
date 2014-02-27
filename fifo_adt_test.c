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
#include <ucontext.h>
#include <signal.h>
#include <sched.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>

/* for mutex testing */
#include <pthread.h>

#include "portable_defns.h"
#include "fifo_queue_adt.h"
#include "ptst.h"

#define SENTINEL_KEYMIN ( 1UL)
#define SENTINEL_KEYMAX (~0UL)

static struct {
    CACHE_PAD(0);
    osi_queue_t *q1;
      CACHE_PAD(1);
    osi_queue_t *q2;
      CACHE_PAD(2);
} shared;

const int n_threads = 8;

typedef struct harness_ulong {
    unsigned long val;
} harness_ulong_t;

typedef struct harness_range {
    unsigned long tid;
    unsigned long start_ix;
    unsigned long n_inserts;
} harness_range_t;

void *
thread_do_test(void *arg)
{
    unsigned long ix, max_ix;
    harness_ulong_t *node;
    harness_range_t *hr;

    hr = (harness_range_t *) arg;

    printf("Starting thread %d, will perf. %d enqueues from %d\n", hr->tid,
           hr->n_inserts, hr->start_ix);

    /* Allocate nodes and insert in skip queue */
    max_ix = hr->start_ix + hr->n_inserts;
    for (ix = hr->start_ix; ix < max_ix; ++ix) {
	node = (harness_ulong_t *) malloc(sizeof(harness_ulong_t));
	memset(node, 0, sizeof(harness_ulong_t));
	node->val = ix;
	osi_fifo_enqueue(shared.q1, node);
        printf("inserted: %d\n", ix);
    }

out:
    return (NULL);
}

int
main(int argc, char **argv)
{

    int ix;
    harness_ulong_t *node;
    pthread_t thrd[n_threads];
    harness_range_t hr[n_threads];

    printf("Starting ADT Test\n");

    /* do this once, 1st thread */
    _init_ptst_subsystem();
    _init_gc_subsystem();
    _init_fifo_queue_subsystem();

    shared.q1 = osi_fifo_queue_alloc();

    for (ix = 0; ix < n_threads; ++ix) {
        
        (hr[ix]).tid = ix;
        (hr[ix]).start_ix = ix * 5000;
        (hr[ix]).n_inserts = 5000;

        pthread_create (&(thrd[ix]), NULL, thread_do_test, (void *) &(hr[ix]));
    }

    for (ix = 0; ix < n_threads; ++ix)
        pthread_join(thrd[ix], NULL);
    
    return 0;
}
