/**
 * $Id$
 *
 * XLOG module
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* History:
 * --------
 * 2004-10-20 - added header name specifier (ramona)
 * 
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h" 
#include "../../trim.h" 

#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"

#include "xl_lib.h"

static str str_null = { "<null>", 6 };
static str str_per = { "%", 1 };

int msg_id = 0;
time_t msg_tm = 0;
int cld_pid = 0;

static int xl_get_null(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->s = str_null.s;
	res->len = str_null.len;
	return 0;
}

static int xl_get_percent(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->s = str_per.s;
	res->len = str_per.len;
	return 0;
}

static int xl_get_pid(struct sip_msg *msg, str *res, str *hp, int hi)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if(cld_pid == 0)
		cld_pid = (int)getpid();
	ch = int2str(cld_pid, &l);

	res->s = ch;
	res->len = l;

	return 0;
}

static int xl_get_times(struct sip_msg *msg, str *res, str *hp, int hi)
{
	int l = 0;
	char *ch = NULL;
		
	if(msg==NULL || res==NULL)
		return -1;

	if(msg_id != msg->id)
	{
		msg_tm = time(NULL);
		msg_id = msg->id;
	}
	ch = int2str(msg_tm, &l);
	
	res->s = ch;
	res->len = l;

	return 0;
}
static int xl_get_timef(struct sip_msg *msg, str *res, str *hp, int hi)
{
	char *ch = NULL;
	
	if(msg==NULL || res==NULL)
		return -1;
	if(msg_id != msg->id)
	{
		msg_tm = time(NULL);
		msg_id = msg->id;
	}
	
	ch = ctime(&msg_tm);
	
	res->s = ch;
	res->len = strlen(ch)-1;

	return 0;
}

static int xl_get_msgid(struct sip_msg *msg, str *res, str *hp, int hi)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	ch = int2str(msg->id, &l);
	res->s = ch;
	res->len = l;

	return 0;
}

static int xl_get_method(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REQUEST)
	{
		res->s = msg->first_line.u.request.method.s;
		res->len = msg->first_line.u.request.method.len;
	}
	else
		return xl_get_null(msg, res, hp, hi);
	
	return 0;
}

static int xl_get_status(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
	{
		res->s = msg->first_line.u.reply.status.s;
		res->len = msg->first_line.u.reply.status.len;		
	}
	else
		return xl_get_null(msg, res, hp, hi);
	
	return 0;
}

static int xl_get_reason(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
	{
		res->s = msg->first_line.u.reply.reason.s;
		res->len = msg->first_line.u.reply.reason.len;		
	}
	else
		return xl_get_null(msg, res, hp, hi);
	
	return 0;
}

static int xl_get_ruri(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have a ruri */
		return xl_get_null(msg, res, hp, hi);

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0)
	{
		LOG(L_ERR, "XLOG: xl_get_ruri: ERROR while parsing the R-URI\n");
		return xl_get_null(msg, res, hp, hi);
	}
	
	res->s = msg->parsed_uri.user.len>0 ? msg->parsed_uri.user.s :
										  msg->parsed_uri.host.s;
	res->len = msg->parsed_uri.user.len+
				msg->parsed_uri.passwd.len+
				msg->parsed_uri.host.len+
				msg->parsed_uri.port.len+
				msg->parsed_uri.params.len+
				msg->parsed_uri.headers.len+
				(msg->parsed_uri.user.len>0?1:0)+
				(msg->parsed_uri.passwd.len>0?1:0)+
				(msg->parsed_uri.port.len>0?1:0)+
				(msg->parsed_uri.params.len>0?1:0)+
				(msg->parsed_uri.headers.len>0?1:0);
	return 0;
}

static int xl_get_contact(struct sip_msg* msg, str* res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->contact==NULL && parse_headers(msg, HDR_CONTACT, 0)==-1) 
	{
		DBG("XLOG: xl_get_contact: no contact header\n");
		return xl_get_null(msg, res, hp, hi);
	}
	
	if(!msg->contact || !msg->contact->body.s || msg->contact->body.len<=0)
    {
		DBG("XLOG: xl_get_contact: no contact header!\n");
		return xl_get_null(msg, res, hp, hi);
	}
	
	res->s = msg->contact->body.s;
	res->len = msg->contact->body.len;

	
//	res->s = ((struct to_body*)msg->contact->parsed)->uri.s;
//	res->len = ((struct to_body*)msg->contact->parsed)->uri.len;

	return 0;
}


static int xl_get_from(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_from_header(msg)==-1)
	{
		LOG(L_ERR, "XLOG: xl_get_from: ERROR cannot parse FROM header\n");
		return xl_get_null(msg, res, hp, hi);
	}
	
	if(msg->from==NULL || get_from(msg)==NULL)
		return xl_get_null(msg, res, hp, hi);

	res->s = get_from(msg)->uri.s;
	res->len = get_from(msg)->uri.len; 
	
	return 0;
}

static int xl_get_from_tag(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_from_header(msg)==-1)
	{
		LOG(L_ERR, "XLOG: xl_get_from: ERROR cannot parse FROM header\n");
		return xl_get_null(msg, res, hp, hi);
	}
	
	
	if(msg->from==NULL || get_from(msg)==NULL 
			|| get_from(msg)->tag_value.s==NULL)
		return xl_get_null(msg, res, hp, hi);

	res->s = get_from(msg)->tag_value.s;
	res->len = get_from(msg)->tag_value.len; 

	return 0;
}


static int xl_get_to(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->to==NULL && parse_headers(msg, HDR_TO, 0)==-1)
	{
		LOG(L_ERR, "XLOG: xl_get_to: ERROR cannot parse TO header\n");
		return xl_get_null(msg, res, hp, hi);
	}
	if(msg->to==NULL || get_to(msg)==NULL)
		return xl_get_null(msg, res, hp, hi);

	res->s = get_to(msg)->uri.s;
	res->len = get_to(msg)->uri.len; 
	
	return 0;
}

static int xl_get_to_tag(struct sip_msg* msg, str* res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->to==NULL && ((parse_headers(msg, HDR_TO, 0)==-1) || 
				(msg->to==NULL)) )
	{
		LOG(L_ERR, "XLOG: xl_get_to: ERROR cannot parse TO header\n");
		return xl_get_null(msg, res, hp, hi);
	}
	
	if (get_to(msg)->tag_value.len <= 0) 
		return xl_get_null(msg, res, hp, hi);
	
	res->s = get_to(msg)->tag_value.s;
	res->len = get_to(msg)->tag_value.len;

	return 0;
}

static int xl_get_cseq(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ, 0)==-1) || 
				(msg->cseq==NULL)) )
	{
		LOG(L_ERR, "XLOG: xl_get_cseq: ERROR cannot parse CSEQ header\n");
		return xl_get_null(msg, res, hp, hi);
	}

	res->s = get_cseq(msg)->number.s;
	res->len = get_cseq(msg)->number.len;

	return 0;
}

static int xl_get_msg_buf(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	res->s = msg->buf;
	res->len = msg->len;

	return 0;
}

static int xl_get_msg_len(struct sip_msg *msg, str *res, str *hp, int hi)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	ch = int2str(msg->len, &l);
	res->s = ch;
	res->len = l;

	return 0;
}

static int xl_get_flags(struct sip_msg *msg, str *res, str *hp, int hi)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	ch = int2str(msg->flags, &l);
	res->s = ch;
	res->len = l;

	return 0;
}

static int xl_get_callid(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL)
		return -1;
	
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID, 0)==-1) ||
				(msg->callid==NULL)) )
	{
		LOG(L_ERR, "XLOG: xl_get_cseq: ERROR cannot parse Call-Id header\n");
		return xl_get_null(msg, res, hp, hi);
	}

	res->s = msg->callid->body.s;
	res->len = msg->callid->body.len;
	trim(res);

	return 0;
}

static int xl_get_srcip(struct sip_msg *msg, str *res, str *hp, int hi)
{
    if(msg==NULL || res==NULL)
        return -1;

    res->s = ip_addr2a(&msg->rcv.src_ip);
    res->len = strlen(res->s);
   
    return 0;
}

static int xl_get_useragent(struct sip_msg *msg, str *res, str *hp, int hi)
{
	if(msg==NULL || res==NULL) 
		return -1;
	if(msg->user_agent==NULL && ((parse_headers(msg, HDR_USERAGENT, 0)==-1)
			 || (msg->user_agent==NULL)))
	{
		LOG(L_ERR, "XLOG: xl_get_useragent: ERROR cannot parse User-Agent header\n");
		return xl_get_null(msg, res, hp, hi);
	}
	
	res->s = msg->user_agent->body.s;
	res->len = msg->user_agent->body.len;
	trim(res);
	
	return 0;
}

static int xl_get_header(struct sip_msg *msg, str *res, str *hp, int hi)
{
	struct hdr_field *hf, *hf0;
	
	if(msg==NULL || res==NULL)
		return -1;

	if(hp==NULL || hp->s==NULL || hp->len<=0)
		return xl_get_null(msg, res, hp, hi);
	
	hf0 = NULL;

	/* we need to be sure we have parsed all headers */
	parse_headers(msg, HDR_EOH, 0);
	for (hf=msg->headers; hf; hf=hf->next)
	{
		if (hf->name.len!=hp->len)
			continue;
		if (strncasecmp(hf->name.s, hp->s, hf->name.len)!=0)
			continue;
		
		hf0 = hf;
		if(hi==0)
			goto done;
		if(hi>0)
			hi--;
	}
	
done:
	if(hf0==NULL || hi>0)
		return xl_get_null(msg, res, hp, hi);
	res->s = hf0->body.s;
	res->len = hf0->body.len;
	trim(res);
	return 0;
}

int xl_parse_format(char *s, xl_elog_p *el)
{
	char *p;
	int n = 0;
	xl_elog_p e, e0;
	
	if(s==NULL || el==NULL)
		return -1;

	DBG("XLOG: xl_parse_format: parsing [%s]\n", s);
	
	p = s;
	*el = NULL;
	e = e0 = NULL;

	while(*p)
	{
		e0 = e;
		e = pkg_malloc(sizeof(xl_elog_t));
		if(!e)
			goto error;
		memset(e, 0, sizeof(xl_elog_t));
		n++;
		if(*el == NULL)
			*el = e;
		if(e0)
			e0->next = e;
	
		e->text.s = p;
		while(*p && *p!='%')
			p++;
		e->text.len = p - e->text.s;
		if(*p == '\0')
			break;

		p++;
		switch(*p)
		{
			case 'c':
				p++;
				switch(*p)
				{
					case 'i':
						e->itf = xl_get_callid;
					break;
					case 's':
						e->itf = xl_get_cseq;
					break;
					case 't':
						e->itf = xl_get_contact;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case 'f':
				p++;
				switch(*p)
				{
					case 'u':
						e->itf = xl_get_from;
					break;
					case 't':
						e->itf = xl_get_from_tag;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case 'i':
				p++;
				switch(*p)
				{
					case 's':
						e->itf = xl_get_srcip;
					break;
					default:
					e->itf = xl_get_null; 			
				}
			break;
			case 'm':
				p++;
				switch(*p)
				{
					case 'b':
						e->itf = xl_get_msg_buf;
					break;
					case 'f':
						e->itf = xl_get_flags;
					break;
					case 'i':
						e->itf = xl_get_msgid;
					break;
					case 'l':
						e->itf = xl_get_msg_len;
					break;
					default:
						e->itf = xl_get_null;
				}
				break;
			case 'p':
				p++;
				switch(*p)
				{
					case 'p':
						e->itf = xl_get_pid;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case 'r':
				p++;
				switch(*p)
				{
					case 'm':
						e->itf = xl_get_method;
					break;
					case 'u':
						e->itf = xl_get_ruri;
					break;
					case 's':
						e->itf = xl_get_status;
					break;
					case 'r':
						e->itf = xl_get_reason;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case 't':
				p++;
				switch(*p)
				{
					case 'u':
						e->itf = xl_get_to;
					break;
					case 't':
						e->itf = xl_get_to_tag;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case 'T':
				p++;
				switch(*p)
				{
					case 's':
						e->itf = xl_get_times;
					break;
					case 'f':
						e->itf = xl_get_timef;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case 'u':
				p++;
				switch(*p)
				{
					case 'a':
						e->itf = xl_get_useragent;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case '{':
				p++;
				/* we expect a letter */
				if((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z'))
				{
					LOG(L_ERR, "xlog: xl_parse_format: error parsing format"
						" [%s] pos [%d]\n", s, (int)(p-s));
					goto error;
				}
				e->hparam.s = p;
				while(*p && *p!='}' && *p!='[')
					p++;
				if(*p == '\0')
				{
					LOG(L_ERR, "xlog: xl_parse_format: error parsing format"
						" [%s] expecting '}' after position [%d]\n", s,
						(int)(e->hparam.s-s));
					goto error;
				}

				e->hparam.len = p - e->hparam.s;
				/* check if we have index */
				if(*p == '[')
				{
					p++;
					if(*p=='-')
					{
						p++;
						if(*p!='1')
						{
							LOG(L_ERR, "xlog: xl_parse_format: error"
								" parsing format [%s] -- only -1 is accepted"
								" as a negative index\n", s);
								goto error;
						}
						e->hindex = -1;
						p++;
					}
					else
					{
						while(*p>='0' && *p<='9')
						{
							e->hindex = e->hindex * 10 + *p - '0';
							p++;
						}
					}
					if(*p != ']')
					{
						LOG(L_ERR, "xlog: xl_parse_format: error parsing format"
							" [%s] expecting ']' after position [%d]\n", s,
							(int)(e->hparam.s - s + e->hparam.len));
						goto error;
					}
					p++;
				}
				if(*p != '}')
				{
					LOG(L_ERR, "xlog: xl_parse_format: error parsing format"
						" [%s] expecting '}' after position [%d]!\n", s,
						(int)(e->hparam.s-s));
					goto error;
				}
				e->itf = xl_get_header;

				DBG("xlog: xl_parse_format: header name [%.*s] index [%d]\n",
						e->hparam.len, e->hparam.s, e->hindex);
			break;
			case '%':
				e->itf = xl_get_percent;
			break;
			default:
				e->itf = xl_get_null;
		}

		if(*p == '\0')
			break;
		p++;
	}
	DBG("XLOG: xl_parse_format: format parsed OK: [%d] items\n", n);

	return 0;

error:
	xl_elog_free_all(*el);
	*el = NULL;
	return -1;
}

int xl_print_log(struct sip_msg* msg, xl_elog_p log, char *buf, int *len)
{
	int n;
	str tok;
	xl_elog_p it;
	char *cur;
	
	if(msg==NULL || log==NULL || buf==NULL || len==NULL)
		return -1;

	if(*len <= 0)
		return -1;

	*buf = '\0';
	cur = buf;
	
	n = 0;
	for (it=log; it; it=it->next)
	{
		/* put the text */
		if(it->text.s && it->text.len>0)
		{
			if(n+it->text.len < *len)
			{
				memcpy(cur, it->text.s, it->text.len);
				n += it->text.len;
				cur += it->text.len;
			}
			else
				goto overflow;
		}
		/* put the value of the specifier */
		if(it->itf && !((*it->itf)(msg, &tok, &(it->hparam), it->hindex)))
		{
			if(n+tok.len < *len)
			{
				memcpy(cur, tok.s, tok.len);
				n += tok.len;
				cur += tok.len;
			}
			else
				goto overflow;
		}
	}
	goto done;
	
overflow:
	LOG(L_ERR,
		"XLOG:xl_print_log: buffer overflow -- increase the buffer size...\n");
	return -1;

done:
	DBG("XLOG: xl_print_log: final buffer length %d\n", n);
	*cur = '\0';
	*len = n;
	return 0;
}

int xl_elog_free_all(xl_elog_p log)
{
	xl_elog_p t;
	while(log)
	{
		t = log;
		log = log->next;
		pkg_free(t);
	}
	return 0;
}

