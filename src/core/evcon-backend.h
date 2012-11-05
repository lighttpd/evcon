#ifndef __EVCON_EVCON_BACKEND_H
#define __EVCON_EVCON_BACKEND_H __EVCON_EVCON_BACKEND_H

#include <evcon.h>

typedef void (*evcon_backend_free_loop_cb)(evcon_loop *loop, void *loop_data, void *backend_data);

/* fd == -1: delete watcher */
typedef void (*evcon_backend_fd_update_cb)(evcon_fd_watcher *watcher, evcon_fd fd, int events, evcon_allocator *allocator, void *loop_data, void *watcher_data);

/* special timeout values:
 *   0: idle watcher (for background jobs)
 *  -1: disable temporarily
 *  -2: delete watcher
 * gets called after *each* timer event to set a new timeout value
 */
typedef void (*evcon_backend_timer_update_cb)(evcon_timer_watcher *watcher, evcon_interval timeout, evcon_allocator *allocator, void *loop_data, void *watcher_data);

typedef enum {
	EVCON_ASYNC_TRIGGER = 0, /* <- trigger be thread safe */
	EVCON_ASYNC_NEW     = 1,
	EVCON_ASYNC_FREE    = 2
} evcon_async_func;

typedef void (*evcon_backend_async_update_cb)(evcon_async_watcher *watcher, evcon_async_func f, evcon_allocator *allocator, void *loop_data, void *watcher_data);

evcon_backend* evcon_backend_new(void *backend_data,
                                 evcon_allocator *allocator,
                                 evcon_backend_free_loop_cb free_loop_cb,
                                 evcon_backend_fd_update_cb fd_update_cb,
                                 evcon_backend_timer_update_cb timer_update_cb,
                                 evcon_backend_async_update_cb async_udpate_cb);
void evcon_backend_free(evcon_backend *backend);

#define EVCON_BACKEND_RECOMMENDED_SIZE (8*sizeof(void*))
/* if memsize is large enough to contain a backend, initialize it and returns @mem. otherwise alloc a new block */
evcon_backend* evcon_backend_init(char *mem, size_t memsize,
                                  void *backend_data,
                                  evcon_allocator *allocator,
                                  evcon_backend_free_loop_cb free_loop_cb,
                                  evcon_backend_fd_update_cb fd_update_cb,
                                  evcon_backend_timer_update_cb timer_update_cb,
                                  evcon_backend_async_update_cb async_udpate_cb);

evcon_loop* evcon_loop_new(evcon_backend *backend, evcon_allocator *allocator);

void* evcon_backend_get_data(evcon_backend *backend);
void* evcon_loop_get_backend_data(evcon_loop *loop);
void* evcon_fd_get_backend_data(evcon_fd_watcher *watcher);
void* evcon_timer_get_backend_data(evcon_timer_watcher *watcher);
void* evcon_async_get_backend_data(evcon_async_watcher *watcher);

void evcon_backend_set_data(evcon_backend *backend, void *data);
void evcon_loop_set_backend_data(evcon_loop *loop, void *data);
void evcon_fd_set_backend_data(evcon_fd_watcher *watcher, void *data);
void evcon_timer_set_backend_data(evcon_timer_watcher *watcher, void *data);
void evcon_async_set_backend_data(evcon_async_watcher *watcher, void *data);


void evcon_feed_fd(evcon_fd_watcher *watcher, int events);
void evcon_feed_timer(evcon_timer_watcher *watcher);
void evcon_feed_async(evcon_async_watcher *watcher);

#endif
