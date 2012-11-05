
#include <evcon.h>
#include <evcon-backend.h>
#include <evcon-allocator.h>

#include <evcon-config-private.h>

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#define EVCON_STR_LEN(s) (s), (sizeof(s)-1)

struct evcon_allocator {
	void* user_data;
	evcon_alloc_cb alloc_cb;
	evcon_free_cb free_cb;
};

struct evcon_backend {
	void *backend_data;
	evcon_allocator *allocator;
	evcon_backend_free_loop_cb free_loop_cb;
	evcon_backend_fd_update_cb fd_update_cb;
	evcon_backend_timer_update_cb timer_update_cb;
	evcon_backend_async_update_cb async_update_cb;
};

struct evcon_loop {
	unsigned int refcount;
	void *backend_data;
	evcon_backend *backend;
	evcon_allocator *allocator;
};

struct evcon_fd_watcher {
	void *user_data;
	void *backend_data;
	unsigned int active:1, incallback:1, delayed_delete:1;
	evcon_loop *loop;
	evcon_fd_cb cb;
	evcon_fd fd;
	int events;
};

struct evcon_timer_watcher {
	void *user_data;
	void *backend_data;
	unsigned int active:1, incallback:1, delayed_delete:1;
	evcon_loop *loop;
	evcon_timer_cb cb;
	evcon_interval timeout, repeat;
};

struct evcon_async_watcher {
	void *user_data;
	void *backend_data;
	unsigned int incallback:1, delayed_delete:1;
	evcon_loop *loop;
	evcon_async_cb cb;
};

/*****************************************************
 *             Allocator                             *
 *****************************************************/

void* evcon_alloc(evcon_allocator* allocator, size_t size) {
	void *ptr;
	if (NULL == allocator) {
		ptr = malloc(size);
	} else {
		ptr = allocator->alloc_cb(size, allocator->user_data);
	}
	if (NULL == ptr) {
		write(2, EVCON_STR_LEN("evcon_alloc: failed to allocate"));
		abort();
	}
	return ptr;
}

void* evcon_alloc0(evcon_allocator* allocator, size_t size) {
	void *ptr = evcon_alloc(allocator, size);
	memset(ptr, 0, size);
	return ptr;
}

void evcon_free(evcon_allocator* allocator, void* ptr, size_t size) {
	if (NULL == ptr) return;
	if (NULL == allocator) {
		free(ptr);
	} else {
		allocator->free_cb(ptr, size, allocator->user_data);
	}
}

evcon_allocator* evcon_allocator_new(void* user_data, evcon_alloc_cb alloc_cb, evcon_free_cb free_cb) {
	evcon_allocator* allocator;

	assert(NULL != alloc_cb);
	assert(NULL != free_cb);

	allocator = alloc_cb(sizeof(evcon_allocator), user_data);
	allocator->user_data = user_data;
	allocator->alloc_cb = alloc_cb;
	allocator->free_cb = free_cb;

	return allocator;
}

void evcon_allocator_free(evcon_allocator* allocator) {
	evcon_free_cb free_cb;
	void* user_data;

	if (NULL == allocator) return;

	free_cb = allocator->free_cb;
	user_data = allocator->user_data;

	memset(allocator, 0, sizeof(evcon_allocator));

	free_cb(allocator, sizeof(evcon_allocator), user_data);
}

evcon_allocator* evcon_allocator_init(char *mem, size_t memsize,
                                      void *user_data, evcon_alloc_cb alloc_cb, evcon_free_cb free_cb) {
	evcon_allocator *allocator = (evcon_allocator*) mem;
	if (sizeof(evcon_allocator) > memsize) {
		return evcon_allocator_new(user_data, alloc_cb, free_cb);
	} else {
		allocator->user_data = user_data;
		allocator->alloc_cb = alloc_cb;
		allocator->free_cb = free_cb;
		return allocator;
	}
}

void* evcon_allocator_get_data(evcon_allocator *allocator) {
	return allocator->user_data;
}

void evcon_allocator_set_data(evcon_allocator *allocator, void *user_data) {
	allocator->user_data = user_data;
}

/*****************************************************
 *             Backend                               *
 *****************************************************/

evcon_backend* evcon_backend_new(void *backend_data,
                                 evcon_allocator *allocator,
                                 evcon_backend_free_loop_cb free_loop_cb,
                                 evcon_backend_fd_update_cb fd_update_cb,
                                 evcon_backend_timer_update_cb timer_update_cb,
                                 evcon_backend_async_update_cb async_update_cb) {
	evcon_backend *backend = evcon_alloc0(allocator, sizeof(evcon_backend));

	backend->backend_data = backend_data;
	backend->allocator = allocator;
	backend->free_loop_cb = free_loop_cb;
	backend->fd_update_cb = fd_update_cb;
	backend->timer_update_cb = timer_update_cb;
	backend->async_update_cb = async_update_cb;

	return backend;
}

void evcon_backend_free(evcon_backend *backend) {
	evcon_allocator *allocator;
	if (NULL == backend) return;

	allocator = backend->allocator;
	memset(backend, 0, sizeof(evcon_backend));
	evcon_free(allocator, backend, sizeof(evcon_backend));
}

evcon_backend* evcon_backend_init(char *mem, size_t memsize,
                                  void *backend_data,
                                  evcon_allocator *allocator,
                                  evcon_backend_free_loop_cb free_loop_cb,
                                  evcon_backend_fd_update_cb fd_update_cb,
                                  evcon_backend_timer_update_cb timer_update_cb,
                                  evcon_backend_async_update_cb async_update_cb) {
	evcon_backend *backend = (evcon_backend*) mem;
	if (sizeof(evcon_backend) > memsize) {
		return evcon_backend_new(backend_data, allocator, free_loop_cb, fd_update_cb, timer_update_cb, async_update_cb);
	} else {
		backend->backend_data = backend_data;
		backend->allocator = allocator;
		backend->free_loop_cb = free_loop_cb;
		backend->fd_update_cb = fd_update_cb;
		backend->timer_update_cb = timer_update_cb;
		backend->async_update_cb = async_update_cb;
	}

	return backend;
}

evcon_loop* evcon_loop_new(evcon_backend *backend, evcon_allocator *allocator) {
	evcon_loop *loop;

	if (NULL == allocator) allocator = backend->allocator;

	loop = evcon_alloc0(allocator, sizeof(evcon_loop));
	loop->refcount = 1;
	loop->allocator = allocator;
	loop->backend = backend;

	return loop;
}

void* evcon_backend_get_data(evcon_backend *backend) {
	return backend->backend_data;
}
void* evcon_loop_get_backend_data(evcon_loop *loop) {
	return loop->backend_data;
}
void* evcon_fd_get_backend_data(evcon_fd_watcher *watcher) {
	return watcher->backend_data;
}
void* evcon_timer_get_backend_data(evcon_timer_watcher *watcher) {
	return watcher->backend_data;
}
void* evcon_async_get_backend_data(evcon_async_watcher *watcher) {
	return watcher->backend_data;
}

void evcon_backend_set_data(evcon_backend *backend, void *data) {
	backend->backend_data = data;
}
void evcon_loop_set_backend_data(evcon_loop *loop, void *data) {
	loop->backend_data = data;
}
void evcon_fd_set_backend_data(evcon_fd_watcher *watcher, void *data) {
	watcher->backend_data = data;
}
void evcon_timer_set_backend_data(evcon_timer_watcher *watcher, void *data) {
	watcher->backend_data = data;
}
void evcon_async_set_backend_data(evcon_async_watcher *watcher, void *data) {
	watcher->backend_data = data;
}

static void evcon_backend_fd_update(evcon_fd_watcher *watcher) {
	evcon_backend *backend = watcher->loop->backend;
	int fd = watcher->fd, events = watcher->events;
	if (!watcher->active || -1 == fd) events = 0;
	backend->fd_update_cb(watcher, fd, events, watcher->loop->allocator, watcher->loop->backend_data, watcher->backend_data);
}

/* this restarts an active timer! */
static void evcon_backend_timer_update(evcon_timer_watcher *watcher) {
	evcon_backend *backend = watcher->loop->backend;
	evcon_interval timeout = watcher->timeout;
	if (!watcher->active || timeout < 0) timeout = -1;
	backend->timer_update_cb(watcher, timeout, watcher->loop->allocator, watcher->loop->backend_data, watcher->backend_data);
}
/* tell backend to delete timer */
static void evcon_backend_timer_delete(evcon_timer_watcher *watcher) {
	evcon_backend *backend = watcher->loop->backend;
	backend->timer_update_cb(watcher, -2, watcher->loop->allocator, watcher->loop->backend_data, watcher->backend_data);
}

void evcon_feed_fd(evcon_fd_watcher *watcher, int events) {
	int oldfd, oldevents;
	if (watcher->incallback) return;

	oldfd = watcher->fd;
	oldevents = watcher->events;

	watcher->incallback = 1;
	watcher->cb(watcher->loop, watcher, oldfd, events, watcher->user_data);
	watcher->incallback = 0;

	if (watcher->delayed_delete) {
		evcon_fd_free(watcher);
		return;
	}

	if (oldfd != watcher->fd || oldevents != watcher->events) evcon_backend_fd_update(watcher);
}

void evcon_feed_timer(evcon_timer_watcher *watcher) {
	if (watcher->incallback) return;
	watcher->timeout = watcher->repeat;

	watcher->incallback = 1;
	watcher->cb(watcher->loop, watcher, watcher->user_data);
	watcher->incallback = 0;

	if (watcher->delayed_delete) {
		evcon_timer_free(watcher);
		return;
	}

	evcon_backend_timer_update(watcher);
}

void evcon_feed_async(evcon_async_watcher *watcher) {
	if (watcher->incallback) return;

	watcher->incallback = 1;
	watcher->cb(watcher->loop, watcher, watcher->user_data);
	watcher->incallback = 0;

	if (watcher->delayed_delete) {
		evcon_async_free(watcher);
		return;
	}
}

/*****************************************************
 *             Main interface                        *
 *****************************************************/

void evcon_loop_ref(evcon_loop *loop) {
	assert(loop->refcount > 0);
	++loop->refcount;
}
void evcon_loop_unref(evcon_loop *loop) {
	if (!loop) return;
	assert(loop->refcount > 0);

	if (0 == --(loop->refcount)) {
		evcon_allocator *allocator = loop->allocator;

		loop->refcount = 1; /* fake reference: allows loops to use own watchers with weak references */
		loop->backend->free_loop_cb(loop, loop->backend_data, loop->backend->backend_data);
		memset(loop, 0, sizeof(evcon_loop));
		evcon_free(allocator, loop, sizeof(evcon_loop));
	}
}

evcon_allocator* evcon_loop_get_allocator(evcon_loop *loop) {
	return loop->allocator;
}

void evcon_init_fd(evcon_fd fd) {
#ifdef FD_CLOEXEC
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
#ifdef O_NONBLOCK
	fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
#elif defined _WIN32
	int i = 1;
	ioctlsocket(fd, FIONBIO, &i);
#else
#error No way found to set non-blocking mode for fds.
#endif
}

evcon_fd_watcher* evcon_fd_new(evcon_loop *loop, evcon_fd_cb cb, evcon_fd fd, int events, void* user_data) {
	evcon_fd_watcher *watcher = evcon_alloc0(loop->allocator, sizeof(evcon_fd_watcher));
	evcon_loop_ref(loop);

	watcher->user_data = user_data;
	watcher->backend_data = NULL;
	watcher->active = watcher->incallback = watcher->delayed_delete = 0;
	watcher->loop = loop;
	watcher->cb = cb;
	watcher->fd = fd;
	watcher->events = events;

	return watcher;
}

void evcon_fd_start(evcon_fd_watcher *watcher) {
	if (!watcher->active) {
		watcher->active = 1;
		evcon_backend_fd_update(watcher);
	}
}

void evcon_fd_stop(evcon_fd_watcher *watcher) {
	if (watcher->active) {
		watcher->active = 0;
		evcon_backend_fd_update(watcher);
	}
}

void evcon_fd_free(evcon_fd_watcher* watcher) {
	watcher->active = 0;
	watcher->fd = -1;
	watcher->events = 0;
	if (watcher->incallback) { /* delay delete */
		watcher->delayed_delete = 1;
	} else {
		evcon_loop *loop = watcher->loop;
		evcon_backend_fd_update(watcher);
		memset(watcher, 0, sizeof(evcon_fd_watcher));
		evcon_free(loop->allocator, watcher, sizeof(evcon_fd_watcher));
		evcon_loop_unref(loop);
	}
}

int evcon_fd_is_active(evcon_fd_watcher* watcher) {
	return watcher->active;
}

evcon_fd_cb evcon_fd_get_cb(evcon_fd_watcher *watcher) {
	return watcher->cb;
}
evcon_fd evcon_fd_get_fd(evcon_fd_watcher *watcher) {
	return watcher->fd;
}
int evcon_fd_get_events(evcon_fd_watcher *watcher) {
	return watcher->events;
}
void* evcon_fd_get_user_data(evcon_fd_watcher *watcher) {
	return watcher->user_data;
}
evcon_loop *evcon_fd_get_loop(evcon_fd_watcher *watcher) {
	return watcher->loop;
}

void evcon_fd_set_cb(evcon_fd_watcher *watcher, evcon_fd_cb cb) {
	watcher->cb = cb;
}
void evcon_fd_set_fd(evcon_fd_watcher *watcher, evcon_fd fd) {
	watcher->fd = fd;
	if (watcher->active && !watcher->incallback) evcon_backend_fd_update(watcher);
}
void evcon_fd_set_events(evcon_fd_watcher *watcher, int events) {
	watcher->events = events;
	if (-1 != watcher->fd && watcher->active && !watcher->incallback) evcon_backend_fd_update(watcher);
}
void evcon_fd_set_user_data(evcon_fd_watcher *watcher, void* user_data) {
	watcher->user_data = user_data;
}

evcon_timer_watcher *evcon_timer_new(evcon_loop *loop, evcon_timer_cb cb, void *user_data) {
	evcon_timer_watcher *watcher = evcon_alloc0(loop->allocator, sizeof(evcon_timer_watcher));
	evcon_loop_ref(loop);

	watcher->user_data = user_data;
	watcher->backend_data = NULL;
	watcher->active = watcher->incallback = watcher->delayed_delete = 0;
	watcher->loop = loop;
	watcher->cb = cb;
	watcher->timeout = -1;
	watcher->repeat = -1;

	return watcher;
}

void evcon_timer_once(evcon_timer_watcher *watcher, evcon_interval timeout) {
	watcher->timeout = timeout;
	watcher->repeat = -1;
	watcher->active = 1;
	if (!watcher->incallback) evcon_backend_timer_update(watcher);
}

void evcon_timer_repeat(evcon_timer_watcher *watcher, evcon_interval repeat) {
	watcher->timeout = repeat;
	watcher->repeat = repeat;
	watcher->active = 1;
	if (!watcher->incallback) evcon_backend_timer_update(watcher);
}

void evcon_timer_stop(evcon_timer_watcher *watcher) {
	if (watcher->active) {
		watcher->active = 0;
		evcon_backend_timer_update(watcher);
	}
}

void evcon_timer_free(evcon_timer_watcher *watcher) {
	watcher->active = 0;
	watcher->timeout = watcher->repeat = -1;
	if (watcher->incallback) { /* delay delete */
		watcher->delayed_delete = 1;
	} else {
		evcon_loop *loop = watcher->loop;
		evcon_backend_timer_delete(watcher);
		memset(watcher, 0, sizeof(evcon_timer_watcher));
		evcon_free(loop->allocator, watcher, sizeof(evcon_timer_watcher));
		evcon_loop_unref(loop);
	}
}

int evcon_timer_is_active(evcon_fd_watcher* watcher) {
	return watcher->active;
}

evcon_timer_cb evcon_timer_get_cb(evcon_timer_watcher *watcher) {
	return watcher->cb;
}
evcon_interval evcon_timer_get_timeout(evcon_timer_watcher *watcher) {
	return watcher->timeout;
}
evcon_interval evcon_timer_get_repeat(evcon_timer_watcher *watcher) {
	return watcher->repeat;
}
void* evcon_timer_get_user_data(evcon_timer_watcher *watcher) {
	return watcher->user_data;
}
evcon_loop *evcon_timer_get_loop(evcon_timer_watcher *watcher) {
	return watcher->loop;
}

void evcon_timer_set_cb(evcon_timer_watcher *watcher, evcon_timer_cb cb) {
	watcher->cb = cb;
}
void evcon_timer_set_repeat(evcon_timer_watcher *watcher, evcon_interval repeat) {
	watcher->repeat = repeat;
}
void evcon_timer_set_user_data(evcon_timer_watcher *watcher, void *user_data) {
	watcher->user_data = user_data;
}

evcon_async_watcher* evcon_async_new(evcon_loop *loop, evcon_async_cb cb, void* user_data) {
	evcon_async_watcher *watcher = evcon_alloc0(loop->allocator, sizeof(evcon_async_watcher));
	evcon_backend *backend = watcher->loop->backend;
	evcon_loop_ref(loop);

	watcher->user_data = user_data;
	watcher->backend_data = NULL;
	watcher->incallback = watcher->delayed_delete = 0;
	watcher->loop = loop;
	watcher->cb = cb;

	backend->async_update_cb(watcher, EVCON_ASYNC_NEW, watcher->loop->allocator, watcher->loop->backend_data, watcher->backend_data);
	return watcher;
}

void evcon_async_wakeup(evcon_async_watcher *watcher) {
	evcon_backend *backend = watcher->loop->backend;
	backend->async_update_cb(watcher, EVCON_ASYNC_TRIGGER, watcher->loop->allocator, watcher->loop->backend_data, watcher->backend_data);
}

void evcon_async_free(evcon_async_watcher* watcher) {
	if (watcher->incallback) { /* delay delete */
		watcher->delayed_delete = 1;
	} else {
		evcon_loop *loop = watcher->loop;
		evcon_backend *backend = watcher->loop->backend;

		backend->async_update_cb(watcher, EVCON_ASYNC_FREE, watcher->loop->allocator, watcher->loop->backend_data, watcher->backend_data);

		memset(watcher, 0, sizeof(evcon_async_watcher));
		evcon_free(loop->allocator, watcher, sizeof(evcon_async_watcher));
		evcon_loop_unref(loop);
	}
}

evcon_async_cb evcon_async_get_cb(evcon_async_watcher *watcher) {
	return watcher->cb;
}
void* evcon_async_get_user_data(evcon_async_watcher *watcher) {
	return watcher->user_data;
}
evcon_loop *evcon_async_get_loop(evcon_async_watcher *watcher) {
	return watcher->loop;
}

void evcon_async_set_cb(evcon_async_watcher *watcher, evcon_async_cb cb) {
	watcher->cb = cb;
}
void evcon_async_set_user_data(evcon_async_watcher *watcher, void* user_data) {
	watcher->user_data = user_data;
}
