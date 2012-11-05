AM_CFLAGS=-I$(srcdir)/../core

install_libs=
install_headers=

if BUILD_QT
install_libs += libevcon-qt.la
install_headers += evcon-qt.h
libevcon_qt_la_CPPFLAGS = $(QT_CFLAGS)
libevcon_qt_la_LDFLAGS = -export-dynamic -no-undefined $(QT_LIBS)
libevcon_qt_la_SOURCES = evcon-qt.cpp
libevcon_qt_la_LIBADD = ../core/libevcon.la
endif

lib_LTLIBRARIES = $(install_libs)
include_HEADERS = $(install_headers)
