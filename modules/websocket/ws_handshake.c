/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <openssl/sha.h>

#include "../../basex.h"
#include "../../data_lump_rpl.h"
#include "../../dprint.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../parser/msg_parser.h"
#include "../sl/sl.h"
#include "ws_handshake.h"
#include "ws_mod.h"

#define WS_VERSION		(13)

#define SEC_WEBSOCKET_KEY	(1<<0)
#define SEC_WEBSOCKET_PROTOCOL	(1<<1)
#define SEC_WEBSOCKET_VERSION	(1<<2)

#define REQUIRED_HEADERS	(SEC_WEBSOCKET_KEY | SEC_WEBSOCKET_PROTOCOL\
					| SEC_WEBSOCKET_VERSION)

static str str_sip = str_init("sip");
static str str_ws_guid = str_init("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

static str str_switching_protocols = str_init("Switching Protocols");
static str str_bad_request = str_init("Bad Request");
static str str_upgrade_required = str_init("Upgrade Required");
static str str_internal_server_error = str_init("Internal Server Error");

#define HDR_BUF_LEN		(256)
static char headers_buf[HDR_BUF_LEN];

#define KEY_BUF_LEN		(28)
static char key_buf[KEY_BUF_LEN];

static int ws_send_reply(sip_msg_t *msg, int code, str *reason, str *hdrs)
{
	if (hdrs && hdrs->len > 0)
	{
		if (add_lump_rpl(msg, hdrs->s, hdrs->len, LUMP_RPL_HDR) == 0)
		{
			LM_ERR("inserting extra-headers lump\n");
			return -1;
		}
	}

	if (ws_slb.freply(msg, code, reason) < 0)
	{
		LM_ERR("sending reply\n");
		return -1;
	}

	return 0;
}

int ws_handle_handshake(struct sip_msg *msg)
{
	str key = {0, 0}, headers = {0, 0}, reply_key = {0, 0};
	unsigned char sha1[20];
	unsigned int hdr_flags = 0;
	int version;
	struct hdr_field *hdr = msg->headers;

	while (hdr != NULL)
	{
		/* Decode and validate Sec-WebSocket-Key */
		if (cmp_hdrname_strzn(&hdr->name,
				"Sec-WebSocket-Key", 17) == 0) 
		{
			if (hdr_flags & SEC_WEBSOCKET_KEY)
			{
				LM_WARN("%.*s found multiple times\n",
					hdr->name.len, hdr->name.s);
				ws_send_reply(msg, 400, &str_bad_request, NULL);
				return 0;
			}

			key = hdr->body;
			hdr_flags |= SEC_WEBSOCKET_KEY;
		}
		/* Decode and validate Sec-WebSocket-Protocol */
		else if (cmp_hdrname_strzn(&hdr->name,
				"Sec-WebSocket-Protocol", 22) == 0)
		{
			if (str_search(&hdr->body, &str_sip) != NULL)
				hdr_flags |= SEC_WEBSOCKET_PROTOCOL;
		}
		/* Decode and validate Sec-WebSocket-Version */
		else if (cmp_hdrname_strzn(&hdr->name,
				"Sec-WebSocket-Version", 21) == 0)
		{
			if (hdr_flags & SEC_WEBSOCKET_VERSION)
			{
				LM_WARN("%.*s found multiple times\n",
					hdr->name.len, hdr->name.s);
				ws_send_reply(msg, 400, &str_bad_request, NULL);
				return 0;
			}

			str2sint(&hdr->body, &version);

			if (version != WS_VERSION)
			{
				LM_WARN("Unsupported protocol version %.*s\n",
					hdr->body.len, hdr->body.s);
				headers.s = headers_buf;
				headers.len = snprintf(headers.s, HDR_BUF_LEN,
					"Sec-WebSocket-Version: %u\r\n",
					WS_VERSION);
				ws_send_reply(msg, 426, &str_upgrade_required,
						&headers);
				return 0;
			}

			hdr_flags |= SEC_WEBSOCKET_VERSION;
		}

		hdr = hdr->next;
	}

	/* Final check that all required headers/values were found */
	if (hdr_flags != REQUIRED_HEADERS)
	{
		LM_WARN("all required headers not present\n");
		ws_send_reply(msg, 400, &str_bad_request, NULL);
		return 0;
	}

	/* Construct reply_key */
	reply_key.s = (char *) pkg_malloc(
				(key.len + str_ws_guid.len) * sizeof(char)); 
	if (reply_key.s == NULL)
	{
		LM_ERR("allocating pkg memory\n");
		ws_send_reply(msg, 500, &str_internal_server_error, NULL);
		return 0;
	}
	memcpy(reply_key.s, key.s, key.len);
	memcpy(reply_key.s + key.len, str_ws_guid.s, str_ws_guid.len);
	reply_key.len = key.len + str_ws_guid.len;
	SHA1((const unsigned char *) reply_key.s, reply_key.len, sha1);
	pkg_free(reply_key.s);
	reply_key.s = key_buf;
	reply_key.len = base64_enc(sha1, 20,
				(unsigned char *) reply_key.s, KEY_BUF_LEN);

	/* Build headers for reply */
	headers.s = headers_buf;
	headers.len = snprintf(headers.s, HDR_BUF_LEN,
			"Sec-WebSocket-Key: %.*s\r\n"
			"Sec-WebSocket-Protocol: %.*s\r\n"
			"Sec-WebSocket-Version: %u\r\n",
			reply_key.len, reply_key.s,
			str_sip.len, str_sip.s,
			WS_VERSION);

	/* TODO: make sure Kamailio core sends future requests on this
		 connection directly to this module */

	/* Send reply */
	ws_send_reply(msg, 101, &str_switching_protocols, &headers);

	return 0;
}
