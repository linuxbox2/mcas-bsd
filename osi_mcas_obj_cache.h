
#ifndef __OSI_MCAS_OBJ_CACHE_H
#define __OSI_MCAS_OBJ_CACHE_H

#include "portable_defns.h"
#include "ptst.h"
#include "gc.h"

typedef int osi_mcas_obj_cache_t;

/* Create a new MCAS GC pool, and return its identifier, which
 * follows future calls */
void osi_mcas_obj_cache_create(gc_global_t *, osi_mcas_obj_cache_t *,
	size_t size, char *tag);	/* alignment? */

/* Allocate an object from the pool identified by
 * gc_id */
void *osi_mcas_obj_cache_alloc(gc_global_t *, osi_mcas_obj_cache_t);

/* Release object obj to its GC pool, identified by
 * gc_id */
void osi_mcas_obj_cache_free(gc_global_t *, osi_mcas_obj_cache_t, void *);

/* Terminate an MCAS GC pool */
void osi_mcas_obj_cache_destroy(osi_mcas_obj_cache_t gc_id);

#endif /* __OSI_MCAS_OBJ_CACHE_H */
