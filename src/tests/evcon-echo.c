
#include "evcon-echo.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define UNUSED(x) ((void)(x))

static void echo_server_con_cb(evcon_loop *loop, evcon_fd_watcher *watcher, evcon_fd fd, int revents, void* user_data) {
	static char buf[512];
	EchoServerConnection *con = (EchoServerConnection*) user_data;
	int r, r1;
	UNUSED(loop);
	UNUSED(watcher);
	UNUSED(revents);

	r = read(fd, buf, sizeof(buf));

	if (-1 == r) {
		switch (errno) {
		case EINTR:
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			return;
		default:
			g_warning("Connection error (fatal): %s\n", g_strerror(errno));
			goto closecon;
		}
	}
	if (0 == r) goto closecon;

	r1 = write(fd, buf, r);

	if (-1 == r1) {
		switch (errno) {
		case EINTR:
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
			g_warning("Connection: write failed (temporary error), loosing buffer: %s\n", g_strerror(errno));
			return;
		default:
			g_warning("Connection error (fatal): %s\n", g_strerror(errno));
			goto closecon;
		}
	}

	if (r1 < r) {
		g_warning("Connection: write not complete, loosing remaining data\n");
	}

	return;

closecon:
	shutdown(fd, SHUT_RDWR);
	close(fd);
	evcon_fd_free(con->conn_watcher);
	g_queue_unlink(&con->srv->connections, &con->con_link);
	g_slice_free(EchoServerConnection, con);
}

static EchoServerConnection* echo_server_con_new(EchoServer* srv, evcon_fd fd) {
	EchoServerConnection *con = g_slice_new0(EchoServerConnection);
	evcon_init_fd(fd);
	con->srv = srv;
	con->conn_watcher = evcon_fd_new(srv->loop, echo_server_con_cb, fd, EVCON_READ, con);
	evcon_fd_start(con->conn_watcher);
	con->con_link.data = con;
	g_queue_push_tail_link(&srv->connections, &con->con_link);
	return con;
}

static void echo_server_listen_cb(evcon_loop *loop, evcon_fd_watcher *watcher, evcon_fd fd, int revents, void* user_data) {
	EchoServer *srv = (EchoServer*) user_data;
	int confd;
	UNUSED(loop);
	UNUSED(watcher);
	UNUSED(revents);

	while (-1 != (confd = accept(fd, NULL, NULL))) {
		echo_server_con_new(srv, confd);
	}

	switch (errno) {
	case EINTR:
	case EAGAIN:
#if EWOULDBLOCK != EAGAIN
	case EWOULDBLOCK:
#endif
		return;
	default:
		g_error("accept() failed: %s\n", g_strerror(errno));
		break;
	}
}

EchoServer* echo_server_new(evcon_loop *loop) {
	EchoServer *srv = g_slice_new0(EchoServer);
	struct sockaddr_in addr;
	int fd;

	evcon_loop_ref(loop);
	srv->loop = loop;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	evcon_init_fd(fd);
	if (-1 == fd) g_error("socket() failed: %s\n", g_strerror(errno));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	if (-1 == bind(fd, (struct sockaddr*) &addr, sizeof(addr))) g_error("bind() failed: %s\n", g_strerror(errno));

	if (-1 == listen(fd, 128)) g_error("listen() failed: %s\n", g_strerror(errno));

	{
		socklen_t addrlen = sizeof(addr);
		if (-1 == getsockname(fd, (struct sockaddr*) &addr, &addrlen)) g_error("getsockname() failed: %s\n", g_strerror(errno));
		srv->port = ntohs(addr.sin_port);
	}

	srv->listen_watcher = evcon_fd_new(srv->loop, echo_server_listen_cb, fd, EVCON_READ, srv);
	evcon_fd_start(srv->listen_watcher);

	return srv;
}

void echo_server_free(EchoServer *srv) {
	GList *link;

	while (NULL != (link = g_queue_pop_head_link(&srv->connections))) {
		EchoServerConnection *con = link->data;
		int fd = evcon_fd_get_fd(con->conn_watcher);

		shutdown(fd, SHUT_RDWR);
		close(fd);
		evcon_fd_free(con->conn_watcher);
		g_slice_free(EchoServerConnection, con);
	}

	close(evcon_fd_get_fd(srv->listen_watcher));
	evcon_fd_free(srv->listen_watcher);

	evcon_loop_unref(srv->loop);
	g_slice_free(EchoServer, srv);
}

#define ECHO_CLIENT_TIMOUT_MSEC (250)

static void echo_client_con_timer_once(EchoClientConnection *con) {
	clock_gettime(CLOCK_MONOTONIC, &con->timer_start);
	evcon_timer_once(con->timout_watcher, EVCON_INTERVAL_FROM_MSEC(ECHO_CLIENT_TIMOUT_MSEC));
}

static void echo_client_con_check_timeout(EchoClientConnection *con) {
	struct timespec ts;
	int msecs;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	msecs = 1000*(ts.tv_sec - con->timer_start.tv_sec) + (ts.tv_nsec - con->timer_start.tv_nsec + 999999)/1000000;

	g_debug("timout triggered after %ims", msecs);

	if (msecs > ECHO_CLIENT_TIMOUT_MSEC + 50) {
		if (msecs > ECHO_CLIENT_TIMOUT_MSEC + 2000) {
			g_error("timeout triggered more than 2s too late (after %ims)", msecs);
		} else {
			g_message("timeout triggered more than 50ms too late (after %ims)", msecs);
		}
	} else if (msecs < ECHO_CLIENT_TIMOUT_MSEC - 5) {
		g_error("timeout triggered more than 5ms too early (after %ims)", msecs);
	}
}

static void echo_client_fd_cb(evcon_loop *loop, evcon_fd_watcher *watcher, evcon_fd fd, int revents, void* user_data) {
	EchoClientConnection *con = (EchoClientConnection*) user_data;
	EchoClient *client = con->client;
	int r;
	UNUSED(loop);
	UNUSED(watcher);

	g_debug("fd %i cb: read=%i, wrote=%i, data size=%i, closing=%i, connected=%i\n", fd, con->did_read, con->did_write, (int) sizeof(con->data), con->closing, con->connected);

	if (!con->connected) {
		struct sockaddr addr;
		socklen_t len;

		len = sizeof(addr);
		if (-1 == getpeername(fd, &addr, &len)) {
			/* connect failed; find out why */
			int err;
			len = sizeof(err);
#ifdef SO_ERROR
			getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&err, &len);
#else
			{
				char ch;
				errno = 0;
				read(fd, &ch, 1);
				err = errno;
			}
#endif
			g_error("Couldn't connect: %s\n", g_strerror(errno));
		}

		con->connected = TRUE;
		echo_client_con_timer_once(con);
		revents = EVCON_WRITE;
	}

	if (0 != (EVCON_WRITE & revents)) {
		if (con->did_write < sizeof(con->data)) {
			r = write(fd, con->data + con->did_write, sizeof(con->data) - con->did_write);
			if (0 > r) {
				switch (errno) {
				case EINTR:
				case EAGAIN:
#if EWOULDBLOCK != EAGAIN
				case EWOULDBLOCK:
#endif
					break;
				default:
					g_error("write() failed: %s\n", g_strerror(errno));
				}
			} else {
				con->did_write += r;
			}
		} else {
			g_warning("got write event for fd %i we didn't want", fd);
		}
		if (con->did_write == sizeof(con->data)) {
			evcon_fd_set_events(con->conn_watcher, EVCON_READ);
		}
	}

	if (0 != (EVCON_READ & revents)) {
		char buf[256];

		r = read(fd, buf, sizeof(buf));
		if (-1 == r) {
			switch (errno) {
			case EINTR:
			case EAGAIN:
#if EWOULDBLOCK != EAGAIN
			case EWOULDBLOCK:
#endif
				break;
			default:
				g_error("read() failed: %s\n", g_strerror(errno));
			}
		} else if (0 == r) {
			if (!con->closing) g_error("unexpected EOF");

			close(fd);
			evcon_fd_free(con->conn_watcher);
			g_queue_unlink(&client->connections, &con->con_link);
			g_slice_free(EchoClientConnection, con);
			if (0 == client->connections.length) {
				client->finished_cb(client, client->finished_data);
			}
			return;
		} else {
			if (r + con->did_read > con->did_write) g_error("received more than we sent");
			if (0 != memcmp(con->data + con->did_read, buf, r)) g_error("received non matching data");
			con->did_read += r;
		}
	}
}

static void echo_client_timeout_cb(evcon_loop *loop, evcon_timer_watcher *watcher, void* user_data) {
	EchoClientConnection *con = (EchoClientConnection*) user_data;
	UNUSED(loop);
	UNUSED(watcher);

	echo_client_con_check_timeout(con);

	if (!con->connected) {
		g_error("didn't connect before timeout");
	}
	if (!con->closing) {
		if (con->did_write < sizeof(con->data)) {
			g_error("didn't write all data before timeout");
		}
		if (con->did_read < con->did_write) {
			g_error("didn't receive all data back before timeout");
		}
		con->closing = TRUE;
		shutdown(evcon_fd_get_fd(con->conn_watcher), SHUT_WR);
	} else {
		g_error("server didn't close connection before timeout");
	}
}

EchoClient* echo_client_new(EchoServer *srv, int count, EchoClientFinishedCB finished_cb, void *finished_data) {
	EchoClient* client = g_slice_new0(EchoClient);
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(srv->port);

	client->finished_cb = finished_cb;
	client->finished_data = finished_data;

	client->srv = srv;
	evcon_loop_ref(srv->loop);
	client->loop = srv->loop;

	for (int i = 0; i < count; ++i) {
		EchoClientConnection *con = g_slice_new0(EchoClientConnection);
		int fd;
		con->client = client;
		for (guint j = 0; j  < sizeof(con->data); ++j) con->data[j] = 'A' + (i << 4) + j;

		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (-1 == fd) g_error("socket() failed: %s\n", g_strerror(errno));
		evcon_init_fd(fd);
		con->connected = FALSE;
		if (-1 == connect(fd, (struct sockaddr*) &addr, sizeof(addr))) {
			switch (errno) {
			case EINPROGRESS:
			case EALREADY:
			case EINTR:
				break;
			default:
				g_error("connect() failed: %s\n", g_strerror(errno));
			}
		} else {
			con->connected = TRUE;
		}
		con->conn_watcher = evcon_fd_new(client->loop, echo_client_fd_cb, fd, EVCON_READ | EVCON_WRITE, con);
		evcon_fd_start(con->conn_watcher);
		con->timout_watcher = evcon_timer_new(client->loop, echo_client_timeout_cb, con);
		echo_client_con_timer_once(con);

		con->con_link.data = con;
		g_queue_push_tail_link(&client->connections, &con->con_link);
	}

	return client;
}

void echo_client_free(EchoClient *client) {
	GList *list;

	while (NULL != (list = g_queue_pop_head_link(&client->connections))) {
		EchoClientConnection *con = (EchoClientConnection*) list->data;

		close(evcon_fd_get_fd(con->conn_watcher));
		evcon_fd_free(con->conn_watcher);
		evcon_timer_free(con->timout_watcher);
		g_slice_free(EchoClientConnection, con);
	}

	evcon_loop_unref(client->loop);
	g_slice_free(EchoClient, client);
}
