
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
#include "../im/im_funcs.h"

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
 * apostrphes escaping
 * - src: source buffer
 * - slen: length of source buffer
 * - dst: destination buffer
 * - dlen: max length of destination buffer
 * #return: destination length => OK; -1 => error
 */
int apo_escape(char* src, int slen, char* dst, int dlen)
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
			case 'i':
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
 * parse content of Content-Type header - only value of C-T at this moment
 * - src: pointer to C-T content
 * - len: length of src
 * - ctype: parsed C-T
 * - flag: what to parse - bit mask of CT_TYPE, CT_CHARSET, CT_MSGR
 *
 * #return: 0 OK ; -1 error
  */
int parse_content_type(char* src, int len, t_content_type* ctype, int flag)
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

/**
 * send a MESSAGE using IM library - deprecated - used t_uac instead
 */
int m_send_message(int mid, str *uri, str *to, str *from, str *contact,
				str *ctype, str *msg)
{
	static char buf[2048];
	static int call_id = 0x4f8a1b49;
	//static int cseq_nr=1;
	union sockaddr_union to_addr;
	struct socket_info* send_sock;
	int buf_len;

	buf_len = sprintf(buf,
		"MESSAGE %.*s SIP/2.0%s"
		"Via: SIP/2.0/UDP %.*s:9%s"
		"From: %.*s;tag=%s%08X;%s"
		"To: %.*s%s"
		"Call-ID: d2d4%04d-e803-%08X-b036-%X@%.*s%s"
		"CSeq: %d MESSAGE%s"
		"%s%.*s%s"
		"Content-Type: %.*s; charset=UTF-8%s"
		"Content-Length: %d%s"
		"%s"
		"%.*s", /*msg*/
		uri->len,uri->s,CRLF,
		sock_info[0].name.len,sock_info[0].name.s,CRLF,
		from->len,from->s,MSILO_TAG,mid,CRLF,
		to->len,to->s,CRLF,
		pids?pids[process_no]:0,rand(),call_id++,
			sock_info[0].address_str.len,sock_info[0].address_str.s,CRLF,
		1/*cseq_nr++*/,CRLF,
		contact->s?"Contact: ":"",contact->len,contact->s,contact->s?CRLF:"",
		ctype->len,ctype->s,CRLF,
		msg->len,CRLF,
		CRLF,
		msg->len,msg->s);

	if (buf_len<=0)
		goto error;
	
	DBG("MSILO: m_send_message: -----\n%.*s\n", buf_len, buf);
	
	if (set_sock_struct( &to_addr, uri)==-1)
		goto error;

	send_sock = get_send_socket(&to_addr);
	if (send_sock==0)
		goto error;

	udp_send(send_sock,buf,buf_len,&(to_addr));

	return 1;
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
int m_build_headers(str *buf, str ctype)
{
	char *p;
	if(!buf || !buf->s || buf->len <= 0 || ctype.len < 0
			|| buf->len < ctype.len+14+CRLF_LEN)
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
int m_build_body(str *body, int date, str msg)
{
	char *p;
	
	if(!body || !(body->s) || body->len <= 0 ||
			date < 0 || msg.len < 0 || (28+msg.len > body->len) )
		goto error;
	
	p = body->s;

	*p++ = '[';
	
	strncpy(p, ctime((const time_t*)(&date)), 25);
	p += 25;

	/**
	if(from.len > 0)
	{
		*p++ = ' ';
		strncpy(p, from.s, from.len);
		p += from.len;
	}
	**/
	
	*p++ = ']';
	*p++ = ' ';
	
	if(msg.len > 0)
	{
		strncpy(p, msg.s, msg.len);
		p += msg.len;
	}

	body->len = p - body->s;
	
	return 0;
error:
	return -1;
}

