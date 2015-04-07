/******************************************************************************
 * gc.c
 *
 * A fully recycling epoch-based garbage collector. Works by counting
 * threads in and out of critical regions, to work out when
 * garbage queues can be fully deleted.
 *
 * Copyright (c) 2001-2003, K A Fraser

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
    * notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    * copyright notice, this list of conditions and the following
    * disclaimer in the documentation and/or other materials provided
    * with the distribution.  Neither the name of the Keir Fraser
    * nor the names of its contributors may be used to endorse or
    * promote products derived from this software without specific
    * prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "portable_defns.h"
#include "random.h"
#include "gc.h"
#include "ptst.h"

#if !defined(SUBSYS_LOG_MACRO)
#define SUBSYS_LOG_MACRO
#else
#include <afsconfig.h>
#include <afs/param.h>
#include <afs/afsutil.h>
#endif
#include "internal.h"

/* XXX generalize */
#ifndef KERNEL

#define MEM_FAIL(_s) \
do { \
    fprintf(stderr, "OUT OF MEMORY: %d bytes at line %d\n", (_s), __LINE__); \
    abort(); \
} while ( 0 )
#endif

/* Allocate more empty chunks from the heap. */
#define CHUNKS_PER_ALLOC 1000
static chunk_t *alloc_more_chunks(void)
{
    int i;
    chunk_t *h, *p;

    SUBSYS_LOG_MACRO(11, ("GC: alloc_more_chunks alloc %lu chunks\n",
        CHUNKS_PER_ALLOC));

    h = p = ALIGNED_ALLOC(CHUNKS_PER_ALLOC * sizeof(*h));
    if ( h == NULL ) MEM_FAIL(CHUNKS_PER_ALLOC * sizeof(*h));

    for ( i = 1; i < CHUNKS_PER_ALLOC; i++ )
    {
        p->next = p + 1;
        p++;
    }

    p->next = h;

    return(h);
}


/* Put a chain of chunks onto a list. */
static void add_chunks_to_list(chunk_t *ch, chunk_t *head)
{
    chunk_t *h_next, *new_h_next, *ch_next;
    ch_next    = ch->next;
    new_h_next = head->next;
    do { ch->next = h_next = new_h_next; WMB_NEAR_CAS(); }
    while ( (new_h_next = CASPO(&head->next, h_next, ch_next)) != h_next );
}


/* Allocate a chain of @n empty chunks. Pointers may be garbage. */
static chunk_t *get_empty_chunks(gc_global_t *gc_global, int n)
{
    int i;
    chunk_t *new_rh, *rh, *rt, *head;

 retry:
    head = gc_global->free_chunks;
    new_rh = head->next;
    do {
        rh = new_rh;
        rt = head;
        WEAK_DEP_ORDER_RMB();
        for ( i = 0; i < n; i++ )
        {
            if ( (rt = rt->next) == head )
            {
                /* Allocate some more chunks. */
                add_chunks_to_list(alloc_more_chunks(), head);
                goto retry;
            }
        }
    }
    while ( (new_rh = CASPO(&head->next, rh, rt->next)) != rh );

    rt->next = rh;
    return(rh);
}


/* Get @n filled chunks, pointing at blocks of @sz bytes each. */
static chunk_t *get_filled_chunks(gc_global_t *gc_global, int n, int sz)
{
    chunk_t *h, *p;
    char *node;
    int i;

#ifdef PROFILE_GC
    ADD_TO(gc_global->total_size, n * BLKS_PER_CHUNK * sz);
    ADD_TO(gc_global->allocations, 1);
#endif

    node = ALIGNED_ALLOC(n * BLKS_PER_CHUNK * sz);
    if ( node == NULL ) MEM_FAIL(n * BLKS_PER_CHUNK * sz);
#ifdef WEAK_MEM_ORDER
    INITIALISE_NODES(node, n * BLKS_PER_CHUNK * sz);
#endif

    h = p = get_empty_chunks(gc_global, n);
    do {
        p->i = BLKS_PER_CHUNK;
        for ( i = 0; i < BLKS_PER_CHUNK; i++ )
        {
            p->blk[i] = node;
            node += sz;
        }
    }
    while ( (p = p->next) != h );

    return(h);
}


/*
 * gc_async_barrier: Cause an asynchronous barrier in all other threads. We do
 * this by causing a TLB shootdown to be propagated to all other processors.
 * Each time such an action is required, this function calls:
 *   mprotect(async_page, <page size>, <new flags>)
 * Each thread's state contains a memory page dedicated for this purpose.
 */
#ifdef WEAK_MEM_ORDER
static void gc_async_barrier(gc_t *gc)
{
    gc_global_t *gc_global = gc->global;
    mprotect(gc->async_page, gc_global->page_size,
             gc->async_page_state ? PROT_READ : PROT_NONE);
    gc->async_page_state = !gc->async_page_state;
}
#else
#define gc_async_barrier(_g) ((void)0)
#endif


/* Grab a level @i allocation chunk from main chain. */
static chunk_t *get_alloc_chunk(gc_t *gc, int i)
{
    chunk_t *alloc, *p, *new_p, *nh;
    unsigned int sz;
    gc_global_t *gc_global = gc->global;

    alloc = gc_global->alloc[i];
    new_p = alloc->next;

    do {
        p = new_p;
        while ( p == alloc )
        {
            sz = gc_global->alloc_size[i];
            nh = get_filled_chunks(gc_global, sz, gc_global->blk_sizes[i]);
            ADD_TO(gc_global->alloc_size[i], sz >> 3);
            gc_async_barrier(gc);
            add_chunks_to_list(nh, alloc);
            p = alloc->next;
        }
        WEAK_DEP_ORDER_RMB();
    }
    while ( (new_p = CASPO(&alloc->next, p, p->next)) != p );

    p->next = p;
    assert(p->i == BLKS_PER_CHUNK);
    return(p);
}


#ifndef MINIMAL_GC
/*
 * gc_reclaim: Scans the list of struct gc_perthread looking for the lowest
 * maximum epoch number seen by a thread that's in the list code. If it's the
 * current epoch, the "nearly-free" lists from the previous epoch are
 * reclaimed, and the epoch is incremented.
 */
static void gc_reclaim(gc_global_t *gc_global)
{
    ptst_t       *ptst, *first_ptst, *our_ptst = NULL;
    gc_t         *gc = NULL;
    unsigned long curr_epoch;
    chunk_t      *ch, *t;
    int           two_ago, three_ago, i, j;

    SUBSYS_LOG_MACRO(11, ("GC: gc_reclaim enter\n"));

    /* Barrier to entering the reclaim critical section. */
    if ( gc_global->inreclaim || CASIO(&gc_global->inreclaim, 0, 1) ) return;

    SUBSYS_LOG_MACRO(11, ("GC: gc_reclaim after inreclaim barrier\n"));

    /*
     * Grab first ptst structure *before* barrier -- prevent bugs
     * on weak-ordered architectures.
     */
    first_ptst = ptst_first(gc_global);
    MB();
    curr_epoch = gc_global->current;

    /* Have all threads seen the current epoch, or not in mutator code? */
    for ( ptst = first_ptst; ptst != NULL; ptst = ptst_next(ptst) )
    {
        if ( (ptst->count > 1) && (ptst->gc->epoch != curr_epoch) ) goto out;
    }


    SUBSYS_LOG_MACRO(11, ("GC: gc_reclaim all-threads see current epoch\n"));

    /*
     * Three-epoch-old garbage lists move to allocation lists.
     * Two-epoch-old garbage lists are cleaned out.
     */
    two_ago   = (curr_epoch+2) % NR_EPOCHS;
    three_ago = (curr_epoch+1) % NR_EPOCHS;
    if ( gc_global->nr_hooks != 0 )
        our_ptst = (ptst_t *)pthread_getspecific(gc_global->ptst_key);
    for ( ptst = first_ptst; ptst != NULL; ptst = ptst_next(ptst) )
    {
        gc = ptst->gc;

        for ( i = 0; i < gc_global->nr_sizes; i++ )
        {
#ifdef WEAK_MEM_ORDER
            int sz = gc_global->blk_sizes[i];
            if ( gc->garbage[two_ago][i] != NULL )
            {
                chunk_t *head = gc->garbage[two_ago][i];
                ch = head;
                do {
                    int j;
                    for ( j = 0; j < ch->i; j++ )
                        INITIALISE_NODES(ch->blk[j], sz);
                }
                while ( (ch = ch->next) != head );
            }
#endif

            /* NB. Leave one chunk behind, as it is probably not yet full. */
            t = gc->garbage[three_ago][i];
            if ( (t == NULL) || ((ch = t->next) == t) ) continue;
            gc->garbage_tail[three_ago][i]->next = ch;
            gc->garbage_tail[three_ago][i] = t;
            t->next = t;

			/* gc inst: compute and log size of returned list */
			{
				chunk_t *ch_head, *ch_next;
				int r_ix, r_len, r_size;
				r_ix = 0;
				r_len = 0;
				r_size = 0;

				/* XXX: nonfatal, may be missing multiplier */
				ch_next = ch_head = ch;
			    do {
					r_len++;
				} while ((ch_next = ch_next->next)
					&& (ch_next != ch_head));

				SUBSYS_LOG_MACRO(11, ("GC: return %d chunks of size %d to "
							 "gc_global->alloc[%d]\n",
							 r_len,
							 gc_global->blk_sizes[i],
							 i));
			}


            add_chunks_to_list(ch, gc_global->alloc[i]);
        }

        for ( i = 0; i < gc_global->nr_hooks; i++ )
        {
            hook_fn_t fn = gc_global->hook_fns[i];
            ch = gc->hook[three_ago][i];
            if ( ch == NULL ) continue;
            gc->hook[three_ago][i] = NULL;

	    if (fn) {
		t = ch;
		do { for ( j = 0; j < t->i; j++ ) fn(our_ptst, t->blk[j]); }
		while ( (t = t->next) != ch );
	    }

			/* gc inst: compute and log size of returned list */
			{
				chunk_t *ch_head, *ch_next;
				int r_ix, r_len, r_size;
				r_ix = 0;
				r_len = 0;

				/* XXX: nonfatal, may be missing multiplier */
				ch_head = ch;
			    do {
					r_len++;
				} while (ch->next && (ch->next != ch_head)
						 && (ch_next = ch->next));

				SUBSYS_LOG_MACRO(11, ("GC: return %d chunks to gc_global->free_chunks\n",
							 r_len));
			}

            add_chunks_to_list(ch, gc_global->free_chunks);
        }
    }

    /* Update current epoch. */
    SUBSYS_LOG_MACRO(11, ("GC: gc_reclaim epoch transition (leaving %lu)\n",
				 curr_epoch));

    WMB();
    gc_global->current = (curr_epoch+1) % NR_EPOCHS;

 out:
    gc_global->inreclaim = 0;
}
#endif /* MINIMAL_GC */


void *gc_alloc(ptst_t *ptst, int alloc_id)
{
    gc_t *gc = ptst->gc;
    chunk_t *ch;
    gc_global_t *gc_global = gc->global;

    ch = gc->alloc[alloc_id];
    if ( ch->i == 0 )
    {
        if ( gc->alloc_chunks[alloc_id]++ == 100 )
        {
            gc->alloc_chunks[alloc_id] = 0;
            add_chunks_to_list(ch, gc_global->free_chunks);
            gc->alloc[alloc_id] = ch = get_alloc_chunk(gc, alloc_id);
        }
        else
        {
            chunk_t *och = ch;
            ch = get_alloc_chunk(gc, alloc_id);
            ch->next  = och->next;
            och->next = ch;
            gc->alloc[alloc_id] = ch;
        }
    }

    return ch->blk[--ch->i];
}

int
gc_get_blocksize(gc_global_t *gc_global, int alloc_id)
{
    return (gc_global->blk_sizes[alloc_id]);
}

char *
gc_get_tag(gc_global_t *gc_global, int alloc_id)
{
    return (gc_global->tags[alloc_id]);
}

static chunk_t *chunk_from_cache(gc_t *gc)
{
    chunk_t *ch = gc->chunk_cache, *p = ch->next;
    gc_global_t *gc_global = gc->global;

    if ( ch == p )
    {
        gc->chunk_cache = get_empty_chunks(gc_global, 100);
    }
    else
    {
        ch->next = p->next;
        p->next  = p;
    }

    p->i = 0;
    return(p);
}


void gc_free(ptst_t *ptst, void *p, int alloc_id)
{
#ifndef MINIMAL_GC
    gc_t *gc = ptst->gc;
    chunk_t *prev, *new, *ch = gc->garbage[gc->epoch][alloc_id];

    if ( ch == NULL )
    {
        gc->garbage[gc->epoch][alloc_id] = ch = chunk_from_cache(gc);
        gc->garbage_tail[gc->epoch][alloc_id] = ch;
    }
    else if ( ch->i == BLKS_PER_CHUNK )
    {
        prev = gc->garbage_tail[gc->epoch][alloc_id];
        new  = chunk_from_cache(gc);
        gc->garbage[gc->epoch][alloc_id] = new;
        new->next  = ch;
        prev->next = new;
        ch = new;
    }

    ch->blk[ch->i++] = p;
#endif
}


void gc_add_ptr_to_hook_list(ptst_t *ptst, void *ptr, int hook_id)
{
    gc_t *gc = ptst->gc;
    chunk_t *och, *ch = gc->hook[gc->epoch][hook_id];

    if ( ch == NULL )
    {
        gc->hook[gc->epoch][hook_id] = ch = chunk_from_cache(gc);
    }
    else
    {
        ch = ch->next;
        if ( ch->i == BLKS_PER_CHUNK )
        {
            och       = gc->hook[gc->epoch][hook_id];
            ch        = chunk_from_cache(gc);
            ch->next  = och->next;
            och->next = ch;
        }
    }

    ch->blk[ch->i++] = ptr;
}


void gc_unsafe_free(ptst_t *ptst, void *p, int alloc_id)
{
    gc_t *gc = ptst->gc;
    chunk_t *ch;

    ch = gc->alloc[alloc_id];
    if ( ch->i < BLKS_PER_CHUNK )
    {
        ch->blk[ch->i++] = p;
    }
    else
    {
        gc_free(ptst, p, alloc_id);
    }
}


void gc_enter(ptst_t *ptst)
{
#ifdef MINIMAL_GC
    ptst->count++;
    MB();
#else
    gc_t *gc = ptst->gc;
    gc_global_t *gc_global = gc->global;
    int new_epoch, cnt;

 retry:
    cnt = ptst->count++;
    MB();
    if ( cnt == 1 )
    {
        new_epoch = gc_global->current;
        if ( gc->epoch != new_epoch )
        {
            gc->epoch = new_epoch;
            gc->entries_since_reclaim        = 0;
#ifdef YIELD_TO_HELP_PROGRESS
            gc->reclaim_attempts_since_yield = 0;
#endif
        }
        else if ( gc->entries_since_reclaim++ == 100 )
        {
            ptst->count--;
#ifdef YIELD_TO_HELP_PROGRESS
            if ( gc->reclaim_attempts_since_yield++ == 10000 )
            {
                gc->reclaim_attempts_since_yield = 0;
                sched_yield();
            }
#endif
            gc->entries_since_reclaim = 0;
            gc_reclaim(gc_global);
            goto retry;
        }
    }
#endif
}


void gc_exit(ptst_t *ptst)
{
    MB();
    ptst->count--;
}


gc_t *gc_init(gc_global_t *gc_global)
{
    gc_t *gc;
    int   i;

    gc = ALIGNED_ALLOC(sizeof(*gc));
    if ( gc == NULL ) MEM_FAIL(sizeof(*gc));
    memset(gc, 0, sizeof(*gc));

    gc->global = gc_global;
#ifdef WEAK_MEM_ORDER
    /* Initialise shootdown state. */
    gc->async_page = mmap(NULL, gc_global->page_size, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ( gc->async_page == (void *)MAP_FAILED ) MEM_FAIL(gc_global->page_size);
    gc->async_page_state = 1;
#endif

    gc->chunk_cache = get_empty_chunks(gc_global, 100);

    /* Get ourselves a set of allocation chunks. */
    for ( i = 0; i < gc_global->nr_sizes; i++ )
    {
        gc->alloc[i] = get_alloc_chunk(gc, i);
    }
    for ( ; i < MAX_SIZES; i++ )
    {
        gc->alloc[i] = chunk_from_cache(gc);
    }

    return(gc);
}


int
gc_add_allocator(gc_global_t *gc_global, int alloc_size, char *tag)
{
    int ni, i;

    RMB();
    FASPO(&gc_global->n_allocators, gc_global->n_allocators + 1);
    if (gc_global->n_allocators > MAX_SIZES) {
	/* critical error */
#if !defined(KERNEL)
	printf("MCAS gc max allocators exceeded, aborting\n");
#endif
	abort();
    }

    i = gc_global->nr_sizes;
    while ((ni = CASIO(&gc_global->nr_sizes, i, i + 1)) != i)
	i = ni;
    gc_global->blk_sizes[i] = alloc_size;
    gc_global->tags[i] = strdup(tag);
    gc_global->alloc_size[i] = ALLOC_CHUNKS_PER_LIST;
    gc_global->alloc[i] = get_filled_chunks(gc_global, ALLOC_CHUNKS_PER_LIST, alloc_size);
    return i;
}


void gc_remove_allocator(gc_global_t *gc_global, int alloc_id)
{
    /* This is a no-op for now. */
}


int gc_add_hook(gc_global_t *gc_global, hook_fn_t fn)
{
    int ni, i = gc_global->nr_hooks;
    while ( (ni = CASIO(&gc_global->nr_hooks, i, i+1)) != i ) i = ni;
    if (gc_global->nr_hooks > MAX_HOOKS) {
	/* critical error */
#if !defined(KERNEL)
	printf("MCAS gc max hooks exceeded, aborting\n");
#endif
	abort();
    }
    gc_global->hook_fns[i] = fn;
    return i;
}


void gc_remove_hook(gc_global_t *gc_global, int hook_id)
{
    gc_global->hook_fns[hook_id] = 0;
}


void _destroy_gc_subsystem(gc_global_t *gc_global)
{
    // assume: 2's complement math and page_size a multiple of 2
    size_t global_size = (sizeof (*gc_global) + (gc_global->page_size-1))
	& -gc_global->page_size;
#ifdef PROFILE_GC
    printf("Total heap: %u bytes (%.2fMB) in %u allocations\n",
           gc_global->total_size, (double)gc_global->total_size / 1000000,
           gc_global->allocations);
#endif
    munmap(gc_global, global_size);
}


gc_global_t * _init_gc_subsystem(void)
{
    gc_global_t *gc_global;
    unsigned int page_size = (unsigned int)sysconf(_SC_PAGESIZE);
	// assume: 2's complement math and page_size a multiple of 2
    size_t global_size = (sizeof (*gc_global) + (page_size-1)) & -page_size;
    int e;

    gc_global = mmap(NULL, global_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // memset(gc_global, 0, sizeof(*gc_global));

    gc_global->page_size   = page_size;
    gc_global->free_chunks = alloc_more_chunks();

    gc_global->nr_hooks = 0;
    gc_global->nr_sizes = 0;

	/* ptst */
    _init_ptst_subsystem(gc_global);
    return gc_global;
}
