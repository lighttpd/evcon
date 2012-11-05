
SUBDIRS = . src

EXTRA_DIST=README.md autogen.sh evcon.pc.in evcon-ev.pc.in evcon-glib.pc.in evcon-event.pc.in
EXTRA_DIST+=libevcon-ev0.symbols libevcon-event0.symbols libevcon-glib0.symbols libevcon0.symbols

ACLOCAL_AMFLAGS=-I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = evcon.pc evcon-ev.pc evcon-glib.pc evcon-event.pc

$(pkgconfig_DATA): config.status
