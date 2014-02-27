/******************************************************************************
 * fifo_mcas_adt.c
 * 
 * A concurrent, non-blocking queue using (M)CAS primitives. The implementation
 * is link-based, modelled in part on the CAS dequeue implementation by Sundell
 * and Tsigas [1]. The algorithm depends on single-word CAS only, ABA avoidance
 * is by pointer marking.  The MCAS garbage collector is used.
 *
 * [1] Håkan Sundell and Philippas Tsigas, "Lock-Free Deques and Doubly Linked
 *     Lists", Journal of Parallel and Distributed Computing, vol. 68, no. 7,
 *     pp. 1008-1020, Elsevier, 2008.
 *
 * Matt Benjamin <matt@linuxbox.com>
 *
 * Caution, pointer values 0x0, 0x01, and 0x02 are reserved.  Fortunately,
 * no real pointer is likely to have one of these values.
 *
 */
/*
 * Copyright (c) 2010                                     
 * The Linux Box Corporation                                    
 * ALL RIGHTS RESERVED                                          
 *                                                              
 * Permission is granted to use, copy, create derivative works  
 * and redistribute this software and such derivative works     
 * for any purpose, so long as the name of the Linux Box        
 * Corporation is not used in any advertising or publicity      
 * pertaining to the use or distribution of this software       
 * without specific, written prior authorization.  If the       
 * above copyright notice or any other identification of the    
 * Linux Box Corporation is included in any copy of any         
 * portion of this software, then the disclaimer below must     
 * also be included.                                            
 *                                                              
 * This software is provided as is, without representation      
 * from the Linux Box Corporation as to its fitness for any     
 * purpose, and without warranty by the Linux Box Corporation   
 * of any kind, either express or implied, including            
 * without limitation the implied warranties of                 
 * merchantability and fitness for a particular purpose.  The   
 * Linux Box Corporation shall not be liable for any damages,
 * including special, indirect, incidental, or consequential
 * damages, with respect to any claim arising out of or in 
 * connection with the use of the software, even if it has been
 * or is hereafter advised of the possibility of such damages.
 */


/* XXXX  This version is in fact...a blocking pthreaded queue and not
 * even optimized--it will be replaced shortly, after clients are blocked
 * in and feature complete --Matt
 */

#define __QUEUE_IMPLEMENTATION__


#define PTHREAD_IMPL_XXX    1


#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "portable_defns.h"

#include <afsconfig.h>
#include <afs/param.h>
#include <assert.h> 
#include <rx/rx.h> /* osi_Assert */
#include <osi/osi_includes.h>
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>
#include <osi/osi_time.h>
#include "osi_mcas_atomic.h"
#include "ptst.h"
#include "fifo_queue_adt.h"

#if !defined(SUBSYS_LOG_MACRO)
#define SUBSYS_LOG_MACRO
#else
#include <afsconfig.h>
#include <afs/param.h>
#include <afs/afsutil.h>
#endif


extern void osi_timespec_add_ms(osi_timespec_t * ts, unsigned long ms);

/*
 * Lock-free queue
 */

#define DEQUEUED_MARK      1
#define YOUR_MARK_HERE     2

typedef struct node_st node_t;
typedef VOLATILE node_t *sh_node_pt;

struct node_st {
    nodeval_t v;
    sh_node_pt prev;
    sh_node_pt next;
};

#define FIFO_DEQUEUE_THREADS_BLOCKED    0x0001

typedef struct cas_fifo_st {
    CACHE_PAD(0);
    sh_node_pt head;
    CACHE_PAD(1);
    sh_node_pt tail;
    CACHE_PAD(2);
    unsigned long len;
    CACHE_PAD(3);
    unsigned long wait;
    /* wait machinery */
    osi_mutex_t mtx;
    osi_condvar_t cv;
} cas_fifo_t;

static osi_mutex_options_t mutex_opts;
static osi_condvar_options_t condvar_opts;


#define NUM_LEVELS          1 /* a fifo is single-level */
#define NODE_ALLOC_LEVEL    0

static int gc_id[NUM_LEVELS];

/*
 * PRIVATE FUNCTIONS
 */

/*
 * Allocate a new node
 */
static node_t *
alloc_node(ptst_t * ptst)
{
    node_t *n;
    n = gc_alloc(ptst, gc_id[NODE_ALLOC_LEVEL]);
    return (n);
}

/* Free a node to the garbage collector. */
static void
free_node(ptst_t * ptst, sh_node_pt n)
{
    gc_free(ptst, (void *) n, gc_id[NODE_ALLOC_LEVEL]);
}

/*
 * PUBLIC FUNCTIONS
 */

/*
 * Called once before any set operations, including set_alloc
 */
void
_init_osi_cas_fifo_subsystem(void)
{
    int i;

    for (i = 0; i < NUM_LEVELS; i++) {
        gc_id[i] = gc_add_allocator(sizeof(node_t) + i * sizeof(node_t *),
                                    "fifo_cas_level");
    }

    osi_mutex_options_Init(&mutex_opts);
    osi_condvar_options_Init(&condvar_opts);

}        /* _init_osi_cas_fifo_subsystem */


osi_queue_t *
osi_cas_fifo_alloc(void)
{
    node_t *n;
    ptst_t *ptst;
    cas_fifo_t *q = (cas_fifo_t *) malloc(sizeof(cas_fifo_t));
    memset(q, 0, sizeof(cas_fifo_t));

#if defined(PTHREAD_IMPL_XXX)
    osi_mutex_Init(&q->mtx, &mutex_opts);
#endif

    ptst = critical_enter();

#ifndef PTHREAD_IMPL_XXX
    /* non-blocking queue */
#else
    /* blocking queue */
    q->head = q->tail = NULL;
#endif
    critical_exit(ptst);

    return (osi_queue_t *) q;
}


int
osi_cas_fifo_enqueue(osi_queue_t *q, fifo_val_t v)
{
    cas_fifo_t *_q = (cas_fifo_t *) q;
    node_t *n;
    ptst_t *ptst;
    int code = 0;

    _q = (cas_fifo_t *) q;

    ptst = critical_enter();

#ifndef PTHREAD_IMPL_XXX /* TODO: FIX */
    /* non-blocking queue */
#else
    /* blocking queue */

    /* all operations much maintain the invariant that one can
     * traverse from head, following head next recursively, ending
     * at tail.  head may be NULL, in which case tail is NULL.
     * 
     * the enqueue operation considers only the
     * head of the queue, which may be NULL, and since blocking impl
     * has no dummy node, conditionally tail, which will be assigned
     * the value of head iff head was NULL on enqueue (ensuring
     * that the first enqueued node may be dequeued from tail.
     *
     * there are two cases:  head is NULL, or 2) head points to some node.
     * if head is NULL, then after enqueue n->next points to NULL, head
     * points to n, and tail points to n.  if head points to some node,
     * then  after enqueue n->next points to the node formerly at
     * head, and head points to n.  tail is not considered, but by invariants
     * it points to some node reachable recursively from head.
     */
    n =  alloc_node(ptst);
    n->v = v;
    n->prev = NULL;

    osi_mutex_Lock(&_q->mtx);

    n->next = _q->head;
    if (_q->head == NULL)
        _q->tail = n;
    else
        _q->head->prev = n;
    _q->head = n;

    (_q->len)++;

    SUBSYS_LOG_MACRO(11, ("FIFO: _q->head %p _q->tail %p _q->len %lu "
                          "_q->wait %d n %p n->v %p\n",
                          _q->head, _q->tail, _q->len, _q->wait, n, n->v));

    /* wake dequeue threads (we already hold _q->mtx) */
    if (_q->wait /* this will be an optimistic read */)
        osi_condvar_Signal(&_q->cv);

    osi_mutex_Unlock(&_q->mtx);
#endif
    
    critical_exit(ptst);

    return (code);

}        /* osi_cas_fifo_enqueue */


fifo_val_t
osi_cas_fifo_dequeue(osi_queue_t *q, unsigned long flags)
{
    node_t *n;
    osi_timespec_t until;
    cas_fifo_t *_q;
    fifo_val_t v;
    ptst_t *ptst;
    int code;
    
    _q = (cas_fifo_t *) q;

    ptst = critical_enter();

#ifndef PTHREAD_IMPL_XXX /* TODO: FIX */
    /* non-blocking queue */
#else
    /* blocking queue */

    /* all operations much maintain the invariant that one can
     * traverse from head, following head next recursively, ending
     * at tail.  head may be NULL, in which case tail is NULL.
     * 
     * the dequeue operation considers only the
     * tail of the queue, which may be NULL, and since blocking impl
     * has no dummy node, conditionally head, which will be assigned
     * to NULL iff tail->prev was NULL before dequeue.  In particular,
     * dequeue may not consider tail's next pointer.
     */
    osi_mutex_Lock(&_q->mtx); /* _q->LOCKED */    
retry:
    v = NULL;
    n = _q->tail;

    /* optionally wait for something to dequeue */
    if (!n) {
        if (flags & FIFO_QUEUE_FLAG_WAIT) {
            _q->wait++;
            osi_timespec_get(&until);
            osi_timespec_add_ms(&until, 300000);
            code =  osi_condvar_WaitTimeoutAbs(&_q->cv, &_q->mtx, &until);
            _q->wait--;
            goto retry;
        } else
            goto out;
    }

    if (_q->tail->prev == NULL /* dequeue head */)
        _q->head = _q->tail = NULL;
    _q->tail = n->prev;
    if (_q->tail)
        _q->tail->next = NULL; /* NOP */
    v = n->v;

    /* and recycle dequeued node */
    free_node(ptst, n);

    /* dec qlen, already locked */
    (_q->len)--;
#endif

out:
    osi_mutex_Unlock(&_q->mtx); /* !_q->LOCKED */
    critical_exit(ptst);

    return (v);

}        /* osi_cas_fifo_dequeue */

unsigned long
osi_cas_fifo_length(osi_queue_t *q)
{
    cas_fifo_t *_q = (cas_fifo_t *) q;
    return (_q->len);

}        /* osi_fifo_length */
