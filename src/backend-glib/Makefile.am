AM_CFLAGS=-I$(srcdir)/../core

install_libs=
install_headers=

if BUILD_GLIB
install_libs += libevcon-glib.la
install_headers += evcon-glib.h
libevcon_glib_la_CPPFLAGS = $(GLIB_CFLAGS)
libevcon_glib_la_LDFLAGS = -export-dynamic -no-undefined $(GLIB_LIBS)
libevcon_glib_la_SOURCES = glib-allocator.c glib-backend.c
libevcon_glib_la_LIBADD = ../core/libevcon.la
endif

lib_LTLIBRARIES = $(install_libs)
include_HEADERS = $(install_headers)
