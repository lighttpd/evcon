#ifndef __EVCON_EVCON_GLIB_H
#define __EVCON_EVCON_GLIB_H __EVCON_EVCON_GLIB_H

#include <evcon.h>

#include <glib.h>

evcon_allocator* evcon_glib_allocator(void);
evcon_loop* evcon_loop_from_glib(GMainContext *ctx, evcon_allocator *allocator);

GMainContext* evcon_loop_glib_get_context(evcon_loop *loop);

#endif
