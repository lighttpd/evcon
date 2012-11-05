AM_CFLAGS=-I$(srcdir)/../core

install_libs=
install_headers=

if BUILD_EVENT
install_libs += libevcon-event.la
install_headers += evcon-event.h
libevcon_event_la_CPPFLAGS = $(GLIB_CFLAGS) $(LIBEVENT_CFLAGS)
libevcon_event_la_LDFLAGS = -export-dynamic -no-undefined $(GLIB_LIBS) $(LIBEVENT_LIBS)
libevcon_event_la_SOURCES = event-backend.c
libevcon_event_la_LIBADD = ../core/libevcon.la
endif

lib_LTLIBRARIES = $(install_libs)
include_HEADERS = $(install_headers)
