/* MIT License
 *
 * Copyright (c) 2025 Novak Stevanović
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights  
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell  
 * copies of the Software, and to permit persons to whom the Software is  
 * furnished to do so, subject to the following conditions:  
 * 
 * The above copyright notice and this permission notice shall be included in all  
 * copies or substantial portions of the Software.  
 * 
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER  
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN  
 * THE SOFTWARE. 
 */

/* -------------------------------------------------------------------------- */
/* START */
/* -------------------------------------------------------------------------- */

#ifndef SARENA_H
#define SARENA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */

/* SArena is a simple thread-safe arena allocator. It contains regions organized
 * into a region list. When the tail of the region list runs out of memory in
 * its internal memory pool, a new region is pushed back to the list.
 *
 * The SArena may also be rewinded, which will empty all of the allocated
 * regions (and their internal memory pools), allowing the user to
 * re-populate the once-used memory pools. It may also be reset, which caueses
 * all regions to be freed except the first one.
 *
 * This arena may perform poorly if 'region_cap' is too small relative to the
 * typical allocation size.
 *
 * It is important to note that each SArena object has its own mutex lock,
 * which ensures thread-safety. */

struct sarena;
typedef struct sarena sarena;

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
 * Return value:
 * ON SUCCESS: address of the newly-allocated SArena;
 * ON FAILURE: NULL. This can occur if the malloc for the first region fails
 * or if 'region cap` is 0. */

sarena* sarena_create(size_t region_cap);

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
 * Return value:
 * ON SUCCESS: address of the newly-allocated memory inside the arena
 * of 'size' bytes. If 'sizxe' is 0, NULL is returned;
 * ON FAILURE: NULL. This can occur if the arena had to allocate a new region
 * of memory via malloc(), and the allocation failed. */

void* sarena_malloc(sarena* arena, size_t size);

/* -------------------------------------------------------------------------- */

/* This function allocates a zero-initialized memory block of the given size 
 * from the SArena. It behaves similarly to sarena_malloc, but ensures that
 * the allocated memory is filled with zeros.

The return value and possible errors are the same as those for sarena_malloc. */

void* sarena_calloc(sarena* arena, size_t size);

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

#ifdef __cplusplus
}
#endif

#endif // _SARENA_H_

/* -------------------------------------------------------------------------- */
/* IMPLEMENTATION */
/* -------------------------------------------------------------------------- */

#ifdef SARENA_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------- */

typedef struct sa_region sa_region;
typedef struct sa_region_list sa_region_list;

struct sa_region
{
    size_t _used_cap;
    size_t _total_cap;
    char* _mem_pool;

    sa_region* _next;
};

static sa_region* _sa_region_alloc(size_t total_cap);
static void _sa_region_destroy(sa_region* region);

/* -------------------------------------------------------------------------- */

struct sa_region_list
{
    sa_region* _head;
    sa_region* _tail;

    size_t _count;
};

static void _sa_region_list_init(sa_region_list* list);
static int _sa_region_list_push_back(sa_region_list* list, size_t total_cap);
static void _sa_region_list_pop_front(sa_region_list* list);

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

struct sarena
{
    sa_region_list _regions;
    size_t _region_cap;

    sa_region* _rewind_it;
};

static sa_region* _sa_region_alloc(size_t total_cap)
{
    sa_region* new_region = (sa_region*)malloc(sizeof(sa_region));

    if(new_region == NULL) return NULL;

    new_region->_next = NULL;
    new_region->_total_cap = 0;
    new_region->_used_cap = 0;

    new_region->_mem_pool = malloc(total_cap);

    if(new_region->_mem_pool == NULL)
    {
        free(new_region);
        return NULL;
    }

    new_region->_total_cap = total_cap;

    return new_region;
}

static void _sa_region_destroy(sa_region* region)
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

static void _sa_region_list_init(sa_region_list* list)
{
    list->_count = 0;
    list->_head = NULL;
    list->_tail = NULL;
}

static int _sa_region_list_push_back(sa_region_list* list, size_t total_cap)
{
    sa_region* new = _sa_region_alloc(total_cap);
    if(new == NULL) return 1;

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

    return 0;
}

static void _sa_region_list_pop_front(sa_region_list* list)
{
    if(list->_head == list->_tail)
    {
        _sa_region_destroy(list->_head);
        list->_head = NULL;
        list->_tail = NULL;
    }
    else
    {
        sa_region* old_head = list->_head;

        list->_head = list->_head->_next;

        _sa_region_destroy(old_head);
    }

    list->_count--;
}

/* -------------------------------------------------------------------------- */

static int _sarena_init(sarena* arena, size_t region_cap);
static void* _sarena_malloc(sarena* arena, size_t size);

/* -------------------------------------------------------------------------- */

sarena* sarena_create(size_t region_cap)
{
    sarena* new = (sarena*)malloc(sizeof(sarena));
    if(new == NULL) return NULL;

    int status = _sarena_init(new, region_cap);

    if(status != 0)
    {
        free(new);
        return NULL;
    }
    else return new;
}

void sarena_destroy(sarena* arena)
{
    if(arena == NULL) return;

    while(arena->_regions._count > 0)
        _sa_region_list_pop_front(&arena->_regions);

    arena->_region_cap = 0;
    arena->_rewind_it = NULL;
    free(arena);
}

void* sarena_malloc(sarena* arena, size_t size)
{
    if(arena == NULL) return NULL;

    void* alloc_addr = _sarena_malloc(arena, size);

    return alloc_addr;
}

void* sarena_calloc(sarena* arena, size_t size)
{
    if(arena == NULL) return NULL;

    void* alloc_addr = _sarena_malloc(arena, size);

    if(alloc_addr != NULL)
        memset(alloc_addr, 0, size);

    return alloc_addr;
}

void sarena_rewind(sarena* arena)
{
    if(arena == NULL) return;

    if(arena->_regions._count == 0) 
        return;

    sa_region* it = arena->_regions._head;

    for(; it != NULL; it = it->_next)
        it->_used_cap = 0;

    // start rewinding if more regions exist
    if(arena->_regions._count > 1)
        arena->_rewind_it = arena->_regions._head;
}

void sarena_reset(sarena* arena)
{
    if(arena == NULL) return;

    while(arena->_regions._count > 1)
        _sa_region_list_pop_front(&arena->_regions);

    arena->_regions._head->_used_cap = 0;
    arena->_rewind_it = NULL;
}

/* -------------------------------------------------------------------------- */

static int _sarena_init(sarena* arena, size_t region_cap)
{
    if(region_cap == 0) return 2;

    arena->_region_cap = region_cap;
    arena->_rewind_it = NULL;
    _sa_region_list_init(&arena->_regions);

    int status = _sa_region_list_push_back(&arena->_regions, region_cap);
    if(status == 0) return 0;
    else return 1;
}

static void* _sarena_malloc(sarena* arena, size_t size)
{
    if((size > arena->_region_cap) || (size == 0))
        return NULL;

    sa_region* curr_region = (arena->_rewind_it == NULL) ?
        arena->_regions._tail : arena->_rewind_it;

    size_t curr_region_cap = curr_region->_total_cap - curr_region->_used_cap;

    if(size > curr_region_cap) // not enough memory in current region
    {
        if(arena->_rewind_it == NULL) // if not rewinding, push back a region
        {
            int status = _sa_region_list_push_back(&arena->_regions, arena->_region_cap);
            if(status != 0)
                return NULL;
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

    return alloc_addr;
}

#endif // SARENA_IMPLEMENTATION
