/*
 * Copyright (C) 2005 Voice System SRL
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


#ifndef NATHELPER_OPTIONS_H_
#define NATHELPER_OPTIONS_H_

#include <stdlib.h>
#include <string.h>

#include "../../core/parser/parse_rr.h"
#include "../../core/str.h"
#include "../../core/ut.h"
#include "../../core/ip_addr.h"
#include "../../core/rand/kam_rand.h"

/* size of buffer used for building SIP PING req */
#define MAX_SIPPING_SIZE 65536

/* helping macros for building SIP PING ping request */
#define append_fix(_p, _s)              \
	do {                                \
		memcpy(_p, _s, sizeof(_s) - 1); \
		_p += sizeof(_s) - 1;           \
	} while(0)

/* info used to generate SIP ping requests */
static int sipping_fromtag = 0;
static char sipping_callid_buf[8];
static int sipping_callid_cnt = 0;
static str sipping_callid = {0, 0};
static str sipping_from = STR_NULL;
static str sipping_method = str_init("OPTIONS");


static void init_sip_ping(void)
{
	int len;
	char *p;

	/* FROM tag - some random number */
	sipping_fromtag = kam_rand();
	/* callid fix part - hexa string */
	len = 8;
	p = sipping_callid_buf;
	int2reverse_hex(&p, &len, kam_rand());
	sipping_callid.s = sipping_callid_buf;
	sipping_callid.len = 8 - len;
	/* callid counter part */
	sipping_callid_cnt = kam_rand();
}


static int sipping_rpl_filter(struct sip_msg *rpl)
{
	struct cseq_body *cseq_b;

	/* first check number of vias -> must be only one */
	if(parse_headers(rpl, HDR_VIA2_F, 0) == -1 || (rpl->via2 != 0))
		goto skip;

	/* check the method -> we need CSeq header */
	if((!rpl->cseq && parse_headers(rpl, HDR_CSEQ_F, 0) != 0)
			|| rpl->cseq == 0) {
		LM_ERR("failed to parse CSeq\n");
		goto error;
	}
	cseq_b = (struct cseq_body *)rpl->cseq->parsed;
	if(cseq_b->method.len != sipping_method.len
			|| strncmp(cseq_b->method.s, sipping_method.s, sipping_method.len)
					   != 0)
		goto skip;

	/* check constant part of callid */
	if((!rpl->callid && parse_headers(rpl, HDR_CALLID_F, 0) != 0)
			|| rpl->callid == 0) {
		LM_ERR("failed to parse Call-ID\n");
		goto error;
	}
	if(rpl->callid->body.len <= sipping_callid.len + 1
			|| strncmp(rpl->callid->body.s, sipping_callid.s,
					   sipping_callid.len)
					   != 0
			|| rpl->callid->body.s[sipping_callid.len] != '-')
		goto skip;

	LM_DBG("reply for SIP natping filtered\n");
	/* it's a reply to a SIP NAT ping -> absorb it and stop any
	 * further processing of it */
	return 0;
skip:
	return 1;
error:
	return -1;
}


/* build the buffer of a SIP ping request */
static inline char *build_sipping(str *curi, struct socket_info *s, str *path,
		str *ruid, unsigned int aorhash, int *len_p)
{
#define s_len(_s) (sizeof(_s) - 1)
#define MAX_BRANCHID 9999999
#define MIN_BRANCHID 1000000
#define LEN_BRANCHID \
	7 /* NOTE: this must be sync with the MX and MIN values !! */
	static char buf[MAX_SIPPING_SIZE];
	char *p;
	int len;
	str vaddr;
	str vport;

	if(sipping_from.s==NULL || sipping_from.len<=0) {
		LM_WARN("SIP ping enabled but no SIP ping From address\n");
		return NULL;
	}
	if(s->useinfo.name.len > 0)
		vaddr = s->useinfo.name;
	else
		vaddr = s->address_str;

	if(s->useinfo.port_no > 0)
		vport = s->useinfo.port_no_str;
	else
		vport = s->port_no_str;

	if(sipping_method.len + 1 + curi->len + s_len(" SIP/2.0" CRLF)
					+ s_len("Via: SIP/2.0/UDP ") + vaddr.len
					+ ((s->address.af == AF_INET6) ? 2 : 0) + 1 + vport.len
					+ s_len(";branch=z9hG4bK") + LEN_BRANCHID
					+ (path->len ? (s_len(CRLF "Route: ") + path->len) : 0)
					+ s_len(CRLF "From: ") + sipping_from.len + s_len(";tag=")
					+ ruid->len + 1 + 8 + 1 + 8 + s_len(CRLF "To: ") + curi->len
					+ s_len(CRLF "Call-ID: ") + sipping_callid.len + 1 + 8 + 1
					+ 8 + 1 + s->address_str.len + s_len(CRLF "CSeq: 1 ")
					+ sipping_method.len
					+ s_len(CRLF "Content-Length: 0" CRLF CRLF)
			> MAX_SIPPING_SIZE) {
		LM_ERR("len exceeds %d\n", MAX_SIPPING_SIZE);
		return 0;
	}

	p = buf;
	append_str(p, sipping_method.s, sipping_method.len);
	*(p++) = ' ';
	append_str(p, curi->s, curi->len);
	append_fix(p, " SIP/2.0" CRLF "Via: SIP/2.0/UDP ");
	if(s->address.af == AF_INET6) { /* Via header IP is a IPv6 reference */
		append_fix(p, "[");
	}
	append_str(p, vaddr.s, vaddr.len);
	if(s->address.af == AF_INET6) {
		append_fix(p, "]");
	}
	*(p++) = ':';
	append_str(p, vport.s, vport.len);
	append_fix(p, ";branch=z9hG4bK");
	int2bstr((long)(rand() / (float)RAND_MAX * (MAX_BRANCHID - MIN_BRANCHID)
					 + MIN_BRANCHID),
			p + LEN_BRANCHID - INT2STR_MAX_LEN + 1, NULL);
	p += LEN_BRANCHID;
	if(path->len) {
		append_fix(p, CRLF "Route: ");
		append_str(p, path->s, path->len);
	}
	append_fix(p, CRLF "From: ");
	append_str(p, sipping_from.s, sipping_from.len);
	append_fix(p, ";tag=");
	append_str(p, ruid->s, ruid->len);
	*(p++) = '-';
	len = 8;
	int2reverse_hex(&p, &len, aorhash);
	*(p++) = '-';
	len = 8;
	int2reverse_hex(&p, &len, sipping_fromtag++);
	append_fix(p, CRLF "To: ");
	append_str(p, curi->s, curi->len);
	append_fix(p, CRLF "Call-ID: ");
	append_str(p, sipping_callid.s, sipping_callid.len);
	*(p++) = '-';
	len = 8;
	int2reverse_hex(&p, &len, sipping_callid_cnt++);
	*(p++) = '-';
	len = 8;
	int2reverse_hex(&p, &len, get_ticks());
	*(p++) = '@';
	append_str(p, s->address_str.s, s->address_str.len);
	append_fix(p, CRLF "CSeq: 1 ");
	append_str(p, sipping_method.s, sipping_method.len);
	append_fix(p, CRLF "Content-Length: 0" CRLF CRLF);

	*len_p = p - buf;
	return buf;
}

#endif
