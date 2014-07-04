/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "msfuncs.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "../../dprint.h"
#include "../../config.h"
#include "../../ut.h"
#include "../../forward.h"
#include "../../resolve.h"
#include "../../globals.h"
#include "../../udp_server.h"
#include "../../pt.h"

#define CONTACT_PREFIX "Contact: <"
#define CONTACT_SUFFIX  ">;msilo=yes"CRLF
#define CONTACT_PREFIX_LEN (sizeof(CONTACT_PREFIX)-1)
#define CONTACT_SUFFIX_LEN  (sizeof(CONTACT_SUFFIX)-1)

#define EAT_SPACES(_p, _e)	\
			while((*(_p)) && ((_p) <= (_e)) && (*(_p)==' '\
					|| *(_p)=='\t')) (_p)++; \
				if((_p)>(_e)) return -2

#define SKIP_CHARS(_p, _n, _e) \
			if( (_p)+(_n) < (_e) ) (_p) += (_n); \
			else goto error


#define NEXT_SEP(_p, _pos, _e) \
			(_pos) = 0; \
			while( (*((_p)+(_pos))) && ((_p)+(_pos) <= (_e)) && \
					(*((_p)+(_pos)) != ' ') \
					&& (*((_p)+(_pos)) != '\t') && (*((_p)+(_pos)) != '=') \
					&& (*((_p)+(_pos)) != ';') && (*((_p)+(_pos)) != '\n')) \
				(_pos)++; \
			if((_p)+(_pos) > (_e)) goto error

/**
 * apostrophes escaping
 * - src: source buffer
 * - slen: length of source buffer
 * - dst: destination buffer
 * - dlen: max length of destination buffer
 * #return: destination length => OK; -1 => error
 */

int m_apo_escape(char* src, int slen, char* dst, int dlen)
{
	int i, j;

	if(!src || !dst || dlen <= 0)
		return -1;

	if(slen == -1)
		slen = strlen(src);

	for(i=j=0; i<slen; i++)
	{
		switch(src[i])
		{
			case '\'':
					if(j+2>=dlen)
						return -2;
					memcpy(&dst[j], "\\'", 2);
					j += 2;
				break;
			default:
				if(j+1>=dlen)
						return -2;
				dst[j] = src[i];
				j++;
		}
	}
	dst[j] = '\0';

	return j;
}

/**
 * extract the value of Content-Type header
 * - src: pointer to C-T content
 * - len: length of src
 * - ctype: parsed C-T
 * - flag: what to parse - bit mask of CT_TYPE, CT_CHARSET, CT_MSGR
 *
 * #return: 0 OK ; -1 error
  */
int m_extract_content_type(char* src, int len, t_content_type* ctype, int flag)
{
	char *p, *end;
	int f = 0, pos;

	if( !src || len <=0 )
		goto error;
	p = src;
	end = p + len;
	while((p < end) && f != flag)
	{
		EAT_SPACES(p, end);
		if((flag & CT_TYPE) && !(f & CT_TYPE))
		{
			NEXT_SEP(p, pos, end);
			if(p[pos] == ';')
			{
				ctype->type.s = p;
				ctype->type.len = pos;
				SKIP_CHARS(p, pos+1, end);
				f |= CT_TYPE;
				continue;
			}
		}
		if((flag & CT_CHARSET) && !(f & CT_CHARSET))
		{
		}
		if((flag & CT_MSGR) && !(f & CT_MSGR))
		{
		}
	}
	return 0;
error:
	return -1;
}

/** build MESSAGE headers 
 *
 * only Content-Type at this moment
 * expects - max buf len of the resulted body in body->len
 *         - body->s MUST be allocated
 * #return: 0 OK ; -1 error
 * */
int m_build_headers(str *buf, str ctype, str contact)
{
	char *p;
	if(!buf || !buf->s || buf->len <= 0 || ctype.len < 0 || contact.len < 0
			|| buf->len <= ctype.len+contact.len+14 /*Content-Type: */
				+CRLF_LEN+CONTACT_PREFIX_LEN+CONTACT_SUFFIX_LEN)
		goto error;

	p = buf->s;
	if(ctype.len > 0)
	{
		strncpy(p, "Content-Type: ", 14);
		p += 14;
		strncpy(p, ctype.s, ctype.len);
		p += ctype.len;
		strncpy(p, CRLF, CRLF_LEN);
		p += CRLF_LEN;
	
	}
	if(contact.len > 0)
	{
		strncpy(p, CONTACT_PREFIX, CONTACT_PREFIX_LEN);
		p += CONTACT_PREFIX_LEN;
		strncpy(p, contact.s, contact.len);
		p += contact.len;
		strncpy(p, CONTACT_SUFFIX, CONTACT_SUFFIX_LEN);
		p += CONTACT_SUFFIX_LEN;
	}
	buf->len = p - buf->s;	
	return 0;
error:
	return -1;
}

/** build MESSAGE body --- add incoming time and 'from' 
 *
 * expects - max buf len of the resulted body in body->len
 *         - body->s MUST be allocated
 * #return: 0 OK ; -1 error
 * */
int m_build_body(str *body, time_t date, str msg)
{
	char *p;
	
	if(!body || !(body->s) || body->len <= 0 ||
			date < 0 || msg.len < 0 || (46+msg.len > body->len) )
		goto error;
	
	p = body->s;

	strncpy(p, "[Offline message - ", 19);
	p += 19;
	
	strncpy(p, ctime(&date), 24);
	p += 24;

	/**
	if(from.len > 0)
	{
		*p++ = ' ';
		strncpy(p, from.s, from.len);
		p += from.len;
	}
	**/
	
	*p++ = ']';
	
	if(msg.len > 0)
	{
		*p++ = ' ';
		strncpy(p, msg.s, msg.len);
		p += msg.len;
	}

	body->len = p - body->s;
	
	return 0;
error:
	return -1;
}

