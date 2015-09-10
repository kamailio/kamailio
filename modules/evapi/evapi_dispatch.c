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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <ev.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/kcore/faked_msg.h"

#include "evapi_dispatch.h"

static int _evapi_notify_sockets[2];
static int _evapi_netstring_format = 1;

#define EVAPI_IPADDR_SIZE	64
typedef struct _evapi_client {
	int connected;
	int sock;
	unsigned short af;
	unsigned short src_port;
	char src_addr[EVAPI_IPADDR_SIZE];
} evapi_client_t;

typedef struct _evapi_env {
	int eset;
	int conidx;
	str msg;
} evapi_env_t;

#define EVAPI_MAX_CLIENTS	8
static evapi_client_t _evapi_clients[EVAPI_MAX_CLIENTS];

typedef struct _evapi_evroutes {
	int con_new;
	int con_closed;
	int msg_received;
} evapi_evroutes_t;

static evapi_evroutes_t _evapi_rts;

/**
 *
 */
void evapi_env_reset(evapi_env_t *evenv)
{
	if(evenv==0)
		return;
	memset(evenv, 0, sizeof(evapi_env_t));
	evenv->conidx = -1;
}

/**
 *
 */
void evapi_init_environment(int dformat)
{
	memset(&_evapi_rts, 0, sizeof(evapi_evroutes_t));

	_evapi_rts.con_new = route_get(&event_rt, "evapi:connection-new");
	if (_evapi_rts.con_new < 0 || event_rt.rlist[_evapi_rts.con_new] == NULL)
		_evapi_rts.con_new = -1;
	_evapi_rts.con_closed = route_get(&event_rt, "evapi:connection-closed");
	if (_evapi_rts.con_closed < 0 || event_rt.rlist[_evapi_rts.con_closed] == NULL)
		_evapi_rts.con_closed = -1;
	_evapi_rts.msg_received = route_get(&event_rt, "evapi:message-received");
	if (_evapi_rts.msg_received < 0 || event_rt.rlist[_evapi_rts.msg_received] == NULL)
		_evapi_rts.msg_received = -1;
	_evapi_netstring_format = dformat;
}

/**
 *
 */
int evapi_run_cfg_route(evapi_env_t *evenv, int rt)
{
	int backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sip_msg_t tmsg;

	if(evenv==0 || evenv->eset==0) {
		LM_ERR("evapi env not set\n");
		return -1;
	}

	if(rt<0)
		return 0;

	fmsg = faked_msg_next();
	memcpy(&tmsg, fmsg, sizeof(sip_msg_t));
	fmsg = &tmsg;
	evapi_set_msg_env(fmsg, evenv);
	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(backup_rt);
	evapi_set_msg_env(fmsg, NULL);
	return 0;
}

/**
 *
 */
int evapi_close_connection(int cidx)
{
	if(cidx<0 || cidx>=EVAPI_MAX_CLIENTS)
		return -1;
	if(_evapi_clients[cidx].connected==1
			&& _evapi_clients[cidx].sock > 0) {
		close(_evapi_clients[cidx].sock);
		_evapi_clients[cidx].connected = 0;
		_evapi_clients[cidx].sock = 0;
		return 0;
	}
	return -2;
}

/**
 *
 */
int evapi_cfg_close(sip_msg_t *msg)
{
	evapi_env_t *evenv;

	if(msg==NULL)
		return -1;

	evenv = (evapi_env_t*)msg->date;

	if(evenv==NULL || evenv->conidx<0 || evenv->conidx>=EVAPI_MAX_CLIENTS)
		return -1;
	return evapi_close_connection(evenv->conidx);
}

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
#define CLIENT_BUFFER_SIZE	32768
	char rbuffer[CLIENT_BUFFER_SIZE];
	ssize_t rlen;
	int i, k;
	evapi_env_t evenv;
	str frame;

	if(EV_ERROR & revents) {
		perror("received invalid event\n");
		return;
	}

	/* read message from client */
	rlen = recv(watcher->fd, rbuffer, CLIENT_BUFFER_SIZE-1, 0);

	if(rlen < 0) {
		LM_ERR("cannot read the client message\n");
		return;
	}

	for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
		if(_evapi_clients[i].connected==1 && _evapi_clients[i].sock==watcher->fd) {
			break;
		}
	}
	if(i==EVAPI_MAX_CLIENTS) {
		LM_ERR("cannot lookup client socket %d\n", watcher->fd);
		return;
	}

	cfg_update();

	evapi_env_reset(&evenv);
	if(rlen == 0) {
		/* client is gone */
		evenv.eset = 1;
		evenv.conidx = i;
		evapi_run_cfg_route(&evenv, _evapi_rts.con_closed);
		_evapi_clients[i].connected = 0;
		_evapi_clients[i].sock = 0;
		ev_io_stop(loop, watcher);
		free(watcher);
		LM_INFO("client closing connection - pos [%d] addr [%s:%d]\n",
				i, _evapi_clients[i].src_addr, _evapi_clients[i].src_port);
		return;
	}

	rbuffer[rlen] = '\0';

	LM_NOTICE("{%d} [%s:%d] - received [%.*s]\n",
			i, _evapi_clients[i].src_addr, _evapi_clients[i].src_port,
			(int)rlen, rbuffer);
	evenv.conidx = i;
	evenv.eset = 1;
	if(_evapi_netstring_format) {
		/* netstring decapsulation */
		k = 0;
		while(k<rlen) {
			frame.len = 0;
			while(k<rlen) {
				if(rbuffer[k]==' ' || rbuffer[k]=='\t'
						|| rbuffer[k]=='\r' || rbuffer[k]=='\n')
					k++;
				else break;
			}
			if(k==rlen) return;
			while(k<rlen) {
				if(rbuffer[k]>='0' && rbuffer[k]<='9') {
					frame.len = frame.len*10 + rbuffer[k] - '0';
				} else {
					if(rbuffer[k]==':')
						break;
					/* invalid character - discard the rest */
					return;
				}
				k++;
			}
			if(k==rlen || frame.len<=0) return;
			if(frame.len + k>=rlen) return;
			k++;
			frame.s = rbuffer + k;
			if(frame.s[frame.len]!=',') return;
			frame.s[frame.len] = '\0';
			k += frame.len ;
			evenv.msg.s = frame.s;
			evenv.msg.len = frame.len;
			evapi_run_cfg_route(&evenv, _evapi_rts.msg_received);
			k++;
		}
	} else {
		evenv.msg.s = rbuffer;
		evenv.msg.len = rlen;
		evapi_run_cfg_route(&evenv, _evapi_rts.msg_received);
	}
}

/**
 *
 */
void evapi_accept_client(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	struct sockaddr caddr;
	socklen_t clen = sizeof(caddr);
	int csock;
	struct ev_io *evapi_client;
	int i;
	evapi_env_t evenv;
	
	evapi_client = (struct ev_io*) malloc (sizeof(struct ev_io));
	if(evapi_client==NULL) {
		LM_ERR("no more memory\n");
		return;
	}

	if(EV_ERROR & revents) {
		LM_ERR("received invalid event\n");
		free(evapi_client);
		return;
	}

	cfg_update();

	/* accept new client connection */
	csock = accept(watcher->fd, (struct sockaddr *)&caddr, &clen);

	if (csock < 0) {
		LM_ERR("cannot accept the client\n");
		free(evapi_client);
		return;
	}
	for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
		if(_evapi_clients[i].connected==0) {
			if (caddr.sa_family == AF_INET) {
				_evapi_clients[i].src_port = ntohs(((struct sockaddr_in*)&caddr)->sin_port);
				if(inet_ntop(AF_INET, &((struct sockaddr_in*)&caddr)->sin_addr,
							_evapi_clients[i].src_addr,
							EVAPI_IPADDR_SIZE)==NULL) {
					LM_ERR("cannot convert ipv4 address\n");
					close(csock);
					free(evapi_client);
					return;
				}
			} else {
				_evapi_clients[i].src_port = ntohs(((struct sockaddr_in6*)&caddr)->sin6_port);
				if(inet_ntop(AF_INET6, &((struct sockaddr_in6*)&caddr)->sin6_addr,
							_evapi_clients[i].src_addr,
							EVAPI_IPADDR_SIZE)==NULL) {
					LM_ERR("cannot convert ipv6 address\n");
					close(csock);
					free(evapi_client);
					return;
				}
			}
			_evapi_clients[i].connected = 1;
			_evapi_clients[i].sock = csock;
			_evapi_clients[i].af = caddr.sa_family;
			break;
		}
	}
	if(i==EVAPI_MAX_CLIENTS) {
		LM_ERR("too many clients\n");
		close(csock);
		free(evapi_client);
		return;
	}

	LM_DBG("new connection - pos[%d] from: [%s:%d]\n", i,
			_evapi_clients[i].src_addr, _evapi_clients[i].src_port);

	evapi_env_reset(&evenv);
	evenv.conidx = i;
	evenv.eset = 1;
	evapi_run_cfg_route(&evenv, _evapi_rts.con_new);

	if(_evapi_clients[i].connected == 0) {
		free(evapi_client);
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

	cfg_update();

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
	int yes_true = 1;

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

	/* Set SO_REUSEADDR option on listening socket so that we don't
	 * have to wait for connections in TIME_WAIT to go away before 
	 * re-binding.
	 */

	if(setsockopt(evapi_srv_sock, SOL_SOCKET, SO_REUSEADDR, 
		&yes_true, sizeof(int)) < 0) {
		LM_ERR("cannot set SO_REUSEADDR option on descriptor\n");
		close(evapi_srv_sock);
		return -1;
	}

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

	LM_DBG("relaying event data [%.*s] (%d)\n",
			evdata->len, evdata->s, evdata->len);

	sbsize = evdata->len;
	sbuf = (str*)shm_malloc(sizeof(str) + ((sbsize+32) * sizeof(char)));
	if(sbuf==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	sbuf->s = (char*)sbuf + sizeof(str);
	if(_evapi_netstring_format) {
		/* netstring encapsulation */
		sbuf->len = snprintf(sbuf->s, sbsize+32,
				EVAPI_RELAY_FORMAT,
				sbsize, evdata->len, evdata->s);
	} else {
		sbuf->len = snprintf(sbuf->s, sbsize+32,
				"%.*s",
				evdata->len, evdata->s);
	}
	if(sbuf->len<=0 || sbuf->len>sbsize+32) {
		shm_free(sbuf);
		LM_ERR("cannot serialize event\n");
		return -1;
	}

	LM_DBG("sending [%p] [%.*s] (%d)\n", sbuf, sbuf->len, sbuf->s, sbuf->len);
	len = write(_evapi_notify_sockets[1], &sbuf, sizeof(str*));
	if(len<=0) {
		LM_ERR("failed to pass the pointer to evapi dispatcher\n");
		return -1;
	}
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

/**
 *
 */
int pv_parse_evapi_name(pv_spec_t *sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3:
			if(strncmp(in->s, "msg", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 6:
			if(strncmp(in->s, "conidx", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 7:
			if(strncmp(in->s, "srcaddr", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "srcport", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV msrp name %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
int pv_get_evapi(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	evapi_env_t *evenv;

	if(param==NULL || res==NULL)
		return -1;

	evenv = (evapi_env_t*)msg->date;

	if(evenv==NULL || evenv->conidx<0 || evenv->conidx>=EVAPI_MAX_CLIENTS)
		return pv_get_null(msg, param, res);

	if(_evapi_clients[evenv->conidx].connected==0
			&& _evapi_clients[evenv->conidx].sock <= 0)
		return pv_get_null(msg, param, res);

	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			return pv_get_sintval(msg, param, res, evenv->conidx);
		case 1:
			if(evenv->msg.s==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &evenv->msg);
		case 2:
			return pv_get_strzval(msg, param, res,
					_evapi_clients[evenv->conidx].src_addr);
		case 3:
			return pv_get_sintval(msg, param, res,
					_evapi_clients[evenv->conidx].src_port);
		default:
			return pv_get_null(msg, param, res);
	}

	return 0;
}

/**
 *
 */
int pv_set_evapi(sip_msg_t *msg, pv_param_t *param, int op,
		pv_value_t *val)
{
	return 0;
}
