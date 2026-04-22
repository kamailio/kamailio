/*
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#include "../../core/basex.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/dprint.h"
#include "../../core/locking.h"
#include "../../core/str.h"
#include "../../core/trim.h"
#include "../../core/tcp_conn.h"
#include "../../core/tcp_server.h"
#include "../../core/counters.h"
#include "../../core/strutils.h"
#include "../../core/crypto/shautils.h"
#include "../../core/globals.h"
#include "../../core/mem/mem.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/proxy.h"
#include "../../core/rand/cryptorand.h"
#include "../sl/sl.h"
#include "../tls/tls_cfg.h"
#include "ws_conn.h"
#include "ws_handshake.h"
#include "websocket.h"
#include "config.h"
#include <strings.h>

#define WS_VERSION (13)

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
static str str_hdr_access_control_allow_origin =
		str_init("Access-Control-Allow-Origin");
#define CONNECTION (1 << 0)
#define UPGRADE (1 << 1)
#define SEC_WEBSOCKET_ACCEPT (1 << 2)
#define SEC_WEBSOCKET_KEY (1 << 3)
#define SEC_WEBSOCKET_PROTOCOL (1 << 4)
#define SEC_WEBSOCKET_VERSION (1 << 5)
#define ORIGIN (1 << 6)

#define REQUIRED_HEADERS                                               \
	(CONNECTION | UPGRADE | SEC_WEBSOCKET_KEY | SEC_WEBSOCKET_PROTOCOL \
			| SEC_WEBSOCKET_VERSION)

/* HTTP status text */
static str str_status_switching_protocols = str_init("Switching Protocols");
static str str_status_bad_request = str_init("Bad Request");
static str str_status_upgrade_required = str_init("Upgrade Required");
static str str_status_internal_server_error = str_init("Internal Server Error");
static str str_status_service_unavailable = str_init("Service Unavailable");

#define HDR_BUF_LEN (512)
static char headers_buf[HDR_BUF_LEN];

static char key_buf[base64_enc_len(SHA1_DIGEST_LENGTH)];

static int ws_token_contains(const str *value, const str *token)
{
	int i;

	if(value == NULL || token == NULL || value->s == NULL || token->s == NULL
			|| value->len < token->len)
		return 0;

	for(i = 0; i <= value->len - token->len; i++) {
		if(strncasecmp(value->s + i, token->s, token->len) == 0)
			return 1;
	}

	return 0;
}

static int ws_str_eq(const str *v1, const str *v2)
{
	if(v1 == NULL || v2 == NULL || v1->len != v2->len)
		return 0;
	return (strncasecmp(v1->s, v2->s, v1->len) == 0) ? 1 : 0;
}

static int ws_compute_accept(str *key, char *obuf, int olen)
{
	unsigned char sha1[SHA1_DIGEST_LENGTH];
	char *tmpbuf;
	int tmplen;
	int ret;

	tmplen = key->len + str_ws_guid.len;
	tmpbuf = (char *)pkg_malloc(tmplen);
	if(tmpbuf == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}

	memcpy(tmpbuf, key->s, key->len);
	memcpy(tmpbuf + key->len, str_ws_guid.s, str_ws_guid.len);
	compute_sha1_raw(sha1, (u_int8_t *)tmpbuf, tmplen);
	pkg_free(tmpbuf);

	ret = base64_enc(sha1, SHA1_DIGEST_LENGTH, (unsigned char *)obuf, olen);
	return ret;
}

static int ws_parse_sub_protocol(str *sub_protocol, unsigned int *sub_proto)
{
	if(sub_protocol == NULL || sub_protocol->s == NULL
			|| sub_protocol->len <= 0)
		return -1;

	if(sub_protocol->len == str_sip.len
			&& strncasecmp(sub_protocol->s, str_sip.s, str_sip.len) == 0) {
		*sub_proto = SUB_PROTOCOL_SIP;
		return 0;
	}

	if(sub_protocol->len == str_msrp.len
			&& strncasecmp(sub_protocol->s, str_msrp.s, str_msrp.len) == 0) {
		*sub_proto = SUB_PROTOCOL_MSRP;
		return 0;
	}

	return -1;
}

int ws_parse_url(str *wsurl, ws_address_t *waddr)
{
	char *p;
	char *end;
	char *hstart;
	char *hend;
	char *ppos;
	int port_no = 0;

	if(wsurl == NULL || wsurl->s == NULL || wsurl->len <= 0 || waddr == NULL) {
		LM_ERR("invalid parameters for websocket url parsing\n");
		return -1;
	}

	memset(waddr, 0, sizeof(*waddr));

	p = wsurl->s;
	end = wsurl->s + wsurl->len;

	if(wsurl->len >= 5 && strncasecmp(p, "ws://", 5) == 0) {
		waddr->proto.s = p;
		waddr->proto.len = 2;
		waddr->proto_no = PROTO_WS;
		p += 5;
	} else if(wsurl->len >= 6 && strncasecmp(p, "wss://", 6) == 0) {
		waddr->proto.s = p;
		waddr->proto.len = 3;
		waddr->proto_no = PROTO_WSS;
		p += 6;
	} else {
		LM_ERR("websocket url must start with ws:// or wss://\n");
		return -1;
	}

	if(p >= end) {
		LM_ERR("websocket url missing host\n");
		return -1;
	}

	hstart = p;
	ppos = memchr(p, '/', end - p);
	hend = (ppos != NULL) ? ppos : end;
	if(hstart >= hend) {
		LM_ERR("websocket url missing host\n");
		return -1;
	}

	if(*hstart == '[') {
		char *brend;

		brend = memchr(hstart + 1, ']', hend - hstart - 1);
		if(brend == NULL) {
			LM_ERR("invalid websocket url: unterminated IPv6 host\n");
			return -1;
		}
		waddr->host.s = hstart + 1;
		waddr->host.len = brend - (hstart + 1);
		if(brend + 1 < hend) {
			if(*(brend + 1) != ':') {
				LM_ERR("invalid websocket url after IPv6 host\n");
				return -1;
			}
			waddr->port.s = brend + 2;
			waddr->port.len = hend - waddr->port.s;
			if(waddr->port.len <= 0) {
				LM_ERR("websocket url has empty port\n");
				return -1;
			}
		}
	} else {
		char *colon;

		colon = memchr(hstart, ':', hend - hstart);
		if(colon != NULL) {
			waddr->host.s = hstart;
			waddr->host.len = colon - hstart;
			waddr->port.s = colon + 1;
			waddr->port.len = hend - waddr->port.s;
			if(waddr->port.len <= 0) {
				LM_ERR("websocket url has empty port\n");
				return -1;
			}
		} else {
			waddr->host.s = hstart;
			waddr->host.len = hend - hstart;
		}
	}

	if(waddr->host.len <= 0) {
		LM_ERR("websocket url has empty host\n");
		return -1;
	}

	if(waddr->port.len > 0) {
		if(str2sint(&waddr->port, &port_no) < 0 || port_no <= 0
				|| port_no > 65535) {
			LM_ERR("websocket url has invalid port\n");
			return -1;
		}
		waddr->port_no = port_no;
	} else if(waddr->proto_no == PROTO_WS) {
		waddr->port_no = 80;
	} else {
		waddr->port_no = 443;
	}

	if(ppos != NULL) {
		waddr->path.s = ppos;
		waddr->path.len = end - ppos;
	} else {
		waddr->path.s = end;
		waddr->path.len = 0;
	}

	return 0;
}

static str *ws_sub_protocol_name(unsigned int sub_proto)
{
	if(sub_proto == SUB_PROTOCOL_SIP)
		return &str_sip;
	if(sub_proto == SUB_PROTOCOL_MSRP)
		return &str_msrp;
	return NULL;
}

static struct tcp_connection *ws_find_tcp_connection(dest_info_t *dst)
{
	union sockaddr_union local_addr;
	union sockaddr_union *from = NULL;
	struct ip_addr ip;
	int port;

	port = su_getport(&dst->to);
	if(port == 0)
		return NULL;

	if(dst->ephemeral.vset) {
		if(init_su(&local_addr, &dst->ephemeral.ip, dst->ephemeral.port) == 0)
			from = &local_addr;
	}

	su2ip_addr(&ip, &dst->to);
	return tcpconn_lookup(0, &ip, port, from,
			(dst->ephemeral.vset) ? dst->ephemeral.port : 0, 0, dst->proto);
}

static int ws_response_get_header(
		char *buf, unsigned int len, str *hname, str *hval)
{
	char *p;
	char *end;
	char *line_end;
	char *colon;
	str vtmp;

	p = buf;
	end = buf + len;

	line_end = strstr(p, "\r\n");
	if(line_end == NULL)
		return -1;
	p = line_end + 2;

	while(p < end && !(p[0] == '\r' && p + 1 < end && p[1] == '\n')) {
		line_end = strstr(p, "\r\n");
		if(line_end == NULL)
			line_end = end;
		colon = memchr(p, ':', line_end - p);
		if(colon != NULL) {
			if((int)(colon - p) == hname->len
					&& strncasecmp(p, hname->s, hname->len) == 0) {
				vtmp.s = colon + 1;
				vtmp.len = line_end - vtmp.s;
				trim(&vtmp);
				*hval = vtmp;
				return 0;
			}
		}
		p = line_end + 2;
	}

	return -1;
}

static void ws_mark_failed_client(ws_connection_t *wsc)
{
	if(wsc == NULL)
		return;

	wsconn_rm(wsc, WSCONN_EVENTROUTE_NO);
	wsconn_put(wsc);
}

static int ws_send_buffer(int conid, char *buf, unsigned int len)
{
	ws_event_info_t wsev;
	sr_event_param_t evp = {0};

	memset(&wsev, 0, sizeof(ws_event_info_t));
	wsev.type = SREV_TCP_WS_FRAME_OUT;
	wsev.buf = buf;
	wsev.len = len;
	wsev.id = conid;
	evp.data = (void *)&wsev;

	return (sr_event_exec(SREV_TCP_WS_FRAME_OUT, &evp) < 0) ? -1 : 1;
}

static int ws_flush_send_queue(ws_connection_t *wsc)
{
	ws_send_item_t *item;
	ws_send_item_t *next;

	if(wsc == NULL)
		return -1;

	item = wsconn_sendq_detach(wsc);
	while(item != NULL) {
		next = item->next;
		if(ws_send_buffer(wsc->id, item->data, item->len) < 0) {
			LM_ERR("failed to flush websocket send queue for connection %d\n",
					wsc->id);
			wsconn_sendq_free_list(next);
			shm_free(item);
			return -1;
		}
		shm_free(item);
		item = next;
	}

	return 0;
}

static int ws_send_reply(sip_msg_t *msg, int code, str *reason, str *hdrs)
{
	if(hdrs && hdrs->len > 0) {
		if(add_lump_rpl(msg, hdrs->s, hdrs->len, LUMP_RPL_HDR) == 0) {
			LM_ERR("inserting extra-headers lump\n");
			update_stat(ws_failed_handshakes, 1);
			return -1;
		}
	}

	if(ws_slb.freply(msg, code, reason) < 0) {
		LM_ERR("sending reply\n");
		update_stat(ws_failed_handshakes, 1);
		return -1;
	}

	update_stat(
			code == 101 ? ws_successful_handshakes : ws_failed_handshakes, 1);

	return 0;
}

int ws_handle_handshake(struct sip_msg *msg)
{
	str key = {0, 0}, headers = {0, 0}, reply_key = {0, 0}, origin = {0, 0};
	unsigned char sha1[SHA1_DIGEST_LENGTH];
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

	if(cfg_get(websocket, ws_cfg, enabled) == 0) {
		LM_INFO("disabled: bouncing handshake\n");
		ws_send_reply(msg, 503, &str_status_service_unavailable, NULL);
		return 0;
	}

	/* Retrieve TCP/TLS connection */
	if((con = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, 0)) == NULL) {
		LM_ERR("retrieving connection\n");
		ws_send_reply(msg, 500, &str_status_internal_server_error, NULL);
		return 0;
	}

	if(con->type != PROTO_TCP && con->type != PROTO_TLS) {
		LM_ERR("unsupported transport: %d", con->type);
		goto end;
	}

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		ws_send_reply(msg, 500, &str_status_internal_server_error, NULL);
		goto end;
	}

	/* Process HTTP headers */
	while(hdr != NULL) {
		/* Decode and validate Connection */
		if(cmp_hdrname_strzn(
				   &hdr->name, str_hdr_connection.s, str_hdr_connection.len)
				== 0) {
			strlower(&hdr->body);
			if(str_search(&hdr->body, &str_upgrade) != NULL) {
				LM_DBG("found %.*s: %.*s\n",

						hdr->name.len, hdr->name.s, hdr->body.len, hdr->body.s);
				hdr_flags |= CONNECTION;
			}
		}
		/* Decode and validate Upgrade */
		else if(cmp_hdrname_strzn(
						&hdr->name, str_hdr_upgrade.s, str_hdr_upgrade.len)
				== 0) {
			strlower(&hdr->body);
			if(str_search(&hdr->body, &str_websocket) != NULL) {
				LM_DBG("found %.*s: %.*s\n", hdr->name.len, hdr->name.s,
						hdr->body.len, hdr->body.s);
				hdr_flags |= UPGRADE;
			}
		}
		/* Decode and validate Sec-WebSocket-Key */
		else if(cmp_hdrname_strzn(&hdr->name, str_hdr_sec_websocket_key.s,
						str_hdr_sec_websocket_key.len)
				== 0) {
			if(hdr_flags & SEC_WEBSOCKET_KEY) {
				LM_WARN("%.*s found multiple times\n", hdr->name.len,
						hdr->name.s);
				ws_send_reply(msg, 400, &str_status_bad_request, NULL);
				goto end;
			}

			LM_DBG("found %.*s: %.*s\n", hdr->name.len, hdr->name.s,
					hdr->body.len, hdr->body.s);
			key = hdr->body;
			hdr_flags |= SEC_WEBSOCKET_KEY;
		}
		/* Decode and validate Sec-WebSocket-Protocol */
		else if(cmp_hdrname_strzn(&hdr->name, str_hdr_sec_websocket_protocol.s,
						str_hdr_sec_websocket_protocol.len)
				== 0) {
			strlower(&hdr->body);
			if(str_search(&hdr->body, &str_sip) != NULL) {
				LM_DBG("found %.*s: %.*s\n", hdr->name.len, hdr->name.s,
						hdr->body.len, hdr->body.s);
				hdr_flags |= SEC_WEBSOCKET_PROTOCOL;
				sub_protocol |= SUB_PROTOCOL_SIP;
			}
			if(str_search(&hdr->body, &str_msrp) != NULL) {
				LM_DBG("found %.*s: %.*s\n", hdr->name.len, hdr->name.s,
						hdr->body.len, hdr->body.s);
				hdr_flags |= SEC_WEBSOCKET_PROTOCOL;
				sub_protocol |= SUB_PROTOCOL_MSRP;
			}
		}
		/* Decode and validate Sec-WebSocket-Version */
		else if(cmp_hdrname_strzn(&hdr->name, str_hdr_sec_websocket_version.s,
						str_hdr_sec_websocket_version.len)
				== 0) {
			if(hdr_flags & SEC_WEBSOCKET_VERSION) {
				LM_WARN("%.*s found multiple times\n", hdr->name.len,
						hdr->name.s);
				ws_send_reply(msg, 400, &str_status_bad_request, NULL);
				goto end;
			}

			str2sint(&hdr->body, &version);

			if(version != WS_VERSION) {
				LM_WARN("Unsupported protocol version %.*s\n", hdr->body.len,
						hdr->body.s);
				headers.s = headers_buf;
				headers.len = snprintf(headers.s, HDR_BUF_LEN, "%.*s: %d\r\n",
						str_hdr_sec_websocket_version.len,
						str_hdr_sec_websocket_version.s, WS_VERSION);
				ws_send_reply(msg, 426, &str_status_upgrade_required, &headers);
				goto end;
			}

			LM_DBG("found %.*s: %.*s\n", hdr->name.len, hdr->name.s,
					hdr->body.len, hdr->body.s);
			hdr_flags |= SEC_WEBSOCKET_VERSION;
		}
		/* Decode Origin */
		else if(cmp_hdrname_strzn(
						&hdr->name, str_hdr_origin.s, str_hdr_origin.len)
				== 0) {
			if(hdr_flags & ORIGIN) {
				LM_WARN("%.*s found multiple times\n", hdr->name.len,
						hdr->name.s);
				ws_send_reply(msg, 400, &str_status_bad_request, NULL);
				goto end;
			}

			LM_DBG("found %.*s: %.*s\n", hdr->name.len, hdr->name.s,
					hdr->body.len, hdr->body.s);
			origin = hdr->body;
			hdr_flags |= ORIGIN;
		}

		hdr = hdr->next;
	}

	/* Final check that all required headers/values were found */
	sub_protocol &= ws_sub_protocols;
	if((hdr_flags & REQUIRED_HEADERS) != REQUIRED_HEADERS
			|| sub_protocol == 0) {

		LM_WARN("required headers not present\n");
		headers.s = headers_buf;
		headers.len = 0;

		if(ws_sub_protocols & SUB_PROTOCOL_SIP)
			headers.len += snprintf(headers.s + headers.len,
					HDR_BUF_LEN - headers.len, "%.*s: %.*s\r\n",
					str_hdr_sec_websocket_protocol.len,
					str_hdr_sec_websocket_protocol.s, str_sip.len, str_sip.s);

		if(ws_sub_protocols & SUB_PROTOCOL_MSRP)
			headers.len += snprintf(headers.s + headers.len,
					HDR_BUF_LEN - headers.len, "%.*s: %.*s\r\n",
					str_hdr_sec_websocket_protocol.len,
					str_hdr_sec_websocket_protocol.s, str_msrp.len, str_msrp.s);

		headers.len +=
				snprintf(headers.s + headers.len, HDR_BUF_LEN - headers.len,
						"%.*s: %d\r\n", str_hdr_sec_websocket_version.len,
						str_hdr_sec_websocket_version.s, WS_VERSION);
		ws_send_reply(msg, 400, &str_status_bad_request, &headers);
		goto end;
	}

	/* Construct reply_key */
	reply_key.s =
			(char *)pkg_malloc((key.len + str_ws_guid.len) * sizeof(char));
	if(reply_key.s == NULL) {
		PKG_MEM_ERROR;
		ws_send_reply(msg, 500, &str_status_internal_server_error, NULL);
		goto end;
	}
	memcpy(reply_key.s, key.s, key.len);
	memcpy(reply_key.s + key.len, str_ws_guid.s, str_ws_guid.len);
	reply_key.len = key.len + str_ws_guid.len;
	compute_sha1_raw(sha1, (u_int8_t *)reply_key.s, reply_key.len);
	pkg_free(reply_key.s);
	reply_key.s = key_buf;
	reply_key.len = base64_enc(sha1, SHA1_DIGEST_LENGTH,
			(unsigned char *)reply_key.s, base64_enc_len(SHA1_DIGEST_LENGTH));

	/* Add the connection to the WebSocket connection table */
	wsconn_add(&msg->rcv, sub_protocol);

	/* Make sure Kamailio core sends future messages on this connection
	   directly to this module */
	if(con->type == PROTO_TLS)
		con->type = con->rcv.proto = PROTO_WSS;
	else
		con->type = con->rcv.proto = PROTO_WS;

	/* Now Kamailio is ready to receive WebSocket frames build and send a
	   101 reply */
	headers.s = headers_buf;
	headers.len = 0;

	if(ws_cors_mode == CORS_MODE_ANY)
		headers.len +=
				snprintf(headers.s + headers.len, HDR_BUF_LEN - headers.len,
						"%.*s: *\r\n", str_hdr_access_control_allow_origin.len,
						str_hdr_access_control_allow_origin.s);
	else if(ws_cors_mode == CORS_MODE_ORIGIN && origin.len > 0)
		headers.len += snprintf(headers.s + headers.len,
				HDR_BUF_LEN - headers.len, "%.*s: %.*s\r\n",
				str_hdr_access_control_allow_origin.len,
				str_hdr_access_control_allow_origin.s, origin.len, origin.s);

	if(sub_protocol & SUB_PROTOCOL_SIP)
		headers.len += snprintf(headers.s + headers.len,
				HDR_BUF_LEN - headers.len, "%.*s: %.*s\r\n",
				str_hdr_sec_websocket_protocol.len,
				str_hdr_sec_websocket_protocol.s, str_sip.len, str_sip.s);
	else if(sub_protocol & SUB_PROTOCOL_MSRP)
		headers.len += snprintf(headers.s + headers.len,
				HDR_BUF_LEN - headers.len, "%.*s: %.*s\r\n",
				str_hdr_sec_websocket_protocol.len,
				str_hdr_sec_websocket_protocol.s, str_msrp.len, str_msrp.s);

	headers.len += snprintf(headers.s + headers.len, HDR_BUF_LEN - headers.len,
			"%.*s: %.*s\r\n"
			"%.*s: %.*s\r\n"
			"%.*s: %.*s\r\n",
			str_hdr_upgrade.len, str_hdr_upgrade.s, str_websocket.len,
			str_websocket.s, str_hdr_connection.len, str_hdr_connection.s,
			str_upgrade.len, str_upgrade.s, str_hdr_sec_websocket_accept.len,
			str_hdr_sec_websocket_accept.s, reply_key.len, reply_key.s);
	msg->rpl_send_flags.f &= ~SND_F_CON_CLOSE;
	if(ws_send_reply(msg, 101, &str_status_switching_protocols, &headers) < 0) {
		if((wsc = wsconn_get(msg->rcv.proto_reserved1)) != NULL) {
			wsconn_rm(wsc, WSCONN_EVENTROUTE_NO);
			wsconn_put(wsc);
		}
		goto end;
	} else {
		if(sub_protocol & SUB_PROTOCOL_SIP)
			update_stat(ws_sip_successful_handshakes, 1);
		else if(sub_protocol & SUB_PROTOCOL_MSRP)
			update_stat(ws_msrp_successful_handshakes, 1);
	}

	tcpconn_put(con);
	return 1;
end:
	if(con)
		tcpconn_put(con);
	return 0;
}

int w_ws_handle_handshake(sip_msg_t *msg, char *p1, char *p2)
{
	return ws_handle_handshake(msg);
}

int ws_connect(sip_msg_t *msg, str *host, int port, str *path,
		str *sub_protocol, int cmode)
{
	char reqbuf[2048];
	unsigned int sub_proto = 0;
	unsigned int i;
	unsigned char raw_key[16];
	str key = STR_NULL;
	str req = STR_NULL;
	str *subpname;
	str lpath;
	str host_hdr;
	proxy_l_t *proxy = NULL;
	dest_info_t dst;
	struct tcp_connection *con = NULL;
	receive_info_t rcv;
	int req_len;
	int ret = -1;
	char hostbuf[512];

	memset(&dst, 0, sizeof(dest_info_t));
	(void)msg;

	if(cfg_get(websocket, ws_cfg, enabled) == 0) {
		LM_INFO("disabled: outbound websocket handshake refused\n");
		return -1;
	}

	if(host == NULL || host->s == NULL || host->len <= 0) {
		LM_ERR("missing host for outbound websocket connection\n");
		return -1;
	}

	if(ws_parse_sub_protocol(sub_protocol, &sub_proto) < 0) {
		LM_ERR("unsupported websocket sub-protocol: %.*s\n", sub_protocol->len,
				sub_protocol->s);
		return -1;
	}

	if((sub_proto & ws_sub_protocols) == 0) {
		LM_ERR("websocket sub-protocol %.*s is not enabled\n",
				sub_protocol->len, sub_protocol->s);
		return -1;
	}

	if(port <= 0)
		port = (cmode) ? 443 : 80;

	lpath = *path;
	if(lpath.s == NULL || lpath.len <= 0) {
		lpath.s = "/";
		lpath.len = 1;
	}

	proxy = mk_proxy(
			host, (unsigned short)port, (cmode) ? PROTO_TLS : PROTO_TCP);
	if(proxy == NULL) {
		LM_ERR("failed to resolve outbound websocket host\n");
		return -1;
	}
	if(proxy2su(&dst.to, proxy) < 0) {
		LM_ERR("failed to build destination socket address\n");
		goto done;
	}

	dst.proto = (cmode) ? PROTO_TLS : PROTO_TCP;
	SND_FLAGS_INIT(&dst.send_flags);
	dst.send_flags.f |= SND_F_FORCE_PROTO;

	con = ws_find_tcp_connection(&dst);
	if(con != NULL) {
		LM_ERR("refusing outbound websocket handshake on an existing %s "
			   "connection"
			   " (id: %d)\n",
				(cmode) ? "tls" : "tcp", con->id);
		tcpconn_put(con);
		con = NULL;
		goto done;
	}

	for(i = 0; i < sizeof(raw_key) / sizeof(unsigned int); i++) {
		unsigned int rval = cryptorand();
		memcpy(raw_key + (i * sizeof(unsigned int)), &rval,
				sizeof(unsigned int));
	}
	key.s = key_buf;
	key.len = base64_enc(raw_key, sizeof(raw_key), (unsigned char *)key.s,
			base64_enc_len(sizeof(raw_key)));

	if(host->len >= (int)sizeof(hostbuf) - 16) {
		LM_ERR("host header too long\n");
		goto done;
	}
	if(memchr(host->s, ':', host->len) && host->s[0] != '[') {
		host_hdr.len = snprintf(hostbuf, sizeof(hostbuf), "[%.*s]:%d",
				host->len, host->s, port);
	} else {
		host_hdr.len = snprintf(
				hostbuf, sizeof(hostbuf), "%.*s:%d", host->len, host->s, port);
	}
	host_hdr.s = hostbuf;

	subpname = ws_sub_protocol_name(sub_proto);
	if(subpname == NULL)
		goto done;

	req_len = snprintf(reqbuf, sizeof(reqbuf),
			"GET %.*s HTTP/1.1\r\n"
			"Host: %.*s\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: %.*s\r\n"
			"Sec-WebSocket-Version: %d\r\n"
			"Sec-WebSocket-Protocol: %.*s\r\n"
			"\r\n",
			lpath.len, lpath.s, host_hdr.len, host_hdr.s, key.len, key.s,
			WS_VERSION, subpname->len, subpname->s);
	if(req_len <= 0 || req_len >= (int)sizeof(reqbuf)) {
		LM_ERR("outbound websocket handshake request too large\n");
		goto done;
	}
	req.s = reqbuf;
	req.len = req_len;

	if(tcp_send(&dst, 0, req.s, req.len) < 0) {
		LM_ERR("sending outbound websocket handshake failed\n");
		goto done;
	}

	con = ws_find_tcp_connection(&dst);
	if(con == NULL) {
		LM_ERR("cannot retrieve outbound websocket tcp connection after "
			   "send\n");
		goto done;
	}

	con->req.flags |= F_TCP_REQ_WS_HANDSHAKE;
	rcv = con->rcv;
	rcv.proto_reserved1 = con->id;
	if(wsconn_add_outgoing(&rcv, sub_proto, &key, host, port, &lpath,
			   (cmode) ? PROTO_WSS : PROTO_WS)
			< 0) {
		LM_ERR("failed to track outbound websocket connection\n");
		con->req.flags &= ~F_TCP_REQ_WS_HANDSHAKE;
		goto done;
	}

	ret = 1;

done:
	if(ret < 0 && con != NULL) {
		con->send_flags.f |= SND_F_CON_CLOSE;
		con->state = S_CONN_BAD;
		con->timeout = get_ticks_raw();
	}
	if(con)
		tcpconn_put(con);
	if(proxy)
		free_proxy(proxy);
	return ret;
}

int ws_connect_url(sip_msg_t *msg, str *wsurl, str *sub_protocol)
{
	ws_address_t waddr;

	if(ws_parse_url(wsurl, &waddr) < 0) {
		LM_ERR("failed to parse websocket url: %.*s\n", wsurl->len, wsurl->s);
		return -1;
	}

	return ws_connect(msg, &waddr.host, waddr.port_no, &waddr.path,
			sub_protocol, (waddr.proto_no == PROTO_WSS) ? 1 : 0);
}

int ws_send(sip_msg_t *msg, str *host, int port, str *path, str *sub_protocol,
		int cmode)
{
	ws_connection_t *wsc = NULL;
	unsigned int sub_proto = 0;
	str lpath = STR_NULL;
	int ret;

	if(msg == NULL || msg->buf == NULL || msg->len <= 0) {
		LM_ERR("missing SIP message to send over websocket\n");
		return -1;
	}
	if(host == NULL || host->s == NULL || host->len <= 0) {
		LM_ERR("missing host for websocket send\n");
		return -1;
	}
	if(ws_parse_sub_protocol(sub_protocol, &sub_proto) < 0) {
		LM_ERR("unsupported websocket sub-protocol: %.*s\n", sub_protocol->len,
				sub_protocol->s);
		return -1;
	}
	if((sub_proto & ws_sub_protocols) == 0) {
		LM_ERR("websocket sub-protocol %.*s is not enabled\n",
				sub_protocol->len, sub_protocol->s);
		return -1;
	}

	if(path != NULL)
		lpath = *path;
	if(lpath.s == NULL || lpath.len <= 0) {
		lpath.s = "/";
		lpath.len = 1;
	}

	if(port <= 0)
		port = (cmode) ? 443 : 80;

	wsc = wsconn_get_outgoing(
			host, port, &lpath, (cmode) ? PROTO_WSS : PROTO_WS, sub_proto);
	if(wsc != NULL) {
		if(wsc->state == WS_S_OPEN) {
			ret = ws_send_buffer(wsc->id, msg->buf, msg->len);
			wsconn_put(wsc);
			return ret;
		}
		ret = wsconn_sendq_push(wsc, msg->buf, msg->len);
		if(ret == 0) {
			wsconn_put(wsc);
			return 1;
		}
		if(ret < 0) {
			wsconn_put(wsc);
			return -1;
		}
		wsconn_put(wsc);
	}

	if(ws_connect(msg, host, port, &lpath, sub_protocol, cmode) < 0) {
		LM_ERR("failed to initiate outbound websocket connection for send\n");
		return -1;
	}

	wsc = wsconn_get_outgoing(
			host, port, &lpath, (cmode) ? PROTO_WSS : PROTO_WS, sub_proto);
	if(wsc == NULL) {
		LM_ERR("cannot retrieve outbound websocket connection after connect\n");
		return -1;
	}

	if(wsc->state == WS_S_OPEN) {
		ret = ws_send_buffer(wsc->id, msg->buf, msg->len);
		wsconn_put(wsc);
		return ret;
	}

	ret = wsconn_sendq_push(wsc, msg->buf, msg->len);
	wsconn_put(wsc);
	if(ret == 0)
		return 1;
	if(ret == 1) {
		wsc = wsconn_get_outgoing(
				host, port, &lpath, (cmode) ? PROTO_WSS : PROTO_WS, sub_proto);
		if(wsc == NULL)
			return -1;
		ret = ws_send_buffer(wsc->id, msg->buf, msg->len);
		wsconn_put(wsc);
		return ret;
	}
	return -1;
}

int w_ws_connect(sip_msg_t *msg, char *phost, char *pport, char *ppath,
		char *psubproto, char *pcmode)
{
	str host = STR_NULL;
	str path = STR_NULL;
	str subproto = STR_NULL;
	int port = 0;
	int cmode = 0;

	if(fixup_get_svalue(msg, (gparam_p)phost, &host) < 0)
		return -1;
	if(fixup_get_ivalue(msg, (gparam_p)pport, &port) < 0)
		return -1;
	if(fixup_get_svalue(msg, (gparam_p)ppath, &path) < 0)
		return -1;
	if(fixup_get_svalue(msg, (gparam_p)psubproto, &subproto) < 0)
		return -1;
	if(fixup_get_ivalue(msg, (gparam_p)pcmode, &cmode) < 0)
		return -1;

	return ws_connect(msg, &host, port, &path, &subproto, cmode);
}

int w_ws_connect_url(sip_msg_t *msg, char *purl, char *psubproto)
{
	str wsurl = STR_NULL;
	str subproto = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_p)purl, &wsurl) < 0)
		return -1;
	if(fixup_get_svalue(msg, (gparam_p)psubproto, &subproto) < 0)
		return -1;

	return ws_connect_url(msg, &wsurl, &subproto);
}

int w_ws_send(sip_msg_t *msg, char *phost, char *pport, char *ppath,
		char *psubproto, char *pcmode)
{
	str host = STR_NULL;
	str path = STR_NULL;
	str subproto = STR_NULL;
	int port = 0;
	int cmode = 0;

	if(fixup_get_svalue(msg, (gparam_p)phost, &host) < 0)
		return -1;
	if(fixup_get_ivalue(msg, (gparam_p)pport, &port) < 0)
		return -1;
	if(fixup_get_svalue(msg, (gparam_p)ppath, &path) < 0)
		return -1;
	if(fixup_get_svalue(msg, (gparam_p)psubproto, &subproto) < 0)
		return -1;
	if(fixup_get_ivalue(msg, (gparam_p)pcmode, &cmode) < 0)
		return -1;

	return ws_send(msg, &host, port, &path, &subproto, cmode);
}

int ws_handle_handshake_response(sr_event_param_t *evp)
{
	tcp_event_info_t *tev;
	ws_connection_t *wsc = NULL;
	str connection = STR_NULL;
	str upgrade = STR_NULL;
	str accept = STR_NULL;
	str protocol = STR_NULL;
	str expected_accept = STR_NULL;
	str request_key = STR_NULL;
	str *subpname;
	char accept_buf[base64_enc_len(SHA1_DIGEST_LENGTH)];
	int status = 0;

	if(evp == NULL || evp->data == NULL)
		return -1;

	tev = (tcp_event_info_t *)evp->data;
	if(tev->con == NULL)
		return -1;

	wsc = wsconn_get(tev->con->id);
	if(wsc == NULL) {
		LM_ERR("received websocket handshake response for unknown connection "
			   "%d\n",
				tev->con->id);
		return -1;
	}

	if(wsc->state != WS_S_CONNECTING || wsc->role != WS_ROLE_CLIENT) {
		wsconn_put(wsc);
		return 0;
	}

	if(sscanf(tev->buf, "HTTP/%*d.%*d %d", &status) != 1 || status != 101) {
		LM_ERR("invalid websocket handshake status on connection %d\n",
				tev->con->id);
		goto error;
	}

	if(ws_response_get_header(
			   tev->buf, tev->len, &str_hdr_connection, &connection)
					< 0
			|| ws_token_contains(&connection, &str_upgrade) == 0) {
		LM_ERR("missing/invalid Connection header in websocket handshake\n");
		goto error;
	}

	if(ws_response_get_header(tev->buf, tev->len, &str_hdr_upgrade, &upgrade)
					< 0
			|| ws_token_contains(&upgrade, &str_websocket) == 0) {
		LM_ERR("missing/invalid Upgrade header in websocket handshake\n");
		goto error;
	}

	if(ws_response_get_header(
			   tev->buf, tev->len, &str_hdr_sec_websocket_accept, &accept)
			< 0) {
		LM_ERR("missing Sec-WebSocket-Accept header in websocket handshake\n");
		goto error;
	}

	expected_accept.s = accept_buf;
	request_key.s = wsc->handshake_key;
	request_key.len = wsc->handshake_key_len;
	expected_accept.len = ws_compute_accept(
			&request_key, expected_accept.s, sizeof(accept_buf));
	if(expected_accept.len <= 0 || ws_str_eq(&accept, &expected_accept) == 0) {
		LM_ERR("Sec-WebSocket-Accept mismatch in websocket handshake\n");
		goto error;
	}

	if(ws_response_get_header(
			   tev->buf, tev->len, &str_hdr_sec_websocket_protocol, &protocol)
			< 0) {
		LM_ERR("missing Sec-WebSocket-Protocol header in websocket "
			   "handshake\n");
		goto error;
	}

	subpname = ws_sub_protocol_name(wsc->sub_protocol);
	if(subpname == NULL || ws_str_eq(&protocol, subpname) == 0) {
		LM_ERR("unexpected websocket sub-protocol in handshake response\n");
		goto error;
	}

	if(tev->con->type == PROTO_TLS)
		tev->con->type = tev->con->rcv.proto = PROTO_WSS;
	else
		tev->con->type = tev->con->rcv.proto = PROTO_WS;

	tev->con->req.flags &= ~F_TCP_REQ_WS_HANDSHAKE;
	wsconn_mark_open(wsc);
	if(ws_flush_send_queue(wsc) < 0) {
		update_stat(ws_failed_handshakes, 1);
		ws_mark_failed_client(wsc);
		return -1;
	}
	update_stat(ws_successful_handshakes, 1);
	if(wsc->sub_protocol == SUB_PROTOCOL_SIP)
		update_stat(ws_sip_successful_handshakes, 1);
	else if(wsc->sub_protocol == SUB_PROTOCOL_MSRP)
		update_stat(ws_msrp_successful_handshakes, 1);

	wsconn_put(wsc);
	return 0;

error:
	tev->con->req.flags &= ~F_TCP_REQ_WS_HANDSHAKE;
	update_stat(ws_failed_handshakes, 1);
	ws_mark_failed_client(wsc);
	return -1;
}

void ws_rpc_disable(rpc_t *rpc, void *ctx)
{
	cfg_get(websocket, ws_cfg, enabled) = 0;
	LM_WARN("disabling websockets - new connections will be dropped\n");
	return;
}

void ws_rpc_enable(rpc_t *rpc, void *ctx)
{
	cfg_get(websocket, ws_cfg, enabled) = 1;
	LM_WARN("enabling websockets\n");
	return;
}

void ws_rpc_connect(rpc_t *rpc, void *ctx)
{
	str host = STR_NULL;
	str path = STR_NULL;
	str subproto = STR_NULL;
	int port = 0;
	int cmode = 0;

	if(rpc->scan(ctx, "SdSSd", &host, &port, &path, &subproto, &cmode) < 5) {
		rpc->fault(ctx, 400,
				"Invalid parameters. Expected host, port, path, subprotocol, "
				"tls");
		return;
	}

	if(ws_connect(NULL, &host, port, &path, &subproto, cmode) < 0) {
		rpc->fault(ctx, 500, "Failed to initiate outbound websocket handshake");
		return;
	}

	rpc->rpl_printf(ctx, "Outbound websocket handshake initiated");
}
