#ifndef __EVCON_EVCON_H
#define __EVCON_EVCON_H __EVCON_EVCON_H

#include <evcon-config.h>

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

/* (positive) time interval in milliseconds; negative values have special meanings */
/* use of macros is recommended, in case someone needs to change this for porting */

typedef int64_t evcon_interval;
#define EVCON_INTERVAL_FROM_DOUBLE_SEC(x) ((evcon_interval) ceil((x)*1.e3))
#define EVCON_INTERVAL_FROM_NSEC(x) ((x+(evcon_interval)1e6-1)/1e6)
#define EVCON_INTERVAL_FROM_USEC(x) ((x+(evcon_interval)1e3-1)/1e3)
#define EVCON_INTERVAL_FROM_MSEC(x) (x)
#define EVCON_INTERVAL_FROM_SEC(x) ((x)*(evcon_interval)1e3)

#define EVCON_INTERVAL_AS_DOUBLE_SEC(x) ((x)*1.e-3)
#define EVCON_INTERVAL_AS_NSEC(x) ((x)*1e6)
#define EVCON_INTERVAL_AS_USEC(x) ((x)*1e3)
#define EVCON_INTERVAL_AS_MSEC(x) (x)
#define EVCON_INTERVAL_AS_SEC(x) ((x+1e3-1)/1e3)


typedef struct evcon_loop evcon_loop;
typedef struct evcon_backend evcon_backend;
typedef struct evcon_allocator evcon_allocator;

typedef struct evcon_fd_watcher evcon_fd_watcher;
typedef struct evcon_timer_watcher evcon_timer_watcher;
typedef struct evcon_async_watcher evcon_async_watcher;

typedef int evcon_fd;

typedef enum {
	EVCON_ERROR     = 0x0001,
	EVCON_READ      = 0x0002,
	EVCON_WRITE     = 0x0004
} evcon_events;

typedef void (*evcon_fd_cb)(evcon_loop *loop, evcon_fd_watcher *watcher, evcon_fd fd, int revents, void* user_data);
typedef void (*evcon_timer_cb)(evcon_loop *loop, evcon_timer_watcher *watcher, void* user_data);
typedef void (*evcon_async_cb)(evcon_loop *loop, evcon_async_watcher *watcher, void* user_data);

/* each watcher keeps a reference; only backends are allowed to 
 * "undo" the reference count for internal watchers (see glib-backend.c for an example)
 */
void evcon_loop_ref(evcon_loop *loop);
void evcon_loop_unref(evcon_loop *loop);

evcon_allocator* evcon_loop_get_allocator(evcon_loop *loop);

/* sets fd to non-blocking (and FD_CLOEXEC if supported) */
void evcon_init_fd(evcon_fd fd);

/* fd watcher */
evcon_fd_watcher* evcon_fd_new(evcon_loop *loop, evcon_fd_cb cb, evcon_fd fd, int events, void* user_data);
void evcon_fd_start(evcon_fd_watcher *watcher);
void evcon_fd_stop(evcon_fd_watcher *watcher);
void evcon_fd_free(evcon_fd_watcher* watcher);
int evcon_fd_is_active(evcon_fd_watcher* watcher); /* 1 == started, 0 == stopped */

evcon_fd_cb evcon_fd_get_cb(evcon_fd_watcher *watcher);
evcon_fd evcon_fd_get_fd(evcon_fd_watcher *watcher);
int evcon_fd_get_events(evcon_fd_watcher *watcher);
void* evcon_fd_get_user_data(evcon_fd_watcher *watcher);
evcon_loop *evcon_fd_get_loop(evcon_fd_watcher *watcher);

void evcon_fd_set_cb(evcon_fd_watcher *watcher, evcon_fd_cb cb);
void evcon_fd_set_fd(evcon_fd_watcher *watcher, evcon_fd fd);
void evcon_fd_set_events(evcon_fd_watcher *watcher, int events);
void evcon_fd_set_user_data(evcon_fd_watcher *watcher, void* user_data);

/* timer watcher. all times are relative, < 0 means "disabled" */
evcon_timer_watcher *evcon_timer_new(evcon_loop *loop, evcon_timer_cb cb, void *user_data);
void evcon_timer_once(evcon_timer_watcher *watcher, evcon_interval timeout); /* (re)start timer; triggering in @timeout seconds, then stop (sets repeat = -1) */
void evcon_timer_repeat(evcon_timer_watcher *watcher, evcon_interval repeat); /* (re)start timer; triggering in @timeout seconds, then start again */
void evcon_timer_stop(evcon_timer_watcher *watcher);
void evcon_timer_free(evcon_timer_watcher *watcher);
int evcon_timer_is_active(evcon_fd_watcher* watcher); /* 1 == started, 0 == stopped */

evcon_timer_cb evcon_timer_get_cb(evcon_timer_watcher *watcher);
evcon_interval evcon_timer_get_timeout(evcon_timer_watcher *watcher); /* last used timeout, not the time until next event. after a trigger this gets setted to the repeat value */
evcon_interval evcon_timer_get_repeat(evcon_timer_watcher *watcher);
void* evcon_timer_get_user_data(evcon_timer_watcher *watcher);
evcon_loop *evcon_timer_get_loop(evcon_timer_watcher *watcher);

void evcon_timer_set_cb(evcon_timer_watcher *watcher, evcon_timer_cb cb);
void evcon_timer_set_repeat(evcon_timer_watcher *watcher, evcon_interval repeat); /* set repeat value for the future, doesn't change current timer nor does it start the watcher */
void evcon_timer_set_user_data(evcon_timer_watcher *watcher, void *user_data);

/* async watcher.  */
evcon_async_watcher *evcon_async_new(evcon_loop *loop, evcon_async_cb cb, void *user_data);
void evcon_async_wakeup(evcon_async_watcher *watcher); /* thread-safe */
void evcon_async_free(evcon_async_watcher *watcher);

evcon_async_cb evcon_async_get_cb(evcon_async_watcher *watcher);
void* evcon_async_get_user_data(evcon_async_watcher *watcher);
evcon_loop *evcon_async_get_loop(evcon_async_watcher *watcher);

void evcon_async_set_cb(evcon_async_watcher *watcher, evcon_async_cb cb);
void evcon_async_set_user_data(evcon_async_watcher *watcher, void *user_data);

#endif
