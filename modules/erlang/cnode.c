/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../mem/mem.h"
#define HANDLE_IO_INLINE
#include "../../io_wait.h"
#include "../../pt.h"  /* process_count */

#ifndef USE_TCP
#error	"USE_TCP must be enabled for this module"
#endif

#include "../../pass_fd.h"

#include "mod_erlang.h"
#include "erl_helpers.h"
#include "cnode.h"
#include "epmd.h"
#include "worker.h"
#include "handle_emsg.h"

#define IO_LISTEN_TIMEOUT	10
#define CONNECT_TIMEOUT	500 /* ms */

static io_wait_h io_h;

cnode_handler_t *enode = NULL;
csockfd_handler_t *csocket_handler = NULL;
erlang_pid *cnode_reply_to_pid = NULL;

/**
 * @brief Initialize Kamailo as C node by active connect as client.
 */
int cnode_connect_to(cnode_handler_t *phandler, ei_cnode *ec, const str *nodename )
{
	struct sockaddr addr = { 0 };
	socklen_t addrlen = sizeof(struct sockaddr);
	int port;

	int rfd;
	int ai_error;
	ip_addr_t ip;

	phandler->request.buff = NULL;
	phandler->response.buff = NULL;
	phandler->sockfd = -1;
	phandler->destroy_f = NULL;

	LM_DBG("connecting to Erlang node %.*s ...\n", STR_FMT(nodename));

	if ((rfd = ei_connect_tmo(ec, nodename->s, CONNECT_TIMEOUT)) < 0)
	{
		switch (erl_errno)
		{
		case EHOSTUNREACH:
			LM_ERR("remote node %.*s is unreachable.\n", STR_FMT(nodename));
			break;
		case ENOMEM:
			LM_ERR("%s.\n", strerror(erl_errno));
			break;
		case EIO:
			LM_ERR("%s. %s.\n", strerror(erl_errno), strerror(errno));
			break;
		default:
			LM_ERR("%s.\n",strerror(erl_errno));
			break;
		}
		LM_ERR("failed erl_errno=%d %s.\n",erl_errno,strerror(erl_errno));
		return -1;
	}

	phandler->ec = *ec;
	phandler->handle_f = handle_cnode;
	phandler->wait_tmo_f = wait_cnode_tmo;
	phandler->destroy_f = destroy_cnode;

	erl_set_nonblock(rfd);

	phandler->sockfd = rfd;

	/* get addr on socket */
	if ((ai_error = getpeername(rfd, &addr, &addrlen)))
	{
		LM_ERR("%s\n",strerror(errno));
	}
	else
	{
		sockaddr2ip_addr(&ip,&addr);
		port = sockaddr_port(&addr);

		LM_DBG("... connected to %.*s %s:%u\n", STR_FMT(nodename), ip_addr2strz(&ip), port);
	}

	strcpy(phandler->conn.nodename, nodename->s);

	/* for #Pid */
	phandler->ec.self.num = phandler->sockfd;
	phandler->new = NULL;

	if (ei_x_new(&phandler->request))
	{
		LOG(L_ERR,"failed to allocate ei_x_buff: %s.\n", strerror(erl_errno));
		return -1;
	}

	if (ei_x_new_with_version(&phandler->response))
	{
		LOG(L_ERR,"failed to allocate ei_x_buff: %s.\n", strerror(erl_errno));
		return -1;
	}

	enode = phandler;
	return 0;
}

int destroy_cnode(handler_common_t *phandler_t)
{
	cnode_handler_t *phandler = (cnode_handler_t*)phandler_t;

	if(phandler->request.buff) ei_x_free(&phandler->request);
	if(phandler->response.buff) ei_x_free(&phandler->response);

	erl_close_socket(phandler->sockfd);

	return 0;
}

int init_cnode_sockets(int cnode_id)
{
	char buff[EI_MAXALIVELEN+1];
	str alivename;
	ei_cnode ec;
	int poll_method;
	handler_common_t *phandler;

	/* generate cnode name */
	alivename.s = buff;
	if (no_cnodes > 1) {
		alivename.len = snprintf(buff,EI_MAXALIVELEN,"%.*s%d",STR_FMT(&cnode_alivename),cnode_id);
	} else {
		alivename = cnode_alivename;
	}

	if (alivename.len > EI_MAXALIVELEN) {
		LM_ERR("alive name %.*s too long max allowed size is %d\n",STR_FMT(&cnode_alivename), EI_MAXALIVELEN);
		return -1;
	}

	/* init Erlang connection */
	if (erl_init_ec(&ec,&alivename,&cnode_host,&cookie)) {
		LM_CRIT("failed to initialize ei_cnode\n");
		return -1;
	}

	poll_method=choose_poll_method();

	LM_DBG("using %s as the I/O watch method (auto detected)\n", poll_method_name(poll_method));

	if (init_io_wait(&io_h,get_max_open_fds(),poll_method))
	{
		LM_CRIT("init_io_wait failed\n");
		return -1;
	}

	phandler = (handler_common_t*)pkg_malloc(sizeof(csockfd_handler_t));
	if (!phandler) {
		LM_ERR("not enough memory\n");
		return -1;
	}

	io_handler_ins(phandler);

	if (csockfd_init((csockfd_handler_t*)phandler, &ec)) {
		return -1;
	}

	if (io_watch_add(&io_h,phandler->sockfd,POLLIN,ERL_CSOCKFD_H,phandler)) {
		LM_CRIT("io_watch_add failed\n");
		return -1;
	}

	phandler = (handler_common_t*)pkg_malloc(sizeof(cnode_handler_t));
	if (!phandler) {
		LM_CRIT("not enough memory\n");
		return -1;
	}

	io_handler_ins(phandler);

	/* connect to remote Erlang node */
	if (cnode_connect_to((cnode_handler_t*)phandler,&ec, erlang_nodename.s?&erlang_nodename:&erlang_node_sname)) {
		/* continue even failed to connect, connection can be established
		 * from Erlang side too */
		io_handler_del(phandler);
	} else if (io_watch_add(&io_h,phandler->sockfd,POLLIN,ERL_CNODE_H,phandler)){
		LM_CRIT("io_watch_add failed\n");
		return -1;
	}

	phandler = (handler_common_t*)pkg_malloc(sizeof(epmd_handler_t));
	if (!phandler) {
		LM_CRIT("not enough memory\n");
		return -1;
	}

	io_handler_ins(phandler);

	/* start epmd handler - publish Kamailo C node */
	if (epmd_init((epmd_handler_t*)phandler) < 0 ) {
		return -1;
	}

	if (io_watch_add(&io_h,phandler->sockfd,POLLIN,ERL_EPMD_H,phandler)){
		LM_CRIT("io_watch_add failed\n");
		return -1;
	}

	return 0;
}

void cnode_main_loop(int cnode_id)
{
	char _cnode_name[MAXNODELEN];
	str *enode_name;
	int r;

	if (snprintf(_cnode_name, MAXNODELEN, "%.*s%d@%.*s", STR_FMT(&cnode_alivename), cnode_id+1,STR_FMT(&cnode_host)) >= MAXNODELEN)
	{
		LM_CRIT("the node name <%.*s%d@%.*s> is too large max length allowed is %d\n",
				STR_FMT(&cnode_alivename), cnode_id+1, STR_FMT(&cnode_host), MAXNODELEN-1);
		return;
	}

	erl_init_common();

	if (init_cnode_sockets(cnode_id)) {
		io_handlers_delete();
		erl_free_common();
		return;
	}

	enode_name = erlang_nodename.s?&erlang_nodename:&erlang_node_sname;

	/* main loop */
	switch(io_h.poll_method){
		case POLL_POLL:
			while(1){
				r = io_wait_loop_poll(&io_h, IO_LISTEN_TIMEOUT, 0);
				if(!r && enode_connect()) {
					LM_ERR("failed reconnect to %.*s\n",STR_FMT(enode_name));
				}
			}
			break;
#ifdef HAVE_SELECT
		case POLL_SELECT:
			while(1){
				r = io_wait_loop_select(&io_h, IO_LISTEN_TIMEOUT, 0);
				if(!r && enode_connect()) {
					LM_ERR("failed reconnect to %.*s\n",STR_FMT(enode_name));
				}
			}
			break;
#endif
#ifdef HAVE_SIGIO_RT
		case POLL_SIGIO_RT:
			while(1){
				r = io_wait_loop_sigio_rt(&io_h, IO_LISTEN_TIMEOUT);
				if(!r && enode_connect()) {
					LM_ERR("failed reconnect to %.*s\n",STR_FMT(enode_name));
				}
			}
			break;
#endif
#ifdef HAVE_EPOLL
			case POLL_EPOLL_LT:
				while(1){
					r = io_wait_loop_epoll(&io_h, IO_LISTEN_TIMEOUT, 0);
					if(!r && enode_connect()) {
						LM_ERR("failed reconnect to %.*s\n",STR_FMT(enode_name));
					}
				}
				break;
			case POLL_EPOLL_ET:
				while(1){
					r = io_wait_loop_epoll(&io_h, IO_LISTEN_TIMEOUT, 1);
					if(!r && enode_connect()) {
						LM_ERR("failed reconnect to %.*s\n",STR_FMT(enode_name));
					}
				}
				break;
#endif
#ifdef HAVE_KQUEUE
			case POLL_KQUEUE:
				while(1){
					r = io_wait_loop_kqueue(&io_h, IO_LISTEN_TIMEOUT, 0);
					if(!r && enode_connect()) {
						LM_ERR("failed reconnect to %.*s\n",STR_FMT(enode_name));
					}
				}
				break;
#endif
#ifdef HAVE_DEVPOLL
                case POLL_DEVPOLL:
                	while(1){
						r = io_wait_loop_devpoll(&io_h, IO_LISTEN_TIMEOUT, 0);
						if(!r && enode_connect()) {
							LM_ERR("failed reconnect to %.*s\n",STR_FMT(enode_name));
						}
                	}
                	break;
#endif
                default:
                	LM_CRIT("BUG: io_listen_loop: no support for poll method "
                			" %s (%d)\n", poll_method_name(io_h.poll_method),
							io_h.poll_method);
                	goto error;
        }

error:
	LOG(L_CRIT, "ERROR: cnode_main_loop exiting ...\n");
}

/* generic handle io routine, it will call the appropriate
 *  handle_xxx() based on the fd_map type
 *
 * params:  fm  - pointer to a fd hash entry
 *          idx - index in the fd_array (or -1 if not known)
 * return: -1 on error
 *          0 on EAGAIN or when by some other way it is known that no more
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfully read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 */
int handle_io(struct fd_map* fm, short events, int idx)
{
	int type;
	handler_common_t *phandler;

	phandler = (handler_common_t*)fm->data;

	if (phandler->handle_f(phandler)) {
		io_watch_del(&io_h,fm->fd,idx,IO_FD_CLOSING);
		io_handler_del(phandler);
		if (fm->type == ERL_WORKER_H) {
			LM_CRIT("error on unix socket, not recoverable error -- aborting\n");
			abort();
		}
		return -1;
	}

	if (phandler->new) {
		switch (fm->type) {
		case ERL_CSOCKFD_H:
			type = ERL_WORKER_H;
			break;
		case ERL_EPMD_H:
			type = ERL_CNODE_H;
			break;
		default:
			LM_ERR("should not be here!\n");
			return -1;
		}
		LM_DBG("add new handler type=%d\n",type);
		if (io_watch_add(&io_h,phandler->new->sockfd,POLLIN,type,(void*)phandler->new)) {
			LM_ERR("failed to add new handler\n");
			return -1;
		}
		io_handler_ins(phandler->new);
		phandler->new = NULL;
	}

	return 1;
}

/**
 * @brief Handle requests from remote Erlang node.
 */
int handle_cnode(handler_common_t *phandler)
{
	int got;
	cnode_handler_t *listener;
	erlang_msg emsg; /* Incoming message */
	ei_x_buff *request;
	ei_x_buff *response;
	int response_start_index;

	listener = (cnode_handler_t*) phandler;
	request = &listener->request;
	response = &listener->response;

	response_start_index = response->index; /* save */
	request->index = 0;

	listener->tick_tmo = 0; /* reset ERL_TICK timer for all events */

	memset((void*)&emsg,0,sizeof(erlang_msg));

	if ((got = ei_xreceive_msg_tmo(listener->sockfd, &emsg, request,
			CONNECT_TIMEOUT)) == ERL_TICK)
	{ /* ignore */
		LM_DBG("%s received ERL_TICK from <%s>.\n", listener->ec.thisnodename, listener->conn.nodename);
		return 0;
	}
	else if (got == ERL_ERROR)
	{
		switch (erl_errno)
		{
		case EAGAIN:
			/* or */
		case ETIMEDOUT:
			response->index = response_start_index; /* restore version point */
			return 0;
			break;
		case EIO:
			LM_ERR("I/O error while receiving message from %s or connection closed.\n", listener->conn.nodename);
			enode = NULL;
			return -1;
			break;
		case ENOMEM:
			LM_ERR("Failed to receive message from %s, not enough memory. Closing connection.\n", listener->conn.nodename);
			enode = NULL;
			return -1;
			break;
		default:
			LM_ERR("Close connection to %s. %s.\n", listener->conn.nodename, strerror(erl_errno));
			enode = NULL;
			return -1;
		}
	}
	else if (got == ERL_MSG)
	{
		response->index = response_start_index; /* restore version point */
		switch (emsg.msgtype)
		{
		case ERL_LINK:
			LM_WARN("ERL_LINK from <%s> -- discarding.\n", listener->conn.nodename);
			break;
		case ERL_REG_SEND:
			PRINT_DBG_REG_SEND(listener->conn.nodename,emsg.from, listener->ec.thisnodename, emsg.toname,request);
			handle_erlang_msg(listener, &emsg);
			break;
		case ERL_SEND:
			PRINT_DBG_SEND(listener->conn.nodename,emsg.to,request);
			handle_erlang_msg(listener, &emsg);
			break;
		case ERL_EXIT:
			LM_WARN("received ERL_EXIT from <%s> -- discarding.\n", listener->conn.nodename);
			break;
		case ERL_UNLINK:
			LM_WARN("ERL_UNLINK from <%s> -- discarding.\n", listener->conn.nodename);
			break;
		case ERL_NODE_LINK:
			LM_WARN("ERL_NODE_LINK from <%s> -- discarding.\n", listener->conn.nodename);
			break;
		case ERL_GROUP_LEADER:
			LM_WARN("ERL_GROUP_LEADER from <%s> -- discarding.\n", listener->conn.nodename);
			break;
		case ERL_EXIT2:
			LM_WARN("ERL_EXIT2 from <%s> -- discarding.\n", listener->conn.nodename);
			break;
		case ERL_PASS_THROUGH:
			LM_WARN("ERL_PASS_THROUGH from <%s> -- discarding.\n", listener->conn.nodename);
			break;
		default:
			LM_WARN("Received unknown msgtype <%ld> from <%s> -- discarding.\n", emsg.msgtype, listener->conn.nodename);
			break;
		}
	}
	else
	{
		LM_ERR("unknown return value from ei_xreceive_msg_tmo.\n");
	}

	response->index = response_start_index; /* restore to version point */

	return  0;
}

int wait_cnode_tmo(handler_common_t *phandler_t)
{
	return 0;
}

int csockfd_init(csockfd_handler_t *phandler, const ei_cnode *ec)
{
	phandler->handle_f = handle_csockfd;
	phandler->wait_tmo_f = NULL;
	phandler->sockfd = csockfd;
	phandler->ec = *ec;
	phandler->new = NULL;

	erl_set_nonblock(csockfd);

	csocket_handler = phandler;

	csockfd = 0;
	return 0;
}

int handle_csockfd(handler_common_t *phandler_t)
{
	csockfd_handler_t *phandler;
	int data[2];
	int fd = -1;

	phandler = (csockfd_handler_t*)phandler_t;

	if (receive_fd(phandler->sockfd,(void*)data,sizeof(data),&fd,0) == -1) {
		LM_ERR("failed to receive socket: %s\n",strerror(errno));
		return -1;
	}

	phandler->new = (handler_common_t*)pkg_malloc(sizeof(worker_handler_t));
	if(!phandler->new) {
		LM_ERR("not enough memory\n");
		return -1;
	}

	io_handler_ins(phandler->new);

	return worker_init((worker_handler_t*)phandler->new,fd,&phandler_t->ec);
}

/*
 * \brief Connect to Erlang node if not connected
 */
int enode_connect() {

	handler_common_t *phandler;

	if (!csocket_handler) {
		return -1;
	}

	if (enode) {
		return 0;
	}

	LM_DBG("not connected, trying to connect...\n");

	phandler = (handler_common_t*)pkg_malloc(sizeof(cnode_handler_t));

	if (!phandler) {
		LM_CRIT("not enough memory\n");
		return -1;
	}

	io_handler_ins(phandler);

	/* connect to remote Erlang node */
	if (cnode_connect_to((cnode_handler_t*)phandler,&csocket_handler->ec, erlang_nodename.s?&erlang_nodename:&erlang_node_sname)) {
		/* continue even failed to connect, connection can be established
		 * from Erlang side too */
		io_handler_del(phandler);
	} else if (io_watch_add(&io_h,phandler->sockfd,POLLIN,ERL_CNODE_H,phandler)){
		LM_CRIT("io_watch_add failed\n");
		erl_close_socket(phandler->sockfd);
		io_handler_del(phandler);
		return -1;
	}

	return 0;
}
