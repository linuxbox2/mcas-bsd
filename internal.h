
/*#define MINIMAL_GC*/
/*#define YIELD_TO_HELP_PROGRESS*/
#define PROFILE_GC

/* Recycled nodes are filled with this value if WEAK_MEM_ORDER. */
#define INVALID_BYTE 0
#define INITIALISE_NODES(_p,_c) memset((_p), INVALID_BYTE, (_c));

/* Number of unique block sizes we can deal with. Equivalently, the
 * number of unique object caches which can be created. */
#define MAX_SIZES 60

#define MAX_HOOKS 4

/*
 * The initial number of allocation chunks for each per-blocksize list.
 * Popular allocation lists will steadily increase the allocation unit
 * in line with demand.
 */
#define ALLOC_CHUNKS_PER_LIST 10

/*
 * How many times should a thread call gc_enter(), seeing the same epoch
 * each time, before it makes a reclaim attempt?
 */
#define ENTRIES_PER_RECLAIM_ATTEMPT 100

/*
 *  0: current epoch -- threads are moving to this;
 * -1: some threads may still throw garbage into this epoch;
 * -2: no threads can see this epoch => we can zero garbage lists;
 * -3: all threads see zeros in these garbage lists => move to alloc lists.
 */
#ifdef WEAK_MEM_ORDER
#define NR_EPOCHS 4
#else
#define NR_EPOCHS 3
#endif

/*
 * A chunk amortises the cost of allocation from shared lists. It also
 * helps when zeroing nodes, as it increases per-cacheline pointer density
 * and means that node locations don't need to be brought into the cache
 * (most architectures have a non-temporal store instruction).
 */
#define BLKS_PER_CHUNK 100
typedef struct chunk_st chunk_t;
struct chunk_st
{
    chunk_t *next;             /* chunk chaining                 */
    unsigned int i;            /* the next entry in blk[] to use */
    void *blk[BLKS_PER_CHUNK];
};

struct gc_global_st
{
    CACHE_PAD(0);

    /* The current epoch. */
    VOLATILE unsigned int current;
    CACHE_PAD(1);

    /* Exclusive access to gc_reclaim(). */
    VOLATILE unsigned int inreclaim;
    CACHE_PAD(2);


    /* Allocator caches currently defined */
    long n_allocators;

    /*
     * RUN-TIME CONSTANTS (to first approximation)
     */

    /* Memory page size, in bytes. */
    unsigned int page_size;

    /* Node sizes (run-time constants). */
    int nr_sizes;
    int blk_sizes[MAX_SIZES];

    /* tags (trace support) */
    char *tags[MAX_SIZES];

    /* Registered epoch hooks. */
    int nr_hooks;
    hook_fn_t hook_fns[MAX_HOOKS];
    CACHE_PAD(3);

    /*
     * DATA WE MAY HIT HARD
     */

    /* Chain of free, empty chunks. */
    chunk_t * VOLATILE free_chunks;

    /* Main allocation lists. */
    chunk_t * VOLATILE alloc[MAX_SIZES];
    VOLATILE unsigned int alloc_size[MAX_SIZES];

    pthread_key_t ptst_key;
    ptst_t *ptst_list;

#ifdef NEED_ID
    static unsigned int next_id;
#endif
#ifdef PROFILE_GC
    VOLATILE unsigned int total_size;
    VOLATILE unsigned int allocations;
#endif
};

/* internal interator for ptst_list */
#define _ptst_first(gc_global)	(gc_global->ptst_list)

/* Per-thread state. */
struct gc_st
{
    /* Epoch that this thread sees. */
    unsigned int epoch;
    gc_global_t *global;

    /* Number of calls to gc_entry() since last gc_reclaim() attempt. */
    unsigned int entries_since_reclaim;

#ifdef YIELD_TO_HELP_PROGRESS
    /* Number of calls to gc_reclaim() since we last yielded. */
    unsigned int reclaim_attempts_since_yield;
#endif

    /* Used by gc_async_barrier(). */
    void *async_page;
    int   async_page_state;

    /* Garbage lists. */
    chunk_t *garbage[NR_EPOCHS][MAX_SIZES];
    chunk_t *garbage_tail[NR_EPOCHS][MAX_SIZES];
    chunk_t *chunk_cache;

    /* Local allocation lists. */
    chunk_t *alloc[MAX_SIZES];
    unsigned int alloc_chunks[MAX_SIZES];

    /* Hook pointer lists. */
    chunk_t *hook[NR_EPOCHS][MAX_HOOKS];
};
