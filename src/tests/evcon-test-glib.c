
#include "evcon-echo.h"
#include <evcon-glib.h>

#define UNUSED(x) ((void)(x))

static void test_glib_client_finished_cb(EchoClient* client, void *user_data) {
	GMainLoop *loop = (GMainLoop*) user_data;
	UNUSED(client);

	g_main_loop_quit(loop);
}


static void test_glib(void) {
	GMainContext *ctx = g_main_context_new();
	GMainLoop *gloop = g_main_loop_new(ctx, FALSE);
	evcon_allocator *alloc = evcon_glib_allocator();
	evcon_loop *loop = evcon_loop_from_glib(ctx, alloc);
	EchoClient *client;
	EchoServer *srv;

	srv = echo_server_new(loop);
	g_debug("Listening on port %i\n", srv->port);

	client = echo_client_new(srv, 5, test_glib_client_finished_cb, gloop);

	g_main_loop_run(gloop);

	echo_client_free(client);
	echo_server_free(srv);

	evcon_loop_unref(loop);

	g_main_loop_unref(gloop);
	g_main_context_unref(ctx);
}



int main(int argc, char** argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/evcon-echo/test-glib", test_glib);

	return g_test_run();
}
