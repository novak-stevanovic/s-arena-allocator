#ifndef _SARENA_H_
#define _SARENA_H_

#include <stddef.h>

/* -------------------------------------------------------------------------- */

/* SArena is a simple thread-safe arena allocator. It contains regions organized
 * into a region list. When the tail of the region list runs out of memory in
 * its internal memory pool, a new region is pushed back to the list.
 *
 * The SArena may also be rewinded, which will empty all of the allocated
 * regions (and their internal memory pools), allowing the user to
 * re-populate the once-used memory pools.
 *
 * This arena may perform poorly if 'region_cap' is too small relative to the
 * typical allocation size.
 *
 * It is important to note that each SArena object has its own lock,
 * assuring optimal performance in a multi-threaded environment. Thread-safety
 * is achieved using pthreads. */

struct sarena;
typedef struct sarena sarena;

typedef int sa_err;

#define SA_SUCCESS 0
#define SA_MALLOC_FAIL 2410
#define SA_INVALID_ARG 2411

/* -------------------------------------------------------------------------- */

/* Dynamically allocates memory for 'struct sarena' and initializes it. 
 * Dynamically allocates memory for the first memory region, initializes it
 * (which also means that it dynamically allocates memory for its memory pool)
 * and pushes it back to the region list.
 *
 * Each region inside the arena will be capable of holding at most
 * 'region_cap' bytes of data.
 *
 * Using an arena before initializing it is undefined behavior.
 *
 * Possible errors:
 * 1. SA_MALLOC_FAIL - if malloc() fails;
 * 2. SA_INVALID_ARG - if 'region_cap' is 0. 
 *
 * Return value:
 * ON SUCCESS: address of the newly-allocated SArena;
 * ON FAILURE: NULL. */

sarena* sarena_create(size_t region_cap, sa_err* out_err);

/* -------------------------------------------------------------------------- */

/* Destroys the arena. This will destroy every region inside the region list.
 * 'Destroying a region implies freeing the memory occupied by the region's
 * memory pool. This also frees the dynamically allocated memory to
 * store the SArena object itself. */

void sarena_destroy(sarena* arena);

/* -------------------------------------------------------------------------- */

/* This function allocates memory within the arena. It finds the currently active
 * region in the list and attempts to allocate the requested memory size within
 * its internal memory pool. 'Currently active region' refers to:
 *
 * 1) If 'rewind mode' is off, the tail of the region list.
 * If that region does not have enough space, a new region may be created and
 * appended to the end of the list.
 *
 * 2) If 'rewind mode' is on, the first region containing free space inside its
 * memory pool. If that region does not have enough space, the next region will
 * be considered. 
 *
 * Possible errors:
 * 1. SA_MALLOC_FAIL - if malloc() fails;
 * 2. SA_INVALID_ARG - if 'size' is 0 or greater than arena's 'region_cap'.
 *
 * Return value:
 * ON SUCCESS: address of the newly-allocated memory inside the arena
 * of 'size' bytes;
 * ON FAILURE: NULL. */

void* sarena_malloc(sarena* arena, size_t size, sa_err* out_err);

/* -------------------------------------------------------------------------- */

/* This function allocates a zero-initialized memory block of the given size 
 * from the SArena. It behaves similarly to sarena_malloc,
 * but ensures that the allocated memory is filled with zeros.

The return value and possible errors are the same as those for sarena_malloc. */

void* sarena_calloc(sarena* arena, size_t size, sa_err* out_err);

/* -------------------------------------------------------------------------- */

/* This function resets the arena by marking all allocated memory within
 * existing regions as available for reuse. It does not free any memory
 * but instead sets all regions' used capacity to zero. 
 * If multiple regions exist, the arena enters 'rewind mode' allowing
 * previously allocated regions to be reused in order. */

void sarena_rewind(sarena* arena);

/* -------------------------------------------------------------------------- */

/* This function deallocates all allocated regions except the first one.
 * This means that all memory occupied by those regions will be freed.
 * The first region will be reset, making its memory available for reuse.
 * After this call, the arena will be in the same state as immediately
 * after sarena_init(). */

void sarena_reset(sarena* arena);

/* -------------------------------------------------------------------------- */
/* IMPLEMENTATION */
/* -------------------------------------------------------------------------- */

#ifdef _SARENA_IMPLEMENTATION_

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define SA_RETURN(ret_val,out_param,out_val)                                   \
{                                                                              \
    if(out_param != NULL) *out_param = out_val;                                \
    return ret_val;                                                            \
}

#define SA_RETURN_VOID(out_param,out_val) SA_RETURN(, out_param, out_val)

#define SA_LOCK(arena) pthread_mutex_lock(&arena->_lock)
#define SA_UNLOCK(arena) pthread_mutex_unlock(&arena->_lock)

/* -------------------------------------------------------------------------- */

typedef struct region region;
typedef struct region_list region_list;

struct region
{
    size_t _used_cap;
    size_t _total_cap;
    char* _mem_pool;

    region* _next;
};

static region* _region_alloc(size_t total_cap);
static void _region_destroy(region* region);

/* -------------------------------------------------------------------------- */

struct region_list
{
    region* _head;
    region* _tail;

    size_t _count;
};

static void _region_list_init(region_list* list);
static sa_err _region_list_push_back(region_list* list, size_t total_cap);
static void _region_list_pop_front(region_list* list);

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

struct sarena
{
    region_list _regions;
    size_t _region_cap;

    region* _rewind_it;

    pthread_mutex_t _lock;
};

static region* _region_alloc(size_t total_cap)
{
    region* new = (region*)malloc(sizeof(region));

    if(new == NULL) return NULL;

    new->_next = NULL;
    new->_total_cap = 0;
    new->_used_cap = 0;

    new->_mem_pool = malloc(total_cap);

    if(new->_mem_pool == NULL)
    {
        free(new);
        return NULL;
    }

    new->_total_cap = total_cap;

    return new;
}

static void _region_destroy(region* region)
{
    region->_next = NULL;
    region->_total_cap = 0;
    region->_used_cap = 0;

    if(region->_mem_pool != NULL) 
        free(region->_mem_pool);
    region->_mem_pool = NULL;

    free(region);
}

/* -------------------------------------------------------------------------- */

static void _region_list_init(region_list* list)
{
    list->_count = 0;
    list->_head = NULL;
    list->_tail = NULL;
}

static sa_err _region_list_push_back(region_list* list, size_t total_cap)
{
    region* new = _region_alloc(total_cap);
    if(new == NULL) return SA_MALLOC_FAIL;

    if(list->_head == NULL)
    {
        list->_head = new;
        list->_tail = new;
    }
    else
    {
        list->_tail->_next = new;
        list->_tail = new;
    }

    list->_count++;

    return SA_SUCCESS;
}

static void _region_list_pop_front(region_list* list)
{
    if(list->_head == list->_tail)
    {
        _region_destroy(list->_head);
        list->_head = NULL;
        list->_tail = NULL;
    }
    else
    {
        region* old_head = list->_head;

        list->_head = list->_head->_next;

        _region_destroy(old_head);
    }

    list->_count--;
}

/* -------------------------------------------------------------------------- */

static void _sarena_init(sarena* arena, size_t region_cap, sa_err* out_err);
static void* _sarena_malloc(sarena* arena, size_t size, sa_err* out_err);

/* -------------------------------------------------------------------------- */

sarena* sarena_create(size_t region_cap, sa_err* out_err)
{
    sarena* new = (sarena*)malloc(sizeof(sarena));
    if(new == NULL)
        SA_RETURN(NULL, out_err, SA_MALLOC_FAIL);

    sa_err err;
    _sarena_init(new, region_cap, &err);

    sarena* ret_val;
    if(err != SA_SUCCESS)
    {
        free(new);
        ret_val = NULL;
    }
    else ret_val = new;

    SA_RETURN(ret_val, out_err, err);
}

void sarena_destroy(sarena* arena)
{
    while(arena->_regions._count > 0)
        _region_list_pop_front(&arena->_regions);

    arena->_region_cap = 0;
    arena->_rewind_it = NULL;
    pthread_mutex_destroy(&arena->_lock);
    free(arena);
}

void* sarena_malloc(sarena* arena, size_t size, sa_err* out_err)
{
    SA_LOCK(arena);

    sa_err status;
    void* alloc_addr = _sarena_malloc(arena, size, &status);

    SA_UNLOCK(arena);
    SA_RETURN(alloc_addr, out_err, status);
}

void* sarena_calloc(sarena* arena, size_t size, sa_err* out_err)
{
    SA_LOCK(arena);

    sa_err status;
    void* alloc_addr = _sarena_malloc(arena, size, &status);

    if(status == SA_SUCCESS)
        memset(alloc_addr, 0, size);

    SA_UNLOCK(arena);
    SA_RETURN(alloc_addr, out_err, status);
}

void sarena_rewind(sarena* arena)
{
    SA_LOCK(arena);

    if(arena->_regions._count == 0) 
    {
        SA_UNLOCK(arena);
        return;
    }

    region* it = arena->_regions._head;

    for(; it != NULL; it = it->_next)
        it->_used_cap = 0;

    // start rewinding if more regions exist
    if(arena->_regions._count > 1)
        arena->_rewind_it = arena->_regions._head;

    SA_UNLOCK(arena);
}

void sarena_reset(sarena* arena)
{
    SA_LOCK(arena);

    while(arena->_regions._count > 0)
        _region_list_pop_front(&arena->_regions);

    sa_err err;
    _sarena_init(arena, arena->_region_cap, &err);

    SA_UNLOCK(arena);
}

/* -------------------------------------------------------------------------- */

static void _sarena_init(sarena* arena, size_t region_cap, sa_err* out_err)
{
    if(region_cap == 0)
        SA_RETURN_VOID(out_err, SA_INVALID_ARG);

    arena->_region_cap = region_cap;
    arena->_rewind_it = NULL;
    _region_list_init(&arena->_regions);

    sa_err status = _region_list_push_back(&arena->_regions, region_cap);
    if(status == SA_SUCCESS)
    {
        pthread_mutex_init(&arena->_lock, NULL);
        SA_RETURN_VOID(out_err, SA_SUCCESS);
    }
    else if(status == SA_MALLOC_FAIL)
    {
        SA_RETURN_VOID(out_err, SA_MALLOC_FAIL);
    }
    else assert(0);
}

static void* _sarena_malloc(sarena* arena, size_t size, sa_err* out_err)
{
    if((size > arena->_region_cap) || (size == 0))
        SA_RETURN(NULL, out_err, SA_INVALID_ARG);

    region* curr_region = (arena->_rewind_it == NULL) ?
        arena->_regions._tail : arena->_rewind_it;

    size_t curr_region_cap = curr_region->_total_cap - curr_region->_used_cap;

    if(size > curr_region_cap) // not enough memory in current region
    {
        if(arena->_rewind_it == NULL) // if not rewinding, push back a region
        {
            sa_err err = _region_list_push_back(&arena->_regions, arena->_region_cap);
            if(err != SA_SUCCESS)
                SA_RETURN(NULL, out_err, SA_MALLOC_FAIL);
        }
        else // if rewinding, advance rewind iterator
        {
            arena->_rewind_it = arena->_rewind_it->_next;

            // if at the tail, turn of rewinding
            if(arena->_rewind_it == arena->_regions._tail)
                arena->_rewind_it = NULL;
        }

        // advance the curr_region ptr after allocing region/advancing rewind
        curr_region = curr_region->_next;
    }

    void* alloc_addr = curr_region->_mem_pool + curr_region->_used_cap;
    curr_region->_used_cap += size;

    SA_RETURN(alloc_addr, out_err, SA_SUCCESS);
}

#endif // _SARENA_IMPLEMENTATION_

#endif // _SARENA_H_
