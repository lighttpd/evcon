
#define _GNU_SOURCE

#include <evcon-glib.h>

#include <evcon-allocator.h>
#include <evcon-backend.h>

#include <evcon-config-private.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define UNUSED(x) ((void)(x))

#ifndef EVCON_GLIB_COMPAT_API
# if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 31
#  define EVCON_GLIB_COMPAT_API 1
# endif
#endif

#ifdef EVCON_GLIB_COMPAT_API

typedef GMutex* evcon_glib_mutex;

static void evcon_glib_mutex_init(evcon_glib_mutex *m) {
	*m = (*g_thread_functions_for_glib_use.mutex_new)();
}

static void evcon_glib_mutex_clear(evcon_glib_mutex *m) {
	GMutex *mx = *m;
	if (g_thread_supported()) (*g_thread_functions_for_glib_use.mutex_free)(mx);
	*m = NULL;
}

static void evcon_glib_mutex_lock(evcon_glib_mutex *m) {
	GMutex *mx = *m;
	if (g_thread_supported()) (*g_thread_functions_for_glib_use.mutex_lock)(mx);
}

static void evcon_glib_mutex_unlock(evcon_glib_mutex *m) {
	GMutex *mx = *m;
	if (g_thread_supported()) (*g_thread_functions_for_glib_use.mutex_unlock)(mx);
}

#else

typedef GMutex evcon_glib_mutex;

static void evcon_glib_mutex_init(evcon_glib_mutex *m) {
	g_mutex_init(m);
}

static void evcon_glib_mutex_clear(evcon_glib_mutex *m) {
	g_mutex_clear(m);
}

static void evcon_glib_mutex_lock(evcon_glib_mutex *m) {
	g_mutex_lock(m);
}

static void evcon_glib_mutex_unlock(evcon_glib_mutex *m) {
	g_mutex_unlock(m);
}

#endif

/* GLib loop wrapper */

typedef struct evcon_glib_data evcon_glib_data;
typedef struct evcon_glib_fd_source evcon_glib_fd_source;
typedef struct evcon_glib_async_watcher evcon_glib_async_watcher;

struct evcon_glib_data {
	GMainContext *ctx;

	gint async_pipe_fds[2];
	evcon_fd_watcher *async_watcher;
	evcon_glib_mutex async_mutex;
	GQueue async_pending;
};

struct evcon_glib_fd_source {
	GSource source;
	GPollFD pollfd;
	evcon_fd_watcher *watcher;
};

struct evcon_glib_async_watcher {
	GList pending_link;
	evcon_async_watcher *orig;
	gboolean active;
};

static void evcon_glib_free_loop(evcon_loop *loop, void *loop_data, void *backend_data) {
	evcon_glib_data *data = (evcon_glib_data*) loop_data;
	UNUSED(loop);
	UNUSED(backend_data);

	evcon_loop_ref(loop);
	evcon_fd_free(data->async_watcher);

	close(data->async_pipe_fds[0]); data->async_pipe_fds[0] = -1;
	close(data->async_pipe_fds[1]); data->async_pipe_fds[1] = -1;

	g_main_context_ref(data->ctx);
	evcon_glib_mutex_clear(&data->async_mutex);
	g_slice_free(evcon_glib_data, data);
}

/* own FD poll handling */

static gboolean fd_source_prepare(GSource *source, gint *timeout);
static gboolean fd_source_check(GSource *source);
static gboolean fd_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);
static void fd_source_finalize(GSource *source);

static GSourceFuncs fd_source_funcs = {
	fd_source_prepare,
	fd_source_check,
	fd_source_dispatch,
	fd_source_finalize, 0, 0
};

static gboolean fd_source_prepare(GSource *source, gint *timeout) {
	UNUSED(source);
	*timeout = -1;
	return FALSE;
}
static gboolean fd_source_check(GSource *source) {
	evcon_glib_fd_source *watch = (evcon_glib_fd_source*) source;
	return 0 != (watch->pollfd.revents & watch->pollfd.events);
}
static gboolean fd_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data) {
	evcon_glib_fd_source *watch = (evcon_glib_fd_source*) source;
	int events;
	UNUSED(callback);
	UNUSED(user_data);

	events = 0;
	watch->pollfd.revents &= watch->pollfd.events;
	if (0 != (watch->pollfd.revents & (G_IO_IN | G_IO_HUP))) events |= EVCON_READ;
	if (0 != (watch->pollfd.revents & G_IO_ERR)) events |= EVCON_ERROR;
	if (0 != (watch->pollfd.revents & G_IO_OUT)) events |= EVCON_WRITE;

	if (0 != events) evcon_feed_fd(watch->watcher, events);

	return TRUE;
}
static void fd_source_finalize(GSource *source) {
	UNUSED(source);
}

static GSource* fd_source_new(evcon_fd_watcher *watcher) {
	GSource *source = g_source_new(&fd_source_funcs, sizeof(evcon_glib_fd_source));
	evcon_glib_fd_source *watch = (evcon_glib_fd_source*) source;
	watch->watcher = watcher;
	watch->pollfd.fd = -1;
	watch->pollfd.events = watch->pollfd.revents = 0;
	evcon_fd_set_backend_data(watcher, watch);
	return source;
}

static void evcon_glib_fd_update(evcon_fd_watcher *watcher, evcon_fd fd, int events, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	GMainContext *ctx = ((evcon_glib_data*) loop_data)->ctx;
	GSource *source = (GSource*) watcher_data;
	evcon_glib_fd_source *watch;
	int evs;
	UNUSED(allocator);

	if (-1 == fd) {
		/* delete watcher */
		if (NULL == source) return;
		g_source_destroy(source);
		g_source_unref(source);
		return;
	}

	if (NULL == source) {
		source = fd_source_new(watcher);
		g_source_attach(source, ctx);
		watch = (evcon_glib_fd_source*) source;
		g_source_add_poll(source, &watch->pollfd);
	} else {
		watch = (evcon_glib_fd_source*) source;
	}

	evs = 0;
	if (0 != (events & EVCON_READ)) evs |= G_IO_IN | G_IO_HUP | G_IO_ERR;
	if (0 != (events & EVCON_WRITE)) evs |= G_IO_OUT | G_IO_ERR;

	if (fd == watch->pollfd.fd && evs == watch->pollfd.events) return;

	if (fd != watch->pollfd.fd) watch->pollfd.revents = 0;

	watch->pollfd.fd = fd;
	watch->pollfd.events = evs;
}

static gboolean evcon_glib_timer_cb(gpointer data) {
	evcon_timer_watcher *watcher = (evcon_timer_watcher*) data;
	evcon_feed_timer(watcher);
	return FALSE;
}

static void evcon_glib_timer_update(evcon_timer_watcher *watcher, evcon_interval timeout, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	GMainContext *ctx = ((evcon_glib_data*) loop_data)->ctx;
	GSource *source = (GSource*) watcher_data;
	UNUSED(allocator);

	if (NULL != source) {
		/* delete old source */
		g_source_destroy(source);
		g_source_unref(source);
	}

	if (timeout < 0) return;

	if (timeout == 0) {
		source = g_idle_source_new();
	} else {
		source = g_timeout_source_new(EVCON_INTERVAL_AS_MSEC(timeout));
	}
	evcon_timer_set_backend_data(watcher, source);
	g_source_set_callback(source, evcon_glib_timer_cb, watcher, NULL);
	g_source_attach(source, ctx);
}


static void evcon_glib_async_update(evcon_async_watcher *watcher, evcon_async_func f, evcon_allocator *allocator, void *loop_data, void *watcher_data) {
	static const char val = 'A';
	evcon_glib_data *data = (evcon_glib_data*) loop_data;
	evcon_glib_async_watcher *w = (evcon_glib_async_watcher*) watcher_data;
	UNUSED(allocator);

	evcon_glib_mutex_lock(&data->async_mutex);

	switch (f) {
	case EVCON_ASYNC_TRIGGER:
		if (!w->active) {
			w->active = TRUE;
			if (0 == data->async_pending.length) {
				int r;
trigger_again:
				r = write(data->async_pipe_fds[1], &val, sizeof(val));
				if (-1 == r) {
					switch (errno) {
					case EINTR:
						goto trigger_again;
					case EAGAIN:
#if EWOULDBLOCK != EAGAIN
					case EWOULDBLOCK:
#endif
						break; /* enough data in the pipe to trigger */
					default:
						g_error("async wake write failed: %s", g_strerror(errno));
					}
				}
			}
			g_queue_push_tail_link(&data->async_pending, &w->pending_link);
		}
		break;
	case EVCON_ASYNC_NEW:
		w = g_slice_new0(evcon_glib_async_watcher);
		w->pending_link.data = w;
		w->orig = watcher;
		evcon_async_set_backend_data(watcher, w);
		break;
	case EVCON_ASYNC_FREE:
		if (NULL == w) goto exit;

		if (w->active) {
			g_queue_unlink(&data->async_pending, &w->pending_link);
			w->active = FALSE;
		}
		g_slice_free(evcon_glib_async_watcher, w);
		evcon_async_set_backend_data(watcher, NULL);
		goto exit;
	}

exit:
	evcon_glib_mutex_unlock(&data->async_mutex);
}

static void evcon_glib_async_cb(evcon_loop *loop, evcon_fd_watcher *watcher, evcon_fd fd, int revents, void* user_data) {
	evcon_glib_data *data = user_data;
	evcon_glib_async_watcher *w;
	char buf[32];
	UNUSED(loop);
	UNUSED(watcher);
	UNUSED(revents);

	(void) read(fd, buf, sizeof(buf));

	for (;;) {
		{
			GList *link;
			evcon_glib_mutex_lock(&data->async_mutex);
			link = g_queue_pop_head_link(&data->async_pending);
			if (NULL != link) {
				w = (evcon_glib_async_watcher*) link->data;
				w->active = FALSE;
			} else {
				w = NULL;
			}
			evcon_glib_mutex_unlock(&data->async_mutex);
		}

		if (NULL == w) break;

		evcon_feed_async(w->orig);
	}
}

static evcon_backend* evcon_glib_backend(void) {
	static char static_backend_buf[EVCON_BACKEND_RECOMMENDED_SIZE];
	static volatile evcon_backend* backend = NULL;

	if (g_once_init_enter(&backend)) {
		evcon_backend* bcknd = evcon_backend_init(static_backend_buf, sizeof(static_backend_buf), NULL, evcon_glib_allocator(), evcon_glib_free_loop, evcon_glib_fd_update, evcon_glib_timer_update, evcon_glib_async_update);

		g_once_init_leave(&backend, bcknd);
	}

	return (evcon_backend*) backend;
}

static gboolean setup_pipe(int fds[2]) {
#ifdef HAVE_PIPE2
	if (-1 == pipe2(fds, O_NONBLOCK | O_CLOEXEC)) {
		g_error("Cannot create pipe: %s\n", g_strerror(errno));
		return FALSE;
	}
#else
	if (-1 == pipe(fds)) {
		g_error("Cannot create pipe: %s\n", g_strerror(errno));
		return FALSE;
	}

	evcon_init_fd(fds[0]);
	evcon_init_fd(fds[1]);
#endif
	return TRUE;
}

evcon_loop* evcon_loop_from_glib(GMainContext *ctx, evcon_allocator *allocator) {
	evcon_backend *backend;
	evcon_glib_data *loop_data;
	evcon_loop *evc_loop;
	int async_pipe_fds[2];

	if (!setup_pipe(async_pipe_fds)) return NULL;

	if (NULL == allocator) allocator = evcon_glib_allocator();

	backend = evcon_glib_backend();
	loop_data = g_slice_new0(evcon_glib_data);
	evc_loop = evcon_loop_new(backend, allocator);

	g_main_context_ref(ctx);
	loop_data->ctx = ctx;
	loop_data->async_pipe_fds[0] = async_pipe_fds[0];
	loop_data->async_pipe_fds[1] = async_pipe_fds[1];
	evcon_glib_mutex_init(&loop_data->async_mutex);
	evcon_loop_set_backend_data(evc_loop, loop_data);

	loop_data->async_watcher = evcon_fd_new(evc_loop, evcon_glib_async_cb, async_pipe_fds[0], EVCON_READ, loop_data);
	evcon_loop_unref(evc_loop);

	return evc_loop;
}

GMainContext* evcon_loop_glib_get_context(evcon_loop *loop) {
	return ((evcon_glib_data*) evcon_loop_get_backend_data(loop))->ctx;
}
