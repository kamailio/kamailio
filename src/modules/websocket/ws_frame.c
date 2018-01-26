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

#include <limits.h>

#ifdef EMBEDDED_UTF8_DECODE
#include "utf8_decode.h"
#else
#include <unistr.h>
#endif

#include "../../core/events.h"
#include "../../core/receive.h"
#include "../../core/stats.h"
#include "../../core/str.h"
#include "../../core/tcp_conn.h"
#include "../../core/tcp_read.h"
#include "../../core/tcp_server.h"
#include "../../core/counters.h"
#include "../../core/mem/mem.h"
#include "ws_conn.h"
#include "ws_frame.h"
#include "websocket.h"
#include "ws_handshake.h"
#include "config.h"

/*    0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+ */

typedef struct
{
	unsigned int fin;
	unsigned int rsv1;
	unsigned int rsv2;
	unsigned int rsv3;
	unsigned int opcode;
	unsigned int mask;
	unsigned int payload_len;
	unsigned char masking_key[4];
	char *payload_data;
	ws_connection_t *wsc;
} ws_frame_t;

typedef enum { CONN_CLOSE_DO = 0, CONN_CLOSE_DONT } conn_close_t;

#define BYTE0_MASK_FIN (0x80)
#define BYTE0_MASK_RSV1 (0x40)
#define BYTE0_MASK_RSV2 (0x20)
#define BYTE0_MASK_RSV3 (0x10)
#define BYTE0_MASK_OPCODE (0x0F)
#define BYTE1_MASK_MASK (0x80)
#define BYTE1_MASK_PAYLOAD_LEN (0x7F)

#define OPCODE_CONTINUATION (0x0)
#define OPCODE_TEXT_FRAME (0x1)
#define OPCODE_BINARY_FRAME (0x2)
/* 0x3 - 0x7 are reserved for further non-control frames */
#define OPCODE_CLOSE (0x8)
#define OPCODE_PING (0x9)
#define OPCODE_PONG (0xa)
/* 0xb - 0xf are reserved for further control frames */

int ws_keepalive_mechanism = DEFAULT_KEEPALIVE_MECHANISM;
str ws_ping_application_data = STR_NULL;

stat_var *ws_failed_connections;
stat_var *ws_local_closed_connections;
stat_var *ws_received_frames;
stat_var *ws_remote_closed_connections;
stat_var *ws_transmitted_frames;
stat_var *ws_sip_failed_connections;
stat_var *ws_sip_local_closed_connections;
stat_var *ws_sip_received_frames;
stat_var *ws_sip_remote_closed_connections;
stat_var *ws_sip_transmitted_frames;
stat_var *ws_msrp_failed_connections;
stat_var *ws_msrp_local_closed_connections;
stat_var *ws_msrp_received_frames;
stat_var *ws_msrp_remote_closed_connections;
stat_var *ws_msrp_transmitted_frames;

/* WebSocket status text */
static str str_status_normal_closure = str_init("Normal closure");
static str str_status_protocol_error = str_init("Protocol error");
static str str_status_unsupported_opcode = str_init("Unsupported opcode");
static str str_status_message_too_big = str_init("Message too big");

/* RPC command status text */
static str str_status_error_closing = str_init("Error closing connection");
static str str_status_error_sending = str_init("Error sending frame");

static int ws_send_crlf(ws_connection_t *wsc, int opcode);

static int encode_and_send_ws_frame(ws_frame_t *frame, conn_close_t conn_close)
{
	int pos = 0, extended_length;
	unsigned int frame_length;
	char *send_buf;
	struct tcp_connection *con;
	struct dest_info dst;
	union sockaddr_union *from = NULL;
	union sockaddr_union local_addr;
	int sub_proto;

	LM_DBG("encoding WebSocket frame\n");

	if(frame->wsc->state != WS_S_OPEN) {
		LM_WARN("sending on closing connection\n");
		return -1;
	}

	wsconn_update(frame->wsc);

	/* Validate the first byte */
	if(!frame->fin) {
		LM_ERR("WebSocket fragmentation not supported in the sip "
			   "sub-protocol\n");
		return -1;
	}

	if(frame->rsv1 || frame->rsv2 || frame->rsv3) {
		LM_ERR("WebSocket reserved fields with non-zero values\n");
		return -1;
	}

	sub_proto = frame->wsc->sub_protocol;

	switch(frame->opcode) {
		case OPCODE_TEXT_FRAME:
		case OPCODE_BINARY_FRAME:
			LM_DBG("supported non-control frame: 0x%x\n",
					(unsigned char)frame->opcode);
			break;
		case OPCODE_CLOSE:
		case OPCODE_PING:
		case OPCODE_PONG:
			LM_DBG("supported control frame: 0x%x\n",
					(unsigned char)frame->opcode);
			break;
		default:
			LM_ERR("unsupported opcode: 0x%x\n", (unsigned char)frame->opcode);
			return -1;
	}

	/* validate the second byte */
	if(frame->mask) {
		LM_ERR("this is a server - all messages sent will be "
			   "unmasked\n");
		return -1;
	}

	if(frame->payload_len < 126)
		extended_length = 0;
	else if(frame->payload_len <= USHRT_MAX)
		extended_length = 2;
	else if(frame->payload_len < UINT_MAX)
		extended_length = 4;
	else {
		LM_ERR(NAME " only supports WebSocket frames with payload "
					"< %u\n",
				UINT_MAX);
		return -1;
	}

	/* Allocate send buffer and build frame */
	frame_length = frame->payload_len + extended_length + 2;
	if((send_buf = pkg_malloc(sizeof(char) * frame_length)) == NULL) {
		LM_ERR("allocating send buffer from pkg memory\n");
		return -1;
	}
	memset(send_buf, 0, sizeof(char) * frame_length);
	send_buf[pos++] = 0x80 | (frame->opcode & 0xff);
	if(extended_length == 0)
		send_buf[pos++] = (frame->payload_len & 0xff);
	else if(extended_length == 2) {
		send_buf[pos++] = 126;
		send_buf[pos++] = (frame->payload_len & 0xff00) >> 8;
		send_buf[pos++] = (frame->payload_len & 0x00ff) >> 0;
	} else {
		send_buf[pos++] = 127;
		send_buf[pos++] = (frame->payload_len & 0xff000000) >> 24;
		send_buf[pos++] = (frame->payload_len & 0x00ff0000) >> 16;
		send_buf[pos++] = (frame->payload_len & 0x0000ff00) >> 8;
		send_buf[pos++] = (frame->payload_len & 0x000000ff) >> 0;
	}
	memcpy(&send_buf[pos], frame->payload_data, frame->payload_len);

	if((con = tcpconn_get(frame->wsc->id, 0, 0, 0, 0)) == NULL) {
		LM_WARN("TCP/TLS connection get failed\n");
		pkg_free(send_buf);
		if(wsconn_rm(frame->wsc, WSCONN_EVENTROUTE_YES) < 0)
			LM_ERR("removing WebSocket connection\n");
		return -1;
	}
	init_dst_from_rcv(&dst, &con->rcv);
	if(conn_close == CONN_CLOSE_DO) {
		dst.send_flags.f |= SND_F_CON_CLOSE;
		if(wsconn_rm(frame->wsc, WSCONN_EVENTROUTE_YES) < 0) {
			LM_ERR("removing WebSocket connection\n");
			tcpconn_put(con);
			pkg_free(send_buf);
			return -1;
		}
	}

	if(dst.proto == PROTO_WS) {
		if(unlikely(tcp_disable)) {
			STATS_TX_DROPS;
			LM_WARN("TCP disabled\n");
			pkg_free(send_buf);
			tcpconn_put(con);
			return -1;
		}
	}
#ifdef USE_TLS
	else if(dst.proto == PROTO_WSS) {
		if(unlikely(tls_disable)) {
			STATS_TX_DROPS;
			LM_WARN("TLS disabled\n");
			pkg_free(send_buf);
			tcpconn_put(con);
			return -1;
		}
	}
#endif /* USE_TLS */

	if(unlikely((dst.send_flags.f & SND_F_FORCE_SOCKET) && dst.send_sock)) {
		local_addr = dst.send_sock->su;
		su_setport(&local_addr, 0);
		from = &local_addr;
	}

	/* Regardless of what has been set before _always_ use existing
	   connections for WebSockets.  This is required because a WebSocket
	   server (which Kamailio is) CANNOT create connections. */
	dst.send_flags.f |= SND_F_FORCE_CON_REUSE;

	if(tcp_send(&dst, from, send_buf, frame_length) < 0) {
		STATS_TX_DROPS;
		LM_ERR("sending WebSocket frame\n");
		pkg_free(send_buf);
		update_stat(ws_failed_connections, 1);
		if(sub_proto == SUB_PROTOCOL_SIP)
			update_stat(ws_sip_failed_connections, 1);
		else if(sub_proto == SUB_PROTOCOL_MSRP)
			update_stat(ws_msrp_failed_connections, 1);
		if(wsconn_rm(frame->wsc, WSCONN_EVENTROUTE_YES) < 0)
			LM_ERR("removing WebSocket connection\n");
		tcpconn_put(con);
		return -1;
	}

	update_stat(ws_transmitted_frames, 1);
	switch(frame->opcode) {
		case OPCODE_TEXT_FRAME:
		case OPCODE_BINARY_FRAME:
			if(frame->wsc->sub_protocol == SUB_PROTOCOL_SIP)
				update_stat(ws_sip_transmitted_frames, 1);
			else if(frame->wsc->sub_protocol == SUB_PROTOCOL_MSRP)
				update_stat(ws_msrp_transmitted_frames, 1);
	}

	pkg_free(send_buf);
	tcpconn_put(con);
	return 0;
}

static int close_connection(ws_connection_t **p_wsc, ws_close_type_t type,
		short int status, str reason)
{
	char *data;
	ws_frame_t frame;
	ws_connection_t *wsc = NULL;
	int sub_proto = -1;
	if(!p_wsc || !(*p_wsc)) {
		LM_ERR("Invalid parameters\n");
		return -1;
	}
	wsc = *p_wsc;

	if(wsc->state == WS_S_OPEN) {
		data = pkg_malloc(sizeof(char) * (reason.len + 2));
		if(data == NULL) {
			LM_ERR("allocating pkg memory\n");
			return -1;
		}

		data[0] = (status & 0xff00) >> 8;
		data[1] = (status & 0x00ff) >> 0;
		memcpy(&data[2], reason.s, reason.len);

		memset(&frame, 0, sizeof(frame));
		frame.fin = 1;
		frame.opcode = OPCODE_CLOSE;
		frame.payload_len = reason.len + 2;
		frame.payload_data = data;
		frame.wsc = wsc;
		sub_proto = wsc->sub_protocol;

		if(encode_and_send_ws_frame(&frame,
				   type == REMOTE_CLOSE ? CONN_CLOSE_DO : CONN_CLOSE_DONT)
				< 0) {
			LM_ERR("sending WebSocket close\n");
			pkg_free(data);
			return -1;
		}

		pkg_free(data);

		if(type == LOCAL_CLOSE) {
			frame.wsc->state = WS_S_CLOSING;
			update_stat(ws_local_closed_connections, 1);
			if(frame.wsc->sub_protocol == SUB_PROTOCOL_SIP)
				update_stat(ws_sip_local_closed_connections, 1);
			else if(frame.wsc->sub_protocol == SUB_PROTOCOL_MSRP)
				update_stat(ws_msrp_local_closed_connections, 1);
		} else {
			update_stat(ws_remote_closed_connections, 1);
			if(sub_proto == SUB_PROTOCOL_SIP)
				update_stat(ws_sip_remote_closed_connections, 1);
			else if(sub_proto == SUB_PROTOCOL_MSRP)
				update_stat(ws_msrp_remote_closed_connections, 1);
		}
	} else /* if (frame->wsc->state == WS_S_CLOSING) */
	{
		wsconn_close_now(wsc);
	}

	return 0;
}

static int decode_and_validate_ws_frame(ws_frame_t *frame,
		tcp_event_info_t *tcpinfo, short *err_code, str *err_text)
{
	unsigned int i, len = tcpinfo->len;
	unsigned int mask_start, j;
	char *buf = tcpinfo->buf;

	LM_DBG("decoding WebSocket frame (len: %u)\n", len);

	wsconn_update(frame->wsc);

	/* Decode and validate first 9 bits */
	if(len < 2) {
		LM_WARN("message is too short (%u)\n", len);
		*err_code = 1002;
		*err_text = str_status_protocol_error;
		return -1;
	}
	frame->fin = (buf[0] & 0xff) & BYTE0_MASK_FIN;
	frame->rsv1 = (buf[0] & 0xff) & BYTE0_MASK_RSV1;
	frame->rsv2 = (buf[0] & 0xff) & BYTE0_MASK_RSV2;
	frame->rsv3 = (buf[0] & 0xff) & BYTE0_MASK_RSV3;
	frame->opcode = (buf[0] & 0xff) & BYTE0_MASK_OPCODE;
	frame->mask = (buf[1] & 0xff) & BYTE1_MASK_MASK;

	if(frame->rsv1 || frame->rsv2 || frame->rsv3) {
		LM_WARN("WebSocket reserved fields with non-zero values\n");
		*err_code = 1002;
		*err_text = str_status_protocol_error;
		return -1;
	}

	switch(frame->opcode) {
		case OPCODE_CONTINUATION:
			LM_DBG("supported continuation frame: 0x%x\n",
					(unsigned char)frame->opcode);
			break;
		case OPCODE_TEXT_FRAME:
		case OPCODE_BINARY_FRAME:
			LM_DBG("supported non-control frame: 0x%x\n",
					(unsigned char)frame->opcode);
			break;

		case OPCODE_CLOSE:
		case OPCODE_PING:
		case OPCODE_PONG:
			LM_DBG("supported control frame: 0x%x\n",
					(unsigned char)frame->opcode);
			break;

		default:
			LM_WARN("unsupported opcode: 0x%x\n", (unsigned char)frame->opcode);
			*err_code = 1008;
			*err_text = str_status_unsupported_opcode;
			return -1;
	}

	if(!frame->mask) {
		LM_WARN("this is a server - all received messages must be "
				"masked\n");
		*err_code = 1002;
		*err_text = str_status_protocol_error;
		return -1;
	}

	/* Decode and validate length */
	frame->payload_len = (buf[1] & 0xff) & BYTE1_MASK_PAYLOAD_LEN;
	if(frame->payload_len == 126) {
		if(len < 4) {
			LM_WARN("message is too short (%u)\n", len);
			*err_code = 1002;
			*err_text = str_status_protocol_error;
			return -1;
		}
		mask_start = 4;

		frame->payload_len = ((buf[2] & 0xff) << 8) | ((buf[3] & 0xff) << 0);
	} else if(frame->payload_len == 127) {
		if(len < 10) {
			LM_WARN("message is too short (%u)\n", len);
			*err_code = 1002;
			*err_text = str_status_protocol_error;
			return -1;
		}
		mask_start = 10;

		if((buf[2] & 0xff) != 0 || (buf[3] & 0xff) != 0 || (buf[4] & 0xff) != 0
				|| (buf[5] & 0xff) != 0) {
			LM_WARN("message is too long (%u)\n", len);
			*err_code = 1009;
			*err_text = str_status_message_too_big;
			return -1;
		}

		/* Only decoding the last four bytes of the length...
		   This limits the size of WebSocket messages that can be
		   handled to 2^32 = which should be plenty for SIP! */
		frame->payload_len = ((buf[6] & 0xff) << 24) | ((buf[7] & 0xff) << 16)
							 | ((buf[8] & 0xff) << 8) | ((buf[9] & 0xff) << 0);
	} else
		mask_start = 2;

	if((unsigned long long)len
			!= (unsigned long long)frame->payload_len + mask_start + 4) {
		LM_WARN("message not complete frame size %u but received %u\n",
				frame->payload_len + mask_start + 4, len);
		*err_code = 1002;
		*err_text = str_status_protocol_error;
		return -1;
	}
	if(frame->payload_len >= BUF_SIZE) {
		LM_WARN("message is too long for our buffer size (%d / %d)\n", BUF_SIZE,
				frame->payload_len);
		*err_code = 1009;
		*err_text = str_status_message_too_big;
		return -1;
	}
	/* Decode mask */
	frame->masking_key[0] = (buf[mask_start + 0] & 0xff);
	frame->masking_key[1] = (buf[mask_start + 1] & 0xff);
	frame->masking_key[2] = (buf[mask_start + 2] & 0xff);
	frame->masking_key[3] = (buf[mask_start + 3] & 0xff);

	frame->payload_data = &buf[mask_start + 4];

	/* Decode and unmask payload */
	for(i = 0; i < frame->payload_len; i++) {
		j = i % 4;
		frame->payload_data[i] = frame->payload_data[i] ^ frame->masking_key[j];
	}

	LM_DBG("Rx (decoded) (len %u): %.*s\n", frame->payload_len,
			(int)frame->payload_len, frame->payload_data);

	return frame->opcode;
}

static int handle_close(ws_frame_t *frame)
{
	unsigned short code = 0;
	str reason = {0, 0};

	if(frame->payload_len >= 2)
		code = ((frame->payload_data[0] & 0xff) << 8)
			   | ((frame->payload_data[1] & 0xff) << 0);

	if(frame->payload_len > 2) {
		reason.s = &frame->payload_data[2];
		reason.len = frame->payload_len - 2;
	}

	LM_DBG("Rx Close: %hu %.*s\n", code, reason.len, reason.s);

	if(close_connection(&frame->wsc,
			   frame->wsc->state == WS_S_OPEN ? REMOTE_CLOSE : LOCAL_CLOSE,
			   1000, str_status_normal_closure)
			< 0) {
		LM_ERR("closing connection\n");
		return -1;
	}

	return 0;
}

static int handle_ping(ws_frame_t *frame)
{
	LM_DBG("Rx Ping: %.*s\n", frame->payload_len, frame->payload_data);

	frame->opcode = OPCODE_PONG;
	frame->mask = 0;

	if(encode_and_send_ws_frame(frame, CONN_CLOSE_DONT) < 0) {
		LM_ERR("sending Pong\n");
		return -1;
	}

	return 0;
}

static int handle_pong(ws_frame_t *frame)
{
	LM_DBG("Rx Pong: %.*s\n", frame->payload_len, frame->payload_data);

	if(strncmp(frame->payload_data, ws_ping_application_data.s,
			   ws_ping_application_data.len)
			== 0)
		frame->wsc->awaiting_pong = 0;

	return 0;
}

int ws_frame_receive(sr_event_param_t *evp)
{
	ws_frame_t frame;
	tcp_event_info_t *tcpinfo = (tcp_event_info_t *)evp->data;
	sr_event_param_t levp = {0};

	int opcode = -1;
	int ret = 0;
	short err_code = 0;
	str err_text = {NULL, 0};

	update_stat(ws_received_frames, 1);

	if(tcpinfo == NULL || tcpinfo->buf == NULL || tcpinfo->len <= 0) {
		LM_WARN("received bad frame\n");
		return -1;
	}

	/* wsc refcnt++ */
	frame.wsc = wsconn_get(tcpinfo->con->id);
	if(frame.wsc == NULL) {
		LM_ERR("WebSocket connection not found\n");
		return -1;
	}

	opcode =
			decode_and_validate_ws_frame(&frame, tcpinfo, &err_code, &err_text);
	if(opcode < 0) {
		if(close_connection(&frame.wsc, LOCAL_CLOSE, err_code, err_text) < 0)
			LM_ERR("closing connection\n");

		wsconn_put(frame.wsc);

		return -1;
	}

	switch(opcode) {
		case OPCODE_CONTINUATION:
			if(likely(frame.wsc->sub_protocol == SUB_PROTOCOL_SIP)) {
				if(frame.wsc->frag_buf.len + frame.payload_len >= BUF_SIZE) {
					LM_ERR("Buffer overflow assembling websocket fragments %d "
						   "+ %d = %d\n",
							frame.wsc->frag_buf.len, frame.payload_len,
							frame.wsc->frag_buf.len + frame.payload_len);
					wsconn_put(frame.wsc);
					return -1;
				}
				memcpy(frame.wsc->frag_buf.s + frame.wsc->frag_buf.len,
						frame.payload_data, frame.payload_len);
				frame.wsc->frag_buf.len += frame.payload_len;
				frame.wsc->frag_buf.s[frame.wsc->frag_buf.len] = '\0';

				if(frame.fin) {
					ret = receive_msg(frame.wsc->frag_buf.s,
							frame.wsc->frag_buf.len, tcpinfo->rcv);
					wsconn_put(frame.wsc);
					return ret;
				}
				wsconn_put(frame.wsc);
				return 0;
			} else {
				LM_ERR("Unsupported fragmented sub-protocol");
				wsconn_put(frame.wsc);
				return -1;
			}
		case OPCODE_TEXT_FRAME:
		case OPCODE_BINARY_FRAME:
			if(likely(frame.wsc->sub_protocol == SUB_PROTOCOL_SIP)) {
				LM_DBG("Rx SIP (or text) message:\n%.*s\n", frame.payload_len,
						frame.payload_data);
				update_stat(ws_sip_received_frames, 1);

				if((frame.payload_len == CRLF_LEN
						   && strncmp(frame.payload_data, CRLF, CRLF_LEN) == 0)
						|| (frame.payload_len == CRLFCRLF_LEN
								   && strncmp(frame.payload_data, CRLFCRLF,
											  CRLFCRLF_LEN)
											  == 0)) {
					ws_send_crlf(frame.wsc, opcode);
					wsconn_put(frame.wsc);
					return 0;
				}
				if(frame.fin) {

					wsconn_put(frame.wsc);

					return receive_msg(frame.payload_data, frame.payload_len,
							tcpinfo->rcv);
				} else {
					memcpy(frame.wsc->frag_buf.s, frame.payload_data,
							frame.payload_len);
					frame.wsc->frag_buf.len = frame.payload_len;
					frame.wsc->frag_buf.s[frame.wsc->frag_buf.len] = '\0';
					wsconn_put(frame.wsc);
					return 0;
				}
			} else if(frame.wsc->sub_protocol == SUB_PROTOCOL_MSRP) {
				LM_DBG("Rx MSRP frame:\n%.*s\n", frame.payload_len,
						frame.payload_data);
				update_stat(ws_msrp_received_frames, 1);
				if(likely(sr_event_enabled(SREV_TCP_MSRP_FRAME))) {
					tcp_event_info_t tev;
					memset(&tev, 0, sizeof(tcp_event_info_t));
					tev.type = SREV_TCP_MSRP_FRAME;
					tev.buf = frame.payload_data;
					tev.len = frame.payload_len;
					tev.rcv = tcpinfo->rcv;
					tev.con = tcpinfo->con;

					wsconn_put(frame.wsc);

					levp.data = (void *)&tev;
					return sr_event_exec(SREV_TCP_MSRP_FRAME, &levp);
				} else {
					LM_ERR("no callback registered for MSRP\n");

					wsconn_put(frame.wsc);

					return -1;
				}
			} else {
				LM_ERR("Unrecognized WebSocket subprotocol: %u\n", frame.wsc->sub_protocol);
				return -1;
			}

		case OPCODE_CLOSE:
			ret = handle_close(&frame);
			if(frame.wsc)
				wsconn_put(frame.wsc);
			return ret;

		case OPCODE_PING:
			ret = handle_ping(&frame);
			if(frame.wsc)
				wsconn_put(frame.wsc);
			return ret;

		case OPCODE_PONG:
			ret = handle_pong(&frame);
			if(frame.wsc)
				wsconn_put(frame.wsc);
			return ret;

		default:
			LM_WARN("received bad frame\n");
			wsconn_put(frame.wsc);
			return -1;
	}
}

int ws_frame_transmit(sr_event_param_t *evp)
{
	ws_event_info_t *wsev = (ws_event_info_t *)evp->data;
	ws_frame_t frame;

	memset(&frame, 0, sizeof(frame));
	frame.fin = 1;
/* Can't be sure whether this message is UTF-8 or not so check to see
	   if it "might" be UTF-8 and send as binary if it definitely isn't */
#ifdef EMBEDDED_UTF8_DECODE
	frame.opcode = IsUTF8((uint8_t *)wsev->buf, wsev->len)
						   ? OPCODE_TEXT_FRAME
						   : OPCODE_BINARY_FRAME;
#else
	frame.opcode = (u8_check((uint8_t *)wsev->buf, wsev->len) == NULL)
						   ? OPCODE_TEXT_FRAME
						   : OPCODE_BINARY_FRAME;
#endif
	frame.payload_len = wsev->len;
	frame.payload_data = wsev->buf;
	frame.wsc = wsconn_get(wsev->id);
	if(frame.wsc == NULL) {
		LM_ERR("WebSocket outbound connection not found\n");
		return -1;
	}

	LM_DBG("Tx message:\n%.*s\n", frame.payload_len, frame.payload_data);

	if(encode_and_send_ws_frame(&frame, CONN_CLOSE_DONT) < 0) {
		LM_ERR("sending message\n");

		wsconn_put(frame.wsc);

		return -1;
	}

	wsconn_put(frame.wsc);

	return 0;
}

static int ping_pong(ws_connection_t *wsc, int opcode)
{
	ws_frame_t frame;

	memset(&frame, 0, sizeof(frame));
	frame.fin = 1;
	frame.opcode = opcode;
	frame.payload_len = ws_ping_application_data.len;
	frame.payload_data = ws_ping_application_data.s;
	frame.wsc = wsc;

	if(encode_and_send_ws_frame(&frame, CONN_CLOSE_DONT) < 0) {
		LM_ERR("sending keepalive\n");
		return -1;
	}

	if(opcode == OPCODE_PING)
		wsc->awaiting_pong = 1;

	return 0;
}

static int ws_send_crlf(ws_connection_t *wsc, int opcode)
{
	ws_frame_t frame;

	memset(&frame, 0, sizeof(frame));
	frame.fin = 1;
	frame.opcode = opcode;
	frame.payload_len = CRLF_LEN;
	frame.payload_data = CRLF;
	frame.wsc = wsc;

	if(encode_and_send_ws_frame(&frame, CONN_CLOSE_DONT) < 0) {
		LM_ERR("failed sending CRLF\n");
		return -1;
	}

	return 0;
}

void ws_keepalive(unsigned int ticks, void *param)
{
	int check_time =
			(int)time(NULL) - cfg_get(websocket, ws_cfg, keepalive_timeout);

	ws_connection_t **list = NULL, **list_head = NULL;
	ws_connection_t *wsc = NULL;

	/* get an array of pointer to all ws connection */
	list_head = wsconn_get_list();
	if(!list_head)
		return;

	list = list_head;
	wsc = *list_head;
	while(wsc && wsc->last_used < check_time) {
		if(wsc->state == WS_S_CLOSING || wsc->awaiting_pong) {
			LM_WARN("forcibly closing connection\n");
			wsconn_close_now(wsc);
		} else {
			int opcode = (ws_keepalive_mechanism == KEEPALIVE_MECHANISM_PING)
								 ? OPCODE_PING
								 : OPCODE_PONG;
			ping_pong(wsc, opcode);
		}

		wsc = *(++list);
	}

	wsconn_put_list(list_head);
}

int ws_close(sip_msg_t *msg)
{
	ws_connection_t *wsc;
	int ret;

	if((wsc = wsconn_get(msg->rcv.proto_reserved1)) == NULL) {
		LM_ERR("failed to retrieve WebSocket connection\n");
		return -1;
	}

	ret = (close_connection(&wsc, LOCAL_CLOSE, 1000, str_status_normal_closure)
				  == 0)
				  ? 1
				  : 0;

	wsconn_put(wsc);

	return ret;
}

int w_ws_close0(sip_msg_t *msg, char *p1, char *p2)
{
	return ws_close(msg);
}

int ws_close2(sip_msg_t *msg, int status, str *reason)
{
	ws_connection_t *wsc;
	int ret;

	if((wsc = wsconn_get(msg->rcv.proto_reserved1)) == NULL) {
		LM_ERR("failed to retrieve WebSocket connection\n");
		return -1;
	}

	ret = (close_connection(&wsc, LOCAL_CLOSE, status, *reason) == 0) ? 1 : 0;

	wsconn_put(wsc);

	return ret;
}

int w_ws_close2(sip_msg_t *msg, char *_status, char *_reason)
{
	int status;
	str reason;

	if(get_int_fparam(&status, msg, (fparam_t *)_status) < 0) {
		LM_ERR("failed to get status code\n");
		return -1;
	}

	if(get_str_fparam(&reason, msg, (fparam_t *)_reason) < 0) {
		LM_ERR("failed to get reason string\n");
		return -1;
	}
	return ws_close2(msg, status, &reason);
}

int ws_close3(sip_msg_t *msg, int status, str *reason, int con)
{
	ws_connection_t *wsc;
	int ret;

	if((wsc = wsconn_get(con)) == NULL) {
		LM_ERR("failed to retrieve WebSocket connection\n");
		return -1;
	}

	ret = (close_connection(&wsc, LOCAL_CLOSE, status, *reason) == 0) ? 1 : 0;

	wsconn_put(wsc);

	return ret;
}

int w_ws_close3(sip_msg_t *msg, char *_status, char *_reason, char *_con)
{
	int status;
	str reason;
	int con;

	if(get_int_fparam(&status, msg, (fparam_t *)_status) < 0) {
		LM_ERR("failed to get status code\n");
		return -1;
	}

	if(get_str_fparam(&reason, msg, (fparam_t *)_reason) < 0) {
		LM_ERR("failed to get reason string\n");
		return -1;
	}

	if(get_int_fparam(&con, msg, (fparam_t *)_con) < 0) {
		LM_ERR("failed to get connection ID\n");
		return -1;
	}

	return ws_close3(msg, status, &reason, con);
}

/*
 * RPC command to set the state of a destination address
 */
void ws_rpc_close(rpc_t *rpc, void *ctx)
{
	unsigned int id;
	int ret;
	ws_connection_t *wsc;

	if(rpc->scan(ctx, "d", (int *)(&id)) < 1) {
		LM_WARN("no connection ID parameter\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}

	if((wsc = wsconn_get(id)) == NULL) {
		LM_WARN("bad connection ID parameter\n");
		rpc->fault(ctx, 500, "Unknown connection ID");
		return;
	}

	ret = close_connection(&wsc, LOCAL_CLOSE, 1000, str_status_normal_closure);

	wsconn_put(wsc);

	if(ret < 0) {
		LM_WARN("closing connection\n");
		rpc->fault(ctx, 500, str_status_error_closing.s);
		return;
	}
}

void ws_rpc_ping_pong(rpc_t *rpc, void *ctx, int opcode)
{
	unsigned int id;
	ws_connection_t *wsc;
	int ret = 0;

	if(rpc->scan(ctx, "d", (int *)(&id)) < 1) {
		LM_WARN("no connection ID parameter\n");
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}

	if((wsc = wsconn_get(id)) == NULL) {
		LM_WARN("bad connection ID parameter\n");
		rpc->fault(ctx, 500, "Unknown connection ID");
		return;
	}

	ret = ping_pong(wsc, opcode);

	wsconn_put(wsc);

	if(ret < 0) {
		LM_WARN("sending %s\n", OPCODE_PING ? "Ping" : "Pong");
		rpc->fault(ctx, 500, str_status_error_sending.s);
		return;
	}
}

void ws_rpc_ping(rpc_t *rpc, void *ctx)
{
	ws_rpc_ping_pong(rpc, ctx, OPCODE_PING);
}

void ws_rpc_pong(rpc_t *rpc, void *ctx)
{
	ws_rpc_ping_pong(rpc, ctx, OPCODE_PONG);
}
