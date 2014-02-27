/*
 * Copyright (c) 2008-2009
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
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#ifndef OSI_MCAS_ATOMIC_H
#define OSI_MCAS_ATOMIC_H

typedef unsigned long osi_atomic_t;

#if defined(AFS_PTHREAD_ENV)

#include "portable_defns.h" /* FASPO, CASIO, etc. */

#define CASIO_ATOMICS 1


#if CASIO_ATOMICS

/* these update in place, discarding the result */
#define osi_atomic_inc(x) ADD_TO((x), 1UL)
#define osi_atomic_dec(x) SUB_FROM((x), 1UL)
#define osi_atomic_dec_n(x, n) SUB_FROM((x), (n))

/* these return the old value of x in r */
#define osi_atomic_add_n_ro(x, n, r) ADD_TO_RETURNING_OLD((x), (n), (r))
#define osi_atomic_sub_n_ro(x, n, r) SUB_FROM_RETURNING_OLD((x), (n), (r))
#define osi_atomic_inc_ro(x, r) ADD_TO_RETURNING_OLD((x), 1UL, (r))
#define osi_atomic_dec_ro(x, r) SUB_FROM_RETURNING_OLD((x), 1UL, (r))

/* these return the new value of x in r */
#define osi_atomic_add_n_rn(x, n, r) ADD_TO_RETURNING_NEW((x), (n), (r))
#define osi_atomic_sub_n_rn(x, n, r) SUB_FROM_RETURNING_NEW((x), (n), (r))
#define osi_atomic_inc_rn(x, r) ADD_TO_RETURNING_NEW((x), 1UL, (r))
#define osi_atomic_dec_rn(x, r) SUB_FROM_RETURNING_NEW((x), 1UL, (r))

#else

#warning Compiling with FASPO and RMB() atomic inc/dec

#define osi_atomic_inc(x) \
do { \
	RMB(); \
	FASPO(&x, x+1); \
} while(0);

#define osi_atomic_dec(x) \
do { \
	RMB(); \
	FASPO(&x, x-1); \
} while(0);

#define osi_atomic_dec_n(x, n) \
do { \
	RMB(); \
	FASPO(&x, x-n); \
} while(0);

#endif /* old atomics */

#endif /* pthreads */

#endif /* OSI_MCAS_ATOMIC_H */
