/*
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
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


#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "dmq.h"
#include "worker.h"
#include "peer.h"
#include "message.h"

str dmq_200_rpl  = str_init("OK");
str dmq_400_rpl  = str_init("Bad request");
str dmq_500_rpl  = str_init("Server Internal Error");
str dmq_404_rpl  = str_init("User Not Found");

/**
 * @brief config function to handle dmq messages
 */
int dmq_handle_message(struct sip_msg* msg, char* str1, char* str2)
{
	dmq_peer_t* peer;
	if ((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s)) {
			LM_ERR("error parsing msg uri\n");
			goto error;
	}
	LM_DBG("dmq_handle_message [%.*s %.*s] [%s %s]\n",
	       msg->first_line.u.request.method.len, msg->first_line.u.request.method.s,
	       msg->first_line.u.request.uri.len, msg->first_line.u.request.uri.s,
	       ZSW(str1), ZSW(str2));
	/* the peer id is given as the userinfo part of the request URI */
	peer = find_peer(msg->parsed_uri.user);
	if(!peer) {
		LM_DBG("no peer found for %.*s\n", msg->parsed_uri.user.len, msg->parsed_uri.user.s);
		if(slb.freply(msg, 404, &dmq_404_rpl) < 0)
		{
			LM_ERR("sending reply\n");
			goto error;
		}
		return 0;
	}
	LM_DBG("dmq_handle_message peer found: %.*s\n", msg->parsed_uri.user.len, msg->parsed_uri.user.s);
	if(add_dmq_job(msg, peer)<0) {
		LM_ERR("failed to add dmq job\n");
		goto error;
	}
	return 0;
error:
	return -1;
}

