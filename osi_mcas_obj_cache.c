#if !defined(SUBSYS_LOG_MACRO)
#define SUBSYS_LOG_MACRO
#else
#include <afsconfig.h>
#include <afs/param.h>
#include <afs/afsutil.h>
#include <osi/osi_includes.h>
#include <osi/osi_types.h>
#endif
#include "osi_mcas_obj_cache.h"
#include "internal.h"
#include "random.h"
#include "ptst.h"

void
osi_mcas_obj_cache_create(gc_global_t *gc_global,
	osi_mcas_obj_cache_t * gc_id, size_t size,
	const char *tag)
{
    SUBSYS_LOG_MACRO(7,
	    ("osi_mcas_obj_cache_create: size, adjsize %d\n", size,
	     size + sizeof(int *)));

    *(int *)gc_id = gc_add_allocator(gc_global, size + sizeof(int *), tag);
}

void *
osi_mcas_obj_cache_alloc_critical(ptst_t *ptst, osi_mcas_obj_cache_t gc_id)
{
    gc_global_t *gc_global = ptst->gc->global;
    void *obj;

    obj = (void *)gc_alloc(ptst, gc_id);

    SUBSYS_LOG_MACRO(11,
			("GC: osi_mcas_obj_cache_alloc: block of size %d "
			 "%p (%s)\n",
			 gc_get_blocksize(gc_global, gc_id),
			 obj,
			 gc_get_tag(gc_global, gc_id)));

    return (obj);
}

void *
osi_mcas_obj_cache_alloc(gc_global_t *gc_global, osi_mcas_obj_cache_t gc_id)
{
    ptst_t *ptst;
    void *obj;

    ptst = critical_enter(gc_global);
    obj = osi_mcas_obj_cache_alloc_critical(ptst, gc_id);
    obj = (void *)gc_alloc(ptst, gc_id);
    critical_exit(ptst);

    return (obj);
}

void
osi_mcas_obj_cache_free_critical(ptst_t *ptst, osi_mcas_obj_cache_t gc_id, void *obj)
{
    gc_global_t *gc_global = ptst->gc->global;

    SUBSYS_LOG_MACRO(11,
			("GC: osi_mcas_obj_cache_free: block of size %d "
			 "%p (%s)\n",
			 gc_get_blocksize(gc_global, gc_id),
			 obj,
			 gc_get_tag(gc_global, gc_id)));

    gc_free(ptst, (void *)obj, gc_id);
}

void
osi_mcas_obj_cache_free(gc_global_t *gc_global, osi_mcas_obj_cache_t gc_id, void *obj)
{
    ptst_t *ptst;

    ptst = critical_enter(gc_global);
    osi_mcas_obj_cache_free_critical(ptst, gc_id, obj);
    critical_exit(ptst);
}

void
osi_mcas_obj_cache_destroy(osi_mcas_obj_cache_t gc_id)
{
    /* TODO:  implement, will need to implement gc_remove_allocator and related
     * modifications in gc.c */
    return;
}
