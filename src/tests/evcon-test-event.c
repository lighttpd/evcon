
#include "evcon-echo.h"

#include <evcon-event.h>

#define UNUSED(x) ((void)(x))

static void test_event_client_finished_cb(EchoClient* client, void *user_data) {
	struct event_base *base = (struct event_base*) user_data;
	UNUSED(client);

	event_base_loopbreak(base);
}


static void test_event(void) {
	struct event_base *base = event_base_new();
	evcon_loop *loop = evcon_loop_from_event(base, NULL);
	EchoClient *client;
	EchoServer *srv;

	srv = echo_server_new(loop);
	g_debug("Listening on port %i\n", srv->port);

	client = echo_client_new(srv, 5, test_event_client_finished_cb, base);

	event_base_dispatch(base);

	echo_client_free(client);
	echo_server_free(srv);

	evcon_loop_unref(loop);

	event_base_free(base);
}

int main(int argc, char** argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/evcon-echo/test-event", test_event);

	return g_test_run();
}
