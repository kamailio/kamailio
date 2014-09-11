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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* History:
 * --------
 * 2004-10-20 - added header name specifier (ramona)
 * 2005-07-04 - added color printing support via escape sequesnces
 *              contributed by Ingo Wolfsberger (ramona)
 * 2005-12-23 - parts from private branch merged (mma)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../trim.h"
#include "../../dset.h"
#include "../../resolve.h"
#include "../../qvalue.h"
#include "../../usr_avp.h"

#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_hname2.h"
#include "../../parser/parse_refer_to.h"

#include "xp_lib.h"
#include <arpa/inet.h>  // inet_ntop

#include "../../select.h"

static str str_null  = STR_STATIC_INIT("<null>");
static str str_empty = STR_STATIC_INIT("");
static str str_per   = STR_STATIC_INIT("%");
static str str_hostname, str_domainname, str_fullname, str_ipaddr;

enum xl_host_t {
	XL_HOST_NULL,
	XL_HOST_NAME,
	XL_HOST_DOMAIN,
	XL_HOST_FULL,
	XL_HOST_IPADDR
};

int msg_id = 0;
time_t msg_tm = 0;
int cld_pid = 0;

#define XLOG_FIELD_DELIM ", "
#define XLOG_FIELD_DELIM_LEN (sizeof(XLOG_FIELD_DELIM) - 1)

#define UNIQUE_ID_LEN 16
static char UNIQUE_ID[UNIQUE_ID_LEN];

#define LOCAL_BUF_SIZE	511
static char local_buf[LOCAL_BUF_SIZE+1];

str* xl_get_nulstr()
{
	return &str_null;
}

static int xl_get_null(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->s = str_null.s;
	res->len = str_null.len;
	return 0;
}

static int xl_get_empty(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->s = str_empty.s;
	res->len = str_empty.len;
	return 0;
}

static int xl_get_percent(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->s = str_per.s;
	res->len = str_per.len;
	return 0;
}

static int xl_get_pid(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if(cld_pid == 0)
		cld_pid = (int)getpid();
	ch = int2str_base_0pad(cld_pid, &l, hi, hi==10?0:8);

	res->s = ch;
	res->len = l;

	return 0;
}

static int xl_get_times(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if(msg_id != msg->id || msg_tm==0)
	{
		msg_tm = time(NULL);
		msg_id = msg->id;
	}
	ch = int2str_base_0pad(msg_tm, &l, hi, hi==10?0:8);

	res->s = ch;
	res->len = l;

	return 0;
}
static int xl_get_timef(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;
	if(msg_id != msg->id || msg_tm==0)
	{
		msg_tm = time(NULL);
		msg_id = msg->id;
	}

	ch = ctime(&msg_tm);

	res->s = ch;
	res->len = strlen(ch)-1;

	return 0;
}

static int xl_get_msgid(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	ch = int2str_base_0pad(msg->id, &l, hi, hi==10?0:8);
	res->s = ch;
	res->len = l;

	return 0;
}

static int xl_get_method(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REQUEST)
	{
		res->s = msg->first_line.u.request.method.s;
		res->len = msg->first_line.u.request.method.len;
	}
	else
		return xl_get_null(msg, res, hp, hi, hf);

	return 0;
}

static int xl_get_status(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
	{
		res->s = msg->first_line.u.reply.status.s;
		res->len = msg->first_line.u.reply.status.len;
	}
	else
		return xl_get_null(msg, res, hp, hi, hf);

	return 0;
}

static int xl_get_reason(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
	{
		res->s = msg->first_line.u.reply.reason.s;
		res->len = msg->first_line.u.reply.reason.len;
	}
	else
		return xl_get_null(msg, res, hp, hi, hf);

	return 0;
}

static int xl_get_ruri(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have a ruri */
		return xl_get_null(msg, res, hp, hi, hf);

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0)
	{
		LOG(L_ERR, "XLOG: xl_get_ruri: ERROR while parsing the R-URI\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	if (msg->new_uri.s!=NULL)
	{
		res->s   = msg->new_uri.s;
		res->len = msg->new_uri.len;
	} else {
		res->s   = msg->first_line.u.request.uri.s;
		res->len = msg->first_line.u.request.uri.len;
	}

	return 0;
}

static int xl_get_contact(struct sip_msg* msg, str* res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->contact==NULL && parse_headers(msg, HDR_CONTACT_F, 0)==-1)
	{
		DBG("XLOG: xl_get_contact: no contact header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	if(!msg->contact || !msg->contact->body.s || msg->contact->body.len<=0)
    {
		DBG("XLOG: xl_get_contact: no contact header!\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	res->s = msg->contact->body.s;
	res->len = msg->contact->body.len;


//	res->s = ((struct to_body*)msg->contact->parsed)->uri.s;
//	res->len = ((struct to_body*)msg->contact->parsed)->uri.len;

	return 0;
}


static int xl_get_from(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_from_header(msg)==-1)
	{
		LOG(L_ERR, "XLOG: xl_get_from: ERROR cannot parse FROM header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	if(msg->from==NULL || get_from(msg)==NULL)
		return xl_get_null(msg, res, hp, hi, hf);

	res->s = get_from(msg)->uri.s;
	res->len = get_from(msg)->uri.len;

	return 0;
}

static int xl_get_from_tag(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_from_header(msg)==-1)
	{
		LOG(L_ERR, "XLOG: xl_get_from: ERROR cannot parse FROM header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}


	if(msg->from==NULL || get_from(msg)==NULL
			|| get_from(msg)->tag_value.s==NULL)
		return xl_get_null(msg, res, hp, hi, hf);

	res->s = get_from(msg)->tag_value.s;
	res->len = get_from(msg)->tag_value.len;

	return 0;
}


static int xl_get_to(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->to==NULL && parse_headers(msg, HDR_TO_F, 0)==-1)
	{
		LOG(L_ERR, "XLOG: xl_get_to: ERROR cannot parse TO header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}
	if(msg->to==NULL || get_to(msg)==NULL)
		return xl_get_null(msg, res, hp, hi, hf);

	res->s = get_to(msg)->uri.s;
	res->len = get_to(msg)->uri.len;

	return 0;
}

static int xl_get_to_tag(struct sip_msg* msg, str* res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->to==NULL && ((parse_headers(msg, HDR_TO_F, 0)==-1) ||
				(msg->to==NULL)) )
	{
		LOG(L_ERR, "XLOG: xl_get_to: ERROR cannot parse TO header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	if (get_to(msg)->tag_value.len <= 0)
		return xl_get_null(msg, res, hp, hi, hf);

	res->s = get_to(msg)->tag_value.s;
	res->len = get_to(msg)->tag_value.len;

	return 0;
}

static int xl_get_cseq(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1) ||
				(msg->cseq==NULL)) )
	{
		LOG(L_ERR, "XLOG: xl_get_cseq: ERROR cannot parse CSEQ header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	res->s = get_cseq(msg)->number.s;
	res->len = get_cseq(msg)->number.len;

	return 0;
}

static int xl_get_msg_buf(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->s = msg->buf;
	res->len = msg->len;

	return 0;
}

static int xl_get_msg_len(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
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

static int xl_get_flags(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
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

static int xl_get_callid(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID_F, 0)==-1) ||
				(msg->callid==NULL)) )
	{
		LOG(L_ERR, "XLOG: xl_get_callid: ERROR cannot parse Call-Id header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	res->s = msg->callid->body.s;
	res->len = msg->callid->body.len;
	trim(res);

	return 0;
}

static int xl_get_srcip(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	res->s = ip_addr2a(&msg->rcv.src_ip);
	res->len = strlen(res->s);

	return 0;
}

static int xl_get_srcport(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	ch = int2str(msg->rcv.src_port, &l);
	res->s = ch;
	res->len = l;

	return 0;
}

static int xl_get_rcvip(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->rcv.bind_address==NULL
			|| msg->rcv.bind_address->address_str.s==NULL)
		return xl_get_null(msg, res, hp, hi, hf);

	res->s   = msg->rcv.bind_address->address_str.s;
	res->len = msg->rcv.bind_address->address_str.len;

	return 0;
}

static int xl_get_rcvport(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(msg->rcv.bind_address==NULL
			|| msg->rcv.bind_address->port_no_str.s==NULL)
		return xl_get_null(msg, res, hp, hi, hf);

	res->s   = msg->rcv.bind_address->port_no_str.s;
	res->len = msg->rcv.bind_address->port_no_str.len;

	return 0;
}

static int xl_get_useragent(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;
	if(msg->user_agent==NULL && ((parse_headers(msg, HDR_USERAGENT_F, 0)==-1)
			 || (msg->user_agent==NULL)))
	{
		DBG("XLOG: xl_get_useragent: User-Agent header not found\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	res->s = msg->user_agent->body.s;
	res->len = msg->user_agent->body.len;
	trim(res);

	return 0;
}

static int xl_get_refer_to(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	if(msg==NULL || res==NULL)
		return -1;

	if(parse_refer_to_header(msg)==-1)
	{
		LOG(L_ERR, "XLOG: xl_get_refer_to: ERROR cannot parse Refer-To header\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	if(msg->refer_to==NULL || get_refer_to(msg)==NULL)
		return xl_get_null(msg, res, hp, hi, hf);

	res->s = get_refer_to(msg)->uri.s;
	res->len = get_refer_to(msg)->uri.len;

	return 0;
}

static int xl_get_dset(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
    if(msg==NULL || res==NULL)
	return -1;

    res->s = print_dset(msg, &res->len);

    if ((res->s) == NULL) return xl_get_null(msg, res, hp, hi, hf);

    res->len -= CRLF_LEN;

    return 0;
}

#define COL_BUF 10

#define append_sstring(p, end, str) \
        do{\
                if ((p)+(sizeof(str)-1)<=(end)){\
                        memcpy((p), str, sizeof(str)-1); \
                        (p)+=sizeof(str)-1; \
                }else{ \
                        /* overflow */ \
                        LOG(L_ERR, "XLOG: append_sstring overflow\n"); \
                        goto error;\
                } \
        } while(0)

static int xl_get_color(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	static char color[COL_BUF];
	char* p;
	char* end;

	p = color;
	end = p + COL_BUF;

	/* excape sequenz */
	append_sstring(p, end, "\033[");

	if(hp->s[0]!='_')
	{
		if (islower((unsigned char)hp->s[0]))
		{
			/* normal font */
			append_sstring(p, end, "0;");
		} else {
			/* bold font */
			append_sstring(p, end, "1;");
			hp->s[0] += 32;
		}
	}

	/* foreground */
	switch(hp->s[0])
	{
		case 'x':
			append_sstring(p, end, "39;");
		break;
		case 's':
			append_sstring(p, end, "30;");
		break;
		case 'r':
			append_sstring(p, end, "31;");
		break;
		case 'g':
			append_sstring(p, end, "32;");
		break;
		case 'y':
			append_sstring(p, end, "33;");
		break;
		case 'b':
			append_sstring(p, end, "34;");
		break;
		case 'p':
			append_sstring(p, end, "35;");
		break;
		case 'c':
			append_sstring(p, end, "36;");
		break;
		case 'w':
			append_sstring(p, end, "37;");
		break;
		default:
			LOG(L_ERR, "XLOG: exit foreground\n");
			return xl_get_empty(msg, res, hp, hi, hf);
	}

	/* background */
	switch(hp->s[1])
	{
		case 'x':
			append_sstring(p, end, "49");
		break;
		case 's':
			append_sstring(p, end, "40");
		break;
		case 'r':
			append_sstring(p, end, "41");
		break;
		case 'g':
			append_sstring(p, end, "42");
		break;
		case 'y':
			append_sstring(p, end, "43");
		break;
		case 'b':
			append_sstring(p, end, "44");
		break;
		case 'p':
			append_sstring(p, end, "45");
		break;
		case 'c':
			append_sstring(p, end, "46");
		break;
		case 'w':
			append_sstring(p, end, "47");
		break;
		default:
			LOG(L_ERR, "XLOG: exit background\n");
			return xl_get_empty(msg, res, hp, hi, hf);
	}

	/* end */
	append_sstring(p, end, "m");

	res->s = color;
	res->len = p-color;
	return 0;

error:
	return -1;
}

static int xl_get_branch(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	str branch;
	qvalue_t q;

	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
		return xl_get_null(msg, res, hp, hi, hf);


	init_branch_iterator();
	branch.s = next_branch(&branch.len, &q, 0, 0, 0, 0, 0, 0, 0);
	if (!branch.s) {
		return xl_get_null(msg, res, hp, hi, hf);
	}

	res->s = branch.s;
	res->len = branch.len;

	return 0;
}

#define Q_PARAM ">;q="
#define Q_PARAM_LEN (sizeof(Q_PARAM) - 1)

static int xl_get_branches(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	str uri;
	qvalue_t q;
	int len, cnt, i;
	unsigned int qlen;
	char *p, *qbuf;

	if(msg==NULL || res==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
		return xl_get_null(msg, res, hp, hi, hf);

	cnt = len = 0;

	init_branch_iterator();
	while ((uri.s = next_branch(&uri.len, &q, 0, 0, 0, 0, 0, 0, 0)))
	{
		cnt++;
		len += uri.len;
		if (q != Q_UNSPECIFIED)
		{
			len += 1 + Q_PARAM_LEN + len_q(q);
		}
	}

	if (cnt == 0)
		return xl_get_empty(msg, res, hp, hi, hf);

	len += (cnt - 1) * XLOG_FIELD_DELIM_LEN;

	if (len + 1 > LOCAL_BUF_SIZE)
	{
		LOG(L_ERR, "ERROR:xl_get_branches: local buffer length exceeded\n");
		return xl_get_null(msg, res, hp, hi, hf);
	}

	i = 0;
	p = local_buf;

	init_branch_iterator();
	while ((uri.s = next_branch(&uri.len, &q, 0, 0, 0, 0, 0, 0, 0)))
	{
		if (i)
		{
			memcpy(p, XLOG_FIELD_DELIM, XLOG_FIELD_DELIM_LEN);
			p += XLOG_FIELD_DELIM_LEN;
		}

		if (q != Q_UNSPECIFIED)
		{
			*p++ = '<';
		}

		memcpy(p, uri.s, uri.len);
		p += uri.len;
		if (q != Q_UNSPECIFIED)
		{
			memcpy(p, Q_PARAM, Q_PARAM_LEN);
			p += Q_PARAM_LEN;

			qbuf = q2str(q, &qlen);
			memcpy(p, qbuf, qlen);
			p += qlen;
		}
		i++;
	}

	res->s = &(local_buf[0]);
	res->len = len;

	return 0;
}

static int xl_get_nexthop(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	*res=*GET_NEXT_HOP(msg);
	return 0;
}

#define XLOG_PRINT_ALL	-2
#define XLOG_PRINT_LAST	-1

static int xl_get_header(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	struct hdr_field *hdrf, *hdrf0;
	char *p;

	if(msg==NULL || res==NULL)
		return -1;

	if(hp==NULL || hp->len==0)
		return xl_get_null(msg, res, hp, hi, hf);

	hdrf0 = NULL;
	p = local_buf;

	/* we need to be sure we have parsed all headers */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hdrf=msg->headers; hdrf; hdrf=hdrf->next)
	{
		if(hp->s==NULL)
		{
			if (hp->len!=hdrf->type)
				continue;
		} else {
			if (hdrf->name.len!=hp->len)
				continue;
			if (strncasecmp(hdrf->name.s, hp->s, hdrf->name.len)!=0)
				continue;
		}

		hdrf0 = hdrf;
		if(hi==XLOG_PRINT_ALL)
		{
			if(p!=local_buf)
			{
				if(p-local_buf+XLOG_FIELD_DELIM_LEN+1>LOCAL_BUF_SIZE)
				{
					LOG(L_ERR,
						"ERROR:xl_get_header: local buffer length exceeded\n");
					return xl_get_null(msg, res, hp, hi, hf);
				}
				memcpy(p, XLOG_FIELD_DELIM, XLOG_FIELD_DELIM_LEN);
				p += XLOG_FIELD_DELIM_LEN;
			}

			if(p-local_buf+hdrf0->body.len+1>LOCAL_BUF_SIZE)
			{
				LOG(L_ERR,
					"ERROR:xl_get_header: local buffer length exceeded!\n");
				return xl_get_null(msg, res, hp, hi, hf);
			}
			memcpy(p, hdrf0->body.s, hdrf0->body.len);
			p += hdrf0->body.len;
			continue;
		}

		if(hi==0)
			goto done;
		if(hi>0)
			hi--;
	}

done:
	if(hi==XLOG_PRINT_ALL)
	{
		*p = 0;
		res->s = local_buf;
		res->len = p - local_buf;
		return 0;
	}

	if(hdrf0==NULL || hi>0)
		return xl_get_null(msg, res, hp, hi, hf);
	res->s = hdrf0->body.s;
	res->len = hdrf0->body.len;
	trim(res);
	return 0;
}

static int inc_hex(char* c)
{
	switch (*c) {
	case '9':
		*c='a';
		return 0;
	case 'f':
		*c='0';
		return 1;
	default:
		(*c)++;
		return 0;
	}
}

static int xl_get_unique(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	int i;

	for (i=UNIQUE_ID_LEN-1; i && inc_hex(&UNIQUE_ID[i--]););
	res->s = &UNIQUE_ID[0];
	res->len = UNIQUE_ID_LEN;
	return 0;
}

static int xl_get_host(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	switch (hi) {
	case XL_HOST_NAME:
		*res = str_hostname;
		return 0;
	case XL_HOST_DOMAIN:
		*res = str_domainname;
		return 0;
	case XL_HOST_FULL:
		*res = str_fullname;
		return 0;
	case XL_HOST_IPADDR:
		*res = str_ipaddr;
		return 0;
	default:
		return xl_get_null(msg, res, hp, hi, hf);
	}
}

static int xl_get_select(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	int i;
	if ((i=run_select(res, (select_t*)hp->s, msg))==1)
		return xl_get_null(msg, res, hp, hi, hf);
	
	return i;
}

static void xl_free_select(str *hp)
{
	if (hp && hp->s)
		free_select((select_t*)hp->s);
}

/* shared memory version of xl_free_select() */
static void xl_shm_free_select(str *hp)
{
	if (hp && hp->s)
		shm_free_select((select_t*)hp->s);
}

static int xl_get_avp(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	int_str name, val;
	struct usr_avp *avp, *lavp;
	struct search_state st;

	if(msg==NULL || res==NULL || hp==NULL)
	return -1;

	name.s=*hp;
if (0){
	lavp=NULL;
	for(avp=search_first_avp(AVP_NAME_STR, name, NULL, &st); avp; avp=search_next_avp(&st, NULL)) {
		lavp=avp;
		if (hi>0)
			hi--;
		else if (hi==0)
			break;
	}

	if (lavp && (hi<=0)) {
		get_avp_val(lavp, &val);
		*res=val.s;
		return 0;
	}
}
	if ((avp=search_avp_by_index(hf, name, &val, hi))) {
		if (avp->flags & AVP_VAL_STR) {
			*res=val.s;
		} else {
			res->s=int2str(val.n, &res->len);
		}
		return 0;
	}

	return xl_get_null(msg, res, hp, hi, hf);
}

/* print special characters, like \r, \n, \t,... */
static int xl_get_special(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	static char	c;

	if(msg==NULL || res==NULL)
		return -1;

	c = (char)hi;
	res->s = &c;
	res->len = 1;
	return 0;
}

/* copy the string withing this range */
static int	range_from = -1;
static int	range_to = -1;

/* get the range of the string that follows */
static int xl_get_range(struct sip_msg *msg, str *res, str *hp, int hi, int hf)
{
	range_from = hi;
	range_to = hf;

	res->s = NULL;
	res->len = 0;
	return 0;
}

static int _xl_elog_free_all(xl_elog_p log, int shm)
{
	xl_elog_p t;
	while(log)
	{
		t = log;
		log = log->next;
		if (t->free_f)
			(*t->free_f)(&(t->hparam));
		if (shm)
			shm_free(t);
		else
			pkg_free(t);
	}
	return 0;
}

/* Parse an xl-formatted string pointed by s.
 * el points to the resulted linked list that is allocated
 * in shared memory when shm==1 otherwise in pkg memory.
 * If parse_cb is not NULL then regular expression back references
 * are passed to the parse_cb function that is supposed to farther parse
 * the back reference and fill in the xl_elog_t structure.
 *
 * Return value:
 *   0: success
 *  -1: error
 */
static int _xl_parse_format(char *s, xl_elog_p *el, int shm, xl_parse_cb parse_cb)
{
	char *p, c;
	int n = 0;
	xl_elog_p e, e0;
	struct hdr_field  hdr;
	str name;
	int avp_flags, avp_index;
	int_str avp_name;
	select_t *sel;
	int *range;
	
	if(s==NULL || el==NULL)
		return -1;

	DBG("XLOG: xl_parse_format: parsing [%s]\n", s);

	p = s;
	*el = NULL;
	e = e0 = NULL;
	range = NULL;

	while(*p)
	{
		e0 = e;
		if (shm)
			e = shm_malloc(sizeof(xl_elog_t));
		else
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
		while(*p && *p!='%' && *p!='\\' && !range)
			p++;

		e->text.len = p - e->text.s;
		if(*p == '\0')
			break;

		if ((*p == '\\') && !range) {
			p++;
			switch(*p)
			{
				case '\\':
					e->itf = xl_get_special;
					e->hindex = '\\';
					break;
				case 'r':
					e->itf = xl_get_special;
					e->hindex = '\r';
					break;
				case 'n':
					e->itf = xl_get_special;
					e->hindex = '\n';
					break;
				case 't':
					e->itf = xl_get_special;
					e->hindex = '\t';
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					/* Regular expression back reference found */
					if (!parse_cb) {
						/* There is no callback function, hence the
						 * result will be written as it is. */
						e->itf = xl_get_special;
						e->hindex = *p;
						break;
					}
					name.s = p;
					/* eat all the numeric characters */
					while ((*(p+1) >= '0') && (*(p+1) <= '9'))
						p++;
					name.len = p - name.s + 1;
					if (parse_cb(&name, shm, e)) {
						ERR("xprint: xl_parse_format: failed to parse '%.*s'\n",
							name.len, name.s);
						goto error;
					}
					break;
				default:
					/* not a special character, it will be just
					written to the result as it is */
					e->itf = xl_get_special;
					e->hindex = *p;
			}
			goto cont;
		}

		if (range)
			range = NULL;
		else
			p++;
		switch(*p)
		{
			case 'b':
				p++;
				switch(*p)
				{
					case 'r':
						e->itf = xl_get_branch;
					break;
					case 'R':
						e->itf = xl_get_branches;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
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
			case 'C':
				p++;
				e->hparam.s = p;

				/* foreground */
				switch(*p)
                {
					case 'x':
					case 's': case 'r': case 'g':
					case 'y': case 'b': case 'p':
					case 'c': case 'w': case 'S':
					case 'R': case 'G': case 'Y':
					case 'B': case 'P': case 'C':
					case 'W':
					break;
					default:
						e->itf = xl_get_empty;
						goto error;
				}
				p++;

				/* background */
				switch(*p)
				{
					case 'x':
					case 's': case 'r': case 'g':
					case 'y': case 'b': case 'p':
					case 'c': case 'w':
					break;
					default:
						e->itf = xl_get_empty;
						goto error;
				}

				/* end */
				e->hparam.len = 2;
				e->itf = xl_get_color;
			break;
			case 'd':
				p++;
				switch(*p)
				{
					case 's':
						e->itf = xl_get_dset;
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
			case 'H':
				p++;
				e->itf = xl_get_host;
				switch(*p)
				{
					case 'n':
						e->hindex = XL_HOST_NAME;
					break;
					case 'd':
						e->hindex = XL_HOST_DOMAIN;
					break;
					case 'f':
						e->hindex = XL_HOST_FULL;
					break;
					case 'i':
						e->hindex = XL_HOST_IPADDR;
					break;
					default:
						e->hindex = XL_HOST_NULL;
						break;
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
						e->hindex = 10;
					break;
					case 'l':
						e->itf = xl_get_msg_len;
					break;
					case 'x':
						e->itf = xl_get_msgid;
						e->hindex = 16;
					break;
					default:
						e->itf = xl_get_null;
				}
				break;
			case 'n':
				p++;
				switch(*p)
				{
					case 'h':
						e->itf = xl_get_nexthop;
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
						e->hindex = 10;
					break;
					case 'x':
						e->itf = xl_get_pid;
						e->hindex = 16;
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
					case 't':
						e->itf = xl_get_refer_to;
					break;
					default:
						e->itf = xl_get_null;
				}
			break;
			case 'R':
				p++;
				switch(*p)
				{
					case 'i':
						e->itf = xl_get_rcvip;
					break;
					case 'p':
						e->itf = xl_get_rcvport;
					break;
					default:
					e->itf = xl_get_null;
				}
			break;
			case 's':
				p++;
				switch(*p)
				{
					case 'i':
						e->itf = xl_get_srcip;
					break;
					case 'p':
						e->itf = xl_get_srcport;
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
						e->hindex = 10;
					break;
					case 'f':
						e->itf = xl_get_timef;
					break;
					case 'x':
						e->itf = xl_get_times;
						e->hindex = 16;
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
					case 'q':
						e->itf = xl_get_unique;
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
					LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
						" [%s] pos [%d]\n", s, (int)(p-s));
					goto error;
				}
				e->hparam.s = p;
				while(*p && *p!='}' && *p!='[')
					p++;
				if(*p == '\0')
				{
					LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
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
							LOG(L_ERR, "xprint: xl_parse_format: error"
								" parsing format [%s] -- only -1 is accepted"
								" as a negative index\n", s);
								goto error;
						}
						e->hindex = XLOG_PRINT_LAST;
						p++;
					} else if (*p=='*') {
						e->hindex = XLOG_PRINT_ALL;
						p++;
					} else {
						while(*p>='0' && *p<='9')
						{
							e->hindex = e->hindex * 10 + *p - '0';
							p++;
						}
					}
					if(*p != ']')
					{
						LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
							" [%s] expecting ']' after position [%d]\n", s,
							(int)(e->hparam.s - s + e->hparam.len));
						goto error;
					}
					p++;
				}
				if(*p != '}')
				{
					LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
						" [%s] expecting '}' after position [%d]!\n", s,
						(int)(e->hparam.s-s));
					goto error;
				}

				DBG("xprint: xl_parse_format: header name [%.*s] index [%d]\n",
						e->hparam.len, e->hparam.s, e->hindex);

				/* optimize for known headers -- fake header name */
				c = e->hparam.s[e->hparam.len];
				e->hparam.s[e->hparam.len] = ':';
				e->hparam.len++;
				/* ugly hack for compact header names -- !!fake length!!
				 * -- parse_hname2 expects name buffer length >= 4
				 */
				if (parse_hname2(e->hparam.s,
						e->hparam.s + ((e->hparam.len<4)?4:e->hparam.len),
						&hdr)==0)
				{
					LOG(L_ERR,"xprint: xl_parse_format: strange error\n");
					goto error;
				}
				e->hparam.len--;
				e->hparam.s[e->hparam.len] = c;
				if (hdr.type!=HDR_OTHER_T && hdr.type!=HDR_ERROR_T)
				{
					LOG(L_INFO,"INFO:xprint: xl_parse_format: using "
						"hdr type (%d) instead of <%.*s>\n",
						hdr.type, e->hparam.len, e->hparam.s);
					e->hparam.len = hdr.type;
					e->hparam.s = NULL;
				}
				e->itf = xl_get_header;
			break;
			case '<':
				p++;
				/* we expect a letter */
				if((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z'))
				{
					LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
						" [%s] pos [%d]\n", s, (int)(p-s));
					goto error;
				}
				e->hparam.s = p;
				while(*p && *p!='>' && *p!='[')
					p++;
				if(*p == '\0')
				{
					LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
						" [%s] expecting '>' after position [%d]\n", s,
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
							LOG(L_ERR, "xprint: xl_parse_format: error"
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
						LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
							" [%s] expecting ']' after position [%d]\n", s,
							(int)(e->hparam.s - s + e->hparam.len));
						goto error;
					}
					p++;
				}
				if(*p != '>')
				{
					LOG(L_ERR, "xprint: xl_parse_format: error parsing format"
						" [%s] expecting '>' after position [%d]!\n", s,
						(int)(e->hparam.s-s));
					goto error;
				}

				DBG("xprint: xl_parse_format: AVP [%.*s] index [%d]\n",
						e->hparam.len, e->hparam.s, e->hindex);

				e->itf = xl_get_avp;
			break;
			case '$':
				p++;
				name.s=p;
				while ( (*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || (*p=='_') || (*p=='+') || (*p=='-') || (*p=='[') || (*p==']') || (*p=='.') ) p++;
				name.len=p-name.s;
				p--;
				if (parse_avp_name(&name, &avp_flags, &avp_name, &avp_index) < 0) {
					ERR("error while parsing AVP name\n");
					goto error;
				}
				e->itf = xl_get_avp;
				e->hflags=avp_flags;
				e->hparam.s=name.s;
				e->hparam.len=name.len;
				e->hindex=avp_index;
				DBG("flags %x  name %.*s  index %d\n", avp_flags, avp_name.s.len, avp_name.s.s, avp_index);
				break;
			case '@':
					/* fill select structure and call resolve_select */
				DBG("xprint: xl_parse_format: @\n");
				if (shm)
					n=shm_parse_select(&p, &sel);
				else
					n=parse_select(&p, &sel);
				if (n<0) {
					ERR("xprint: xl_parse_format: parse_select returned error\n");
					goto error;
				}
				e->itf = xl_get_select;
				e->hparam.s = (char*)sel;
				e->free_f = (shm) ? xl_shm_free_select : xl_free_select;
				p--;
				break;
			case '%':
				e->itf = xl_get_percent;
				break;
			case ' ':	/* enables spaceless terminating of avp, e.g. "blah%$avp% text goes on" */
			case '|':
				e->itf = xl_get_empty;
				break;
			case '[':
				range = &e->hindex;
				e->itf = xl_get_range;
				while (1) {
					p++;
					if (((*p) >= '0') && ((*p) <= '9')) {
						(*range) *= 10;
						(*range) += (*p) - '0';

					} else if ((*p) == '-') {
						if (range == &e->hindex) {
							range = &e->hflags;
						} else {
							ERR("xprint: xl_parse_format: syntax error in the range specification\n");
							goto error;
						}

					} else if ((*p) == ']') {
						if (range == &e->hindex) {
							/* no range, only a single number */
							e->hflags = e->hindex;
						} else if (e->hflags == 0) {
							/* only the left side is defined */
							e->hflags = -1;
						} else if (e->hindex > e->hflags) {
							ERR("xprint: xl_parse_format: syntax error in the range specification\n");
							goto error;
						}
						break;

					} else {
						ERR("xprint: xl_parse_format: syntax error in the range specification\n");
						goto error;
					}
				}
				break;
			default:
				e->itf = xl_get_null;
		}

cont:
		if(*p == '\0')
			break;
		p++;
	}
	DBG("XLOG: xl_parse_format: format parsed OK: [%d] items\n", n);

	return 0;

error:
	_xl_elog_free_all(*el, shm);
	*el = NULL;
	return -1;
}

/* wrapper function for _xl_parse_format()
 * pkg memory version
 */
int xl_parse_format(char *s, xl_elog_p *el)
{
	return _xl_parse_format(s, el, 0 /* pkg mem */, NULL /* callback */);
}

/* wrapper function for _xl_parse_format()
 * shm memory version
 */
int xl_shm_parse_format(char *s, xl_elog_p *el)
{
	return _xl_parse_format(s, el, 1 /* shm mem */, NULL /* callback */);
}

/* wrapper function for _xl_parse_format()
 * pkg memory version
 */
int xl_parse_format2(char *s, xl_elog_p *el, xl_parse_cb cb)
{
	return _xl_parse_format(s, el, 0 /* pkg mem */, cb);
}

/* wrapper function for _xl_parse_format()
 * shm memory version
 */
int xl_shm_parse_format2(char *s, xl_elog_p *el, xl_parse_cb cb)
{
	return _xl_parse_format(s, el, 1 /* shm mem */, cb);
}

int xl_print_log(struct sip_msg* msg, xl_elog_p log, char *buf, int *len)
{
	int n, h;
	str tok;
	xl_elog_p it;
	char *cur;

	if(msg==NULL || log==NULL || buf==NULL || len==NULL)
		return -1;

	if(*len <= 0)
		return -1;

	*buf = '\0';
	cur = buf;

	h = 0;
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
		if(it->itf
				/* && ((*it->itf != xl_get_color) || (log_stderr!=0)) */
				&& !((*it->itf)(msg, &tok, &(it->hparam), it->hindex, it->hflags)))
		{
			if (*it->itf == xl_get_range)
				continue;

			/* cut the string to the required size */
			if (range_to >= 0) {
				if (range_to + 1 < tok.len)
					tok.len = range_to + 1;
				range_to = -1;
			}
			if (range_from > 0) {
				if (range_from + 1 > tok.len) {
					range_from = -1;
					/* nothing to copy */
					continue;
				}
				tok.s += range_from;
				tok.len -= range_from;
				range_from = -1;
			}

			if (tok.len == 0)
				continue;

			if(n+tok.len < *len)
			{
				memcpy(cur, tok.s, tok.len);
				n += tok.len;
				cur += tok.len;

				/* check for color entries to reset later */
				if (*it->itf == xl_get_color) {
					h = 1;
				}
			}
			else
				goto overflow;
		}
	}

	/* reset to default after entry */
	if (h == 1)
	{
		h = sizeof("\033[0m")-1;
		if (n+h < *len)
		{
			memcpy(cur, "\033[0m", h);
			n += h;
			cur += h;
			} else {
				goto overflow;
			}
	}

	goto done;

overflow:
	LOG(L_ERR,
		"XLOG:xl_print_log: buffer overflow -- increase the buffer size...\n");
	LOG(L_ERR, "Pos: %d, Add: %d, Len: %d, Buf:%.*s\n", n, tok.len, *len, n, buf);
	return -1;

done:
	DBG("XLOG: xl_print_log: final buffer length %d\n", n);
	*cur = '\0';
	*len = n;
	return 0;
}

/* wrapper function for _xl_elog_free_all()
 * pkg memory version
 */
int xl_elog_free_all(xl_elog_p log)
{
	return _xl_elog_free_all(log, 0 /* pkg mem */);
}

/* wrapper function for _xl_elog_free_all()
 * shm memory version
 */
int xl_elog_shm_free_all(xl_elog_p log)
{
	return _xl_elog_free_all(log, 1 /* shm mem */);
}

int xl_bind(xl_api_t *xl_api)
{
	xl_api->xprint = xl_print_log;
	xl_api->xparse = xl_parse_format;
	xl_api->shm_xparse = xl_shm_parse_format;
	xl_api->xparse2 = xl_parse_format2;
	xl_api->shm_xparse2 = xl_shm_parse_format2;
	xl_api->xfree = xl_elog_free_all;
	xl_api->shm_xfree = xl_elog_shm_free_all;
	xl_api->xnulstr = xl_get_nulstr;
	return 0;
}

int xl_mod_init()
{
#ifdef HOST_NAME_MAX
#define HOSTNAME_MAX HOST_NAME_MAX
#else
#define HOSTNAME_MAX 256
#endif
	char *s, *d;
	struct hostent *he;
	int i;

	s=(char*)pkg_malloc(HOSTNAME_MAX);
	if (!s) return -1;
	if (gethostname(s, HOSTNAME_MAX)<0) {
		str_fullname.len = 0;
		str_fullname.s = NULL;
		str_hostname.len = 0;
		str_hostname.s = NULL;
		str_domainname.len = 0;
		str_domainname.s = NULL;
	} else {
		str_fullname.len = strlen(s);
		s = pkg_realloc(s, str_fullname.len+1); /* this will leave the ending \0 */
		if (!s) { /* should never happen because decreasing size */
			pkg_free(s);
			return -1;
		}
		str_fullname.s = s;

		d=strchr(s, '.');
		if (d) {
			str_hostname.len=d-s;
			str_hostname.s=s;
			str_domainname.len=str_fullname.len-str_hostname.len-1;
			str_domainname.s=d+1;
		} else {
			str_hostname=str_fullname;
			str_domainname.len=0;
			str_domainname.s=NULL;
		}
		s=(char*)pkg_malloc(HOSTNAME_MAX);
		if (!s) {
			pkg_free(str_fullname.s);
			return -1;
		}
	}

	str_ipaddr.len=0;
	str_ipaddr.s=NULL;
	if (str_fullname.len) {
		he=resolvehost(str_fullname.s);
		if (he) {
			if ((strlen(he->h_name)!=str_fullname.len) || strncmp(he->h_name, str_fullname.s, str_fullname.len)) {
				LOG(L_WARN, "WARNING: xl_mod_init: DIFFERENT hostname '%.*s' and gethostbyname '%s'\n", str_fullname.len, ZSW(str_hostname.s), he->h_name);
			}

			if (he->h_addr_list) {
				for (i=0; he->h_addr_list[i]; i++) {
					if (inet_ntop(he->h_addrtype, he->h_addr_list[i], s, HOSTNAME_MAX)) {
						if (str_ipaddr.len==0) {
							str_ipaddr.len=strlen(s);
							str_ipaddr.s=(char*)pkg_malloc(str_ipaddr.len);
							if (str_ipaddr.s) {
								memcpy(str_ipaddr.s, s, str_ipaddr.len);
							} else {
								str_ipaddr.len=0;
								LOG(L_ERR, "ERROR: xl_mod_init: No memory left for str_ipaddr\n");
							}
						} else if (strncmp(str_ipaddr.s, s, str_ipaddr.len)!=0) {
							LOG(L_WARN, "WARNING: xl_mod_init: more IP %s not used\n", s);
						}
					}
				}
			} else {
				LOG(L_WARN, "WARNING: xl_mod_init: can't resolve hostname's address\n");
			}

		}
	}
	pkg_free(s);

	DBG("Hostname:   %.*s\n", str_hostname.len, ZSW(str_hostname.s));
	DBG("Domainname: %.*s\n", str_domainname.len, ZSW(str_domainname.s));
	DBG("Fullname:   %.*s\n", str_fullname.len, ZSW(str_fullname.s));
	DBG("IPaddr:     %.*s\n", str_ipaddr.len, ZSW(str_ipaddr.s));

	return 0;
}

int xl_child_init(int rank)
{
	int i, x, rb, cb;

	for (i=RAND_MAX, rb=0; i; rb++, i>>=1);

	cb=x=0; /* x asiignment to make gcc happy */
	for (i=0; i<UNIQUE_ID_LEN; i++) {
		if (!cb) {
			cb=rb;
			x=rand();
		}
		UNIQUE_ID[i]=fourbits2char[x&0x0F];
		x>>=rb;
		cb-=rb;
	}

	return 0;
}
