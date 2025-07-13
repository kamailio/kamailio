/*
 * message first line parsing automaton
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*! \file
 * \brief Parser :: SIP first line parsing automaton
 *
 * \ingroup parser
 */


#include "../comp_defs.h"
#include "../dprint.h"
#include "msg_parser.h"
#include "parser_f.h"
#include "../mem/mem.h"
#include "../ut.h"


int http_reply_parse = 0;

/* grammar:
 *  request  =  method SP uri SP version CRLF
 *  response =  version SP status  SP reason  CRLF
 *  (version = "SIP/2.0")
*/


/* parses the first line, returns pointer to  next line  & fills fl;
 * also  modifies buffer (to avoid extra copy ops) */
char *parse_first_line(char *buffer, unsigned int len, struct msg_start *fl)
{

	char *tmp;
	char *second;
	char *third;
	char *nl;
	int offset;
	/* int l; */
	char *end;
	char s1, s2, s3;
	char *prn;
	unsigned int t;

	/* grammar:
		request  =  method SP uri SP version CRLF
		response =  version SP status SP reason CRLF
		(version = "SIP/2.0")
	*/

	memset(fl, 0, sizeof(struct msg_start));
	offset = 0;
	end = buffer + len;
	/* see if it's a reply (status) */

	/* jku  -- parse well-known methods */

	/* drop messages which are so short they are for sure useless;
	 * utilize knowledge of minimum size in parsing the first token */
	if(len <= 16) {
		LM_INFO("message too short: %d [%.*s]\n", len, len, buffer);
		goto error1;
	}
	tmp = buffer;
	/* is it perhaps a reply, ie does it start with "SIP...." ? */
	if((*tmp == 'S' || *tmp == 's')
			&& strncasecmp(tmp + 1, &SIP_VERSION[1], SIP_VERSION_LEN - 1) == 0
			&& (*(tmp + SIP_VERSION_LEN) == ' ')) {
		fl->type = SIP_REPLY;
		fl->flags |= FLINE_FLAG_PROTO_SIP;
		fl->u.reply.version.len = SIP_VERSION_LEN;
		tmp = buffer + SIP_VERSION_LEN;
	} else if(http_reply_parse != 0 && (*tmp == 'H' || *tmp == 'h')) {
		/* 'HTTP/1.[0|1]' */
		if(strncasecmp(tmp + 1, &HTTP_VERSION[1], HTTP_VERSION_LEN - 1) == 0
				&& ((*(tmp + HTTP_VERSION_LEN) == '0')
						|| (*(tmp + HTTP_VERSION_LEN) == '1'))
				&& (*(tmp + HTTP_VERSION_LEN + 1) == ' ')) {
			/* hack to be able to route http replies
			 * Note: - the http reply must have a via
			 *       - the message is marked as SIP_REPLY (ugly)
			 */
			fl->type = SIP_REPLY;
			fl->flags |= FLINE_FLAG_PROTO_HTTP;
			fl->u.reply.version.len =
					HTTP_VERSION_LEN + 1 /*include last digit*/;
			tmp = buffer + HTTP_VERSION_LEN + 1 /* last digit */;
			/* 'HTTP/2' */
		} else if(strncasecmp(tmp + 1, &HTTP2_VERSION[1], HTTP2_VERSION_LEN - 1)
						  == 0
				  && (*(tmp + HTTP2_VERSION_LEN) == ' ')) {
			fl->type = SIP_REPLY;
			fl->flags |= (FLINE_FLAG_PROTO_HTTP | FLINE_FLAG_PROTO_HTTP2);
			fl->u.reply.version.len = HTTP2_VERSION_LEN;
			tmp = buffer + HTTP2_VERSION_LEN;
		}
	} else
		IFISMETHOD(INVITE, 'I')
	else IFISMETHOD(CANCEL, 'C') else IFISMETHOD(ACK, 'A') else IFISMETHOD(
			BYE, 'B') else IFISMETHOD(INFO, 'I') else IFISMETHOD(REGISTER,
			'R') else IFISMETHOD(SUBSCRIBE, 'S') else IFISMETHOD(NOTIFY,
			'N') else IFISMETHOD(MESSAGE, 'M') else IFISMETHOD(OPTIONS,
			'O') else IFISMETHOD(PRACK, 'P') else IFISMETHOD(UPDATE,
			'U') else IFISMETHOD(REFER, 'R') else IFISMETHOD(PUBLISH,
			'P') else IFISMETHOD(KDMQ, 'K') else IFISMETHOD(GET,
			'G') else IFISMETHOD(POST, 'P') else IFISMETHOD(PUT,
			'P') else IFISMETHOD(DELETE, 'D')
			/* if you want to add another method XXX, include METHOD_XXX in
	 * H-file (this is the value which you will take later in
	 * processing and define XXX_LEN as length of method name;
	 * then just call IFISMETHOD( XXX, 'X' ) ... 'X' is the first
	 * letter; everything must be capitals
	*/
			else
	{
		/* neither reply, nor any of known method requests,
		 * let's believe it is an unknown method request */
		tmp = eat_token_end(buffer, buffer + len);
		if((tmp == buffer) || (tmp >= end)) {
			LM_INFO("empty or bad first line\n");
			goto error1;
		}
		if(*tmp != ' ') {
			LM_INFO("method not followed by SP\n");
			goto error1;
		}
		fl->type = SIP_REQUEST;
		fl->u.request.method_value = METHOD_OTHER;
		fl->u.request.method.len = tmp - buffer;
	}


	/* identifying type of message over now;
	 * tmp points at space after; go ahead */

	fl->u.request.method.s = buffer; /* store ptr to first token */
	second = tmp + 1;				 /* jump to second token */
	offset = second - buffer;

	/* EoJku */

	/* next element */
	tmp = eat_token_end(second, second + len - offset);
	if(tmp >= end) {
		goto error;
	}
	offset += tmp - second;
	third = eat_space_end(tmp, tmp + len - offset);
	offset += third - tmp;
	if((third == tmp) || (tmp >= end)) {
		goto error;
	}
	fl->u.request.uri.s = second;
	fl->u.request.uri.len = tmp - second;

	/* jku: parse status code */
	if(fl->type == SIP_REPLY) {
		if(fl->u.request.uri.len != 3) {
			LM_INFO("len(status code)!=3: %.*s\n", fl->u.request.uri.len,
					ZSW(second));
			goto error;
		}
		s1 = *second;
		s2 = *(second + 1);
		s3 = *(second + 2);
		if(s1 >= '0' && s1 <= '9' && s2 >= '0' && s2 <= '9' && s3 >= '0'
				&& s3 <= '9') {
			fl->u.reply.statuscode =
					(s1 - '0') * 100 + 10 * (s2 - '0') + (s3 - '0');
		} else {
			LM_INFO("status code non-numerical: %.*s\n", fl->u.request.uri.len,
					ZSW(second));
			goto error;
		}
	}
	/* EoJku */

	/*  last part: for a request it must be the version, for a reply
	 *  it can contain almost anything, including spaces, so we don't care
	 *  about it*/
	if(fl->type == SIP_REQUEST) {
		tmp = eat_token_end(third, third + len - offset);
		offset += tmp - third;
		if((tmp == third) || (tmp >= end)) {
			goto error;
		}
		if(!is_empty_end(tmp, tmp + len - offset)) {
			goto error;
		}
	} else {
		/* find end of line ('\n' or '\r') */
		tmp = eat_token2_end(third, third + len - offset, '\r');
		if(tmp >= end) { /* no crlf in packet => invalid */
			goto error;
		}
		offset += tmp - third;
	}
	nl = eat_line(tmp, len - offset);
	if(nl >= end) { /* no crlf in packet or only 1 line > invalid */
		goto error;
	}
	fl->u.request.version.s = third;
	fl->u.request.version.len = tmp - third;
	fl->len = nl - buffer;

	if(fl->type == SIP_REQUEST) {
		if(fl->u.request.version.len >= SIP_VERSION_LEN
				&& (fl->u.request.version.s[0] == 'S'
						|| fl->u.request.version.s[0] == 's')
				&& !strncasecmp(fl->u.request.version.s + 1, &SIP_VERSION[1],
						SIP_VERSION_LEN - 1)) {
			fl->flags |= FLINE_FLAG_PROTO_SIP;
		} else if(fl->u.request.version.len >= 4
				  && (fl->u.request.version.s[0] == 'H'
						  || fl->u.request.version.s[0] == 'h')) {
			if(fl->u.request.version.len >= HTTP_VERSION_LEN
					&& !strncasecmp(fl->u.request.version.s + 1,
							&HTTP_VERSION[1], HTTP_VERSION_LEN - 1)) {
				fl->flags |= FLINE_FLAG_PROTO_HTTP;
			} else if(fl->u.request.version.len >= HTTP2_VERSION_LEN
					  && !strncasecmp(fl->u.request.version.s + 1,
							  &HTTP2_VERSION[1], HTTP2_VERSION_LEN - 1)) {
				fl->flags |= (FLINE_FLAG_PROTO_HTTP | FLINE_FLAG_PROTO_HTTP2);
			}
		}
	}

	LM_DBG("first line type %d (%s) flags %d\n", (int)fl->type,
			(fl->type == SIP_REPLY) ? "reply(status)" : "request",
			(int)fl->flags);

	return nl;

error:
	LM_DBG("bad %s first line\n",
			(fl->type == SIP_REPLY) ? "reply(status)" : "request");

	LM_DBG("at line 0 char %d: \n", offset);
	prn = pkg_malloc(offset);
	if(prn) {
		for(t = 0; t < offset; t++)
			if(*(buffer + t))
				*(prn + t) = *(buffer + t);
			else
				*(prn + t) = (char)176; /* '�' */
		LM_DBG("parsed so far: %.*s\n", offset, ZSW(prn));
		pkg_free(prn);
	} else {
		PKG_MEM_ERROR;
	}
error1:
	fl->type = SIP_INVALID;
	LOG(cfg_get(core, core_cfg, sip_parser_log),
			"parse_first_line: bad message (offset: %d)\n", offset);
	/* skip  line */
	nl = eat_line(buffer, len);
	return nl;
}

char *parse_fline(char *buffer, char *end, struct msg_start *fl)
{
	if(end <= buffer) {
		/* make it throw error via parse_first_line() for consistency */
		return parse_first_line(buffer, 0, fl);
	}
	return parse_first_line(buffer, (unsigned int)(end - buffer), fl);
}
