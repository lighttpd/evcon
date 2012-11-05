#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([evcon], [0.1.0], [lighttpd@stbuehler.de])
AC_CONFIG_SRCDIR([src/core/evcon.c])
AC_CONFIG_HEADERS([src/core/evcon-config-private.h])
AC_CONFIG_HEADERS([src/core/evcon-config.h])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

m4_ifndef([PKG_PROG_PKG_CONFIG], [m4_fatal([pkg-config not installed])])
m4_ifndef([AC_PROG_LIBTOOL], [m4_fatal([libtool not installed])])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET


# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([dup2 pipe2])

# Checks for libraries.

AC_ARG_ENABLE([glib], AS_HELP_STRING([--disable-glib], [Disable building glib wrapper]), [build_glib=no], [build_glib=yes])
AC_ARG_ENABLE([ev], AS_HELP_STRING([--disable-ev], [Disable building ev wrapper]), [build_ev=no], [build_ev=yes])
AC_ARG_ENABLE([event], AS_HELP_STRING([--disable-event], [Disable building event wrapper]), [build_event=no], [build_event=yes])

if test "x${build_glib}" != "xno" -o "x${build_ev}" != "xno" -o "x${build_event}" != "xno"; then
	AC_MSG_CHECKING([Enabled at least one backend. Requires glib.])

	# glib
	PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.16.0], [],[AC_MSG_ERROR("glib-2.0 >= 2.16.0 not found")])
fi

AC_ARG_ENABLE(glib-compat,
 AC_HELP_STRING([--enable-glib-compat],[build for older glib versions even with new headers]),
 AC_DEFINE([EVCON_GLIB_COMPAT_API], [1], [build for older glib versions even with new headers]),[])

LIBEV_CFLAGS=""
LIBEV_LIBS=""
if test "x${build_ev}" != "xno"; then
	AC_MSG_CHECKING([Enabled ev wrapper. Requires libev.])

	# libev
	AC_MSG_CHECKING([for libev support])
	AC_ARG_WITH([libev],
		[AS_HELP_STRING([--with-libev@<:@=PATH@:>@],[Search for libev in PATH/include and PATH/lib])],
		[WITH_LIBEV=$withval],[WITH_LIBEV=yes])

	PKG_CHECK_MODULES([LIBEV], [libev], [], [
		# no pkg-config for libev, searching manually:

		if test "$WITH_LIBEV" != "yes"; then
			LIBEV_CFLAGS="-I$WITH_LIBEV/include"
			LIBEV_LIBS="-L$WITH_LIBEV/lib -lev"
		else
			AC_CHECK_HEADERS([ev.h],[
				AC_CHECK_LIB([ev], [ev_time], [
						LIBEV_LIBS="-lev"
					],[
						AC_MSG_ERROR([libev not found])
					]
				)],[
					AC_MSG_ERROR([libev not found])
				]
			)
		fi
	])
fi
AC_SUBST([LIBEV_CFLAGS])
AC_SUBST([LIBEV_LIBS])

if test "x${build_event}" != "xno"; then
	AC_MSG_CHECKING([Enabled event wrapper. Requires libevent.])

	# event
	PKG_CHECK_MODULES([LIBEVENT], [libevent >= 2], [
		# we want event_core, not event
		LIBEVENT_LIBS=`echo "$LIBEVENT_LIBS" | sed 's#\(^\| \)-levent\($\| \)# -levent_core #'`
	],[AC_MSG_ERROR("libevent >= 2 not found")])
fi


AM_CONDITIONAL([BUILD_GLIB], [test "x${build_glib}" != "xno"])
AM_CONDITIONAL([BUILD_EV], [test "x${build_ev}" != "xno"])
AM_CONDITIONAL([BUILD_EVENT], [test "x${build_event}" != "xno"])


#AC_ARG_ENABLE([qt], AS_HELP_STRING([--disable-qt], [Disable building qt wrapper]), [build_qt=$withval], [build_qt=yes])
#
#if test "x${build_qt}" != "xno"; then
#	AC_MSG_CHECKING([Enabled qt wrapper. Requires c++ and qt4.])
#	AC_PROG_CXX
#	PKG_CHECK_MODULES([QT], [QtCore >= 4.0.0], [],[AC_MSG_ERROR("QtCore >= 4.0.0 not found")])
#fi
#AM_CONDITIONAL([BUILD_QT], [test "x${build_qt}" != "xno"])


# check for extra compiler options (warning options)
if test "${GCC}" = "yes"; then
    CFLAGS="${CFLAGS} -Wall -W -Wshadow -pedantic -std=gnu99"
fi

AC_ARG_ENABLE(extra-warnings,
 AC_HELP_STRING([--enable-extra-warnings],[enable extra warnings (gcc specific)]),
 [case "${enableval}" in
   yes) extrawarnings=true ;;
    no) extrawarnings=false ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-extra-warnings) ;;
  esac],[extrawarnings=false])

if test x$extrawarnings = xtrue; then
    CFLAGS="${CFLAGS} -g -O2 -g2 -Wall -Wmissing-declarations -Wdeclaration-after-statement -Wno-pointer-sign -Wcast-align -Winline -Wsign-compare -Wnested-externs -Wpointer-arith -Wl,--as-needed -Wformat-security"
fi

AC_CONFIG_FILES([Makefile src/Makefile src/core/Makefile src/backend-glib/Makefile src/backend-ev/Makefile src/backend-event/Makefile src/tests/Makefile evcon.pc evcon-ev.pc evcon-glib.pc evcon-event.pc])
AC_OUTPUT
