/*
 * siptrace module - helper module to trace sip messages
 *
 * Copyright (C) 2017 kamailio.org
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../core/dprint.h"
#include "../../core/proxy.h"
#include "../../core/forward.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_cseq.h"

#include "siptrace_send.h"


extern int *xheaders_write_flag;
extern int *xheaders_read_flag;
extern str dup_uri_str;
extern sip_uri_t *dup_uri;

/**
 *
 */
int sip_trace_prepare(sip_msg_t *msg)
{
	if(parse_from_header(msg) == -1 || msg->from == NULL
			|| get_from(msg) == NULL) {
		LM_ERR("cannot parse FROM header\n");
		goto error;
	}

	if(parse_to_header(msg) == -1 || msg->to == NULL || get_to(msg) == NULL) {
		LM_ERR("cannot parse To header\n");
		goto error;
	}

	if(parse_headers(msg, HDR_CALLID_F, 0) != 0 || msg->callid == NULL
			|| msg->callid->body.s == NULL) {
		LM_ERR("cannot parse call-id\n");
		goto error;
	}

	if(msg->cseq == NULL && ((parse_headers(msg, HDR_CSEQ_F, 0) == -1)
									|| (msg->cseq == NULL))) {
		LM_ERR("cannot parse cseq\n");
		goto error;
	}

	return 0;
error:
	return -1;
}

/**
 * Appends x-headers to the message in sto->body containing data from sto
 */
int sip_trace_xheaders_write(struct _siptrace_data *sto)
{
	char *buf = NULL;
	int bytes_written = 0;
	char *eoh = NULL;
	int eoh_offset = 0;
	char *new_eoh = NULL;

	if(xheaders_write_flag == NULL || *xheaders_write_flag == 0)
		return 0;

	// Memory for the message with some additional headers.
	// It gets free()ed in sip_trace_xheaders_free().
	buf = pkg_malloc(sto->body.len + XHEADERS_BUFSIZE);
	if(buf == NULL) {
		LM_ERR("sip_trace_xheaders_write: out of memory\n");
		return -1;
	}

	// Copy the whole message to buf first; it must be \0-terminated for
	// strstr() to work. Then search for the end-of-header sequence.
	memcpy(buf, sto->body.s, sto->body.len);
	buf[sto->body.len] = '\0';
	eoh = strstr(buf, "\r\n\r\n");
	if(eoh == NULL) {
		LM_ERR("sip_trace_xheaders_write: malformed message\n");
		goto error;
	}
	eoh += 2; // the first \r\n belongs to the last header => skip it

	// Write the new headers a the end-of-header position. This overwrites
	// the \r\n terminating the old headers and the beginning of the message
	// body. Both will be recovered later.
	bytes_written =
			snprintf(eoh, XHEADERS_BUFSIZE, "X-Siptrace-Fromip: %.*s\r\n"
											"X-Siptrace-Toip: %.*s\r\n"
											"X-Siptrace-Time: %llu %llu\r\n"
											"X-Siptrace-Method: %.*s\r\n"
											"X-Siptrace-Dir: %s\r\n",
					sto->fromip.len, sto->fromip.s, sto->toip.len, sto->toip.s,
					(unsigned long long)sto->tv.tv_sec,
					(unsigned long long)sto->tv.tv_usec, sto->method.len,
					sto->method.s, sto->dir);
	if(bytes_written >= XHEADERS_BUFSIZE) {
		LM_ERR("sip_trace_xheaders_write: string too long\n");
		goto error;
	}

	// Copy the \r\n terminating the old headers and the message body from the
	// old buffer in sto->body.s to the new end-of-header in buf.
	eoh_offset = eoh - buf;
	new_eoh = eoh + bytes_written;
	memcpy(new_eoh, sto->body.s + eoh_offset, sto->body.len - eoh_offset);

	// Change sto to point to the new buffer.
	sto->body.s = buf;
	sto->body.len += bytes_written;
	return 0;
error:
	if(buf != NULL)
		pkg_free(buf);
	return -1;
}

/**
 * Parses x-headers, saves the data back to sto, and removes the x-headers
 * from the message in sto->buf
 */
int sip_trace_xheaders_read(struct _siptrace_data *sto)
{
	char *searchend = NULL;
	char *eoh = NULL;
	char *xheaders = NULL;
	long long unsigned int tv_sec, tv_usec;

	if(xheaders_read_flag == NULL || *xheaders_read_flag == 0)
		return 0;

	// Find the end-of-header marker \r\n\r\n
	searchend = sto->body.s + sto->body.len - 3;
	eoh = memchr(sto->body.s, '\r', searchend - eoh);
	while(eoh != NULL && eoh < searchend) {
		if(memcmp(eoh, "\r\n\r\n", 4) == 0)
			break;
		eoh = memchr(eoh + 1, '\r', searchend - eoh);
	}
	if(eoh == NULL) {
		LM_ERR("sip_trace_xheaders_read: malformed message\n");
		return -1;
	}

	// Find x-headers: eoh will be overwritten by \0 to allow the use of
	// strstr(). The byte at eoh will later be recovered, when the
	// message body is shifted towards the beginning of the message
	// to remove the x-headers.
	*eoh = '\0';
	xheaders = strstr(sto->body.s, "\r\nX-Siptrace-Fromip: ");
	if(xheaders == NULL) {
		LM_ERR("sip_trace_xheaders_read: message without x-headers "
			   "from %.*s, callid %.*s\n",
				sto->fromip.len, sto->fromip.s, sto->callid.len, sto->callid.s);
		return -1;
	}

	// Allocate memory for new strings in sto
	// (gets free()ed in sip_trace_xheaders_free() )
	sto->fromip.s = pkg_malloc(51);
	sto->toip.s = pkg_malloc(51);
	sto->method.s = pkg_malloc(51);
	sto->dir = pkg_malloc(4);
	if(!(sto->fromip.s && sto->toip.s && sto->method.s && sto->dir)) {
		LM_ERR("sip_trace_xheaders_read: out of memory\n");
		goto erroraftermalloc;
	}

	// Parse the x-headers: scanf()
	if(sscanf(xheaders, "\r\n"
						"X-Siptrace-Fromip: %50s\r\n"
						"X-Siptrace-Toip: %50s\r\n"
						"X-Siptrace-Time: %llu %llu\r\n"
						"X-Siptrace-Method: %50s\r\n"
						"X-Siptrace-Dir: %3s",
			   sto->fromip.s, sto->toip.s, &tv_sec, &tv_usec, sto->method.s,
			   sto->dir)
			== EOF) {
		LM_ERR("sip_trace_xheaders_read: malformed x-headers\n");
		goto erroraftermalloc;
	}
	sto->fromip.len = strlen(sto->fromip.s);
	sto->toip.len = strlen(sto->toip.s);
	sto->tv.tv_sec = (time_t)tv_sec;
	sto->tv.tv_usec = (suseconds_t)tv_usec;
	sto->method.len = strlen(sto->method.s);

	// Remove the x-headers: the message body is shifted towards the beginning
	// of the message, overwriting the x-headers. Before that, the byte at eoh
	// is recovered.
	*eoh = '\r';
	memmove(xheaders, eoh, sto->body.len - (eoh - sto->body.s));
	sto->body.len -= eoh - xheaders;

	return 0;

erroraftermalloc:
	if(sto->fromip.s)
		pkg_free(sto->fromip.s);
	if(sto->toip.s)
		pkg_free(sto->toip.s);
	if(sto->method.s)
		pkg_free(sto->method.s);
	if(sto->dir)
		pkg_free(sto->dir);
	return -1;
}

/**
 * Frees the memory allocated by sip_trace_xheaders_{write,read}
 */
int sip_trace_xheaders_free(struct _siptrace_data *sto)
{
	if(xheaders_write_flag != NULL && *xheaders_write_flag != 0) {
		if(sto->body.s)
			pkg_free(sto->body.s);
	}

	if(xheaders_read_flag != NULL && *xheaders_read_flag != 0) {
		if(sto->fromip.s)
			pkg_free(sto->fromip.s);
		if(sto->toip.s)
			pkg_free(sto->toip.s);
		if(sto->dir)
			pkg_free(sto->dir);
	}

	return 0;
}


/**
 *
 */
int trace_send_duplicate(char *buf, int len, struct dest_info *dst2)
{
	struct dest_info dst;
	struct proxy_l *p = NULL;

	if(buf == NULL || len <= 0)
		return -1;

	/* either modparam dup_uri or siptrace param dst2 */
	if((dup_uri_str.s == 0 || dup_uri == NULL) && (dst2 == NULL)) {
		LM_WARN("Neither dup_uri modparam or siptrace destination uri param used!\n");
		return 0;
	}

	init_dest_info(&dst);

	if(!dst2) {
		/* create a temporary proxy from dst param */
		dst.proto = PROTO_UDP;
		p = mk_proxy(&dup_uri->host,
				(dup_uri->port_no) ? dup_uri->port_no : SIP_PORT, dst.proto);
		if(p == 0) {
			LM_ERR("bad host name in uri\n");
			return -1;
		}
		hostent2su(
				&dst.to, &p->host, p->addr_idx, (p->port) ? p->port : SIP_PORT);

		dst.send_sock = get_send_socket(0, &dst.to, dst.proto);
		if(dst.send_sock == 0) {
			LM_ERR("can't forward to af %d, proto %d no corresponding"
				   " listening socket\n",
					dst.to.s.sa_family, dst.proto);
			goto error;
		}
	}

	if(msg_send((dst2) ? dst2 : &dst, buf, len) < 0) {
		LM_ERR("cannot send duplicate message\n");
		goto error;
	}

	if(p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}
	return 0;
error:
	if(p) {
		free_proxy(p); /* frees only p content, not p itself */
		pkg_free(p);
	}
	return -1;
}

/**
 *
 */
char* siptrace_proto_name(int vproto)
{
	switch(vproto) {
		case PROTO_TCP:
			return "tcp";
		case PROTO_TLS:
			return "tls";
		case PROTO_SCTP:
			return "sctp";
		case PROTO_WS:
			return "ws";
		case PROTO_WSS:
			return "wss";
		default:
			return "udp";
	}
}
