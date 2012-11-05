
#include <evcon-ev.h>

#include <evcon-allocator.h>
#include <evcon-backend.h>

#include <evcon-config-private.h>

#include <glib.h>

#define UNUSED(x) ((void)(x))

/* ev loop wrapper */

static void evcon_ev_free_loop(evcon_loop *loop, void *loop_data, void *backend_data) {
	UNUSED(loop);
	UNUSED(backend_data);
	UNUSED(loop_data);
}

static void evcon_ev_fd_cb(struct ev_loop *loop, ev_io *w, int revents) {
	evcon_fd_watcher *watcher = (evcon_fd_watcher*) w->data;
	int events;
	UNUSED(loop);

	events = 0;
	if (0 != (revents & EV_ERROR)) events |= EVCON_ERROR;
	if (0 != (revents & EV_READ)) events |= EVCON_READ;
	if (0 != (revents & EV_WRITE)) events |= EVCON_WRITE;

	evcon_feed_fd(watcher, events);
}

static void evcon_ev_fd_update(evcon_fd_watcher *watcher, evcon_fd fd, int events, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	ev_io *w = (ev_io*) watcher_data;
	struct ev_loop *evl = (struct ev_loop*) loop_data;
	int evs;

	if (-1 == fd) {
		/* delete watcher */
		if (NULL == w) return;

		ev_io_stop(evl, w);
		evcon_free(allocator, w, sizeof(*w));
		evcon_fd_set_backend_data(watcher, NULL);
		return;
	}

	evs = 0;
	if (0 != (events & EVCON_READ)) evs |= EV_READ;
	if (0 != (events & EVCON_WRITE)) evs |= EV_WRITE;

	if (NULL == w) {
		w = evcon_alloc0(allocator, sizeof(ev_io));
		evcon_fd_set_backend_data(watcher, w);
		ev_io_init(w, evcon_ev_fd_cb, fd, evs);
		w->data = watcher;
		if (0 != evs) ev_io_start(evl, w);
		return;
	}

	if (w->events == evs && fd == w->fd) return;

	ev_io_stop(evl, w);
	ev_io_set(w, fd, evs);
	if (0 != evs) ev_io_start(evl, w);
}

static void evcon_ev_timer_cb(struct ev_loop *loop, ev_timer *w, int revents) {
	evcon_timer_watcher *watcher = (evcon_timer_watcher*) w->data;
	UNUSED(loop);
	UNUSED(revents);

	evcon_feed_timer(watcher);
}

static void evcon_ev_timer_update(evcon_timer_watcher *watcher, evcon_interval timeout, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	ev_timer *w = (ev_timer*) watcher_data;
	struct ev_loop *evl = (struct ev_loop*) loop_data;

	if (-2 == timeout) {
		/* delete watcher */
		if (NULL == w) return;

		ev_timer_stop(evl, w);
		evcon_free(allocator, w, sizeof(*w));
		evcon_timer_set_backend_data(watcher, NULL);
		return;
	}

	if (-1 == timeout && NULL == w) return;

	if (-1 == timeout) {
		ev_timer_stop(evl, w);
		return;
	}

	if (NULL == w) {
		w = evcon_alloc0(allocator, sizeof(ev_timer));
		evcon_timer_set_backend_data(watcher, w);
		ev_timer_init(w, evcon_ev_timer_cb, EVCON_INTERVAL_AS_DOUBLE_SEC(timeout), 0.);
		w->data = watcher;
		ev_timer_start(evl, w);
		return;
	}

	ev_timer_stop(evl, w);
	ev_timer_set(w, EVCON_INTERVAL_AS_DOUBLE_SEC(timeout), 0.);
	ev_timer_start(evl, w);
}

static void evcon_ev_async_cb(struct ev_loop *loop, ev_async *w, int revents) {
	evcon_async_watcher *watcher = (evcon_async_watcher*) w->data;
	UNUSED(loop);
	UNUSED(revents);

	evcon_feed_async(watcher);
}

static void evcon_ev_async_update(evcon_async_watcher *watcher, evcon_async_func f, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	ev_async *w = (ev_async*) watcher_data;
	struct ev_loop *evl = (struct ev_loop*) loop_data;

	switch (f) {
	case EVCON_ASYNC_TRIGGER:
		ev_async_send(evl, w);
		break;
	case EVCON_ASYNC_NEW:
		w = evcon_alloc0(allocator, sizeof(ev_async));
		evcon_async_set_backend_data(watcher, w);
		ev_async_init(w, evcon_ev_async_cb);
		w->data = watcher;
		ev_async_start(evl, w);
		break;
	case EVCON_ASYNC_FREE:
		if (NULL == w) return;

		ev_async_stop(evl, w);
		evcon_free(allocator, w, sizeof(*w));
		evcon_async_set_backend_data(watcher, NULL);
		return;
	}
}

static evcon_backend* evcon_ev_backend(evcon_allocator* allocator) {
	static char static_backend_buf[EVCON_BACKEND_RECOMMENDED_SIZE];
	static volatile evcon_backend* backend = NULL;

	if (g_once_init_enter(&backend)) {
		evcon_backend* bcknd = evcon_backend_init(static_backend_buf, sizeof(static_backend_buf), NULL, allocator, evcon_ev_free_loop, evcon_ev_fd_update, evcon_ev_timer_update, evcon_ev_async_update);

		g_once_init_leave(&backend, bcknd);
	}

	return (evcon_backend*) backend;
}

evcon_loop* evcon_loop_from_ev(struct ev_loop *loop, evcon_allocator* allocator) {
	evcon_backend *backend = evcon_ev_backend(allocator);
	evcon_loop *evc_loop = evcon_loop_new(backend, allocator);

	evcon_loop_set_backend_data(evc_loop, loop);

	return evc_loop;
}
