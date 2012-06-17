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

#include "../../tcp_conn.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/tree.h"
#include "ws_frame.h"
#include "ws_mod.h"

/*   0                   1                   2                   3
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

typedef struct {
	unsigned int fin;
	unsigned int rsv1;
	unsigned int rsv2;
	unsigned int rsv3;
	unsigned int opcode;
	unsigned int mask;
	unsigned int payload_len;
	unsigned char masking_key[4];
	char *payload_data;
	tcp_event_info_t *tcpinfo;
} ws_frame_t;

#define BYTE0_MASK_FIN		(0x80)
#define BYTE0_MASK_RSV1		(0x40)
#define BYTE0_MASK_RSV2		(0x20)
#define BYTE0_MASK_RSV3 	(0x10)
#define BYTE0_MASK_OPCODE	(0x0F)
#define BYTE1_MASK_MASK		(0x80)
#define BYTE1_MASK_PAYLOAD_LEN	(0x7F)

#define OPCODE_CONTINUATION	(0x0)
#define OPCODE_TEXT_FRAME	(0x1)
#define OPCODE_BINARY_FRAME	(0x2)
/* 0x3 - 0x7 are reserved for further non-control frames */
#define OPCODE_CLOSE		(0x8)
#define OPCODE_PING		(0x9)
#define OPCODE_PONG		(0xa)
/* 0xb - 0xf are reserved for further control frames */


static int decode_and_validate_ws_frame(ws_frame_t *frame)
{
	unsigned int i, len=frame->tcpinfo->len;
	int mask_start, j;
	char *buf = frame->tcpinfo->buf;

	/* Decode and validate first 9 bits */
	if (len < 2)
	{
		LM_WARN("message is too short\n");
		return -1;
	}
	frame->fin = (buf[0] & 0xff) & BYTE0_MASK_FIN;
	frame->rsv1 = (buf[0] & 0xff) & BYTE0_MASK_RSV1;
	frame->rsv2 = (buf[0] & 0xff) & BYTE0_MASK_RSV2;
	frame->rsv3 = (buf[0] & 0xff) & BYTE0_MASK_RSV3;
	frame->opcode = (buf[0] & 0xff) & BYTE0_MASK_OPCODE;
	frame->mask = (buf[1] & 0xff) & BYTE1_MASK_MASK;
	
	if (!frame->fin)
	{
		LM_WARN("WebSocket fragmentation not supported in the sip "
			"sub-protocol\n");
		return -1;
	}

	if (frame->rsv1 || frame->rsv2 || frame->rsv3)
	{
		LM_WARN("WebSocket reserved fields with non-zero values\n");
		return -1;
	}

	switch(frame->opcode)
	{
	case OPCODE_TEXT_FRAME:
	case OPCODE_BINARY_FRAME:
		LM_INFO("supported non-control frame: 0x%x\n",
			(unsigned char) frame->opcode);
		break;

	case OPCODE_CLOSE:
	case OPCODE_PING:
	case OPCODE_PONG:
		LM_INFO("supported control frame: 0x%x\n",
			(unsigned char) frame->opcode);
		break;

	default:
		LM_WARN("unsupported opcode: 0x%x\n",
			(unsigned char) frame->opcode);
		return -1;
	}

	if (!frame->mask)
	{
		LM_WARN("this is a server - all received messages must be "
			"masked\n");
		return -1;
	}

	/* Decode and validate length */
	frame->payload_len = (buf[1] & 0xff) & BYTE1_MASK_PAYLOAD_LEN;
	if (frame->payload_len == 126)
	{
		if (len < 4)
		{
			LM_WARN("message is too short\n");
			return -1;
		}
		mask_start = 4;

		frame->payload_len = 	  ((buf[2] & 0xff) <<  8)
					| ((buf[3] & 0xff) <<  0);
	}
	else if (frame->payload_len == 127)
	{
		if (len < 10)
		{
			LM_WARN("message is too short\n");
			return -1;
		}
		mask_start = 10;

		/* Only decoding the last four bytes of the length...
		   This limits the size of WebSocket messages that can be
		   handled to 2^32 = which should be plenty for SIP! */
	 	frame->payload_len =	  ((buf[6] & 0xff) << 24)
					| ((buf[7] & 0xff) << 16)
					| ((buf[8] & 0xff) <<  8)
					| ((buf[9] & 0xff) <<  0);
	}
	else
		mask_start = 2;

	/* Decode mask */
	frame->masking_key[0] = (buf[mask_start + 0] & 0xff);
	frame->masking_key[1] = (buf[mask_start + 1] & 0xff);
	frame->masking_key[2] = (buf[mask_start + 2] & 0xff);
	frame->masking_key[3] = (buf[mask_start + 3] & 0xff);

	/* Decode and unmask payload */
	if (len != frame->payload_len + mask_start + 4)
	{
		LM_WARN("message not complete frame size %u but received %u\n",
			frame->payload_len + mask_start + 4, len);
		return -1;
	}
	frame->payload_data = &buf[mask_start + 4];
	for (i = 0; i < frame->payload_len; i++)
	{
		j = i % 4;
		frame->payload_data[i]
			= frame->payload_data[i] ^ frame->masking_key[j];
	}

	LM_INFO("Rx (decoded): %.*s\n",
		(int) frame->payload_len, frame->payload_data);

	return frame->opcode;
}

static int encode_and_send_ws_frame(ws_frame_t *frame)
{
	/* TODO: convert ws_frame_t into a binary WebSocket frame and send over
	   TCP/TLS */

	update_stat(ws_transmitted_frames, 1);

	return 0;
}

static int handle_sip_message(ws_frame_t *frame)
{
	LM_INFO("Received SIP message\n");

	/* TODO: drop SIP message into route {} for processing */

	return 0;
}

static int handle_close(ws_frame_t *frame)
{
	unsigned short code = 0;
	str reason = {0, 0};

	update_stat(ws_remote_closed_connections, 1);
	update_stat(ws_current_connections, -1);
	LM_INFO("Received Close\n");

	if (frame->payload_len >= 2)
		code =    ((frame->payload_data[0] & 0xff) << 8)
			| ((frame->payload_data[1] & 0xff) << 0);

	if (frame->payload_len > 2)
	{
		reason.s = &frame->payload_data[2];
		reason.len = frame->payload_len - 2;
	}

	LM_INFO("Close: %hu %.*s\n", code, reason.len, reason.s); 

	/* TODO: cleanly close TCP/TLS connection */

	return 0;
}

static int handle_ping(ws_frame_t *frame)
{
	ws_frame_t ws_frame;

	LM_INFO("Received Ping\n");

	memset(&ws_frame, 0, sizeof(ws_frame_t));
	ws_frame.fin = 1;
	ws_frame.opcode = OPCODE_PONG;
	ws_frame.payload_len = frame->payload_len;
	ws_frame.payload_data =  frame->payload_data;
	ws_frame.tcpinfo = frame->tcpinfo;

	encode_and_send_ws_frame(&ws_frame);

	return 0;
}

static int handle_pong(ws_frame_t *frame)
{
	LM_INFO("Received Pong\n");

	LM_INFO("Pong: %.*s\n", frame->payload_len, frame->payload_data);

	return 0;
}

int ws_frame_received(void *data)
{
	ws_frame_t ws_frame;
	tcp_event_info_t *tev = (tcp_event_info_t *) data;

	update_stat(ws_received_frames, 1);

	if (tev == NULL || tev->buf == NULL || tev->len <= 0)
	{
		LM_WARN("received bad frame\n");
		return -1;
	}

	ws_frame.tcpinfo = tev;
	switch(decode_and_validate_ws_frame(&ws_frame))
	{
	case OPCODE_TEXT_FRAME:
	case OPCODE_BINARY_FRAME:
		if (handle_sip_message(&ws_frame) < 0)
		{
			LM_ERR("handling SIP message\n");
			return -1;
		}
		break;

	case OPCODE_CLOSE:
		if (handle_close(&ws_frame) < 0)
		{
			LM_ERR("handling Close\n");
			return -1;
		}
		break;

	case OPCODE_PING:
		if (handle_ping(&ws_frame) < 0)
		{
			LM_ERR("handling Ping\n");
			return -1;
		}
		break;

	case OPCODE_PONG:
		if (handle_pong(&ws_frame) < 0)
		{
			LM_ERR("handling Pong\n");
			return -1;
		}
		break;
		
	default:
		LM_WARN("received bad frame\n");
		return -1;
	}

	return 0;
}

struct mi_root *ws_mi_close(struct mi_root *cmd, void *param)
{
	/* TODO Close specified or all connections */
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

struct mi_root *ws_mi_ping(struct mi_root *cmd, void *param)
{
	/* TODO Ping specified connection */
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}

struct mi_root *ws_mi_pong(struct mi_root *cmd, void *param)
{
	/* TODO Pong specified connection */
	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}
