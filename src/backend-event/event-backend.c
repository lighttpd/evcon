
#include <evcon-event.h>

#include <evcon-allocator.h>
#include <evcon-backend.h>

#include <evcon-config-private.h>

#include <glib.h>

#define UNUSED(x) ((void)(x))

/* event loop wrapper */

static void evcon_event_free_loop(evcon_loop *loop, void *loop_data, void *backend_data) {
	UNUSED(loop);
	UNUSED(backend_data);
	UNUSED(loop_data);
}

static void evcon_event_fd_cb(evutil_socket_t fd, short revents, void *user_data) {
	evcon_fd_watcher *watcher = (evcon_fd_watcher*) user_data;
	int events;
	UNUSED(fd);

	events = 0;
	if (0 != (revents & EV_READ)) events |= EVCON_READ;
	if (0 != (revents & EV_WRITE)) events |= EVCON_WRITE;

	evcon_feed_fd(watcher, events);
}

static void evcon_event_fd_update(evcon_fd_watcher *watcher, evcon_fd fd, int events, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	struct event *w = (struct event*) watcher_data;
	struct event_base *base = (struct event_base*) loop_data;
	short evs;
	UNUSED(allocator);

	if (-1 == fd) {
		/* delete watcher */
		if (NULL == w) return;

		event_free(w);
		evcon_fd_set_backend_data(watcher, NULL);
		return;
	}

	evs = EV_PERSIST;
	if (0 != (events & EVCON_READ)) evs |= EV_READ;
	if (0 != (events & EVCON_WRITE)) evs |= EV_WRITE;

	if (NULL == w) {
		w = event_new(base, fd, evs, evcon_event_fd_cb, watcher);
		evcon_fd_set_backend_data(watcher, w);
		if (EV_PERSIST != evs) event_add(w, NULL);
		return;
	}

	if (event_get_events(w) == evs && event_get_fd(w) == fd) return;

	event_del(w);
	event_assign(w, base, fd, evs, evcon_event_fd_cb, watcher);
	if (EV_PERSIST != evs) event_add(w, NULL);
}

static void evcon_event_timer_cb(evutil_socket_t fd, short revents, void *user_data) {
	evcon_timer_watcher *watcher = (evcon_timer_watcher*) user_data;
	UNUSED(fd);
	UNUSED(revents);

	evcon_feed_timer(watcher);
}

static void evcon_event_timer_update(evcon_timer_watcher *watcher, evcon_interval timeout, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	struct event *w = (struct event*) watcher_data;
	struct event_base *base = (struct event_base*) loop_data;
	struct timeval tv;
	UNUSED(allocator);

	if (-2 == timeout) {
		/* delete watcher */
		if (NULL == w) return;

		event_free(w);
		evcon_timer_set_backend_data(watcher, NULL);
		return;
	}

	if (-1 == timeout && NULL == w) return;

	if (-1 == timeout) {
		event_del(w);
		return;
	}

	if (NULL == w) {
		w = event_new(base, -1, EV_TIMEOUT, evcon_event_timer_cb, watcher);
		evcon_timer_set_backend_data(watcher, w);
	}

	tv.tv_sec = EVCON_INTERVAL_AS_SEC(timeout);
	//tv.tv_usec = EVCON_INTERVAL_AS_USEC(timeout) % 1000000;
	event_add(w, &tv);
}

static void evcon_event_async_cb(evutil_socket_t fd, short revents, void *user_data) {
	evcon_async_watcher *watcher = (evcon_async_watcher*) user_data;
	UNUSED(fd);
	UNUSED(revents);

	evcon_feed_async(watcher);
}

static void evcon_event_async_update(evcon_async_watcher *watcher, evcon_async_func f, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	struct event *w = (struct event*) watcher_data;
	struct event_base *base = (struct event_base*) loop_data;
	UNUSED(allocator);

	switch (f) {
	case EVCON_ASYNC_TRIGGER:
		event_active(w, EV_SIGNAL, 0);
		break;
	case EVCON_ASYNC_NEW:
		w = event_new(base, -1, EV_PERSIST, evcon_event_async_cb, watcher);
		evcon_async_set_backend_data(watcher, w);
		event_add(w, NULL);
		break;
	case EVCON_ASYNC_FREE:
		if (NULL == w) return;

		event_free(w);
		evcon_async_set_backend_data(watcher, NULL);
		return;
	}
}

static evcon_backend* evcon_event_backend(evcon_allocator* allocator) {
	static char static_backend_buf[EVCON_BACKEND_RECOMMENDED_SIZE];
	static volatile evcon_backend* backend = NULL;

	if (g_once_init_enter(&backend)) {
		evcon_backend* bcknd = evcon_backend_init(static_backend_buf, sizeof(static_backend_buf), NULL, allocator, evcon_event_free_loop, evcon_event_fd_update, evcon_event_timer_update, evcon_event_async_update);

		g_once_init_leave(&backend, bcknd);
	}

	return (evcon_backend*) backend;
}

evcon_loop* evcon_loop_from_event(struct event_base *base, evcon_allocator* allocator) {
	evcon_backend *backend = evcon_event_backend(allocator);
	evcon_loop *evc_loop = evcon_loop_new(backend, allocator);

	evcon_loop_set_backend_data(evc_loop, base);

	return evc_loop;
}
