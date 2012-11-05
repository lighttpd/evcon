#ifndef __EVCON_EVCON_EV_H
#define __EVCON_EVCON_EV_H __EVCON_EVCON_EV_H

#include <evcon.h>

#include <ev.h>

evcon_loop* evcon_loop_from_ev(struct ev_loop *loop, evcon_allocator* allocator);

#endif
