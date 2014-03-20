/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

#include <ev.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"

static int _evapi_notify_sockets[2];

typedef struct _evapi_client {
	int connected;
	int sock;
} evapi_client_t;

#define EVAPI_MAX_CLIENTS	8
static evapi_client_t _evapi_clients[EVAPI_MAX_CLIENTS];

/**
 *
 */
int evapi_init_notify_sockets(void)
{
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, _evapi_notify_sockets) < 0) {
		LM_ERR("opening notify stream socket pair\n");
		return -1;
	}
	LM_DBG("inter-process event notification sockets initialized\n");
	return 0;
}

/**
 *
 */
void evapi_close_notify_sockets_child(void)
{
	LM_DBG("closing the notification socket used by children\n");
	close(_evapi_notify_sockets[1]);
}

/**
 *
 */
void evapi_close_notify_sockets_parent(void)
{
	LM_DBG("closing the notification socket used by parent\n");
	close(_evapi_notify_sockets[0]);
}

/**
 *
 */
int evapi_dispatch_notify(char *obuf, int olen)
{
	int i;
	int n;
	int wlen;

	n = 0;
	for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
		if(_evapi_clients[i].connected==1 && _evapi_clients[i].sock>0) {
			wlen = write(_evapi_clients[i].sock, obuf, olen);
			if(wlen!=olen) {
				LM_DBG("failed to write all packet (%d out of %d) on socket %d index [%d]\n",
						wlen, olen, _evapi_clients[i].sock, i);
			}
			n++;
		}
	}

	return n;
}

/**
 *
 */
void evapi_recv_client(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
#define CLIENT_BUFFER_SIZE	1024
	char rbuffer[CLIENT_BUFFER_SIZE];
	ssize_t rlen;
	int i;

	if(EV_ERROR & revents) {
		perror("received invalid event\n");
		return;
	}

	/* read message from client */
	rlen = recv(watcher->fd, rbuffer, CLIENT_BUFFER_SIZE, 0);

	if(rlen < 0) {
		LM_ERR("cannot read the client message\n");
		return;
	}

	if(rlen == 0) {
		/* client is gone */
		for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
			if(_evapi_clients[i].connected==1 && _evapi_clients[i].sock==watcher->fd) {
				_evapi_clients[i].connected = 0;
				_evapi_clients[i].sock = 0;
				break;
			}
		}
		ev_io_stop(loop, watcher);
		free(watcher);
		LM_INFO("client closing connection\n");
		return;
	}

	LM_NOTICE("received [%.*s]\n", (int)rlen, rbuffer);
}

/**
 *
 */
void evapi_accept_client(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	struct sockaddr_in caddr;
	socklen_t clen = sizeof(caddr);
	int csock;
	struct ev_io *evapi_client;
	int i;
	
	evapi_client = (struct ev_io*) malloc (sizeof(struct ev_io));
	if(evapi_client==NULL) {
		perror("no more memory\n");
		return;
	}

	if(EV_ERROR & revents) {
		perror("received invalid event\n");
		return;
	}

	/* accept new client connection */
	csock = accept(watcher->fd, (struct sockaddr *)&caddr, &clen);

	if (csock < 0) {
		LM_ERR("cannot accept the client\n");
		return;
	}
	for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
		if(_evapi_clients[i].connected==0) {
			_evapi_clients[i].connected = 1;
			_evapi_clients[i].sock = csock;
			break;
		}
	}
	if(i==EVAPI_MAX_CLIENTS) {
		LM_ERR("too many clients\n");
		close(csock);
		return;
	}

	/* start watcher to read messages from whatchers */
	ev_io_init(evapi_client, evapi_recv_client, csock, EV_READ);
	ev_io_start(loop, evapi_client);
}

/**
 *
 */
void evapi_recv_notify(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	str *sbuf;
	int rlen;

	if(EV_ERROR & revents) {
		perror("received invalid event\n");
		return;
	}

	/* read message from client */
	rlen = read(watcher->fd, &sbuf, sizeof(str*));

	if(rlen != sizeof(str*)) {
		LM_ERR("cannot read the sip worker message\n");
		return;
	}

	LM_DBG("received [%p] [%.*s] (%d)\n", sbuf, sbuf->len, sbuf->s, sbuf->len);
	evapi_dispatch_notify(sbuf->s, sbuf->len);
	shm_free(sbuf);
}

/**
 *
 */
int evapi_run_dispatcher(char *laddr, int lport)
{
	int evapi_srv_sock;
	struct sockaddr_in evapi_srv_addr;
	struct ev_loop *loop;
	struct hostent *h = NULL;
	struct ev_io io_server;
	struct ev_io io_notify;

	LM_DBG("starting dispatcher processing\n");

	memset(_evapi_clients, 0, sizeof(evapi_client_t) * EVAPI_MAX_CLIENTS);

	loop = ev_default_loop(0);

	if(loop==NULL) {
		LM_ERR("cannot get libev loop\n");
		return -1;
	}

    h = gethostbyname(laddr);
    if (h == NULL || (h->h_addrtype != AF_INET && h->h_addrtype != AF_INET6)) {
    	LM_ERR("cannot resolve local server address [%s]\n", laddr);
        return -1;
    }
    if(h->h_addrtype == AF_INET) {
    	evapi_srv_sock = socket(PF_INET, SOCK_STREAM, 0);
	} else {
		evapi_srv_sock = socket(PF_INET6, SOCK_STREAM, 0);
	}
	if( evapi_srv_sock < 0 )
	{
		LM_ERR("cannot create server socket (family %d)\n", h->h_addrtype);
		return -1;
	}
	/* set non-blocking flag */
	fcntl(evapi_srv_sock, F_SETFL, fcntl(evapi_srv_sock, F_GETFL) | O_NONBLOCK);

	bzero(&evapi_srv_addr, sizeof(evapi_srv_addr));
	evapi_srv_addr.sin_family = h->h_addrtype;
	evapi_srv_addr.sin_port   = htons((short)lport);
	evapi_srv_addr.sin_addr  = *(struct in_addr*)h->h_addr;

	if (bind(evapi_srv_sock, (struct sockaddr*)&evapi_srv_addr,
				sizeof(evapi_srv_addr)) < 0) {
		LM_ERR("cannot bind to local address and port [%s:%d]\n", laddr, lport);
		close(evapi_srv_sock);
		return -1;
	}
	if (listen(evapi_srv_sock, 4) < 0) {
		LM_ERR("listen error\n");
		close(evapi_srv_sock);
		return -1;
	}
	ev_io_init(&io_server, evapi_accept_client, evapi_srv_sock, EV_READ);
	ev_io_start(loop, &io_server);
	ev_io_init(&io_notify, evapi_recv_notify, _evapi_notify_sockets[0], EV_READ);
	ev_io_start(loop, &io_notify);

	while(1) {
		ev_loop (loop, 0);
	}

	return 0;
}

/**
 *
 */
int evapi_run_worker(int prank)
{
	LM_DBG("started worker process: %d\n", prank);
	while(1) {
		sleep(3);
	}
}

/**
 *
 */
int evapi_relay(str *evdata)
{
#define EVAPI_RELAY_FORMAT "%d:%.*s,"

	int len;
	int sbsize;
	str *sbuf;

	LM_DBG("relaying event data [%.*s]\n",
			evdata->len, evdata->s);

	sbsize = evdata->len;
	sbuf = (str*)shm_malloc(sizeof(str) + ((sbsize+32) * sizeof(char)));
	if(sbuf==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	sbuf->s = (char*)sbuf + sizeof(str);
	sbuf->len = snprintf(sbuf->s, sbsize+32,
			EVAPI_RELAY_FORMAT,
			sbsize, evdata->len, evdata->s);
	if(sbuf->len<=0 || sbuf->len>sbsize+32) {
		shm_free(sbuf);
		LM_ERR("cannot serialize event\n");
		return -1;
	}

	len = write(_evapi_notify_sockets[1], &sbuf, sizeof(str*));
	if(len<=0) {
		LM_ERR("failed to pass the pointer to evapi dispatcher\n");
		return -1;
	}
	LM_DBG("sent [%p] [%.*s] (%d)\n", sbuf, sbuf->len, sbuf->s, sbuf->len);
	return 0;
}

#if 0
/**
 *
 */
int evapi_relay(str *event, str *data)
{
#define EVAPI_RELAY_FORMAT "%d:{\n \"event\":\"%.*s\",\n \"data\":%.*s\n},"

	int len;
	int sbsize;
	str *sbuf;

	LM_DBG("relaying event [%.*s] data [%.*s]\n",
			event->len, event->s, data->len, data->s);

	sbsize = sizeof(EVAPI_RELAY_FORMAT) + event->len + data->len - 13;
	sbuf = (str*)shm_malloc(sizeof(str) + ((sbsize+32) * sizeof(char)));
	if(sbuf==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	sbuf->s = (char*)sbuf + sizeof(str);
	sbuf->len = snprintf(sbuf->s, sbsize+32,
			EVAPI_RELAY_FORMAT,
			sbsize, event->len, event->s, data->len, data->s);
	if(sbuf->len<=0 || sbuf->len>sbsize+32) {
		shm_free(sbuf);
		LM_ERR("cannot serialize event\n");
		return -1;
	}

	len = write(_evapi_notify_sockets[1], &sbuf, sizeof(str*));
	if(len<=0) {
		LM_ERR("failed to pass the pointer to evapi dispatcher\n");
		return -1;
	}
	LM_DBG("sent [%p] [%.*s] (%d)\n", sbuf, sbuf->len, sbuf->s, sbuf->len);
	return 0;
}
#endif
