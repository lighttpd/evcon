
#include "evcon-echo.h"
#include <evcon-ev.h>
#include <evcon-glib.h>

#define UNUSED(x) ((void)(x))

static void test_ev_client_finished_cb(EchoClient* client, void *user_data) {
	struct ev_loop *loop = (struct ev_loop*) user_data;
	UNUSED(client);

	ev_break(loop, EVBREAK_ONE);
}


static void test_ev(void) {
	struct ev_loop *l = ev_loop_new(0);
	evcon_allocator *alloc = evcon_glib_allocator();
	evcon_loop *loop = evcon_loop_from_ev(l, alloc);
	EchoClient *client;
	EchoServer *srv;

	srv = echo_server_new(loop);
	g_debug("Listening on port %i\n", srv->port);

	client = echo_client_new(srv, 5, test_ev_client_finished_cb, l);

	ev_run(l, 0);

	echo_client_free(client);
	echo_server_free(srv);

	evcon_loop_unref(loop);

	ev_loop_destroy(l);
}


int main(int argc, char** argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/evcon-echo/test-ev", test_ev);

	return g_test_run();
}
