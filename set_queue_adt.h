/******************************************************************************
 * set_queue_adt.h
 *
 * Matt Benjamin <matt@linuxbox.com>
 *
 * Adapts MCAS set interface to use a pointer-key and typed comparison
 * function style (because often, your key isn't an integer).
 *
 * Also, set_for_each (and future additions) allow a set to be iterated.
 * Current set_for_each iteration is unordered.
 *
 * Caution, pointer values 0x0, 0x01, and 0x02 are reserved.  Fortunately,
 * no real pointer is likely to have one of these values.
 *

Copyright (c) 2003, Keir Fraser All rights reserved.

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

#ifndef __SET_ADT_H__
#define __SET_ADT_H__


#ifdef __cplusplus
extern "C" {
#endif

typedef void *setkey_t;
typedef void *setval_t;


/* Internally used key values with special meanings. */
/* any calling code has to at least
 *   understand KEYMIN and KEYMAX.
 */
#define INVALID_FIELD   (0)		/* Uninitialised field value.     */
#define SENTINEL_KEYMIN ((void*)(1UL))	/* Key value of first dummy node. */
#define SENTINEL_KEYMAX ((void*)(~0UL))	/* Key value of last dummy node.  */

#ifdef __SET_IMPLEMENTATION__


/*************************************
 * INTERNAL DEFINITIONS
 */

/* Fine for 2^NUM_LEVELS nodes. */
#define NUM_LEVELS 20
//#define NUM_LEVELS 19

/*
 * SUPPORT FOR WEAK ORDERING OF MEMORY ACCESSES
 */

#ifdef WEAK_MEM_ORDER

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f)                                       \
do {                                                            \
    (_x) = (_f);                                                \
    if ( (_x) == INVALID_FIELD ) { RMB(); (_x) = (_f); }        \
    assert((_x) != INVALID_FIELD);                              \
} while ( 0 )

#else

/* Read field @_f into variable @_x. */
#define READ_FIELD(_x,_f) ((_x) = (_f))

typedef struct set_st osi_set_t;

#endif

typedef struct set_st osi_set_t;

#else /* __SET_IMPLEMENTATION__ */

typedef void osi_set_t;		/* opaque */

#endif /* __SET_IMPLEMENTATION__ */

/*************************************
 * PUBLIC DEFINITIONS
 */

/*
 * Key range accepted by set functions.
 * We lose three values (conveniently at top end of key space).
 *  - Known invalid value to which all fields are initialised.
 *  - Sentinel key values for up to two dummy nodes.
 */
#define KEY_MIN  ( 0U)
#define KEY_MAX  ((~0U) - 3)

/* Set element comparison function */
typedef int (*osi_set_cmp_func) (const void *lhs, const void *rhs);

/* Each-element function passed to set_for_each */
typedef void (*osi_set_each_func) (osi_set_t * l, setval_t k, setval_t v, void *arg);

void _init_osi_cas_skip_subsystem(gc_global_t *);

/*
 * Allocate an empty set.
 *
 * @cmpf - function to compare two keys, it must return an integer
 *         less than, equal to, or greater than 0 if 1st argument
 *         orders less than, equal to, or greater than the 2nd, as
 *         in qsort(3)
 */
osi_set_t *osi_cas_skip_alloc(int (*cmpf) (const void *, const void *));

/*
 * Remove a set.  Caller is responsible for making sure it's not in use.
 */
void osi_cas_skip_free(gc_global_t *, osi_set_t *);
void osi_cas_skip_free_critical(ptst_t *, osi_set_t *);

/*
 * Add mapping (@k -> @v) into set @s. Return previous mapped value if
 * one existed, or NULL if no previous mapping for @k existed.
 *
 * If @overwrite is FALSE, then if a mapping already exists it is not
 * modified, and the existing value is returned unchanged. It is possible
 * to see if the value was changed by observing if the return value is NULL.
 */
setval_t osi_cas_skip_update(gc_global_t *g, osi_set_t * s, setkey_t k,
			     setval_t v, int overwrite);
setval_t osi_cas_skip_update_critical(ptst_t *p, osi_set_t * s, setkey_t k,
			     setval_t v, int overwrite);

/*
 * Remove mapping for key @k from set @s. Return value associated with
 * removed mapping, or NULL is there was no mapping to delete.
 */
setval_t osi_cas_skip_remove(gc_global_t *g, osi_set_t * s, setkey_t k);
setval_t osi_cas_skip_remove_critical(ptst_t *p, osi_set_t * s, setkey_t k);

/*
 * Look up mapping for key @k in set @s. Return value if found, else NULL.
 */
setval_t osi_cas_skip_lookup(gc_global_t *g, osi_set_t * s, setkey_t k);
setval_t osi_cas_skip_lookup_critical(ptst_t *p, osi_set_t * s, setkey_t k);


/* Hybrid Set/Queue Operations (Matt) */

/* Iterate over a sequential structure, calling callback_func
 * on each (undeleted) element visited.  Unordered.
 */
void osi_cas_skip_for_each(gc_global_t *, osi_set_t * l,
			   osi_set_each_func each_func, void *arg);
void osi_cas_skip_for_each_critical(ptst_t *, osi_set_t * l,
			   osi_set_each_func each_func, void *arg);


#ifdef __cplusplus
}
#endif

#endif /* __SET_ADT_H__ */
