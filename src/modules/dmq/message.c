/*
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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


#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_from.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/ut.h"
#include "../../core/rand/kam_rand.h"
#include "dmq.h"
#include "worker.h"
#include "peer.h"
#include "message.h"
#include "dmq_funcs.h"

str dmq_200_rpl = str_init("OK");
str dmq_400_rpl = str_init("Bad request");
str dmq_500_rpl = str_init("Server Internal Error");
str dmq_404_rpl = str_init("User Not Found");

/**
 * @brief set the body of a response
 */
int set_reply_body(struct sip_msg *msg, str *body, str *content_type)
{
	char *buf;
	int len;

	/* add content-type */
	len = sizeof("Content-Type: ") - 1 + content_type->len + CRLF_LEN;
	buf = pkg_malloc(sizeof(char) * (len));

	if(buf == 0) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(buf, "Content-Type: ", sizeof("Content-Type: ") - 1);
	memcpy(buf + sizeof("Content-Type: ") - 1, content_type->s,
			content_type->len);
	memcpy(buf + sizeof("Content-Type: ") - 1 + content_type->len, CRLF,
			CRLF_LEN);
	if(add_lump_rpl(msg, buf, len, LUMP_RPL_HDR) == 0) {
		LM_ERR("failed to insert content-type lump\n");
		pkg_free(buf);
		return -1;
	}
	pkg_free(buf);

	/* add body */
	if(add_lump_rpl(msg, body->s, body->len, LUMP_RPL_BODY) == 0) {
		LM_ERR("cannot add body lump\n");
		return -1;
	}

	return 1;
}

/**
 * @brief config function to handle dmq messages
 */
int ki_dmq_handle_message_rc(sip_msg_t *msg, int returnval)
{
	dmq_peer_t *peer;
	if((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s)) {
		LM_ERR("error parsing msg uri\n");
		goto error;
	}
	LM_DBG("dmq_handle_message [%.*s %.*s]\n",
			msg->first_line.u.request.method.len,
			msg->first_line.u.request.method.s,
			msg->first_line.u.request.uri.len, msg->first_line.u.request.uri.s);
	/* the peer id is given as the userinfo part of the request URI */
	peer = find_peer(msg->parsed_uri.user);
	if(!peer) {
		LM_DBG("no peer found for %.*s\n", msg->parsed_uri.user.len,
				msg->parsed_uri.user.s);
		if(_dmq_slb.freply(msg, 404, &dmq_404_rpl) < 0) {
			LM_ERR("sending reply\n");
			goto error;
		}
		return returnval;
	}
	LM_DBG("dmq_handle_message peer found: %.*s\n", msg->parsed_uri.user.len,
			msg->parsed_uri.user.s);
	if(add_dmq_job(msg, peer) < 0) {
		LM_ERR("failed to add dmq job\n");
		goto error;
	}
	return returnval;
error:
	return -1;
}


int ki_dmq_handle_message(sip_msg_t *msg)
{
	return ki_dmq_handle_message_rc(msg, 0);
}

int w_dmq_handle_message(struct sip_msg *msg, char *str1, char *str2)
{
	int i = 0;
	if(str1) {
		if(get_int_fparam(&i, msg, (fparam_t *)str1) < 0)
			return -1;
	}
	if(i > 1)
		i = 1;
	return ki_dmq_handle_message_rc(msg, i);
}

int dmq_handle_message(struct sip_msg *msg, char *str1, char *str2)
{
	return ki_dmq_handle_message_rc(msg, 0);
}

/**
 * @brief config function to process dmq messages
 */
int ki_dmq_process_message_rc(sip_msg_t *msg, int returnval)
{
	dmq_peer_t *peer;
	peer_reponse_t peer_response;
	dmq_node_t *dmq_node = NULL;
	int ret;

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("failed to parse sip message headers\n");
		goto error;
	}

	if((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s)) {
		LM_ERR("error parsing msg uri\n");
		goto error;
	}
	LM_DBG("dmq_handle_message [%.*s %.*s]\n",
			msg->first_line.u.request.method.len,
			msg->first_line.u.request.method.s,
			msg->first_line.u.request.uri.len, msg->first_line.u.request.uri.s);

	/* the peer id is given as the userinfo part of the request URI */
	peer = find_peer(msg->parsed_uri.user);
	if(!peer) {
		LM_DBG("no peer found for %.*s\n", msg->parsed_uri.user.len,
				msg->parsed_uri.user.s);
		if(_dmq_slb.freply(msg, 404, &dmq_404_rpl) < 0) {
			LM_ERR("sending reply\n");
			goto error;
		}
		return returnval;
	}
	LM_DBG("dmq_handle_message peer found: %.*s\n", msg->parsed_uri.user.len,
			msg->parsed_uri.user.s);
	memset(&peer_response, 0, sizeof(peer_response));

	if(parse_from_header(msg) < 0) {
		LM_ERR("bad sip message or missing From hdr\n");
	} else {
		dmq_node = find_dmq_node_uri(dmq_node_list, &get_from(msg)->uri);
	}

	ret = peer->callback(msg, &peer_response, dmq_node);
	if(ret < 0) {
		LM_ERR("processing failed\n");
		goto error;
	}
	/* add the body to the reply */
	if(peer_response.body.s) {
		if(set_reply_body(msg, &peer_response.body, &peer_response.content_type)
				< 0) {
			LM_ERR("error adding reply body lumps\n");
			goto error;
		}
	}
	/* send the reply */
	if(peer_response.resp_code > 0 && peer_response.reason.s != NULL
			&& peer_response.reason.len > 0) {
		if(_dmq_slb.freply(msg, peer_response.resp_code, &peer_response.reason)
				< 0) {
			LM_ERR("error sending reply\n");
		} else {
			LM_DBG("done sending reply\n");
		}
	} else {
		LM_WARN("no reply sent\n");
	}

	return returnval;
error:
	return -1;
}


int ki_dmq_process_message(sip_msg_t *msg)
{
	return ki_dmq_process_message_rc(msg, 0);
}

int w_dmq_process_message(struct sip_msg *msg, char *str1, char *str2)
{
	int i = 0;
	if(str1) {
		if(get_int_fparam(&i, msg, (fparam_t *)str1) < 0)
			return -1;
	}
	if(i > 1)
		i = 1;
	return ki_dmq_process_message_rc(msg, i);
}

int dmq_process_message(struct sip_msg *msg, char *str1, char *str2)
{
	return ki_dmq_process_message_rc(msg, 0);
}


/**
 * @brief handle or process a custom dmq message using
 * peer, from uri, body and content type from parameters
 */
int ki_dmq_do_custom(sip_msg_t *msg, str *peer, str *from,
		str *body, str *content_type, int handle)
{
	dmq_peer_t *found_peer;
	peer_reponse_t peer_response;
	dmq_node_t *dmq_node = NULL;
	sip_msg_t *new_msg;
#define MAX_BRANCHID 9999999
#define MIN_BRANCHID 1000000
#define DMQ_MSG_BUF_SIZE 65507
	char buf[DMQ_MSG_BUF_SIZE];
	int wlen;
	str sproto = STR_NULL;
	str host = dmq_server_uri.host;
	str port = dmq_server_uri.port;
	str to = {0, 0};

	found_peer = find_peer(*peer);
	if(!found_peer) {
		LM_DBG("no peer found for %.*s\n", peer->len, peer->s);
		return -1;
	}
	LM_DBG("dmq_handle_custom peer found: %.*s\n", peer->len, peer->s);

	if(build_uri_str(&found_peer->peer_id, &dmq_server_uri, &to) < 0) {
		LM_ERR("error building from string\n");
		goto error;
	}
	get_valid_proto_string(dmq_server_uri.proto, 1, 1, &sproto);

	wlen = snprintf(buf, sizeof(buf),
			"%.*s %.*s SIP/2.0\r\n"
			"Via: SIP/2.0/%.*s %.*s:%.*s;branch=z9hG4bK%ld\r\n"
			"From: <%.*s>;tag=%ld\r\n"
			"To: <%.*s>\r\n"
			"Call-ID: %ld@%.*s\r\n"
			"CSeq: 1 %.*s\r\n"
			"Max-Forwards: 1\r\n"
			"Content-Type: %.*s\r\n"
			"Content-Length: %d\r\n\r\n"
			"%.*s",
			/* request line */
			dmq_request_method.len, dmq_request_method.s,
			to.len, to.s,
			/* Via */
			sproto.len, sproto.s,
			host.len, host.s,
			port.len, port.s,
			(long)(kam_rand() / (float)KAM_RAND_MAX
							* (MAX_BRANCHID - MIN_BRANCHID)
					+ MIN_BRANCHID),
			/* From */
			from->len, from->s,
			(long)(kam_rand() / (float)KAM_RAND_MAX
							* (MAX_BRANCHID - MIN_BRANCHID)
					+ MIN_BRANCHID),
			/* To */
			to.len, to.s,
			/* Call-ID */
			(long)(kam_rand() / (float)KAM_RAND_MAX
							* (MAX_BRANCHID - MIN_BRANCHID)
					+ MIN_BRANCHID),
			host.len, host.s,
			/* CSeq */
			dmq_request_method.len, dmq_request_method.s,
			/* Content-Type */
			content_type->len, content_type->s,
			/* Content-Length */
			body->len,
			/* body */
			body->len, body->s);
	LM_DBG("dmq custom request (len: %d) [[\n%.*s]]\n", wlen,
			wlen, buf);

	if(wlen < 0 || wlen >= DMQ_MSG_BUF_SIZE) {
		LM_ERR("failed to build synthetic dmq message buffer\n");
		pkg_free(buf);
		return -1;
	}

	new_msg = pkg_malloc(sizeof(sip_msg_t));
	if(new_msg == 0) {
		LM_ERR("no pkg mem for sip msg\n");
		goto error;
	}
	memset(new_msg, 0, sizeof(sip_msg_t));
	new_msg->buf = buf;
	new_msg->len = wlen;
	new_msg->id = (long)(kam_rand() / (float)KAM_RAND_MAX
									* (MAX_BRANCHID - MIN_BRANCHID) + MIN_BRANCHID);

	if(parse_msg(buf, wlen, new_msg) != 0) {
		LM_ERR("failed to parse synthetic dmq message\n");
		goto error;
	}

	if(handle) {
		/* handle custom messages */
		if(add_dmq_job(new_msg, found_peer) < 0) {
			LM_ERR("failed to add dmq job\n");
			goto error;
		}
	} else {
		/* proceed custom message */
		memset(&peer_response, 0, sizeof(peer_response));
		/* resolve dmq_node from the from parameter (originating node uri) */
		dmq_node = find_dmq_node_uri(dmq_node_list, from);
		if(found_peer->callback(new_msg, &peer_response, dmq_node) < 0) {
			LM_ERR("processing failed\n");
			goto error;
		}
	}

	free_sip_msg(new_msg);
	pkg_free(new_msg);
	return 1;

error:
	free_sip_msg(new_msg);
	pkg_free(new_msg);
	return -1;
}

int dmq_do_custom(struct sip_msg *msg, char *peer, char *from,
		char *body, char *content_type, int handle)
{
	str peer_str;
	str from_str;
	str body_str;
	str ct_str;

	if(get_str_fparam(&peer_str, msg, (fparam_t *)peer) < 0) {
		LM_ERR("cannot get peer value\n");
		return -1;
	}
	if(get_str_fparam(&from_str, msg, (fparam_t *)from) < 0) {
		LM_ERR("cannot get from value\n");
		return -1;
	}
	if(get_str_fparam(&body_str, msg, (fparam_t *)body) < 0) {
		LM_ERR("cannot get body value\n");
		return -1;
	}
	if(get_str_fparam(&ct_str, msg, (fparam_t *)content_type) < 0) {
		LM_ERR("cannot get content-type value\n");
		return -1;
	}

	return ki_dmq_do_custom(msg, &peer_str, &from_str, &body_str, &ct_str, handle);
}

int dmq_process_custom(struct sip_msg *msg, char *peer, char *from,
		char *body, char *content_type)
{
	return dmq_do_custom(msg, peer, from, body, content_type, 0);
}
int ki_dmq_process_custom(sip_msg_t *msg, str *peer, str *from,
		str *body, str *content_type)
{
	return ki_dmq_do_custom(msg, peer, from, body, content_type, 0);
}

int dmq_handle_custom(struct sip_msg *msg, char *peer, char *from,
		char *body, char *content_type)
{
	return dmq_do_custom(msg, peer, from, body, content_type, 1);
}
int ki_dmq_handle_custom(sip_msg_t *msg, str *peer, str *from,
		str *body, str *content_type)
{
	return ki_dmq_do_custom(msg, peer, from, body, content_type, 1);
}
