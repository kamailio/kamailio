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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/receive.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"

#include "evapi_dispatch.h"

static int _evapi_notify_sockets[2];
static int _evapi_netstring_format = 1;

extern str _evapi_event_callback;
extern int _evapi_dispatcher_pid;
extern int _evapi_max_clients;

#define EVAPI_IPADDR_SIZE	64
#define EVAPI_TAG_SIZE	64
#define CLIENT_BUFFER_SIZE	32768
typedef struct _evapi_client {
	int connected;
	int sock;
	unsigned short af;
	unsigned short src_port;
	char src_addr[EVAPI_IPADDR_SIZE];
	char tag[EVAPI_IPADDR_SIZE];
	str  stag;
	char rbuffer[CLIENT_BUFFER_SIZE];
	unsigned int rpos;
} evapi_client_t;

typedef struct _evapi_env {
	int eset;
	int conidx;
	str msg;
} evapi_env_t;

typedef struct _evapi_msg {
	str data;
	str tag;
	int unicast;
} evapi_msg_t;

#define EVAPI_MAX_CLIENTS	_evapi_max_clients

/* last one used for error handling, not a real connected client */
static evapi_client_t *_evapi_clients = NULL;

typedef struct _evapi_evroutes {
	int con_new;
	str con_new_name;
	int con_closed;
	str con_closed_name;
	int msg_received;
	str msg_received_name;
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

	_evapi_rts.con_new_name.s = "evapi:connection-new";
	_evapi_rts.con_new_name.len = strlen(_evapi_rts.con_new_name.s);
	_evapi_rts.con_new = route_lookup(&event_rt, "evapi:connection-new");
	if (_evapi_rts.con_new < 0 || event_rt.rlist[_evapi_rts.con_new] == NULL)
		_evapi_rts.con_new = -1;

	_evapi_rts.con_closed_name.s = "evapi:connection-closed";
	_evapi_rts.con_closed_name.len = strlen(_evapi_rts.con_closed_name.s);
	_evapi_rts.con_closed = route_lookup(&event_rt, "evapi:connection-closed");
	if (_evapi_rts.con_closed < 0 || event_rt.rlist[_evapi_rts.con_closed] == NULL)
		_evapi_rts.con_closed = -1;

	_evapi_rts.msg_received_name.s = "evapi:message-received";
	_evapi_rts.msg_received_name.len = strlen(_evapi_rts.msg_received_name.s);
	_evapi_rts.msg_received = route_lookup(&event_rt, "evapi:message-received");
	if (_evapi_rts.msg_received < 0 || event_rt.rlist[_evapi_rts.msg_received] == NULL)
		_evapi_rts.msg_received = -1;

	_evapi_netstring_format = dformat;
}

/**
 *
 */
int evapi_run_cfg_route(evapi_env_t *evenv, int rt, str *rtname)
{
	int backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sip_msg_t tmsg;
	sr_kemi_eng_t *keng = NULL;

	if(evenv==0 || evenv->eset==0) {
		LM_ERR("evapi env not set\n");
		return -1;
	}

	if((rt<0) && (_evapi_event_callback.s==NULL || _evapi_event_callback.len<=0))
		return 0;

	if(faked_msg_get_new(&tmsg)<0) {
		LM_ERR("failed to get a new faked message\n");
		return -1;
	}
	fmsg = &tmsg;
	evapi_set_msg_env(fmsg, evenv);
	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if(rt>=0) {
		run_top_route(event_rt.rlist[rt], fmsg, 0);
	} else {
		keng = sr_kemi_eng_get();
		if(keng!=NULL) {
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE,
						&_evapi_event_callback, rtname)<0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	set_route_type(backup_rt);
	evapi_set_msg_env(fmsg, NULL);
	/* free the structure -- it is a clone of faked msg */
	free_sip_msg(fmsg);
	ksr_msg_env_reset();
	return 0;
}

/**
 *
 */
int evapi_close_connection(int cidx)
{
	if(cidx<0 || cidx>=EVAPI_MAX_CLIENTS || _evapi_clients==NULL)
		return -1;
	if(_evapi_clients[cidx].connected==1
			&& _evapi_clients[cidx].sock >= 0) {
		close(_evapi_clients[cidx].sock);
		_evapi_clients[cidx].connected = 0;
		_evapi_clients[cidx].sock = -1;
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

	evenv = evapi_get_msg_env(msg);

	if(evenv==NULL || evenv->conidx<0 || evenv->conidx>=EVAPI_MAX_CLIENTS)
		return -1;
	return evapi_close_connection(evenv->conidx);
}

/**
 *
 */
int evapi_set_tag(sip_msg_t* msg, str* stag)
{
	evapi_env_t *evenv;

	if(msg==NULL || stag==NULL || _evapi_clients==NULL)
		return -1;

	evenv = evapi_get_msg_env(msg);

	if(evenv==NULL || evenv->conidx<0 || evenv->conidx>=EVAPI_MAX_CLIENTS)
		return -1;

	if(!(_evapi_clients[evenv->conidx].connected==1
			&& _evapi_clients[evenv->conidx].sock >= 0)) {
		LM_ERR("connection not established\n");
		return -1;
	}

	if(stag->len>=EVAPI_TAG_SIZE) {
		LM_ERR("tag size too big: %d / %d\n", stag->len, EVAPI_TAG_SIZE);
		return -1;
	}
	_evapi_clients[evenv->conidx].stag.s = _evapi_clients[evenv->conidx].tag;
	strncpy(_evapi_clients[evenv->conidx].stag.s, stag->s, stag->len);
	_evapi_clients[evenv->conidx].stag.s[stag->len] = '\0';
	_evapi_clients[evenv->conidx].stag.len = stag->len;
	return 1;
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
	LM_DBG("inter-process event notification sockets initialized: %d ~ %d\n",
			_evapi_notify_sockets[0], _evapi_notify_sockets[1]);
	return 0;
}

/**
 *
 */
void evapi_close_notify_sockets_child(void)
{
	LM_DBG("closing the notification socket used by children\n");
	close(_evapi_notify_sockets[1]);
	_evapi_notify_sockets[1] = -1;
}

/**
 *
 */
void evapi_close_notify_sockets_parent(void)
{
	LM_DBG("closing the notification socket used by parent\n");
	close(_evapi_notify_sockets[0]);
	_evapi_notify_sockets[0] = -1;
}

/**
 *
 */
int evapi_dispatch_notify(evapi_msg_t *emsg)
{
	int i;
	int n;
	int wlen;

	if(_evapi_clients==NULL) {
		return 0;
	}

	n = 0;
	for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
		if(_evapi_clients[i].connected==1 && _evapi_clients[i].sock>=0) {
			if(emsg->tag.s==NULL || (emsg->tag.len == _evapi_clients[i].stag.len
						&& strncmp(_evapi_clients[i].stag.s,
									emsg->tag.s, emsg->tag.len)==0)) {
				wlen = write(_evapi_clients[i].sock, emsg->data.s,
						emsg->data.len);
				if(wlen!=emsg->data.len) {
					LM_DBG("failed to write all packet (%d out of %d) on socket"
							" %d index [%d]\n",
							wlen, emsg->data.len, _evapi_clients[i].sock, i);
				}
				n++;
				if (emsg->unicast){
					break;
				}
			}
		}
	}

	LM_DBG("the message was sent to %d clients\n", n);

	return n;
}

/**
 *
 */
void evapi_recv_client(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	ssize_t rlen;
	int i, k;
	evapi_env_t evenv;
	str frame;
	char *sfp;
	char *efp;

	if(EV_ERROR & revents) {
		LM_ERR("received invalid event (%d)\n", revents);
		return;
	}
	if(_evapi_clients==NULL) {
		LM_ERR("no client structures\n");
		return;
	}

	for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
		if(_evapi_clients[i].connected==1 && _evapi_clients[i].sock==watcher->fd) {
			break;
		}
	}
	if(i==EVAPI_MAX_CLIENTS) {
		LM_ERR("cannot lookup client socket %d\n", watcher->fd);
		/* try to empty the socket anyhow */
		rlen = recv(watcher->fd, _evapi_clients[i].rbuffer, CLIENT_BUFFER_SIZE-1, 0);
		return;
	}

	/* read message from client */
	rlen = recv(watcher->fd, _evapi_clients[i].rbuffer + _evapi_clients[i].rpos,
			CLIENT_BUFFER_SIZE - 1 - _evapi_clients[i].rpos, 0);

	if(rlen < 0) {
		LM_ERR("cannot read the client message\n");
		_evapi_clients[i].rpos = 0;
		return;
	}


	cfg_update();

	evapi_env_reset(&evenv);
	if(rlen == 0) {
		/* client is gone */
		evenv.eset = 1;
		evenv.conidx = i;
		evapi_run_cfg_route(&evenv, _evapi_rts.con_closed,
				&_evapi_rts.con_closed_name);
		_evapi_clients[i].connected = 0;
		if(_evapi_clients[i].sock>=0) {
			close(_evapi_clients[i].sock);
		}
		_evapi_clients[i].sock = -1;
		_evapi_clients[i].rpos = 0;
		ev_io_stop(loop, watcher);
		free(watcher);
		LM_INFO("client closing connection - pos [%d] addr [%s:%d]\n",
				i, _evapi_clients[i].src_addr, _evapi_clients[i].src_port);
		return;
	}

	_evapi_clients[i].rbuffer[_evapi_clients[i].rpos+rlen] = '\0';

	LM_DBG("{%d} [%s:%d] - received [%.*s] (%d) (%d)\n",
		i, _evapi_clients[i].src_addr, _evapi_clients[i].src_port,
		(int)rlen, _evapi_clients[i].rbuffer+_evapi_clients[i].rpos,
		(int)rlen, (int)_evapi_clients[i].rpos);
	evenv.conidx = i;
	evenv.eset = 1;
	if(_evapi_netstring_format) {
		/* netstring decapsulation */
		k = 0;
		while(k<_evapi_clients[i].rpos+rlen) {
			frame.len = 0;
			while(k<_evapi_clients[i].rpos+rlen) {
				if(_evapi_clients[i].rbuffer[k]==' '
						|| _evapi_clients[i].rbuffer[k]=='\t'
						|| _evapi_clients[i].rbuffer[k]=='\r'
						|| _evapi_clients[i].rbuffer[k]=='\n')
					k++;
				else break;
			}
			if(k==_evapi_clients[i].rpos+rlen) {
				_evapi_clients[i].rpos = 0;
				LM_DBG("empty content\n");
				return;
			}
			/* pointer to start of whole frame */
			sfp = _evapi_clients[i].rbuffer + k;
			while(k<_evapi_clients[i].rpos+rlen) {
				if(_evapi_clients[i].rbuffer[k]>='0' && _evapi_clients[i].rbuffer[k]<='9') {
					frame.len = frame.len*10 + _evapi_clients[i].rbuffer[k] - '0';
				} else {
					if(_evapi_clients[i].rbuffer[k]==':')
						break;
					/* invalid character - discard the rest */
					_evapi_clients[i].rpos = 0;
					LM_DBG("invalid char when searching for size [%c] [%.*s] (%d) (%d)\n",
							_evapi_clients[i].rbuffer[k],
							(int)(_evapi_clients[i].rpos+rlen), _evapi_clients[i].rbuffer,
							(int)(_evapi_clients[i].rpos+rlen), k);
					return;
				}
				k++;
			}
			if(k==_evapi_clients[i].rpos+rlen || frame.len<=0) {
				LM_DBG("invalid frame len: %d kpos: %d rpos: %u rlen: %lu\n",
						frame.len, k, _evapi_clients[i].rpos, rlen);
				_evapi_clients[i].rpos = 0;
				return;
			}
			if(frame.len + k>=_evapi_clients[i].rpos + rlen) {
				/* partial data - shift back in buffer and wait to read more */
				efp = _evapi_clients[i].rbuffer + _evapi_clients[i].rpos + rlen;
				if(efp<=sfp) {
					_evapi_clients[i].rpos = 0;
					LM_DBG("weird - invalid size for residual data\n");
					return;
				}
				_evapi_clients[i].rpos = (unsigned int)(efp-sfp);
				if(efp-sfp > sfp-_evapi_clients[i].rbuffer) {
					memcpy(_evapi_clients[i].rbuffer, sfp, _evapi_clients[i].rpos);
				} else {
					for(k=0; k<_evapi_clients[i].rpos; k++) {
						_evapi_clients[i].rbuffer[k] = sfp[k];
					}
				}
				LM_DBG("residual data [%.*s] (%d)\n",
						_evapi_clients[i].rpos, _evapi_clients[i].rbuffer,
						_evapi_clients[i].rpos);
				return;
			}
			k++;
			frame.s = _evapi_clients[i].rbuffer + k;
			if(frame.s[frame.len]!=',') {
				/* invalid data - discard and reset buffer */
				LM_DBG("frame size mismatch the ending char (%c): [%.*s] (%d)\n",
						frame.s[frame.len], frame.len, frame.s, frame.len);
				_evapi_clients[i].rpos = 0 ;
				return;
			}
			frame.s[frame.len] = '\0';
			k += frame.len ;
			evenv.msg.s = frame.s;
			evenv.msg.len = frame.len;
			LM_DBG("executing event route for frame: [%.*s] (%d)\n",
						frame.len, frame.s, frame.len);
			evapi_run_cfg_route(&evenv, _evapi_rts.msg_received,
					&_evapi_rts.msg_received_name);
			k++;
		}
		_evapi_clients[i].rpos = 0 ;
	} else {
		evenv.msg.s = _evapi_clients[i].rbuffer;
		evenv.msg.len = rlen;
		evapi_run_cfg_route(&evenv, _evapi_rts.msg_received,
				&_evapi_rts.msg_received_name);
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
	int optval;
	socklen_t optlen;

	if(_evapi_clients==NULL) {
		LM_ERR("no client structures\n");
		return;
	}
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
		LM_ERR("cannot accept the client '%s' err='%d'\n", gai_strerror(csock), csock);
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
			optval = 1;
			optlen = sizeof(optval);
			if(setsockopt(csock, SOL_SOCKET, SO_KEEPALIVE,
						&optval, optlen) < 0) {
				LM_WARN("failed to enable keepalive on socket %d\n", csock);
			}
			_evapi_clients[i].connected = 1;
			_evapi_clients[i].sock = csock;
			_evapi_clients[i].af = caddr.sa_family;
			break;
		}
	}
	if(i>=EVAPI_MAX_CLIENTS) {
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
	evapi_run_cfg_route(&evenv, _evapi_rts.con_new, &_evapi_rts.con_new_name);

	if(_evapi_clients[i].connected == 0) {
		free(evapi_client);
		return;
	}

	/* start watcher to read messages from watchers */
	ev_io_init(evapi_client, evapi_recv_client, csock, EV_READ);
	ev_io_start(loop, evapi_client);
}

/**
 *
 */
void evapi_recv_notify(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	evapi_msg_t *emsg = NULL;
	int rlen;

	if(EV_ERROR & revents) {
		perror("received invalid event\n");
		return;
	}

	cfg_update();

	/* read message from client */
	rlen = read(watcher->fd, &emsg, sizeof(evapi_msg_t*));

	if(rlen != sizeof(evapi_msg_t*) || emsg==NULL) {
		LM_ERR("cannot read the sip worker message\n");
		return;
	}

	LM_DBG("received [%p] [%.*s] (%d)\n", (void*)emsg,
			emsg->data.len, emsg->data.s, emsg->data.len);
	evapi_dispatch_notify(emsg);
	shm_free(emsg);
}

/**
 *
 */
int evapi_run_dispatcher(char *laddr, int lport)
{
	int evapi_srv_sock;
	struct ev_loop *loop;
	struct addrinfo ai_hints;
	struct addrinfo *ai_res = NULL;
	int ai_ret = 0;
	struct ev_io io_server;
	struct ev_io io_notify;
	int yes_true = 1;
	int fflags = 0;
	int i;

	LM_DBG("starting dispatcher processing\n");

	_evapi_clients = (evapi_client_t*)malloc(sizeof(evapi_client_t)
			* (EVAPI_MAX_CLIENTS+1));
	if(_evapi_clients==NULL) {
		LM_ERR("failed to allocate client structures\n");
		exit(-1);
	}
	memset(_evapi_clients, 0, sizeof(evapi_client_t) * EVAPI_MAX_CLIENTS);
	for(i=0; i<EVAPI_MAX_CLIENTS; i++) {
		_evapi_clients[i].sock = -1;
	}
	loop = ev_default_loop(0);

	if(loop==NULL) {
		LM_ERR("cannot get libev loop\n");
		return -1;
	}

	memset(&ai_hints, 0, sizeof(struct addrinfo));
	ai_hints.ai_family = AF_UNSPEC;		/* allow IPv4 or IPv6 */
	ai_hints.ai_socktype = SOCK_STREAM;	/* stream socket */
	ai_ret = getaddrinfo(laddr, int2str(lport, NULL), &ai_hints, &ai_res);
	if (ai_ret != 0) {
		LM_ERR("getaddrinfo failed: %d %s\n", ai_ret, gai_strerror(ai_ret));
		return -1;
	}
	evapi_srv_sock = socket(ai_res->ai_family, ai_res->ai_socktype,
			ai_res->ai_protocol);
	if( evapi_srv_sock < 0 ) {
		LM_ERR("cannot create server socket (family %d)\n", ai_res->ai_family);
		freeaddrinfo(ai_res);
		return -1;
	}

	/* set non-blocking flag */
	fflags = fcntl(evapi_srv_sock, F_GETFL);
	if(fflags<0) {
		LM_ERR("failed to get the srv socket flags\n");
		close(evapi_srv_sock);
		freeaddrinfo(ai_res);
		return -1;
	}
	if (fcntl(evapi_srv_sock, F_SETFL, fflags | O_NONBLOCK)<0) {
		LM_ERR("failed to set srv socket flags\n");
		close(evapi_srv_sock);
		freeaddrinfo(ai_res);
		return -1;
	}

	/* Set SO_REUSEADDR option on listening socket so that we don't
	 * have to wait for connections in TIME_WAIT to go away before
	 * re-binding.
	 */

	if(setsockopt(evapi_srv_sock, SOL_SOCKET, SO_REUSEADDR,
		&yes_true, sizeof(int)) < 0) {
		LM_ERR("cannot set SO_REUSEADDR option on descriptor\n");
		close(evapi_srv_sock);
		freeaddrinfo(ai_res);
		return -1;
	}

	if (bind(evapi_srv_sock, ai_res->ai_addr, ai_res->ai_addrlen) < 0) {
		LM_ERR("cannot bind to local address and port [%s:%d]\n", laddr, lport);
		close(evapi_srv_sock);
		freeaddrinfo(ai_res);
		return -1;
	}
	freeaddrinfo(ai_res);
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
int _evapi_relay(str *evdata, str *ctag, int unicast)
{
#define EVAPI_RELAY_FORMAT "%d:%.*s,"

	int len;
	int sbsize;
	evapi_msg_t *emsg;

	LM_DBG("relaying event data [%.*s] (%d)\n",
			evdata->len, evdata->s, evdata->len);

	sbsize = evdata->len;
	len = sizeof(evapi_msg_t)
		+ ((sbsize + 32 + ((ctag && ctag->len>0)?(ctag->len+2):0)) * sizeof(char));
	emsg = (evapi_msg_t*)shm_malloc(len);
	if(emsg==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(emsg, 0, len);
	emsg->data.s = (char*)emsg + sizeof(evapi_msg_t);
	if(_evapi_netstring_format) {
		/* netstring encapsulation */
		emsg->data.len = snprintf(emsg->data.s, sbsize+32,
				EVAPI_RELAY_FORMAT,
				sbsize, evdata->len, evdata->s);
	} else {
		emsg->data.len = snprintf(emsg->data.s, sbsize+32,
				"%.*s",
				evdata->len, evdata->s);
	}
	if(emsg->data.len<=0 || emsg->data.len>sbsize+32) {
		shm_free(emsg);
		LM_ERR("cannot serialize event\n");
		return -1;
	}
	if(ctag && ctag->len>0) {
		emsg->tag.s = emsg->data.s + sbsize + 32;
		strncpy(emsg->tag.s, ctag->s, ctag->len);
		emsg->tag.len = ctag->len;
	}

	if (unicast){
		emsg->unicast = unicast;
	}

	LM_DBG("sending [%p] [%.*s] (%d)\n", (void*)emsg, emsg->data.len,
			emsg->data.s, emsg->data.len);
	if(_evapi_notify_sockets[1]!=-1) {
		len = write(_evapi_notify_sockets[1], &emsg, sizeof(evapi_msg_t*));
		if(len<=0) {
			shm_free(emsg);
			LM_ERR("failed to pass the pointer to evapi dispatcher\n");
			return -1;
		}
	} else {
		cfg_update();
		LM_DBG("dispatching [%p] [%.*s] (%d)\n", (void*)emsg,
				emsg->data.len, emsg->data.s, emsg->data.len);
		if(evapi_dispatch_notify(emsg) == 0) {
			shm_free(emsg);
			LM_WARN("message not delivered - no client connected\n");
			return -1;
		}
		shm_free(emsg);
	}
	return 0;
}

/**
 *
 */
int evapi_relay(str *evdata)
{
	return _evapi_relay(evdata, NULL, 0);
}

/**
 *
 */
int evapi_relay_multicast(str *evdata, str *ctag){
	return _evapi_relay(evdata, ctag, 0);
}

/**
 *
 */
int evapi_relay_unicast(str *evdata, str *ctag){
	return _evapi_relay(evdata, ctag, 1);
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
	LM_DBG("sent [%p] [%.*s] (%d)\n", (void*)sbuf, sbuf->len, sbuf->s, sbuf->len);
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

	if(_evapi_clients==NULL) {
		return pv_get_null(msg, param, res);
	}
	evenv = evapi_get_msg_env(msg);

	if(evenv==NULL || evenv->conidx<0 || evenv->conidx>=EVAPI_MAX_CLIENTS)
		return pv_get_null(msg, param, res);

	if(_evapi_clients[evenv->conidx].connected==0
			&& _evapi_clients[evenv->conidx].sock < 0)
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
