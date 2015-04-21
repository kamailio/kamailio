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

#include <ei.h>
#include <netinet/ip.h> /*IPTOS_LOWDELAY*/

#include "../../dprint.h"
#include "../../ip_addr.h"

#include "mod_erlang.h"
#include "erl_helpers.h"
#include "epmd.h"
#include "cnode.h"

#ifndef CONNECT_TIMEOUT
#define CONNECT_TIMEOUT 500 /* ms */
#endif

/**
 * \brief Initialize EPMD handler
 */
int epmd_init(epmd_handler_t *epmd)
{
	epmd->sockfd = erl_init_node(&epmd->ec, &cnode_alivename, &cnode_host, &cookie);
	epmd->handle_f = handle_epmd;
	epmd->wait_tmo_f = NULL;
	epmd->destroy_f  = NULL;
	epmd->new        = NULL;

	return epmd->sockfd;
}

/**
 * \brief Handle connections from epmd.
 */
int handle_epmd(handler_common_t *phandler)
{
	struct ip_addr ip;
	struct sockaddr addr = { 0 };
	struct sockaddr *paddr;
	socklen_t addrlen = sizeof(struct sockaddr);
	int port;
	int ai_error;
	int sockfd;
	int on;

	epmd_handler_t *me;
	cnode_handler_t *remotenode = NULL;
	ErlConnect conn;

	paddr = &addr;
	me = (epmd_handler_t*) phandler;

	if ((sockfd = ei_accept_tmo(&me->ec, me->sockfd, &conn, CONNECT_TIMEOUT))
			== ERL_ERROR) {
		LM_ERR("error on accept connection: %s.\n", strerror(erl_errno));

		if (erl_errno == ETIMEDOUT) {
			return 0;
		} else if (errno) {
			LM_ERR("socket error: %s\n", strerror(errno));
			return -1;
		} else {
			/* if errno didn't get set, assume nothing *too much* horrible occurred */
			LM_NOTICE("ignored error in ei_accept, probably: bad client version, "
					"bad cookie or bad node name\n");
		}
		return 0;
	}

	/* get remote address info of connected socket */
	if ((ai_error = getpeername(sockfd, paddr, &addrlen)))
	{
		LM_ERR("%s\n",strerror(errno));
	}
	else
	{
		sockaddr2ip_addr(&ip,paddr);
		port = sockaddr_port(paddr);

		LM_DBG("connected from %s port %u\n", ip_addr2strz(&ip), port);
	}

	remotenode = (cnode_handler_t*)pkg_malloc(sizeof(cnode_handler_t));
	if (!remotenode) {
		erl_close_socket(sockfd);
		return -1;
	}

	/* tos */
	on=IPTOS_LOWDELAY;
	if (setsockopt(sockfd, IPPROTO_IP, IP_TOS, (void*)&on,sizeof(on)) ==-1) {
			LM_WARN("setsockopt set tos failed: %s\n", strerror(errno));
			/* continue since this is not critical */
	}

	if (erl_set_nonblock(sockfd)){
		LM_ERR("set non blocking socket failed\n");
	}

	memset((void*)remotenode,0,sizeof(cnode_handler_t));

	remotenode->handle_f = handle_cnode;
	remotenode->wait_tmo_f = wait_cnode_tmo;
	remotenode->destroy_f = destroy_cnode;
	remotenode->sockfd = sockfd;
	remotenode->ec = me->ec;
	remotenode->conn = conn;
	/* for #Pid */
	remotenode->ec.self.num = sockfd;

	if (ei_x_new(&remotenode->request))
	{
		LOG(L_ERR,"failed to allocate ei_x_buff: %s.\n", strerror(erl_errno));
		return -1;
	}

	if (ei_x_new_with_version(&remotenode->response))
	{
		LOG(L_ERR,"failed to allocate ei_x_buff: %s.\n", strerror(erl_errno));
		return -1;
	}

	phandler->new = (handler_common_t*)remotenode;

	/* activate node */
	enode = remotenode;

	return 0;
}
