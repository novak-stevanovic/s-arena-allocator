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
 * THE SOFTWARE. */

/* ========================================================================== */
/* -------------------------------------------------------------------------- */
/* DEFINE */
/* -------------------------------------------------------------------------- */
/* ========================================================================== */

#ifndef SARENA_H
#define SARENA_H

#include <stddef.h>

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
#error "C99 or newer is required"
#endif /* C99 check */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#include <stdalign.h>
#define SA__MAX_ALIGN alignof(max_align_t)
#else
struct sa__align_helper_struct
{
    char c;
    union
    {
        long double ld;
        void* p;
        long long ll;
    } align;
};
#define SA__MAX_ALIGN offsetof(struct sa__align_helper_struct, align)
#endif // defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)

/* ========================================================================== */
/* -------------------------------------------------------------------------- */
/* PUBLIC */
/* -------------------------------------------------------------------------- */
/* ========================================================================== */

/* ========================================================================== */

/* SArena is a simple arena allocator that allocates memory from fixed-capacity
 * regions. A new region is appended when the current region runs out of space.
 *
 * Each region can hold at most `region_cap` bytes, so a single allocation
 * cannot exceed that size.
 *
 * `sarena_rewind()` makes memory in all existing regions available for reuse.
 * `sarena_reset()` additionally frees all regions except the first one.
 *
 * SArena is not thread-safe. */

struct sarena;
typedef struct sarena sarena;

/* ========================================================================== */

/* Creates an arena whose regions can each hold `region_cap` bytes.
 *
 * RETURN VALUE: Newly allocated arena on success, NULL if `region_cap` is 0
 * or memory allocation fails. */

sarena* sarena_create(size_t region_cap);

/* ========================================================================== */

/* Destroys the arena and frees all memory owned by it.
 *
 * If `arena` is NULL, this function has no effect. */

void sarena_destroy(sarena* arena);

/* ========================================================================== */

/* Allocates `size` bytes from the arena.
 *
 * RETURN VALUE: Allocated memory on success, NULL if `arena` is NULL, `size`
 * is 0, `size` exceeds the region capacity, or memory allocation fails. */

void* sarena_malloc(sarena* arena, size_t size);

/* ========================================================================== */

/* Allocates `size` zero-initialized bytes from the arena.
 *
 * RETURN VALUE: Allocated memory on success, NULL if `arena` is NULL, `size`
 * is 0, `size` exceeds the region capacity, or memory allocation fails. */

void* sarena_calloc(sarena* arena, size_t size);

/* ========================================================================== */

/* Makes all memory in existing regions available for reuse without freeing
 * the regions.
 *
 * If `arena` is NULL, this function has no effect. */

void sarena_rewind(sarena* arena);

/* ========================================================================== */

/* Frees all regions except the first and makes the first region available for
 * reuse.
 *
 * If `arena` is NULL, this function has no effect. */

void sarena_reset(sarena* arena);

/* ========================================================================== */

#endif // _SARENA_H_

/* ========================================================================== */
/* -------------------------------------------------------------------------- */
/* INTERNAL */
/* -------------------------------------------------------------------------- */
/* ========================================================================== */

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

static sa_region* sa__region_alloc(size_t total_cap);
static void sa__region_destroy(sa_region* region);

/* -------------------------------------------------------------------------- */

struct sa_region_list
{
    sa_region* _head;
    sa_region* _tail;

    size_t _count;
};

static void sa__region_list_init(sa_region_list* list);
static int sa__region_list_push_back(sa_region_list* list, size_t total_cap);
static void sa__region_list_pop_front(sa_region_list* list);

/* ========================================================================== */

struct sarena
{
    sa_region_list _regions;
    size_t _region_cap;

    sa_region* _rewind_it;
};

static sa_region* sa__region_alloc(size_t total_cap)
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

static void sa__region_destroy(sa_region* region)
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

static void sa__region_list_init(sa_region_list* list)
{
    list->_count = 0;
    list->_head = NULL;
    list->_tail = NULL;
}

static int sa__region_list_push_back(sa_region_list* list, size_t total_cap)
{
    sa_region* new = sa__region_alloc(total_cap);
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

static void sa__region_list_pop_front(sa_region_list* list)
{
    if(list->_head == list->_tail)
    {
        sa__region_destroy(list->_head);
        list->_head = NULL;
        list->_tail = NULL;
    }
    else
    {
        sa_region* old_head = list->_head;

        list->_head = list->_head->_next;

        sa__region_destroy(old_head);
    }

    list->_count--;
}

/* -------------------------------------------------------------------------- */

static int sarena__init(sarena* arena, size_t region_cap);
static void* sarena__malloc(sarena* arena, size_t size);

/* -------------------------------------------------------------------------- */

sarena* sarena_create(size_t region_cap)
{
    sarena* new = (sarena*)malloc(sizeof(sarena));
    if(new == NULL) return NULL;

    int status = sarena__init(new, region_cap);

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
        sa__region_list_pop_front(&arena->_regions);

    arena->_region_cap = 0;
    arena->_rewind_it = NULL;
    free(arena);
}

void* sarena_malloc(sarena* arena, size_t size)
{
    if(arena == NULL) return NULL;

    void* alloc_addr = sarena__malloc(arena, size);

    return alloc_addr;
}

void* sarena_calloc(sarena* arena, size_t size)
{
    if(arena == NULL) return NULL;

    void* alloc_addr = sarena__malloc(arena, size);

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
        sa__region_list_pop_front(&arena->_regions);

    arena->_regions._head->_used_cap = 0;
    arena->_rewind_it = NULL;
}

/* -------------------------------------------------------------------------- */

static int sarena__init(sarena* arena, size_t region_cap)
{
    if(region_cap == 0) return 2;

    arena->_region_cap = region_cap;
    arena->_rewind_it = NULL;
    sa__region_list_init(&arena->_regions);

    int status = sa__region_list_push_back(&arena->_regions, region_cap);
    if(status == 0) return 0;
    else return 1;
}

static void* sarena__malloc(sarena* arena, size_t size)
{
    if((size == 0) || (size > arena->_region_cap)) return NULL;

    sa_region* curr_region = (arena->_rewind_it == NULL) ?
        arena->_regions._tail : arena->_rewind_it;

    size_t curr_region_cap = 
        (curr_region->_total_cap > curr_region->_used_cap) ?
        curr_region->_total_cap - curr_region->_used_cap :
        0;

    if(size > curr_region_cap) // not enough memory in current region
    {
        if(arena->_rewind_it == NULL) // if not rewinding, push back a region
        {
            int status = sa__region_list_push_back(&arena->_regions, arena->_region_cap);
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

    /* Account for allocated, alignment */

    size_t remaining = curr_region->_total_cap - curr_region->_used_cap;

    size_t extra = size % SA__MAX_ALIGN;
    size_t padding = (SA__MAX_ALIGN - extra) % SA__MAX_ALIGN;

    if(padding <= remaining - size)
        curr_region->_used_cap += padding + size;
    else
        curr_region->_used_cap = curr_region->_total_cap;

    return alloc_addr;
}

/* -------------------------------------------------------------------------- */

#endif // SARENA_IMPLEMENTATION
