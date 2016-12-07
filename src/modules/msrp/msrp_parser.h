/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _MSRP_PARSER_H_
#define _MSRP_PARSER_H_

#include "../../str.h"
#include "../../tcp_conn.h"

#define MSRP_REQUEST	1
#define MSRP_REPLY		2

#define MSRP_PORT 2855

#define MSRP_MAX_HDRS_SIZE	4096
#define MSRP_MAX_BODY_SIZE	10240
#define MSRP_MAX_FRAME_SIZE (MSRP_MAX_HDRS_SIZE + 2 + MSRP_MAX_BODY_SIZE)

#define MSRP_REQ_OTHER		0
#define MSRP_REQ_SEND		1
#define MSRP_REQ_AUTH		2
#define MSRP_REQ_REPORT		3

#define MSRP_REQ_RPLSTART	10000

#define MSRP_RPL_CODE(n)	((n) - MSRP_REQ_RPLSTART)

typedef struct msrp_fline {
	str buf;
	int msgtypeid;
	str protocol;
	str transaction;
	str rtype;
	int rtypeid;
	str rtext;
} msrp_fline_t;

#define MSRP_SCHEME_MSRP	1
#define MSRP_SCHEME_MSRPS	2

#define MSRP_PROTO_TCP	1
#define MSRP_PROTO_WS	2

typedef struct msrp_uri {
	str buf;
	str scheme;
	int scheme_no;
	str userinfo;
	str user;
	str host;
	str port;
	int port_no;
	str session;
	str proto;
	int proto_no;
	str params;
} msrp_uri_t;

int msrp_parse_uri(char *start, int len, msrp_uri_t *uri);

#define MSRP_HDR_OTHER			0
#define MSRP_HDR_FROM_PATH		1
#define MSRP_HDR_TO_PATH		2
#define MSRP_HDR_USE_PATH		3
#define MSRP_HDR_MESSAGE_ID		4
#define MSRP_HDR_BYTE_RANGE		5
#define MSRP_HDR_STATUS			6
#define MSRP_HDR_SUCCESS_REPORT	7
#define MSRP_HDR_CONTENT_TYPE	8
#define MSRP_HDR_AUTH			9
#define MSRP_HDR_WWWAUTH		10
#define MSRP_HDR_AUTHINFO		11
#define MSRP_HDR_EXPIRES		12

#define MSRP_DATA_SET	1

typedef struct msrp_data {
	void (*free_fn)(void*);
	int flags;
	void *data;
} msrp_data_t;

typedef struct msrp_hdr {
	str buf;
	int htype;
	str name;
	str body;
	msrp_data_t parsed;
	struct msrp_hdr *next;
} msrp_hdr_t;

typedef struct msrp_frame {
	str buf;             /* the whole message */
	msrp_fline_t fline;  /* first line parsed */
	str hbody;           /* all headers as a buf */
	str mbody;           /* the message body */
	str endline;         /* end line of the */
	msrp_hdr_t *headers; /* list of parsed headers */
	tcp_event_info_t *tcpinfo;
} msrp_frame_t;

int msrp_parse_frame(msrp_frame_t *mf);
int msrp_parse_fline(msrp_frame_t *mf);
int msrp_parse_headers(msrp_frame_t *mf);

int msrp_parse_hdr_to_path(msrp_frame_t *mf);
int msrp_parse_hdr_from_path(msrp_frame_t *mf);

void msrp_destroy_frame(msrp_frame_t *mf);
void msrp_free_frame(msrp_frame_t *mf);

msrp_hdr_t *msrp_get_hdr_by_id(msrp_frame_t *mf, int hdrid);

msrp_frame_t *msrp_get_current_frame(void);

typedef struct str_array {
	unsigned int size;
	str *list;
} str_array_t;

int msrp_frame_get_sessionid(msrp_frame_t *mf, str *sres);
int msrp_frame_get_first_from_path(msrp_frame_t *mf, str *sres);
int msrp_frame_get_expires(msrp_frame_t *mf, int *expires);
#endif
