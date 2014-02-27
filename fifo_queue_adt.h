/******************************************************************************
 * fifo_queue_adt.h
 * 
 * Matt Benjamin <matt@linuxbox.com>
 *
 * Abtract interface to a non-blocking fifo queue.
 *
 * Caution, pointer values 0x0, 0x01, and 0x02 are reserved.  Fortunately,
 * no real pointer is likely to have one of these values.
 * 

Portions Copyright (c) 2003, Keir Fraser All rights reserved.

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

#ifndef __FIFO_ADT_H__
#define __FIFO_ADT_H__


typedef void *fifo_val_t;


#ifdef __QUEUE_IMPLEMENTATION__


/*************************************
 * INTERNAL DEFINITIONS
 */

/* Internal key values with special meanings. */
#define INVALID_FIELD   (0)	/* Uninitialised field value.     */
#define SENTINEL_KEYMIN ( 1UL)	/* Key value of first dummy node. */
#define SENTINEL_KEYMAX (~0UL)	/* Key value of last dummy node.  */

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

#endif /* !WEAK_MEM_ORDER */

#endif /* __QUEUE_IMPLEMENTATION */

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

#define FIFO_QUEUE_FLAG_NONE    0x0000
#define FIFO_QUEUE_FLAG_WAIT    0x0001

typedef void * nodeval_t;

typedef void osi_queue_t; /* opaque */

/* element on-enqueue hook function */
typedef void (*enqueue_hook_func) (osi_queue_t *q, fifo_val_t v);

/* element on-dequeue hook function */
typedef void (*dequeue_hook_func) (osi_queue_t *q, fifo_val_t v);


void _init_osi_cas_fifo_subsystem(void);

/*
 * Allocate an empty queue
 */
osi_queue_t *osi_cas_fifo_alloc(void);

/*
 * Enqueue value @v on FIFO queue @q
 */
int osi_cas_fifo_enqueue(osi_queue_t *q, fifo_val_t v);

/*
 * Dequeue value @ret from FIFO queue @q
 */
fifo_val_t osi_cas_fifo_dequeue(osi_queue_t *q, unsigned long flags);

/*
 * Return approximate length of FIFO @q
 */
unsigned long osi_cas_fifo_length(osi_queue_t *q);


#endif /* __FIFO_ADT_H__ */
