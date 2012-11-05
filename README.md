Description
-----------

evon is a generic wrapper library that sits between libraries that need socket (file descriptor), timeout and (thread safe) asynchronous events, and an application that wants to use the library.

Platforms
---------

Should work on all POSIX compatible platforms.

Features
--------

Event types:

* read and write events for asynchronous file descriptors (sockets)
* simple timeout events
* (thread safe) asynchronous events (notifications - for example from other threads, that wakeup the event loop)

Backends for:

* [libev](http://software.schmorp.de/pkg/libev.html)
* [libevent](http://libevent.org/)
* [glib](http://developer.gnome.org/glib/unstable/glib-The-Main-Event-Loop.html)


Simple Scenario
---------------

You want to write an application that uses two different event based libraries; if these libraries don't use the same event loop (say X uses libev and Y libevent) you will have difficulties using them - you could give each library its own thread for example, but embedding one event loop in another is usually not easy.

If the libraries were built against evcon, you could use any event loop - if there is no backend for it yet, you probably can write one.

Examples
--------

See `src/tests/evcon-echo.c` and `src/tests/evcon-echo.h` for an example "library", and `src/tests/evcon-test-*.c` for how to use them in an application.

Building from git
-----------------

Run the following in the source directory to prepare the build system:

    ./autogen.sh

You will need automake and autoconf for this.

Building
--------

All backends need glib (>= 2.14).
The libev backend needs libev >= 4, the libevent backend needs libevent >= 2.

Build in a sub directory:

    mkdir build
    cd build
    ../configure
    make check

Install (probably has to be run as root):

    make install

As always it is recommended to use a package system to install files instead (dpkg, rpm, ...).
