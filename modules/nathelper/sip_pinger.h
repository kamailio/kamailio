/* $Id$
 *
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
 * History:
 * ---------
 * 2005-07-11  created (bogdan)
 */


#ifndef NATHELPER_OPTIONS_H_
#define NATHELPER_OPTIONS_H_

#include <stdlib.h>
#include <string.h>

#include "../../parser/parse_rr.h"
#include "../../str.h"
#include "../../ut.h"
#include "../../ip_addr.h"

/* size of buffer used for building SIP PING req */
#define MAX_SIPPING_SIZE 65536

/* helping macros for building SIP PING ping request */
#define append_fix( _p, _s) \
	do {\
		memcpy(_p, _s, sizeof(_s)-1);\
		_p += sizeof(_s)-1;\
	}while(0)

/* info used to generate SIP ping requests */
static int  sipping_fromtag = 0;
static char sipping_callid_buf[8];
static int  sipping_callid_cnt = 0;
static str  sipping_callid = {0,0};
static str  sipping_from = STR_NULL;
static str  sipping_method = str_init("OPTIONS");



static void init_sip_ping(void)
{
	int len;
	char *p;

	/* FROM tag - some random number */
	sipping_fromtag = rand();
	/* callid fix part - hexa string */
	len = 8;
	p = sipping_callid_buf;
	int2reverse_hex( &p, &len, rand() );
	sipping_callid.s = sipping_callid_buf;
	sipping_callid.len = 8-len;
	/* callid counter part */
	sipping_callid_cnt = rand();
}



static int sipping_rpl_filter(struct sip_msg *rpl)
{
	struct cseq_body* cseq_b;

	/* first check number of vias -> must be only one */
	if (parse_headers( rpl, HDR_VIA2_F, 0 )==-1 || (rpl->via2!=0))
		goto skip;

	/* check the method -> we need CSeq header */
	if ( (!rpl->cseq && parse_headers(rpl,HDR_CSEQ_F,0)!=0) || rpl->cseq==0 ) {
		LM_ERR("failed to parse CSeq\n");
		goto error;
	}
	cseq_b = (struct cseq_body*)rpl->cseq->parsed;
	if (cseq_b->method.len!=sipping_method.len ||
	strncmp(cseq_b->method.s,sipping_method.s,sipping_method.len)!=0)
		goto skip;

	/* check constant part of callid */
	if ( (!rpl->callid && parse_headers(rpl,HDR_CALLID_F,0)!=0) ||
	rpl->callid==0 ) {
		LM_ERR("failed to parse Call-ID\n");
		goto error;
	}
	if ( rpl->callid->body.len<=sipping_callid.len+1 ||
	strncmp(rpl->callid->body.s,sipping_callid.s,sipping_callid.len)!=0 ||
	rpl->callid->body.s[sipping_callid.len]!='-')
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
static inline char* build_sipping(str *curi, struct socket_info* s, str *path,
								str *ruid, unsigned int aorhash, int *len_p)
{
#define s_len(_s) (sizeof(_s)-1)
	static char buf[MAX_SIPPING_SIZE];
	char *p;
	int len;

	if ( sipping_method.len + 1 + curi->len + s_len(" SIP/2.0"CRLF) +
		s_len("Via: SIP/2.0/UDP ") + s->address_str.len +
				1 + s->port_no_str.len + s_len(";branch=0") +
		(path->len ? (s_len(CRLF"Route: ") + path->len) : 0) +
		s_len(CRLF"From: ") +  sipping_from.len + s_len(";tag=") +
				ruid->len + 1 + 8 + 1 + 8 +
		s_len(CRLF"To: ") + curi->len +
		s_len(CRLF"Call-ID: ") + sipping_callid.len + 1 + 8 + 1 + 8 + 1 +
				s->address_str.len +
		s_len(CRLF"CSeq: 1 ") + sipping_method.len +
		s_len(CRLF"Content-Length: 0" CRLF CRLF)
			> MAX_SIPPING_SIZE )
	{
		LM_ERR("len exceeds %d\n",MAX_SIPPING_SIZE);
		return 0;
	}

	p = buf;
	append_str( p, sipping_method.s, sipping_method.len);
	*(p++) = ' ';
	append_str( p, curi->s, curi->len);
	append_fix( p, " SIP/2.0"CRLF"Via: SIP/2.0/UDP ");
	append_str( p, s->address_str.s, s->address_str.len);
	*(p++) = ':';
	append_str( p, s->port_no_str.s, s->port_no_str.len);
	if (path->len) {
		append_fix( p, ";branch=0"CRLF"Route: ");
		append_str( p, path->s, path->len);
		append_fix( p, CRLF"From: ");
	} else {
		append_fix( p, ";branch=0"CRLF"From: ");
	}
	append_str( p, sipping_from.s, sipping_from.len);
	append_fix( p, ";tag=");
	append_str( p, ruid->s, ruid->len);
	*(p++) = '-';
	len = 8;
	int2reverse_hex( &p, &len, aorhash );
	*(p++) = '-';
	len = 8;
	int2reverse_hex( &p, &len, sipping_fromtag++ );
	append_fix( p, CRLF"To: ");
	append_str( p, curi->s, curi->len);
	append_fix( p, CRLF"Call-ID: ");
	append_str( p, sipping_callid.s, sipping_callid.len);
	*(p++) = '-';
	len = 8;
	int2reverse_hex( &p, &len, sipping_callid_cnt++ );
	*(p++) = '-';
	len = 8;
	int2reverse_hex( &p, &len, get_ticks() );
	*(p++) = '@';
	append_str( p, s->address_str.s, s->address_str.len);
	append_fix( p, CRLF"CSeq: 1 ");
	append_str( p, sipping_method.s, sipping_method.len);
	append_fix( p, CRLF"Content-Length: 0" CRLF CRLF);

	*len_p = p - buf;
	return buf;
}


#endif
