/*
 * $Id$
 *
 * JABBER module
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _XML_JAB_H_
#define _XML_JAB_H_

#include "../../str.h"

/**********             ***/

#define _xml_msg_transform(_dst, _src, _len, _i) \
	for((_i)=0; (_i) < (_len); (_i)++) \
	{ \
		if(*((_src)+(_i)) == '<') \
		{ \
			*(_dst)++ = '&'; \
			*(_dst)++ = 'l'; \
			*(_dst)++ = 't'; \
			*(_dst)++ = ';'; \
		} \
		else \
		{ \
			if(*((_src)+(_i)) == '>') \
			{ \
				*(_dst)++ = '&'; \
				*(_dst)++ = 'g'; \
				*(_dst)++ = 't'; \
				*(_dst)++ = ';'; \
			} \
			else \
			{ \
				if(*((_src)+(_i)) == '&') \
				{ \
					*(_dst)++ = '&'; \
					*(_dst)++ = 'a'; \
					*(_dst)++ = 'm'; \
					*(_dst)++ = 'p'; \
					*(_dst)++ = ';'; \
				} \
				else \
				{ \
					*(_dst)++ = *((_src)+(_i)); \
				} \
			} \
		} \
	} \
	*(_dst) = 0;

/**********             ***/

typedef struct _jab_jmsg
{
	str to;
	str from;
	str type;
	str id;
	str body;
	str error;
	str errcode;
} t_jab_jmsg, *jab_jmsg;

int xml_escape(char *, int, char *, int);
int xml_escape_len(char *, int);
int xml_unescape(char *, int, char *, int);

int j2s_parse_jmsgx(const char *, int, jab_jmsg);
int j2s_parse_jmsg(const char *, int, jab_jmsg);

#endif

