#include "sarena.h"

#include <pthread.h>
#include <assert.h>

#define SA_RETURN(ret_val,out_param,out_val)                                   \
{                                                                              \
    if(out_param != NULL) *out_param = out_val;                                \
    return ret_val;                                                            \
}

#define SA_RETURN_VOID(out_param,out_val) SA_RETURN(, out_param, out_val)

#define SA_LOCK(arena) pthread_mutex_lock(&arena->_lock)
#define SA_UNLOCK(arena) pthread_mutex_unlock(&arena->_lock)

/* -------------------------------------------------------------------------- */

typedef struct Region Region;
typedef struct RegionList RegionList;

struct Region
{
    size_t _used_cap;
    size_t _total_cap;
    char* _mem_pool;

    Region* _next;
};

static Region* _region_alloc(size_t total_cap);
static void _region_destroy(Region* region);

/* -------------------------------------------------------------------------- */

struct RegionList
{
    Region* _head;
    Region* _tail;

    size_t _count;
};

static void _region_list_init(RegionList* list);
static sa_err _region_list_push_back(RegionList* list, size_t total_cap);
static void _region_list_pop_front(RegionList* list);

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

struct SArena
{
    RegionList _regions;
    size_t _region_cap;

    Region* _rewind_it;

    pthread_mutex_t _lock;
};

static Region* _region_alloc(size_t total_cap)
{
    Region* new = (Region*)malloc(sizeof(Region));

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

static void _region_destroy(Region* region)
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

static void _region_list_init(RegionList* list)
{
    list->_count = 0;
    list->_head = NULL;
    list->_tail = NULL;
}

static sa_err _region_list_push_back(RegionList* list, size_t total_cap)
{
    Region* new = _region_alloc(total_cap);
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

static void _region_list_pop_front(RegionList* list)
{
    if(list->_head == list->_tail)
    {
        _region_destroy(list->_head);
        list->_head = NULL;
        list->_tail = NULL;
    }
    else
    {
        Region* old_head = list->_head;

        list->_head = list->_head->_next;

        _region_destroy(old_head);
    }

    list->_count--;
}

/* -------------------------------------------------------------------------- */

SArena* sarena_create(size_t region_cap, sa_err* out_err)
{
    if(region_cap == 0)
        SA_RETURN(NULL, out_err, SA_INVALID_ARG);

    SArena* new = (SArena*)malloc(sizeof(SArena));
    if(new == NULL)
        SA_RETURN(NULL, out_err, SA_MALLOC_FAIL);

    new->_region_cap = region_cap;
    new->_rewind_it = NULL;
    _region_list_init(&new->_regions);

    sa_err status = _region_list_push_back(&new->_regions, region_cap);
    if(status == SA_SUCCESS)
    {
        pthread_mutex_init(&new->_lock, NULL);
        SA_RETURN(new, out_err, SA_SUCCESS);
    }
    else if(status == SA_MALLOC_FAIL)
    {
        free(new);
        SA_RETURN(NULL, out_err, SA_MALLOC_FAIL);
    }
    else assert(0);
}

void sarena_destroy(SArena* arena)
{
    while(arena->_regions._count > 0)
        _region_list_pop_front(&arena->_regions);

    arena->_region_cap = 0;
    arena->_rewind_it = NULL;
    pthread_mutex_destroy(&arena->_lock);
    free(arena);
}

void* sarena_alloc(SArena* arena, size_t size, sa_err* out_err)
{
    SA_LOCK(arena);

    if((size > arena->_region_cap) || (size == 0))
    {
        SA_UNLOCK(arena);
        SA_RETURN(NULL, out_err, SA_INVALID_ARG);
    }

    Region* curr_region = (arena->_rewind_it == NULL) ?
        arena->_regions._tail : arena->_rewind_it;

    size_t curr_region_cap = curr_region->_total_cap - curr_region->_used_cap;

    if(size > curr_region_cap) // not enough memory in current region
    {
        if(arena->_rewind_it == NULL) // if not rewinding, push back a region
        {
            sa_err err = _region_list_push_back(&arena->_regions, arena->_region_cap);
            if(err != SA_SUCCESS)
            {
                SA_UNLOCK(arena);
                SA_RETURN(NULL, out_err, SA_MALLOC_FAIL);
            }
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

    SA_UNLOCK(arena);
    SA_RETURN(alloc_addr, out_err, SA_SUCCESS);
}

void sarena_rewind(SArena* arena)
{
    SA_LOCK(arena);

    if(arena->_regions._count == 0) 
    {
        SA_UNLOCK(arena);
        return;
    }

    Region* it = arena->_regions._head;

    for(; it != NULL; it = it->_next)
        it->_used_cap = 0;

    // start rewinding if more regions exist
    if(arena->_regions._count > 1)
        arena->_rewind_it = arena->_regions._head;

    SA_UNLOCK(arena);
}
