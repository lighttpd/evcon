#ifndef __EVCON_ECHO_H
#define __EVCON_ECHO_H __EVCON_ECHO_H

#include <evcon.h>
#include <glib.h>

#include <time.h>

typedef struct EchoServerConnection EchoServerConnection;
typedef struct EchoServer EchoServer;

struct EchoServerConnection {
	EchoServer *srv;
	evcon_fd_watcher *conn_watcher;
	GList con_link;
};
struct EchoServer {
	unsigned short port;
	evcon_loop *loop;
	evcon_fd_watcher *listen_watcher;
	GQueue connections; /* <EchoServerConnection> */
};

EchoServer* echo_server_new(evcon_loop *loop);
void echo_server_free(EchoServer *srv);


typedef struct EchoClientConnection EchoClientConnection;
typedef struct EchoClient EchoClient;
typedef void (*EchoClientFinishedCB)(EchoClient* client, void *user_data);

struct EchoClientConnection {
	EchoClient *client;
	evcon_fd_watcher *conn_watcher;
	evcon_timer_watcher *timout_watcher;
	struct timespec timer_start;
	GList con_link;
	gboolean connected, closing;
	char data[128];
	guint did_read, did_write;
};
struct EchoClient {
	EchoServer *srv;
	evcon_loop *loop;
	GQueue connections; /* <EchoClientConnection> */

	EchoClientFinishedCB finished_cb;
	void *finished_data;
};

EchoClient* echo_client_new(EchoServer *srv, int count, EchoClientFinishedCB finished_cb, void *finished_data);
void echo_client_free(EchoClient *client);

#endif
