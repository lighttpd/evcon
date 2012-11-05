#ifndef __EVCON_EVCON_ALLOCATOR_H
#define __EVCON_EVCON_ALLOCATOR_H __EVCON_EVCON_ALLOCATOR_H

#include <evcon.h>

/* Slab allocator */

/*
 * public interface
 */

void* evcon_alloc(evcon_allocator* allocator, size_t size);
void* evcon_alloc0(evcon_allocator* allocator, size_t size);
void evcon_free(evcon_allocator* allocator, void* ptr, size_t size);

/*
 * implementation interface
 */

typedef void* (*evcon_alloc_cb)(size_t size, void* user_data);
typedef void (*evcon_free_cb)(void *ptr, size_t size, void *user_data);

evcon_allocator* evcon_allocator_new(void* user_data, evcon_alloc_cb alloc_cb, evcon_free_cb free_cb);
void evcon_allocator_free(evcon_allocator* allocator); /* freeing the allocator should be the last thing your app does */

#define EVCON_ALLOCATOR_RECOMMENDED_SIZE (4*sizeof(void*))
/* if memsize is large enough to contain a backend, initialize it and returns mem. otherwise allocates a new block */
evcon_allocator* evcon_allocator_init(char *mem, size_t memsize,
                                      void *user_data, evcon_alloc_cb alloc_cb, evcon_free_cb free_cb);

/* allows to change user_data */
void* evcon_allocator_get_data(evcon_allocator *allocator);
void evcon_allocator_set_data(evcon_allocator *allocator, void *user_data);

#endif
