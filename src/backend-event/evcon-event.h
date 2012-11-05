#ifndef __EVCON_EVCON_EVENT_H
#define __EVCON_EVCON_EVENT_H __EVCON_EVCON_EVENT_H

#include <evcon.h>

#include <event2/event.h>

evcon_loop* evcon_loop_from_event(struct event_base *base, evcon_allocator* allocator);

#endif
