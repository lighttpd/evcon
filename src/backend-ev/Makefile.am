AM_CFLAGS=-I$(srcdir)/../core

install_libs=
install_headers=

if BUILD_EV
install_libs += libevcon-ev.la
install_headers += evcon-ev.h
libevcon_ev_la_CPPFLAGS = $(GLIB_CFLAGS) $(LIBEV_CFLAGS)
libevcon_ev_la_LDFLAGS = -export-dynamic -no-undefined $(GLIB_LIBS) $(LIBEV_LIBS)
libevcon_ev_la_SOURCES = ev-backend.c
libevcon_ev_la_LIBADD = ../core/libevcon.la
endif

lib_LTLIBRARIES = $(install_libs)
include_HEADERS = $(install_headers)
