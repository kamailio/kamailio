/*
 * $Id$
 *
 * JABBER module
 *
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

