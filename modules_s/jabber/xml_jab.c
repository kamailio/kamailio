/*
 * $Id$
 *
 * JABBER module
 *
 */

#include "xml_jab.h"


#include <unistd.h>
#include <string.h>

#include "../../dprint.h"

#define EAT_SPACES_R(_p, _e)	\
			while((*(_p)) && ((_p) <= (_e)) && (*(_p)==' '\
					|| *(_p)=='\t')) (_p)++; \
				if((_p)>(_e)) return -2

#define EAT_SPACES(_p)	while((*(_p)) && (*(_p)==' ' || *(_p)=='\t')) (_p)++

#define NEXT_ALCHAR(_p, _pos)	\
			(_pos) = 0; \
			while((*((_p)+(_pos))) && \
						(*((_p)+(_pos))<'A' || *((_p)+(_pos))>'Z') \
						&& (*((_p)+(_pos))<'a' || *((_p)+(_pos))>'z')) \
				(_pos)++

#define NEXT_ALCHARX(_p, _pos, _e)	\
			(_pos) = 0; \
			while((*((_p)+(_pos))) && ((_p)+(_pos) <= (_e)) && \
						(*((_p)+(_pos))<'A' || *((_p)+(_pos))>'Z') \
						&& (*((_p)+(_pos))<'a' || *((_p)+(_pos))>'z') \
						&& (*((_p)+(_pos))!='>') \
						&& (*((_p)+(_pos))!='<') && (*((_p)+(_pos))!='/')) \
				(_pos)++; \
			if((_p)+(_pos) > (_e)) return -2

#define NEXT_CHAR(_p, _c, _pos) \
			(_pos) = 0; \
			while((*((_p)+(_pos))) && *((_p)+(_pos))!=(_c)) (_pos)++

#define NEXT_CHAR_R(_p, _c, _pos, _e) \
			(_pos) = 0; \
			while( (*((_p)+(_pos))) && ((_p)+(_pos) <= (_e)) && \
					(*((_p)+(_pos)) != (_c)) ) (_pos)++; \
			if((_p)+(_pos) > (_e)) return -2

#define NEXT_TAGSEP_R(_p, _pos, _e) \
			(_pos) = 0; \
			while( (*((_p)+(_pos))) && ((_p)+(_pos) <= (_e)) && \
					(*((_p)+(_pos)) != ' ') \
					&& (*((_p)+(_pos)) != '\t') && (*((_p)+(_pos)) != '>') \
					&& (*((_p)+(_pos)) != '/') ) \
				(_pos)++; \
			if((_p)+(_pos) > (_e)) return -2

#define SKIP_CHARS(_p, _n)	(_p) += (_n)
#define SKIP_CHARS_R(_p, _n, _e) \
			if( (_p)+(_n) < (_e) ) (_p) += (_n); else return -2

#define IGNORE_TAG(_p, _p1, _l1, _e, _s)  \
			(_s) = 0;\
			while(1)\
			{\
				while( (*(_p)) && ((_p)+2<=(_e)) && (*(_p) != '<') && \
						(*(_p) != '>') ) \
					(_p)++; \
				if( !(*(_p)) || ((_p)+(_l1)>(_e)) ) \
									return -2; \
				if((_s)==0 && *(_p)=='>') \
				{ \
					if(*((_p)-1) == '/') \
					{ \
						(_p)++; \
							break; \
					} \
					else \
					(_p)++; \
					(_s) = 1; \
				} else if(*(_p) == '<' && *((_p)+1)=='/') \
				{ \
					(_p) += 2; \
					if(!strncasecmp((_p), (_p1), (_l1)) ) \
					{ \
					(_p) += (_l1); \
					if(*(_p) == '>') \
					{ \
						(_p)++; \
						break; \
					} \
					else \
						return -2; \
				} \
				else \
					(_p)++; \
			} \
			else \
				(_p)++; \
		}

/*****  XML Escaping  *****/
/**
 *
 */
int xml_escape(char *src, int slen, char *dst, int dlen)
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
			case '&':
					if(j+5>=dlen)
						return -2;
					memcpy(&dst[j], "&amp;", 5);
					j += 5;
				break;
			case '\'':
					if(j+6>=dlen)
						return -2;
					memcpy(&dst[j], "&apos;", 6);
					j += 6;
				break;
			case '"':
					if(j+6>=dlen)
						return -2;
					memcpy(&dst[j], "&quot;", 6);
					j += 6;
				break;
			case '<':
					if(j+4>=dlen)
						return -2;
					memcpy(&dst[j], "&lt;", 4);
					j += 4;
				break;
			case '>':
					if(j+4>=dlen)
						return -2;
					memcpy(&dst[j], "&gt;", 4);
					j += 4;
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
 *
 */
int xml_escape_len(char *src, int slen)
{
	int i, nlen;

	if(!src)
		return -1;

	if(slen == -1)
		slen = strlen(src);

	nlen = slen;
	for(i=0; i<slen; i++)
	{
		switch(src[i])
		{
			case '&': nlen += 4;
				break;
			case '<': nlen += 3;
				break;
			case '>': nlen += 3;
				break;
			case '\'': nlen += 5;
				break;
			case '"': nlen += 5;
				break;
		}
	}

	return nlen;
}

/**
 *
 */
int xml_unescape(char *src, int slen, char *dst, int dlen)
{
	int i,j;

	if(!src || !dst || dlen < slen)
		return -1;
	
	if(slen == -1)
		slen = strlen(src);
	
	if(!strchr(src, '&'))
	{
		memcpy(dst, src, slen);
		dst[slen] = 0;
		return slen;
	}

	for(i=j=0; i<slen; i++)
	{
		if(src[i] == '&')
		{
			i++;
			if(strncmp(&src[i], "amp;", 4) == 0)
			{
				dst[j] = '&';
				i += 3;
			}
			else if(strncmp(&src[i], "quot;", 5) == 0)
			{
				dst[j] = '"';
				i += 4;
			}
			else if(strncmp(&src[i], "apos;", 5) == 0)
			{
				dst[j] = '\'';
				i += 4;
			}
			else if(strncmp(&src[i], "lt;", 3) == 0)
			{
				dst[j] = '<';
				i += 2;
			}
			else if(strncmp(&src[i], "gt;", 3) == 0)
			{
				dst[j] = '>';
				i += 2;
			}
			else
			{
				dst[j] = src[--i];
			}
		}
		else
		{
			dst[j] = src[i];
		}
		j++;
	}
	dst[j] = '\0';

	return j;
}

/**************************    *************************/

/**
 *
 */
int j2s_parse_jmsgx(const char *msg, int len, jab_jmsg jmsg)
{
	char *p0, *p1;
	char *end;
	int pos, l1, attrf, sflag;
	enum {ST_BEGIN, ST_MESSAGE, ST_BODY, ST_ERROR} state;
	
	if(msg == NULL || jmsg == NULL)
		return -1;
	
	jmsg->body.len = 0;
	jmsg->error.len = 0;
	jmsg->errcode.len = 0;
	jmsg->from.len = 0;
	jmsg->to.len = 0;
	jmsg->id.len = 0;
	jmsg->type.len = 0;
	attrf = 1;
	
	end = (char*)(msg + len);
	p0 = (char*)msg;
	state = ST_BEGIN;
	
	
	while((p0) && (p0 <= end))
	{
		switch(state)
		{
			case ST_BEGIN:
					NEXT_CHAR_R(p0, '<', pos, end);
					SKIP_CHARS(p0, pos);
					if(!strncasecmp(p0, "<message ", 9))
					{
						DBG("JABBER: j2s_parse_jmsg: message\n");
						SKIP_CHARS_R(p0, 9, end);
						state = ST_MESSAGE;
					}
				break;
			case ST_MESSAGE:
					NEXT_ALCHARX(p0, pos, end);
					SKIP_CHARS(p0, pos);
					if(attrf)
					{
						if((jmsg->from.len == 0) 
							&& !strncasecmp(p0, "from", 4))
						{
							SKIP_CHARS_R(p0, 4, end);
							DBG("JABBER: j2s_parse_jmsg: from\n");
							EAT_SPACES_R(p0, end);
							if(*p0 == '=')
							{
								NEXT_CHAR_R(p0, '\'', pos, end);
								SKIP_CHARS(p0, pos+1);
								jmsg->from.s = p0;
								NEXT_CHAR_R(p0, '\'', pos, end);
								DBG("JABBER: j2s_parse_jmsg: from %d,%d\n",
									(int)(jmsg->from.s - msg), pos-1);
								jmsg->from.len = pos;
								SKIP_CHARS(p0, pos+1);
							}
							else
								return -2;
						}
						else if((jmsg->to.len == 0) 
								&& !strncasecmp(p0, "to", 2))
						{
							SKIP_CHARS(p0, 2);
							DBG("JABBER: j2s_parse_jmsg: to\n");
							EAT_SPACES_R(p0, end);
							if(*p0 == '=')
							{
								NEXT_CHAR_R(p0, '\'', pos, end);
								SKIP_CHARS(p0, pos+1);
								jmsg->to.s = p0;
								NEXT_CHAR_R(p0, '\'', pos, end);
								DBG("JABBER: j2s_parse_jmsg: to %d,%d\n", 
									(int)(jmsg->to.s - msg), pos-1);
								jmsg->to.len = pos;
								SKIP_CHARS(p0, pos+1);
							}
							else
								return -2;
						}
						else if((jmsg->id.len == 0) 
								&& !strncasecmp(p0, "id", 2))
						{
							SKIP_CHARS(p0, 2);
							DBG("JABBER: j2s_parse_jmsg: id\n");
							EAT_SPACES_R(p0, end);
							if(*p0 == '=')
							{
								NEXT_CHAR_R(p0, '\'', pos, end);
								SKIP_CHARS(p0, pos+1);
								//jmsg->to.s = p0;
								NEXT_CHAR_R(p0, '\'', pos, end);
								//jmsg->to.len = pos;
								SKIP_CHARS(p0, pos+1);
							}
							else
								return -2;
						}
						else if((jmsg->type.len == 0) 
								&& !strncasecmp(p0, "type", 4))
						{
							DBG("JABBER: j2s_parse_jmsg: type\n");
							SKIP_CHARS(p0, 4);
							EAT_SPACES_R(p0, end);
							if(*p0 == '=')
							{
								NEXT_CHAR_R(p0, '\'', pos, end);
								SKIP_CHARS(p0, pos+1);
								jmsg->type.s = p0;
								NEXT_CHAR_R(p0, '\'', pos, end);
								jmsg->type.len = pos;
								SKIP_CHARS(p0, pos+1);
							}
							else
								return -2;
						}
						else if(*p0 == '>')
						{
							SKIP_CHARS(p0, 1);
							attrf = 0;
							DBG("JABBER: j2s_parse_jmsg: message attributes \
								parsed\n");
						}
						else if(!strncasecmp(p0, "/>", 2))
						{
							return -2;
						}
						else
						{
							NEXT_CHAR_R(p0, '=', pos, end);
							DBG("JABBER: j2s_parse_jmsg: unknow message \
								attribute [%.*s]\n", pos, p0);
							SKIP_CHARS(p0, pos+1);
							NEXT_CHAR_R(p0, '\'', pos, end);
							SKIP_CHARS(p0, pos+1);
							NEXT_CHAR_R(p0, '\'', pos, end);
							SKIP_CHARS(p0, pos+1);
							DBG("JABBER: j2s_parse_jmsg: unknow message \
								attribute was skipped\n");
							// return -2;
						}
					}
					else
					{
						if(*p0 == '<')
						{
							if(!strncasecmp(p0, "</message", 9))
							{
								SKIP_CHARS(p0, 9);
								EAT_SPACES_R(p0, end);
								if(*p0 == '>')
									return 0;
								else
									return -2;
							}
							if(!strncasecmp(p0, "<body", 5))
							{
								state = ST_BODY;
								SKIP_CHARS(p0, 5);
							}
							else if(!strncasecmp(p0, "<error", 6))
							{
								state = ST_ERROR;
								SKIP_CHARS(p0, 6);
							}
							else
							{
								SKIP_CHARS(p0, 1);
								NEXT_TAGSEP_R(p0, pos, end);
								p1 = p0;
								l1 = pos;
								SKIP_CHARS(p0, pos);
								DBG("JABBER: j2s_parse_jmsg: ignoring tag \
									<%.*s>\n", l1, p1);
								IGNORE_TAG(p0, p1, l1, end, sflag);
								DBG("JABBER: j2s_parse_jmsg: tag <%.*s> \
									ignored\n", l1, p1);
							}
						}
						else
							return -2;
					}		
				break;
			case ST_BODY:
					DBG("JABBER: j2s_parse_jmsg: body\n");
					EAT_SPACES_R(p0, end);
					if(*p0 == '>')
					{
						SKIP_CHARS(p0, 1);
						jmsg->body.s = p0;
						NEXT_CHAR_R(p0, '<', pos, end);
						SKIP_CHARS(p0, pos+1);
						if(!strncasecmp(p0, "/body", 5))
						{
							DBG("JABBER: j2s_parse_jmsg: body %d,%d\n", 
								(int)(jmsg->body.s - msg), pos-1);
							jmsg->body.len = pos;
							SKIP_CHARS(p0, 5);
							EAT_SPACES_R(p0, end);
							if(*p0 =='>')
							{
								SKIP_CHARS(p0, 1);
								state = ST_MESSAGE;
							}
							else
								return -2;
						}
						else
							return -2;
					}
					else
						return -2;
				break;
			case ST_ERROR:
					DBG("JABBER: j2s_parse_jmsg: error\n");
					EAT_SPACES_R(p0, end);
					if(!strncasecmp(p0, "code", 4))
					{
						SKIP_CHARS(p0, 4);
						EAT_SPACES_R(p0, end);
						if(*p0 == '=')
						{
							NEXT_CHAR_R(p0, '\'', pos, end);
							SKIP_CHARS(p0, pos+1);
							jmsg->errcode.s = p0;
							NEXT_CHAR_R(p0, '\'', pos, end);
							jmsg->errcode.len = pos;
							SKIP_CHARS(p0, pos + 1);
						}
					}
					EAT_SPACES_R(p0, end);
					if(*p0 == '>')
					{
						SKIP_CHARS(p0, 1);
						jmsg->error.s = p0;
						NEXT_CHAR_R(p0, '<', pos, end);
						SKIP_CHARS(p0, pos+1);
						if(!strncasecmp(p0, "/error", 6))
						{
							jmsg->error.len = pos;
							SKIP_CHARS(p0, 6);
							EAT_SPACES_R(p0, end);
							if(*p0 =='>')
							{
								SKIP_CHARS(p0, 1);
								state = ST_MESSAGE;
							}
							else
								return -2;
						}
						else
							return -2;
					}
					else
						return -2;
				break;
		}
	}
	return 0;
}

/**
 *
 */
int j2s_parse_jmsg(const char *msg, int len, jab_jmsg jmsg)
{
	char *p0;
	char *end;
	int pos;
	
	if(msg == NULL || jmsg == NULL)
		return -1;
	jmsg->body.len = 0;
	jmsg->error.len = 0;
	jmsg->errcode.len = 0;
	jmsg->from.len = 0;
	jmsg->to.len = 0;
	jmsg->type.len = 0;
	
	end = (char*)(msg + len);
	p0 = (char*)msg;
	
	while((p0) && (p0 < end))
	{
		/* skip spaces and tabs if any */
		//EAT_SPACES(p0);
		NEXT_ALCHAR(p0, pos);
		SKIP_CHARS(p0, pos);
		
		if(!strncasecmp(p0, "message", 7))
		{
			DBG("JABBER: j2s_parse_jmsg: message\n");
			SKIP_CHARS(p0, 7);
			continue;
		}
		else if(!strncasecmp(p0, "body", 4) && jmsg->body.len==0)
		{
			SKIP_CHARS(p0, 4);
			DBG("JABBER: j2s_parse_jmsg: body\n");
			EAT_SPACES(p0);
			if(*p0 == '>')
			{
				SKIP_CHARS(p0, 1);
				jmsg->body.s = p0;
				NEXT_CHAR(p0, '<', pos);
				SKIP_CHARS(p0, pos+1);
				if(!strncasecmp(p0, "/body", 5))
				{
					DBG("JABBER: j2s_parse_jmsg: body %d,%d\n", 
						(int)(jmsg->body.s - msg), pos-1);
					jmsg->body.len = pos;
					SKIP_CHARS(p0, 5);
				}
				else
					SKIP_CHARS(p0, 1);
			}
		}
		else if(!strncasecmp(p0, "from", 4) && jmsg->from.len==0)
		{
			SKIP_CHARS(p0, 4);
			DBG("JABBER: j2s_parse_jmsg: from\n");
			EAT_SPACES(p0);
			if(*p0 == '=')
			{
				NEXT_CHAR(p0, '\'', pos);
				SKIP_CHARS(p0, pos+1);
				jmsg->from.s = p0;
				NEXT_CHAR(p0, '\'', pos);
				DBG("JABBER: j2s_parse_jmsg: from %d,%d\n", 
					(int)(jmsg->from.s - msg), pos-1);
				jmsg->from.len = pos;
				SKIP_CHARS(p0, pos);
			}
		}
		else if(!strncasecmp(p0, "to", 2) && jmsg->to.len==0)
		{
			SKIP_CHARS(p0, 2);
			DBG("JABBER: j2s_parse_jmsg: to\n");
			EAT_SPACES(p0);
			if(*p0 == '=')
			{
				NEXT_CHAR(p0, '\'', pos);
				SKIP_CHARS(p0, pos+1);
				jmsg->to.s = p0;
				NEXT_CHAR(p0, '\'', pos);
				jmsg->to.len = pos;
				SKIP_CHARS(p0, pos);
			}
		}
		else if(!strncasecmp(p0, "type", 4) && jmsg->type.len==0)
		{
			DBG("JABBER: j2s_parse_jmsg: type\n");
			SKIP_CHARS(p0, 4);
			EAT_SPACES(p0);
			if(*p0 == '=')
			{
				NEXT_CHAR(p0, '\'', pos);
				SKIP_CHARS(p0, pos+1);
				jmsg->type.s = p0;
				NEXT_CHAR(p0, '\'', pos);
				jmsg->type.len = pos;
				SKIP_CHARS(p0, pos);
			}
		}
		else if(!strncasecmp(p0, "error", 5) && jmsg->error.len==0)
		{
			SKIP_CHARS(p0, 5);
			DBG("JABBER: j2s_parse_jmsg: error\n");
			EAT_SPACES(p0);
			if(!strncasecmp(p0, "code", 4))
			{
				SKIP_CHARS(p0, 4);
				EAT_SPACES(p0);
				if(*p0 == '=')
				{
					NEXT_CHAR(p0, '\'', pos);
					SKIP_CHARS(p0, pos+1);
					jmsg->errcode.s = p0;
					NEXT_CHAR(p0, '\'', pos);
					jmsg->errcode.len = pos;
					SKIP_CHARS(p0, pos);
				}
			}
			
			NEXT_CHAR(p0, '>', pos);
			SKIP_CHARS(p0, pos+1);
			jmsg->error.s = p0;
			NEXT_CHAR(p0, '<', pos);
			SKIP_CHARS(p0, pos+1);
			if(!strncasecmp(p0, "/error", 6))
			{
				jmsg->error.len = pos;
				SKIP_CHARS(p0, 6);
			}
			else
				SKIP_CHARS(p0, 1);
		}

		if(p0 < end)
			SKIP_CHARS(p0, 1);
		else
			break;
	}
	return 0;
}

