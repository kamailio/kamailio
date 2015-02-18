/*
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#include <openssl/sha.h>

#include "../../basex.h"
#include "../../data_lump_rpl.h"
#include "../../dprint.h"
#include "../../locking.h"
#include "../../str.h"
#include "../../tcp_conn.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../lib/kmi/tree.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../sl/sl.h"
#include "../tls/tls_cfg.h"
#include "ws_conn.h"
#include "ws_handshake.h"
#include "ws_mod.h"
#include "config.h"

#define WS_VERSION		(13)

int ws_sub_protocols = DEFAULT_SUB_PROTOCOLS;
int ws_cors_mode = CORS_MODE_NONE;

stat_var *ws_failed_handshakes;
stat_var *ws_successful_handshakes;
stat_var *ws_sip_successful_handshakes;
stat_var *ws_msrp_successful_handshakes;

static str str_sip = str_init("sip");
static str str_msrp = str_init("msrp");
static str str_upgrade = str_init("upgrade");
static str str_websocket = str_init("websocket");
static str str_ws_guid = str_init("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

/* HTTP headers */
static str str_hdr_connection = str_init("Connection");
static str str_hdr_upgrade = str_init("Upgrade");
static str str_hdr_sec_websocket_accept = str_init("Sec-WebSocket-Accept");
static str str_hdr_sec_websocket_key = str_init("Sec-WebSocket-Key");
static str str_hdr_sec_websocket_protocol = str_init("Sec-WebSocket-Protocol");
static str str_hdr_sec_websocket_version = str_init("Sec-WebSocket-Version");
static str str_hdr_origin = str_init("Origin");
static str str_hdr_access_control_allow_origin
				= str_init("Access-Control-Allow-Origin");
#define CONNECTION		(1<<0)
#define UPGRADE			(1<<1)
#define SEC_WEBSOCKET_ACCEPT	(1<<2)
#define SEC_WEBSOCKET_KEY	(1<<3)
#define SEC_WEBSOCKET_PROTOCOL	(1<<4)
#define SEC_WEBSOCKET_VERSION	(1<<5)
#define ORIGIN			(1<<6)

#define REQUIRED_HEADERS	(CONNECTION | UPGRADE | SEC_WEBSOCKET_KEY\
					| SEC_WEBSOCKET_PROTOCOL\
					| SEC_WEBSOCKET_VERSION)

/* HTTP status text */
static str str_status_switching_protocols = str_init("Switching Protocols");
static str str_status_bad_request = str_init("Bad Request");
static str str_status_upgrade_required = str_init("Upgrade Required");
static str str_status_internal_server_error = str_init("Internal Server Error");
static str str_status_service_unavailable = str_init("Service Unavailable");

#define HDR_BUF_LEN		(512)
static char headers_buf[HDR_BUF_LEN];

static char key_buf[base64_enc_len(SHA_DIGEST_LENGTH)];

static int ws_send_reply(sip_msg_t *msg, int code, str *reason, str *hdrs)
{
	if (hdrs && hdrs->len > 0)
	{
		if (add_lump_rpl(msg, hdrs->s, hdrs->len, LUMP_RPL_HDR) == 0)
		{
			LM_ERR("inserting extra-headers lump\n");
			update_stat(ws_failed_handshakes, 1);
			return -1;
		}
	}

	if (ws_slb.freply(msg, code, reason) < 0)
	{
		LM_ERR("sending reply\n");
		update_stat(ws_failed_handshakes, 1);
		return -1;
	}

	update_stat(
		code == 101 ? ws_successful_handshakes : ws_failed_handshakes,
		1);

	return 0;
}

int ws_handle_handshake(struct sip_msg *msg)
{
	str key = {0, 0}, headers = {0, 0}, reply_key = {0, 0}, origin = {0, 0};
	unsigned char sha1[SHA_DIGEST_LENGTH];
	unsigned int hdr_flags = 0, sub_protocol = 0;
	int version = 0;
	struct hdr_field *hdr = msg->headers;
	struct tcp_connection *con;
	ws_connection_t *wsc;

	/* Make sure that the connection is closed after the response _and_
	   the existing connection (from the request) is reused for the
	   response.  The close flag will be unset later if the handshake is
	   successful. */
	msg->rpl_send_flags.f |= SND_F_CON_CLOSE;
	msg->rpl_send_flags.f |= SND_F_FORCE_CON_REUSE;

	if (cfg_get(websocket, ws_cfg, enabled) == 0)
	{
		LM_INFO("disabled: bouncing handshake\n");
		ws_send_reply(msg, 503, &str_status_service_unavailable,
				NULL);
		return 0;
	}

	/* Retrieve TCP/TLS connection */
	if ((con = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, 0)) == NULL)
	{
		LM_ERR("retrieving connection\n");
		ws_send_reply(msg, 500, &str_status_internal_server_error,
				NULL);
		return 0;
	}

	if (con->type != PROTO_TCP && con->type != PROTO_TLS)
	{
		LM_ERR("unsupported transport: %d", con->type);
		goto end;
	}

	if (parse_headers(msg, HDR_EOH_F, 0) < 0)
	{
		LM_ERR("error parsing headers\n");
		ws_send_reply(msg, 500, &str_status_internal_server_error,
				NULL);
		goto end;
	}

	/* Process HTTP headers */
	while (hdr != NULL)
	{
		/* Decode and validate Connection */
		if (cmp_hdrname_strzn(&hdr->name,
				str_hdr_connection.s,
				str_hdr_connection.len) == 0)
		{
			strlower(&hdr->body);
			if (str_search(&hdr->body, &str_upgrade) != NULL)
			{
				LM_DBG("found %.*s: %.*s\n",

					hdr->name.len, hdr->name.s,
					hdr->body.len, hdr->body.s);
				hdr_flags |= CONNECTION;
			}
		}
		/* Decode and validate Upgrade */
		else if (cmp_hdrname_strzn(&hdr->name,
				str_hdr_upgrade.s,
				str_hdr_upgrade.len) == 0)
		{
			strlower(&hdr->body);
			if (str_search(&hdr->body, &str_websocket) != NULL)
			{
				LM_DBG("found %.*s: %.*s\n",
					hdr->name.len, hdr->name.s,
					hdr->body.len, hdr->body.s);
				hdr_flags |= UPGRADE;
			}
		}
		/* Decode and validate Sec-WebSocket-Key */
		else if (cmp_hdrname_strzn(&hdr->name,
				str_hdr_sec_websocket_key.s, 
				str_hdr_sec_websocket_key.len) == 0) 
		{
			if (hdr_flags & SEC_WEBSOCKET_KEY)
			{
				LM_WARN("%.*s found multiple times\n",
					hdr->name.len, hdr->name.s);
				ws_send_reply(msg, 400,
						&str_status_bad_request,
						NULL);
				goto end;
			}

			LM_DBG("found %.*s: %.*s\n",
				hdr->name.len, hdr->name.s,
				hdr->body.len, hdr->body.s);
			key = hdr->body;
			hdr_flags |= SEC_WEBSOCKET_KEY;
		}
		/* Decode and validate Sec-WebSocket-Protocol */
		else if (cmp_hdrname_strzn(&hdr->name,
				str_hdr_sec_websocket_protocol.s,
				str_hdr_sec_websocket_protocol.len) == 0)
		{
			strlower(&hdr->body);
			if (str_search(&hdr->body, &str_sip) != NULL)
			{
				LM_DBG("found %.*s: %.*s\n",
					hdr->name.len, hdr->name.s,
					hdr->body.len, hdr->body.s);
				hdr_flags |= SEC_WEBSOCKET_PROTOCOL;
				sub_protocol |= SUB_PROTOCOL_SIP;
			}
			if (str_search(&hdr->body, &str_msrp) != NULL)
			{
				LM_DBG("found %.*s: %.*s\n",
					hdr->name.len, hdr->name.s,
					hdr->body.len, hdr->body.s);
				hdr_flags |= SEC_WEBSOCKET_PROTOCOL;
				sub_protocol |= SUB_PROTOCOL_MSRP;
			}
		}
		/* Decode and validate Sec-WebSocket-Version */
		else if (cmp_hdrname_strzn(&hdr->name,
				str_hdr_sec_websocket_version.s,
				str_hdr_sec_websocket_version.len) == 0)
		{
			if (hdr_flags & SEC_WEBSOCKET_VERSION)
			{
				LM_WARN("%.*s found multiple times\n",
					hdr->name.len, hdr->name.s);
				ws_send_reply(msg, 400,
						&str_status_bad_request,
						NULL);
				goto end;
			}

			str2sint(&hdr->body, &version);

			if (version != WS_VERSION)
			{
				LM_WARN("Unsupported protocol version %.*s\n",
					hdr->body.len, hdr->body.s);
				headers.s = headers_buf;
				headers.len = snprintf(headers.s, HDR_BUF_LEN,
					"%.*s: %d\r\n",
					str_hdr_sec_websocket_version.len,
					str_hdr_sec_websocket_version.s,
					WS_VERSION);
				ws_send_reply(msg, 426,
						&str_status_upgrade_required,
						&headers);
				goto end;
			}

			LM_DBG("found %.*s: %.*s\n",
				hdr->name.len, hdr->name.s,
				hdr->body.len, hdr->body.s);
			hdr_flags |= SEC_WEBSOCKET_VERSION;
		}
		/* Decode Origin */
		else if (cmp_hdrname_strzn(&hdr->name,
				str_hdr_origin.s,
				str_hdr_origin.len) == 0)
		{
			if (hdr_flags & ORIGIN)
			{
				LM_WARN("%.*s found multiple times\n",
					hdr->name.len, hdr->name.s);
				ws_send_reply(msg, 400,
						&str_status_bad_request,
						NULL);
				goto end;
			}

			LM_DBG("found %.*s: %.*s\n",
				hdr->name.len, hdr->name.s,
				hdr->body.len, hdr->body.s);
			origin = hdr->body;
			hdr_flags |= ORIGIN;
		}

		hdr = hdr->next;
	}

	/* Final check that all required headers/values were found */
	sub_protocol &= ws_sub_protocols;
	if ((hdr_flags & REQUIRED_HEADERS) != REQUIRED_HEADERS
			|| sub_protocol == 0)
	{

		LM_WARN("required headers not present\n");
		headers.s = headers_buf;
		headers.len = 0;

		if (ws_sub_protocols & SUB_PROTOCOL_SIP)
			headers.len += snprintf(headers.s + headers.len,
						HDR_BUF_LEN - headers.len,
						"%.*s: %.*s\r\n",
					str_hdr_sec_websocket_protocol.len,
					str_hdr_sec_websocket_protocol.s,
					str_sip.len, str_sip.s);

		if (ws_sub_protocols & SUB_PROTOCOL_MSRP)
			headers.len += snprintf(headers.s + headers.len,
						HDR_BUF_LEN - headers.len,
						"%.*s: %.*s\r\n",
					str_hdr_sec_websocket_protocol.len,
					str_hdr_sec_websocket_protocol.s,
					str_msrp.len, str_msrp.s);

		headers.len += snprintf(headers.s + headers.len,
					HDR_BUF_LEN - headers.len,
					"%.*s: %d\r\n",
					str_hdr_sec_websocket_version.len,
					str_hdr_sec_websocket_version.s,
					WS_VERSION);
		ws_send_reply(msg, 400, &str_status_bad_request, &headers);
		goto end;
	}

	/* Construct reply_key */
	reply_key.s = (char *) pkg_malloc(
				(key.len + str_ws_guid.len) * sizeof(char)); 
	if (reply_key.s == NULL)
	{
		LM_ERR("allocating pkg memory\n");
		ws_send_reply(msg, 500, &str_status_internal_server_error,
				NULL);
		goto end;
	}
	memcpy(reply_key.s, key.s, key.len);
	memcpy(reply_key.s + key.len, str_ws_guid.s, str_ws_guid.len);
	reply_key.len = key.len + str_ws_guid.len;
	SHA1((const unsigned char *) reply_key.s, reply_key.len, sha1);
	pkg_free(reply_key.s);
	reply_key.s = key_buf;
	reply_key.len = base64_enc(sha1, SHA_DIGEST_LENGTH,
				(unsigned char *) reply_key.s,
				base64_enc_len(SHA_DIGEST_LENGTH));

	/* Add the connection to the WebSocket connection table */
	wsconn_add(msg->rcv, sub_protocol);

	/* Make sure Kamailio core sends future messages on this connection
	   directly to this module */
	if (con->type == PROTO_TLS)
		con->type = con->rcv.proto = PROTO_WSS;
	else
		con->type = con->rcv.proto = PROTO_WS;

	/* Now Kamailio is ready to receive WebSocket frames build and send a
	   101 reply */
	headers.s = headers_buf;
	headers.len = 0;

	if (ws_cors_mode == CORS_MODE_ANY)
		headers.len += snprintf(headers.s + headers.len,
					HDR_BUF_LEN - headers.len,
					"%.*s: *\r\n",
					str_hdr_access_control_allow_origin.len,
					str_hdr_access_control_allow_origin.s);
	else if (ws_cors_mode == CORS_MODE_ORIGIN && origin.len > 0)
		headers.len += snprintf(headers.s + headers.len,
					HDR_BUF_LEN - headers.len,
					"%.*s: %.*s\r\n",
					str_hdr_access_control_allow_origin.len,
					str_hdr_access_control_allow_origin.s,
					origin.len,
					origin.s);

	if (sub_protocol & SUB_PROTOCOL_SIP)
		headers.len += snprintf(headers.s + headers.len,
					HDR_BUF_LEN - headers.len,
					"%.*s: %.*s\r\n",
					str_hdr_sec_websocket_protocol.len,
					str_hdr_sec_websocket_protocol.s,
					str_sip.len, str_sip.s);
	else if (sub_protocol & SUB_PROTOCOL_MSRP)
		headers.len += snprintf(headers.s + headers.len,
					HDR_BUF_LEN - headers.len,
					"%.*s: %.*s\r\n",
					str_hdr_sec_websocket_protocol.len,
					str_hdr_sec_websocket_protocol.s,
					str_msrp.len, str_msrp.s);

	headers.len += snprintf(headers.s + headers.len,
				HDR_BUF_LEN - headers.len,
				"%.*s: %.*s\r\n"
				"%.*s: %.*s\r\n"
				"%.*s: %.*s\r\n",
				str_hdr_upgrade.len, str_hdr_upgrade.s,
				str_websocket.len, str_websocket.s,
				str_hdr_connection.len, str_hdr_connection.s,
				str_upgrade.len, str_upgrade.s,
				str_hdr_sec_websocket_accept.len,
				str_hdr_sec_websocket_accept.s, reply_key.len,
				reply_key.s);
	msg->rpl_send_flags.f &= ~SND_F_CON_CLOSE;
	if (ws_send_reply(msg, 101, &str_status_switching_protocols,
				&headers) < 0)
	{
		if ((wsc = wsconn_get(msg->rcv.proto_reserved1)) != NULL)
		{
			wsconn_rm(wsc, WSCONN_EVENTROUTE_NO);
			wsconn_put(wsc);
		}
		goto end;
	}
	else
	{
		if (sub_protocol & SUB_PROTOCOL_SIP)
			update_stat(ws_sip_successful_handshakes, 1);
		else if (sub_protocol & SUB_PROTOCOL_MSRP)
			update_stat(ws_msrp_successful_handshakes, 1);
	}

	tcpconn_put(con);
	return 1;
end:
	if (con)
		tcpconn_put(con);
	return 0;
}

struct mi_root *ws_mi_disable(struct mi_root *cmd, void *param)
{
	cfg_get(websocket, ws_cfg, enabled) = 0;
	LM_WARN("disabling websockets - new connections will be dropped\n");
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

struct mi_root *ws_mi_enable(struct mi_root *cmd, void *param)
{
	cfg_get(websocket, ws_cfg, enabled) = 1;
	LM_WARN("enabling websockets\n");
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}
