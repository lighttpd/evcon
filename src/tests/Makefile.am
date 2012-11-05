
AM_CFLAGS = -I$(srcdir)/../core -I$(srcdir)/../backend-ev -I$(srcdir)/../backend-glib -I$(srcdir)/../backend-event
AM_CFLAGS += $(GLIB_CFLAGS) $(LIBEV_CFLAGS) $(LIBEVENT_CFLAGS)

test_binaries =

if BUILD_EV
test_binaries += evcon-test-ev
evcon_test_ev_SOURCES = evcon-test-ev.c evcon-echo.c
evcon_test_ev_LDFLAGS = -export-dynamic -avoid-version -no-undefined $(GLIB_LIBS) $(LIBEV_LIBS)
evcon_test_ev_LDADD = ../backend-ev/libevcon-ev.la ../backend-glib/libevcon-glib.la ../core/libevcon.la
endif

if BUILD_GLIB
test_binaries += evcon-test-glib
evcon_test_glib_SOURCES = evcon-test-glib.c evcon-echo.c
evcon_test_glib_LDFLAGS = -export-dynamic -avoid-version -no-undefined $(GLIB_LIBS)
evcon_test_glib_LDADD = ../backend-glib/libevcon-glib.la ../core/libevcon.la
endif

if BUILD_EVENT
test_binaries += evcon-test-event
evcon_test_event_SOURCES = evcon-test-event.c evcon-echo.c
evcon_test_event_LDFLAGS = -export-dynamic -avoid-version -no-undefined $(LIBEVENT_LIBS)
evcon_test_event_LDADD = ../backend-event/libevcon-event.la ../core/libevcon.la
endif

EXTRA_DIST = evcon-echo.h

check_PROGRAMS=$(test_binaries)

TESTS=$(test_binaries)
TESTS_ENVIRONMENT=gtester --verbose
