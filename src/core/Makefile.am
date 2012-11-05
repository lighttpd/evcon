
install_libs=
install_headers=

install_libs += libevcon.la
install_headers += evcon.h evcon-config.h evcon-allocator.h evcon-backend.h
libevcon_la_LDFLAGS = -export-dynamic -no-undefined
libevcon_la_SOURCES = evcon.c

lib_LTLIBRARIES = $(install_libs)
include_HEADERS = $(install_headers)

dist-hook:
	rm -f $(distdir)/evcon-config.h
