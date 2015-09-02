/*
 * Copyright (C) 2005-2006 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio core :: select framework, basic core functions (mma)
 * \ingroup core
 * Module: \ref core
 */

#include <stdlib.h> 
#include "select.h"
#include "select_core.h"
#include "select_buf.h"
#include "dprint.h"
#include "trim.h"
#include "ut.h"
#include "globals.h"
#include "parser/parser_f.h"
#include "parser/hf.h"
#include "parser/parse_from.h"
#include "parser/parse_to.h"
#include "parser/contact/parse_contact.h"
#include "parser/contact/contact.h"
#include "parser/parse_via.h"
#include "parser/parse_uri.h"
#include "parser/parse_event.h"
#include "parser/parse_rr.h"
#include "parser/digest/digest.h"
#include "mem/mem.h"
#include "parser/parse_hname2.h"
#include "ip_addr.h"
#include "parser/parse_expires.h"
#include "parser/parse_refer_to.h"
#include "parser/parse_rpid.h"
#include "parser/parse_content.h"
#include "parser/parse_body.h"
#include "dset.h"
#include "sr_module.h"
#include "resolve.h"
#include "forward.h"

#define RETURN0_res(x) {*res=(x);return 0;}
#define TRIM_RET0_res(x) {*res=(x);trim(res);return 0;} 
#define TEST_RET_res_body(x) if (x){*res=(x)->body;return 0;}else return 1;
#define TEST_RET_res_value(x) if (x){*res=(x)->value;return 0;}else return 1;

int select_ruri(str* res, select_t* s, struct sip_msg* msg)
{
	/* Parse the RURI even if it is not needed right now
	 * because the nested select calls can access the
	 * parsed URI in this case.
	 * Go ahead even if the parsing fails, so the
	 * value of the broken RURI can be accessed at least.
	 * Subsequent select calls will fail when they try to parse
	 * the URI anyway. (Miklos)
	 */
	parse_sip_msg_uri(msg);

	if (msg->parsed_uri_ok)
		select_uri_p = &msg->parsed_uri;

	if (msg->first_line.type==SIP_REQUEST) {
		if(msg->new_uri.s) {
			RETURN0_res(msg->new_uri);
		}
		else {
			RETURN0_res(msg->first_line.u.request.uri);
		}
	}
	return -1;
}

int select_dst_uri(str* res, select_t* s, struct sip_msg* msg)
{
	if (msg->first_line.type!=SIP_REQUEST)
		return -1;
	RETURN0_res(msg->dst_uri);
}

int select_next_hop(str* res, select_t* s, struct sip_msg* msg)
{
	if (msg->first_line.type==SIP_REQUEST) {
		if(msg->dst_uri.s) {
			RETURN0_res(msg->dst_uri);
		}
		else if(msg->new_uri.s) {
			if (msg->parsed_uri_ok)
				select_uri_p = &msg->parsed_uri;
			RETURN0_res(msg->new_uri);
		}
		else {
			if (msg->parsed_uri_ok)
				select_uri_p = &msg->parsed_uri;
			else if (msg->parsed_orig_ruri_ok)
				select_uri_p = &msg->parsed_orig_ruri;
			RETURN0_res(msg->first_line.u.request.uri);
		}
	}
	return -1;
}

int select_next_hop_src_ip(str* res, select_t* s, struct sip_msg* msg) {
	struct socket_info* socket_info;
	union sockaddr_union to;
	char proto;
	struct sip_uri *u, next_hop;
	str *dst_host;

	if (msg->first_line.type!=SIP_REQUEST) 
		return -1;

	if (msg->force_send_socket) {
		*res = msg->force_send_socket->address_str;
		return 0;
	}

	if (msg->dst_uri.len) {
		if (parse_uri(msg->dst_uri.s, msg->dst_uri.len, &next_hop) < 0)
			return -1;
		u = &next_hop;
	}
	else {
		if (parse_sip_msg_uri(msg) < 0)
			return -1;
		u = &msg->parsed_uri;
	}
#ifdef USE_TLS
	if (u->type==SIPS_URI_T)
		proto = PROTO_TLS;
	else
#endif
		proto = u->proto;

#ifdef HONOR_MADDR
	if (u->maddr_val.s && u->maddr_val.len)
		dst_host = &u->maddr_val;
	else
#endif
		dst_host = &u->host;

	if (sip_hostport2su(&to, dst_host, u->port_no, &proto) < 0)
		return -1;
	socket_info = get_send_socket(msg, &to, proto);
	if (!socket_info)
		return -1;

	*res = socket_info->address_str;
	return 0;
}

#define SELECT_uri_header(_name_) \
int select_##_name_(str* res, select_t* s, struct sip_msg* msg) \
{ \
	if (parse_##_name_##_header(msg)<0) \
		return -1; \
	RETURN0_res(msg->_name_->body); \
} \
\
int select_##_name_##_uri(str* res, select_t* s, struct sip_msg* msg) \
{ \
	if (parse_##_name_##_header(msg)<0) \
		return -1; \
	RETURN0_res(get_##_name_(msg)->uri); \
} \
\
int select_##_name_##_tag(str* res, select_t* s, struct sip_msg* msg) \
{ \
	if (parse_##_name_##_header(msg)<0) \
		return -1; \
	RETURN0_res(get_##_name_(msg)->tag_value); \
} \
\
int select_##_name_##_name(str* res, select_t* s, struct sip_msg* msg) \
{ \
	if (parse_##_name_##_header(msg)<0) \
		return -1; \
	RETURN0_res(get_##_name_(msg)->display); \
} \
\
int select_##_name_##_params(str* res, select_t* s, struct sip_msg* msg) \
{ \
	struct to_param* p; \
	if (parse_##_name_##_header(msg)<0) \
		return -1; \
	\
	p = get_##_name_(msg)->param_lst; \
	while (p) { \
		if ((p->name.len==s->params[s->n-1].v.s.len) \
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) { \
			RETURN0_res(p->value); \
		} \
		p = p->next; \
	} \
	return 1; \
} 

SELECT_uri_header(to)
SELECT_uri_header(from)
SELECT_uri_header(refer_to)
SELECT_uri_header(rpid)

int parse_contact_header( struct sip_msg *msg)
{
        if ( !msg->contact && ( parse_headers(msg,HDR_CONTACT_F,0)==-1 || !msg->contact)) {
                LM_DBG("bad msg or missing CONTACT header\n");
                return -1;
        }

        if (msg->contact->parsed)
                return 0;

	return parse_contact(msg->contact);
}

#define get_contact(msg) ((contact_body_t*)(msg->contact->parsed))

int select_contact(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_contact_header(msg)<0)
		return -1;
	RETURN0_res(msg->contact->body);
}

int select_contact_uri(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	RETURN0_res(c->uri);
}

int select_contact_name(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	RETURN0_res(c->name);
}

int select_contact_params_spec(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	
	if (s->params[s->n-1].type != SEL_PARAM_DIV) {
		BUG("Last parameter should have type DIV (converted)\n");
		return -1;
	}
	
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	
	switch (s->params[s->n-1].v.i) {
	case SEL_PARAM_Q:
		TEST_RET_res_body(c->q);
	case SEL_PARAM_EXPIRES:
		TEST_RET_res_body(c->expires);
	case SEL_PARAM_METHODS:
		TEST_RET_res_body(c->methods);
	case SEL_PARAM_RECEIVED:
		TEST_RET_res_body(c->received);
	case SEL_PARAM_INSTANCE:
		TEST_RET_res_body(c->instance);
	default:
		BUG("Unexpected parameter value \"%d\"\n", s->params[s->n-1].v.i);
		return -1;
	}
	return -1;
}

int select_contact_params(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	param_t* p;
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	p = c->params;
	while (p) {
		if ((p->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) {
			RETURN0_res(p->body)
		}
		p = p->next;
	}
	return 1;
}

int select_via(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	if ((s->n == 1) || (s->params[1].type == SEL_PARAM_STR)) {
		if (parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	res->s=p->name.s;
	res->len=p->bsize;
	trim(res);
	return 0;
}

int select_via_name(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type == SEL_PARAM_STR) {
		if(parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->name);
}

int select_via_version(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type == SEL_PARAM_STR) {
		if (parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->version);
}

int select_via_transport(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type == SEL_PARAM_STR) {
		if(parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->transport);
}

int select_via_host(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type == SEL_PARAM_STR) {
		if (parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->host);
}

int select_via_port(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type == SEL_PARAM_STR) {
		if (parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->port_str);
}

int select_via_comment(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type == SEL_PARAM_STR) {
		if(parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->comment);
}

int select_via_params(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	struct via_param *q;

	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type == SEL_PARAM_STR) {
		if (parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	
	for (q = p->param_lst;q;q=q->next) {
		if ((q->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(q->name.s, s->params[s->n-1].v.s.s,q->name.len)) {
			RETURN0_res(q->value);
		}
	}
	return 1;
}

int select_via_params_spec(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;

	if (s->params[s->n-1].type != SEL_PARAM_DIV) {
		BUG("Last parameter should have type DIV (converted)\n");
		return -1;
	}
	
	// it's not neccessary to test if (s->n > 1)
	if (s->params[1].type != SEL_PARAM_INT) {
		if(parse_via_header(msg, 1, &p)<0) return -1;
	} else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	
	switch (s->params[s->n-1].v.i) {
	case SEL_PARAM_BRANCH:
		TEST_RET_res_value(p->branch);
	case SEL_PARAM_RECEIVED:
		TEST_RET_res_value(p->received);
	case SEL_PARAM_RPORT:
		TEST_RET_res_value(p->rport);
	case SEL_PARAM_I:
		TEST_RET_res_value(p->i);
	case SEL_PARAM_ALIAS:
		TEST_RET_res_value(p->alias);
	default:
		BUG("Unexpected parameter value \"%d\"\n", s->params[s->n-1].v.i);
		return -1;
	}
	return -1;
}

int select_msg(str* res, select_t* s, struct sip_msg* msg)
{
	res->s = msg->buf;
	res->len = msg->len;
	return 0;
}

int select_msg_first_line(str* res, select_t* s, struct sip_msg* msg) 
{
	res->s=SIP_MSG_START(msg);
	res->len=msg->first_line.len;
	trim_trailing(res);
	return 0;
}

int select_msg_type(str* res, select_t* s, struct sip_msg* msg) {
	return uint_to_static_buffer(res, msg->first_line.type);
} 

int select_msg_len(str* res, select_t* s, struct sip_msg* msg) {
	return uint_to_static_buffer(res, msg->len);
}

int select_msg_id(str* res, select_t* s, struct sip_msg* msg) {
	return uint_to_static_buffer(res, msg->id);
}

int select_msg_id_hex(str* res, select_t* s, struct sip_msg* msg) {
	return uint_to_static_buffer_ex(res, msg->id, 16, 8);
}

int select_msg_flags(str* res, select_t* s, struct sip_msg* msg) { 
	return uint_to_static_buffer(res, msg->flags);
} 

int select_msg_body(str* res, select_t* s, struct sip_msg* msg)
{
	res->s = get_body(msg);
	res->len = msg->buf+msg->len - res->s;
	return 0;	
}

/* returns the sdp part of the message body */
int select_msg_body_sdp(str* res, select_t* sel, struct sip_msg* msg)
{
	/* try to get the body part with application/sdp */
	if ((res->s = get_body_part(msg,
				TYPE_APPLICATION, SUBTYPE_SDP,
				&res->len))
	)
		return 0;
	else
		return -1;
}

/* returns the value of the requested SDP line */
int select_sdp_line(str* res, select_t* sel, struct sip_msg* msg)
{
	int	len;
	char	*buf;
	char	*buf_end, *line_end;
	char	line;

	if (msg == NULL) {
		if (res!=NULL) return -1;
		if (sel->n < 5) return -1;

		if (sel->params[4].type != SEL_PARAM_STR) {
			LM_ERR("wrong parameter type");
			return -1;
		}
		if ((sel->params[4].v.s.len < 1) ||
			(sel->params[4].v.s.len > 2) ||
			((sel->params[4].v.s.len == 2) && (sel->params[4].v.s.s[1] != '='))
		) {
			LM_ERR("wrong sdp line format: %.*s\n",
				sel->params[4].v.s.len, sel->params[4].v.s.s);
			return -1;
		}
		return 0;
	}

	/* try to get the body part with application/sdp */
	if (!(buf = get_body_part(msg,
				TYPE_APPLICATION, SUBTYPE_SDP,
				&len))
	)
		return -1;

	buf_end = buf + len;
	line = *(sel->params[4].v.s.s);

	while (buf < buf_end) {
		if (*buf == line) {
			/* the requested SDP line is found, return its value */
			buf++;
			if ((buf >= buf_end) || (*buf != '=')) {
				LM_ERR("wrong SDP line format\n");
				return -1;
			}
			buf++;

			line_end = buf;
			while ((line_end < buf_end) && (*line_end != '\n'))
				line_end++;

			if (line_end >= buf_end) {
				LM_ERR("wrong SDP line format\n");
				return -1;
			}
			line_end--;
			if (*line_end == '\r') line_end--;

			if (line_end < buf) {
				LM_ERR("wrong SDP line format\n");
				return -1;
			}

			res->s = buf;
			res->len = line_end - buf + 1;
			return 0;
		}
		while ((buf < buf_end) && (*buf != '\n'))
			buf++;
		buf++;
	}

	return -1;
}

int select_msg_header(str* res, select_t* s, struct sip_msg* msg)
{
	/* get all headers */
	char *c;
	res->s = SIP_MSG_START(msg) + msg->first_line.len; 
	c = get_body(msg);
	res->len = c - res->s;
	return 0;
}

int select_anyheader(str* res, select_t* s, struct sip_msg* msg)
{
	struct hdr_field *hf, *hf0;
	int hi;
	char c;
	struct hdr_field hdr;
	
	if(msg==NULL) {
		if (res!=NULL) return -1;

		/* "fixup" call, res & msg are NULL */
		if (s->n <3) return -1;

		if (s->params[2].type==SEL_PARAM_STR) {
				/* replace _ with - (for P-xxx headers) */
			for (hi=s->params[2].v.s.len-1; hi>0; hi--)
				if (s->params[2].v.s.s[hi]=='_')
					s->params[2].v.s.s[hi]='-';
				/* if header name is parseable, parse it and set SEL_PARAM_DIV */
			c=s->params[2].v.s.s[s->params[2].v.s.len];
			s->params[2].v.s.s[s->params[2].v.s.len]=':';
			if (parse_hname2_short(s->params[2].v.s.s,s->params[2].v.s.s+(s->params[2].v.s.len<3?4:s->params[2].v.s.len+1),
						&hdr)==0) {
				LM_ERR("fixup_call:parse error\n");
				return -1;
			}
			s->params[2].v.s.s[s->params[2].v.s.len]=c;
			
			if (hdr.type!=HDR_OTHER_T && hdr.type!=HDR_ERROR_T) {
				/* pkg_free(s->params[1].v.s.s); */
				/* don't free it (the mem can leak only once at startup)
				 * the parsed string can live inside larger string block
				 * e.g. when xlog's select is parsed
				 */
				s->params[2].type = SEL_PARAM_DIV;
				s->params[2].v.i = hdr.type;
			}
		}
		return 1;
	}

	hf0 = NULL;

	/* extract header index if present */
	if (s->param_offset[select_level+1] == 4) {
		if (s->params[3].type == SEL_PARAM_INT) {
			hi = s->params[3].v.i;
		} else {
			hi = -1;
		}
	} else {
		hi = 1;
	}

	/* we need to be sure we have parsed all headers */
	if (!msg->eoh && (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->eoh)) {
		LM_ERR("bad msg while parsing to EOH \n");
		return -1;
	}
	for (hf=msg->headers; hf; hf=hf->next) {
		if(s->params[2].type==SEL_PARAM_DIV) {
			if (s->params[2].v.i!=hf->type)	continue;
		} else if(s->params[2].type==SEL_PARAM_STR) {
			if (s->params[2].v.s.len!=hf->name.len)	continue;
			if (strncasecmp(s->params[2].v.s.s, hf->name.s, hf->name.len)!=0) continue;
		}
		hf0 = hf;
		hi--;
		if (!hi) break;
	}
	if(hf0==NULL || hi>0)
		return 1;
	res->s = hf0->body.s;
	res->len = hf0->body.len;
	trim(res);
	return 0;
}

//ABSTRACT_F(select_anyheader_params)
// Instead of ABSTRACT_F(select_anyheader_params)
// use function which uses select_core_table
// to avoid gcc warning about not used
 
int select_anyheader_params(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_core_table.next)
		return -1;
	else
		return -1;
}

ABSTRACT_F(select_any_uri)

static struct sip_uri uri;

int select_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_uri_p == NULL) {
		trim(res);
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	if (select_uri_p->type==ERROR_URI_T)
		return -1;

	uri_type_to_str(select_uri_p->type, res);
	return 0;
}

int select_uri_user(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	if (select_uri_p->flags & URI_USER_NORMALIZE) {
		if (!(res->s=get_static_buffer(select_uri_p->user.len)))
			return -1;
		if ((res->len=normalize_tel_user(res->s, (&select_uri_p->user)))==0)
			return 1;
		return 0;
	}
	RETURN0_res(select_uri_p->user);
}

/* search for a parameter with "name"
 * Return value:
 *	0: not found
 *	1: found
 *	-1: error
 *
 * val is set to the value of the parameter.
 */
static inline int search_param(str params, char *name, int name_len,
				str *val)
{
	param_hooks_t h;
	param_t *p, *list;

	if (params.s == NULL)
		return 0;

	if (parse_params(&params, CLASS_ANY, &h, &list) < 0)
		return -1;
	for (p = list; p; p=p->next) {
		if ((p->name.len == name_len)
			&& (strncasecmp(p->name.s, name, name_len) == 0)
		) {
			*val=p->body;
			free_params(list);
			return 1;
		}
	}
	free_params(list);
	return 0;
}

/* Return the value of the "rn" parameter if exists, otherwise the user name.
 * The user name is normalized if needed, i.e. visual separators are removed,
 * the "rn" param is always normalized. */
int select_uri_rn_user(str* res, select_t* s, struct sip_msg* msg)
{
	int	ret;
	str	val;

	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	/* search for the "rn" parameter */
	if ((ret = search_param(select_uri_p->params, "rn", 2, &val)) != 0)
		goto done;

	if (select_uri_p->sip_params.s != select_uri_p->params.s) {
		/* check also the original sip: URI parameters */
		if ((ret = search_param(select_uri_p->sip_params, "rn", 2, &val)) != 0)
			goto done;
	}

	if ((select_uri_p->flags & URI_USER_NORMALIZE) == 0)
		RETURN0_res(select_uri_p->user);
	/* else normalize the user name */
	val = select_uri_p->user;
done:
	if (ret < 0)
		return -1; /* error */

	if (!(res->s=get_static_buffer(val.len)))
		return -1;
	if ((res->len=normalize_tel_user(res->s, &val))==0)
		return 1;
	return 0;
}

int select_uri_pwd(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	RETURN0_res(select_uri_p->passwd);
}

int select_uri_host(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	RETURN0_res(select_uri_p->host);
}

int select_uri_port(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	RETURN0_res(select_uri_p->port);
}

int select_uri_hostport(str* res, select_t* s, struct sip_msg* msg)
{
	char* p;
	int size;
	
	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	if (!select_uri_p->host.len)
		return -1;
	
	if (select_uri_p->port.len) {
		res->s=select_uri_p->host.s;
		res->len=select_uri_p->host.len+select_uri_p->port.len+1;
		return 0;
	}
	
	size=select_uri_p->host.len+5;
	if (!(p = get_static_buffer(size)))
		return -1;
			
	strncpy(p, select_uri_p->host.s, select_uri_p->host.len);
	switch (select_uri_p->type) {
		case SIPS_URI_T:
		case TELS_URI_T:
			strncpy(p+select_uri_p->host.len, ":5061", 5); 
			break;
		case SIP_URI_T:
		case TEL_URI_T:
		case URN_URI_T:
			strncpy(p+select_uri_p->host.len, ":5060", 5);
			break;
		case ERROR_URI_T:
			return -1;
	}
	res->s = p;
	res->len = size;
	return 0;
}

int select_uri_proto(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}

	if (select_uri_p->proto != PROTO_NONE) {
		proto_type_to_str(select_uri_p->proto, res);
	} else {
		switch (select_uri_p->type) {
			case SIPS_URI_T:
			case TELS_URI_T:
				proto_type_to_str(PROTO_TLS, res);
				break;
			case SIP_URI_T:
			case TEL_URI_T:
			case URN_URI_T:
				proto_type_to_str(PROTO_UDP, res);
				break;
			case ERROR_URI_T:
				return -1;
		}
	}
	return 0;
}

int select_uri_params(str* res, select_t* s, struct sip_msg* msg)
{
	int	ret;
	if (!msg || !res) {
		return select_any_params(res, s, msg);
	}

	if (select_uri_p == NULL) {
		if (parse_uri(res->s, res->len, &uri)<0)
			return -1;
		select_uri_p = &uri;
	}
	
	if (s->param_offset[select_level+1]-s->param_offset[select_level]==1)
		RETURN0_res(select_uri_p->params);

	*res=select_uri_p->params;
	ret = select_any_params(res, s, msg);
	if ((ret < 0)
		&& (select_uri_p->sip_params.s != NULL)
		&& (select_uri_p->sip_params.s != select_uri_p->params.s)
	) {
		/* Search also in the original sip: uri parameters. */
		*res = select_uri_p->sip_params;
		ret = select_any_params(res, s, msg);
	}
	return ret;
}

int select_any_params(str* res, select_t* s, struct sip_msg* msg)
{
	str* wanted;
	int i;

	if (!msg || !res) {
		if (s->param_offset[select_level+1]-s->param_offset[select_level]==1) return 0;
		if (s->params[s->param_offset[select_level]+1].type!=SEL_PARAM_STR) return -1;
		wanted=&s->params[s->param_offset[select_level]+1].v.s;
		for (i=0; i<wanted->len; i++) 
			if (wanted->s[i]=='_') 
				wanted->s[i]='-';
		return 0;
	}
	
	if (s->params[s->param_offset[select_level]+1].type!=SEL_PARAM_STR) return -1;
	wanted=&s->params[s->param_offset[select_level]+1].v.s;
	
	if (!res->len) return -1;

	if (search_param(*res, wanted->s, wanted->len, res) <= 0) {
		LM_DBG("uri.params.%s NOT FOUND !\n", wanted->s);
		return -1;
	} else {
		return (res->len) ? 0 : 1;
	}
}

int select_event(str* res, select_t* s, struct sip_msg* msg)
{
	if (!msg->event && parse_headers(msg, HDR_EVENT_F, 0) == -1) {
		LM_ERR("Error while searching Event header field\n");
		return -1;
	}

	if (!msg->event) {
		LM_DBG("Event header field not found\n");
		return -1;
	}

	if (parse_event(msg->event) < 0) {
		LM_ERR("Error while parsing Event header field\n");
		return -1;
	}

	*res = ((event_t*)msg->event->parsed)->name;
	return 0;
}



static int parse_rr_header(struct sip_msg *msg)
{
        if ( !msg->record_route && ( parse_headers(msg,HDR_RECORDROUTE_F,0) == -1)) {
                LM_ERR("bad msg or missing Record-Route header\n");
                return -1;
        }

	if (!msg->record_route) {
		LM_DBG("No Record-Route header field found\n");
		return -1;
	}

	return parse_rr(msg->record_route);
}

#define get_rr(msg) ((rr_t*)(msg->record_route->parsed))


int select_rr(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_rr_header(msg)<0)
		return -1;
	RETURN0_res(msg->record_route->body);
}

int select_rr_uri(str* res, select_t* s, struct sip_msg* msg)
{
	rr_t* r;
	if (parse_rr_header(msg)<0)
		return -1;
	
	r = get_rr(msg);
	if (!r)
		return 1;
	RETURN0_res(r->nameaddr.uri);
}

int select_rr_name(str* res, select_t* s, struct sip_msg* msg)
{
	rr_t* r;
	if (parse_rr_header(msg)<0)
		return -1;
	
	r = get_rr(msg);
	if (!r)
		return 1;
	RETURN0_res(r->nameaddr.name);
}

int select_rr_params(str* res, select_t* s, struct sip_msg* msg)
{
	rr_t* r;
	param_t* p;
	if (parse_rr_header(msg)<0)
		return -1;
	
	r = get_rr(msg);
	if (!r)
		return 1;
	p = r->params;
	while (p) {
		if ((p->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) {
			RETURN0_res(p->body)
		}
		p = p->next;
	}
	return 0;
}


static inline struct cseq_body* sel_parse_cseq(struct sip_msg* msg)
{
        if (!msg->cseq && (parse_headers(msg, HDR_CSEQ_F, 0) == -1)) {
                LM_ERR("Unable to parse CSeq header\n");
                return 0;
        }

	if (!msg->cseq) {
		LM_DBG("No CSeq header field found\n");
		return 0;
	}

	return get_cseq(msg);
}



int select_cseq(str* res, select_t* s, struct sip_msg* msg)
{
	struct cseq_body* cs;

	cs = sel_parse_cseq(msg);
	if (!cs) return -1;
	*res = msg->cseq->body;
	return 0;
}

int select_cseq_num(str* res, select_t* s, struct sip_msg* msg)
{
	struct cseq_body* cs;

	cs = sel_parse_cseq(msg);
	if (!cs) return -1;
	*res = cs->number;
	return 0;
}

int select_cseq_method(str* res, select_t* s, struct sip_msg* msg)
{
	struct cseq_body* cs;

	cs = sel_parse_cseq(msg);
	if (!cs) return -1;
	*res = cs->method;
	return 0;
}

int get_credentials(struct sip_msg* msg, select_t* s, struct hdr_field** hdr)
{
	int ret;
	str realm;
	hdr_types_t hdr_type;

	*hdr = NULL;

	if (!msg) {
		/* fix-up call check domain for fparam conversion */
		void * ptr;
		char chr;
		ptr=(void *)(s->params[1].v.s.s);
		chr=s->params[1].v.s.s[s->params[1].v.s.len];
		s->params[1].v.s.s[s->params[1].v.s.len]=0;
		ret=fixup_var_str_12(&ptr,0);
		if (ret>=0) {
			s->params[1].v.s.s[s->params[1].v.s.len]=chr;
			s->params[1].v.p=ptr;
			s->params[1].type=SEL_PARAM_PTR;
		}
		return ret;
	}
	

	/* Try to find credentials with corresponding realm
	 * in the message, parse them and return pointer to
	 * parsed structure
	 */
	if (s->params[1].type==SEL_PARAM_PTR) {
		if (get_str_fparam(&realm, msg, s->params[1].v.p)<0)
			return -1;
	} else {
		realm = s->params[1].v.s;
	}

	switch (s->params[0].v.i) {
	case SEL_AUTH_WWW:
		hdr_type = HDR_AUTHORIZATION_T;
		break;

	case SEL_AUTH_PROXY:
		hdr_type = HDR_PROXYAUTH_T;
		break;

	default:
		BUG("Unexpected parameter value \"%d\"\n", s->params[0].v.i);
		return -1;
	}

	ret = find_credentials(msg, &realm, hdr_type, hdr);
	return ret;
}


int select_auth(str* res, select_t* s, struct sip_msg* msg)
{
	int ret;
	struct hdr_field* hdr;

	if (s->n != 2 && s->params[1].type != SEL_PARAM_STR
	              && s->params[1].type != SEL_PARAM_PTR) return -1;

	if (s->params[0].type != SEL_PARAM_DIV) {
		BUG("Last parameter should have type DIV (converted)\n");
		return -1;
	}

	ret = get_credentials(msg, s, &hdr);
	if (!hdr) return ret;
	RETURN0_res(hdr->body);
}

int select_auth_param(str* res, select_t* s, struct sip_msg* msg)
{
	int ret;
	struct hdr_field* hdr;
	dig_cred_t* cred;

	if ((s->n != 3 && s->n != 4) || (s->params[s->n - 1].type != SEL_PARAM_DIV)) return -1;

	ret = get_credentials(msg, s, &hdr);
	if (!hdr) return ret;
	cred = &((auth_body_t*)hdr->parsed)->digest;

	switch(s->params[s->n - 1].v.i) {
	case SEL_AUTH_USER:     RETURN0_res(cred->username.user);
	case SEL_AUTH_DOMAIN:   RETURN0_res(cred->username.domain);
	case SEL_AUTH_USERNAME: RETURN0_res(cred->username.whole);
	case SEL_AUTH_REALM:    RETURN0_res(cred->realm);
	case SEL_AUTH_NONCE:    RETURN0_res(cred->nonce);
	case SEL_AUTH_URI:      RETURN0_res(cred->uri);
	case SEL_AUTH_CNONCE:   RETURN0_res(cred->cnonce);
	case SEL_AUTH_NC:       RETURN0_res(cred->nc);
	case SEL_AUTH_RESPONSE: RETURN0_res(cred->response);
	case SEL_AUTH_OPAQUE:   RETURN0_res(cred->opaque);
	case SEL_AUTH_ALG:      RETURN0_res(cred->alg.alg_str);
	case SEL_AUTH_QOP:      RETURN0_res(cred->qop.qop_str);
	default:
		BUG("Unsupported digest credentials parameter in select\n");
		return -1;
	}
}

int select_auth_username(str* res, select_t* s, struct sip_msg* msg)
{
	return select_auth_param(res, s, msg);
}

int select_auth_username_comp(str* res, select_t* s, struct sip_msg* msg)
{
	return select_auth_param(res, s, msg);
}

ABSTRACT_F(select_any_nameaddr)

int select_nameaddr_name(str* res, select_t* s, struct sip_msg* msg)
{
	char *p;
	
	p=find_not_quoted(res, '<');
	if (!p) {
		LM_DBG("no < found, whole string is uri\n");
		res->len=0;
		return 1;
	}

	res->len=p-res->s;
	while (res->len && SP(res->s[res->len-1])) res->len--;
	return 0;
}

int select_nameaddr_uri(str* res, select_t* s, struct sip_msg* msg)
{
	char *p;
	
	p=find_not_quoted(res, '<');
	if (!p) {
		LM_DBG("no < found, string up to first semicolon is uri\n");
		p = q_memchr(res->s, ';', res->len);
		if (p)
			res->len = p-res->s;
		return 0;
	}

	res->len=res->len - (p-res->s) -1;
	res->s=p +1;
	
	p=find_not_quoted(res, '>');
	if (!p) {
		LM_ERR("no > found, invalid nameaddr value\n");
		return -1;
	}

	res->len=p-res->s;
	return 0;
}

int select_nameaddr_params(str* res, select_t* s, struct sip_msg* msg)
{
	char *p;
	
	p=find_not_quoted(res, '<');
	if (!p) {
		p=find_not_quoted(res, ';');
	} else {
		res->len=res->len - (p-res->s) -1;
		res->s=p +1;
		p=find_not_quoted(res, '>');
		if (!p) {
			LM_ERR("no > found, invalid nameaddr value\n");
			return -1;
		}
		res->len=res->len - (p-res->s) -1;
		res->s=p +1;
		
		p=find_not_quoted(res, ';');
	}
	if (!p) return 1;
	
	res->len=res->len - (p-res->s) -1;
	res->s=p +1;
	
	if (s->param_offset[select_level+1]-s->param_offset[select_level]==1)
		return (res->len ? 0 : 1);

	return select_any_params(res, s, msg);
}

ABSTRACT_F(select_src)
ABSTRACT_F(select_dst)
ABSTRACT_F(select_rcv)
int select_ip_port(str* res, select_t* s, struct sip_msg* msg)
{
	str ip_str=STR_NULL, port_str=STR_NULL, proto_str=STR_NULL;
	int param, pos;
	

	if ((s->n != 2) || (s->params[1].type != SEL_PARAM_DIV)) return -1;
	param=s->params[1].v.i;

	if (param & SEL_SRC) {
		if (param & SEL_IP) {
			ip_str.s = ip_addr2a(&msg->rcv.src_ip);
			ip_str.len = strlen(ip_str.s);
		}
		if (param & SEL_PORT) {
			port_str.s = int2str(msg->rcv.src_port, &port_str.len);
		}
	} else if (param & SEL_DST) {
		if (param & SEL_IP) {
			ip_str.s = ip_addr2a(&msg->rcv.dst_ip);
			ip_str.len = strlen(ip_str.s);
		}
		if (param & SEL_PORT) {
			port_str.s = int2str(msg->rcv.dst_port, &port_str.len);
		}
	} else if (param & SEL_RCV) {
		if (param & SEL_IP) {
			ip_str = msg->rcv.bind_address->address_str;
		}
		if (param & SEL_PORT) {
			port_str = msg->rcv.bind_address->port_no_str;
		}
		if (param & SEL_PROTO) {
			switch (msg->rcv.proto) {
			case PROTO_NONE:
				proto_str.s = 0;
				proto_str.len = 0;
				break;

			case PROTO_UDP:
				proto_str.s = "udp";
				proto_str.len = 3;
				break;

			case PROTO_TCP:
				proto_str.s = "tcp";
				proto_str.len = 3;
				break;

			case PROTO_TLS:
				proto_str.s = "tls";
				proto_str.len = 3;
				break;

			case PROTO_SCTP:
				proto_str.s = "sctp";
				proto_str.len = 4;
				break;

			default:
				LM_ERR("Unknown transport protocol\n");
				return -1;
			}
		}
	} else {
		return -1;
	}

	res->s = get_static_buffer(ip_str.len+port_str.len+proto_str.len+3);
	if (!res->s) return -1;

	pos=0;
	if (param & SEL_PROTO) {
		memcpy(res->s, proto_str.s, proto_str.len);
		pos += proto_str.len;
	}
	if (param & SEL_IP) {
		if (pos) res->s[pos++] = ':';
		memcpy(res->s+pos, ip_str.s, ip_str.len);
		pos += ip_str.len;
	}
	if (param & SEL_PORT) {
		if (pos) res->s[pos++] = ':';
		memcpy(res->s+pos, port_str.s, port_str.len);
		pos += port_str.len;
	}
	res->s[pos] = 0;
	res->len = pos;
	return (pos==0 ? 1 : 0);

}


int select_expires(str* res, select_t* s, struct sip_msg* msg)
{
	if (!msg->expires && (parse_headers(msg, HDR_EXPIRES_F, 0) == -1)) {
		return -1; /* error */
	}

	if (!msg->expires) {
		return 1;  /* null */
	}

	if (msg->expires->parsed == NULL && parse_expires(msg->expires) < 0) {
		return -1;
	}

	RETURN0_res(((struct exp_body*)msg->expires->parsed)->text);
}

#define SELECT_plain_header(_sel_name_,_fld_name_,_hdr_f_) \
int select_##_sel_name_(str* res, select_t* s, struct sip_msg* msg) \
{ \
	if (!msg->_fld_name_ && (parse_headers(msg, _hdr_f_, 0) == -1)) { \
		return -1; \
	} \
	if (!msg->_fld_name_) { \
		return 1; \
	} \
	RETURN0_res(msg->_fld_name_->body); \
}

SELECT_plain_header(call_id, callid, HDR_CALLID_F)
SELECT_plain_header(max_forwards, maxforwards, HDR_MAXFORWARDS_F)
SELECT_plain_header(content_type, content_type, HDR_CONTENTTYPE_F)
SELECT_plain_header(content_length, content_length, HDR_CONTENTLENGTH_F)
SELECT_plain_header(user_agent, user_agent, HDR_USERAGENT_F)
SELECT_plain_header(subject, subject, HDR_SUBJECT_F)
SELECT_plain_header(organization, organization, HDR_ORGANIZATION_F)
SELECT_plain_header(priority, priority, HDR_PRIORITY_F)
SELECT_plain_header(session_expires, session_expires, HDR_SESSIONEXPIRES_F)
SELECT_plain_header(min_se, min_se, HDR_MIN_SE_F)
SELECT_plain_header(sip_if_match, sipifmatch, HDR_SIPIFMATCH_F)
SELECT_plain_header(date, date, HDR_DATE_F)
SELECT_plain_header(identity, identity, HDR_IDENTITY_F)
SELECT_plain_header(identity_info, identity_info, HDR_IDENTITY_INFO_F)

int select_msg_request(str* res, select_t* s, struct sip_msg* msg)
{
	if (msg->first_line.type==SIP_REQUEST) { 
		return select_msg(res, s, msg); 
	}
	else
		return -1;	
}

int select_msg_response(str* res, select_t* s, struct sip_msg* msg)
{
	if (msg->first_line.type==SIP_REPLY) {
		return select_msg(res, s, msg); 
	}
	else
		return -1;	
}

#define SELECT_first_line(_sel_name_,_type_,_fld_) \
int select_msg_##_sel_name_(str* res, select_t* s, struct sip_msg* msg) {\
	if (msg->first_line.type==_type_) { \
		RETURN0_res(msg->first_line._fld_); \
	} else return -1; \
}

SELECT_first_line(request_method,SIP_REQUEST,u.request.method)
SELECT_first_line(request_uri,SIP_REQUEST,u.request.uri)
SELECT_first_line(request_version,SIP_REQUEST,u.request.version)
SELECT_first_line(response_version,SIP_REPLY,u.reply.version)
SELECT_first_line(response_status,SIP_REPLY,u.reply.status)
SELECT_first_line(response_reason,SIP_REPLY,u.reply.reason)


int select_version(str* res, select_t* s, struct sip_msg* msg)
{
	switch (msg->first_line.type) { 
		case SIP_REQUEST:
			RETURN0_res(msg->first_line.u.request.version)
			break;
		case SIP_REPLY:
			RETURN0_res(msg->first_line.u.reply.version)
			break;
		default:
			return -1;
	}
}

ABSTRACT_F(select_sys)

int select_sys_pid(str* res, select_t* s, struct sip_msg* msg) {
	return uint_to_static_buffer(res, getpid());
}

int select_sys_server_id(str* res, select_t* s, struct sip_msg* msg) {
	return int_to_static_buffer(res, server_id);
}

int select_sys_unique(str* res, select_t* s, struct sip_msg* msg) {
	#define UNIQUE_ID_PID_LEN 4
	#define UNIQUE_ID_TIME_LEN 8
	#define UNIQUE_ID_FIX_LEN (UNIQUE_ID_PID_LEN+1+UNIQUE_ID_TIME_LEN+1)
	#define UNIQUE_ID_RAND_LEN 8
	static char uniq_id[UNIQUE_ID_FIX_LEN+UNIQUE_ID_RAND_LEN];
	static int uniq_for_pid = -1;
	int i;

	if (uniq_for_pid != getpid()) {
		/* first call for this process */
		int cb, rb, x, l;
		char *c;
		/* init gloabally uniq part */
		c = int2str_base_0pad(getpid(), &l, 16, UNIQUE_ID_PID_LEN);
		memcpy(uniq_id, c, UNIQUE_ID_PID_LEN);
		uniq_id[UNIQUE_ID_PID_LEN] = '-';
		c = int2str_base_0pad(time(NULL), &l, 16, UNIQUE_ID_TIME_LEN);
		memcpy(uniq_id+UNIQUE_ID_PID_LEN+1, c, UNIQUE_ID_TIME_LEN);
		uniq_id[UNIQUE_ID_PID_LEN+1+UNIQUE_ID_TIME_LEN] = '-';

		/* init random part */
		for (i = RAND_MAX, rb=0; i; rb++, i>>=1);
		for (i = UNIQUE_ID_FIX_LEN, cb = 0, x = 0; i < UNIQUE_ID_FIX_LEN+UNIQUE_ID_RAND_LEN; i++) {
			if (!cb) {
				cb = rb;
				x = rand();
			}
			uniq_id[i] = fourbits2char[x & 0x0F];
			x >>= rb;
			cb -= rb;
		}
		uniq_for_pid = getpid();
	}
        for (i = UNIQUE_ID_FIX_LEN + UNIQUE_ID_RAND_LEN - 1; i >= UNIQUE_ID_FIX_LEN; i--) {
		switch (uniq_id[i]) {
			case '9':
				uniq_id[i]='a';
				i = 0;
				break;
			case 'f':
				uniq_id[i]='0';
				/* go on */
				break;
			default:
				uniq_id[i]++;
				i = 0;
				break;
		}
	}
	res->s = uniq_id;   /* I think it's not worth copying at static buffer, I hope there is no real meaning of @sys.unique==@sys.unique */
	res->len = UNIQUE_ID_FIX_LEN+UNIQUE_ID_RAND_LEN;
	return 0;
}


int select_sys_now(str* res, select_t* s, struct sip_msg* msg) {
	return uint_to_static_buffer(res, time(NULL));
}

int select_sys_now_fmt(str* res, select_t* s, struct sip_msg* msg)
{
#define SEL_POS 2
	time_t t;
	struct tm *tm;
	
	t = time(NULL);
	switch (s->params[SEL_POS].v.i) {
		case SEL_NOW_GMT:
			tm = gmtime(&t);
			break;
	
		case SEL_NOW_LOCAL:
			tm = localtime(&t);
			break;
		default:
			BUG("Unexpected parameter value 'now' \"%d\"\n", s->params[SEL_POS].v.i);
			return -1;
	}
	if (s->n <= SEL_POS+1) {
		char *c;
		c = asctime(tm);
		res->len = strlen(c);
		while (res->len && c[res->len-1] < ' ') res->len--; /* rtrim */
		res->s = get_static_buffer(res->len);
		if (!res->s) return -1;
		memcpy(res->s, c, res->len);
	}
	else {
		char c, buff[80];
		c = s->params[SEL_POS+1].v.s.s[s->params[SEL_POS+1].v.s.len];
		s->params[SEL_POS+1].v.s.s[s->params[SEL_POS+1].v.s.len] = '\0';
		res->len = strftime(buff, sizeof(buff)-1, s->params[SEL_POS+1].v.s.s, tm);
		s->params[SEL_POS+1].v.s.s[s->params[SEL_POS+1].v.s.len] = c;
		res->s = get_static_buffer(res->len);
		if (!res->s) return -1;
		memcpy(res->s, buff, res->len);
	}
	return 0;
#undef SEL_POS
}

ABSTRACT_F(select_branch)

int select_branch_count(str* res, select_t* s, struct sip_msg* msg) {
	return uint_to_static_buffer(res, nr_branches);
}

int select_branch_uri(str* res, select_t* s, struct sip_msg* msg) {
#define SEL_POS 1 
#define Q_PARAM ">;q="
#define Q_PARAM_LEN (sizeof(Q_PARAM) - 1)

	qvalue_t q;
	int l, n;
	str dst_uri;
	if (s->n <= SEL_POS+1 && nr_branches > 1) { /* get all branches, if nr_branches==1 then use faster algorithm */
		int len;
		unsigned l2;
		char *c;
		init_branch_iterator();
		len = 0;
		while ((c = next_branch(&l, &q, &dst_uri, 0, 0, 0, 0, 0, 0))) {

			if (s->params[SEL_POS].v.i & SEL_BRANCH_DST_URI) {
				l = dst_uri.len;
				c = dst_uri.s;
			}
			if (s->params[SEL_POS].v.i & (SEL_BRANCH_URI|SEL_BRANCH_DST_URI) ) {
				len += l;
			}
	                if (q != Q_UNSPECIFIED && (s->params[SEL_POS].v.i & SEL_BRANCH_Q)) {
				len += len_q(q);
				if (s->params[SEL_POS].v.i & SEL_BRANCH_URI) {
					len += 1 + Q_PARAM_LEN;
				}
			}
 			len += 1;
		}
		if (!len) return 1;
		res->s = get_static_buffer(len);
		if (!res->s) return -1;
		
		init_branch_iterator();
		res->len = 0;
		n = 0;
		while ((c = next_branch(&l, &q, &dst_uri, 0, 0, 0, 0, 0, 0))) {
			if (s->params[SEL_POS].v.i & SEL_BRANCH_DST_URI) {
				l = dst_uri.len;
				c = dst_uri.s;
			}
			if (n) {
				res->s[res->len] = ',';
				res->len++;
			}
			if ((s->params[SEL_POS].v.i & SEL_BRANCH_Q) == 0) {
				q = Q_UNSPECIFIED;
			}
			if ((s->params[SEL_POS].v.i & (SEL_BRANCH_URI|SEL_BRANCH_DST_URI)) && q != Q_UNSPECIFIED) {
				res->s[res->len] = '<';
				res->len++;
				memcpy(res->s+res->len, c, l);
				res->len += l; 
				memcpy(res->s+res->len, Q_PARAM, Q_PARAM_LEN);
				res->len += Q_PARAM_LEN;
				c = q2str(q, &l2); l = l2;
				memcpy(res->s+res->len, c, l);
				res->len += l; 
			}
			else if (s->params[SEL_POS].v.i & (SEL_BRANCH_URI|SEL_BRANCH_DST_URI)) {
				memcpy(res->s+res->len, c, l);
				res->len += l; 
			}
			else if (q != Q_UNSPECIFIED) {
				c = q2str(q, &l2); l = l2;
				memcpy(res->s+res->len, c, l);
				res->len += l; 
			}
			n++;
		}
	}
	else {
		unsigned l2;
		char *c;
		n = s->params[SEL_POS+1].v.i;
		if (n < 0 || n >= nr_branches) 
			return -1;
		init_branch_iterator();
		for (; (c = next_branch(&l, &q, &dst_uri, 0, 0, 0, 0, 0, 0)) && n; n--);
		if (!c) return 1;
		
		if (s->params[SEL_POS].v.i & SEL_BRANCH_DST_URI) {
			l = dst_uri.len;
			c = dst_uri.s;
		}
		if ((s->params[SEL_POS].v.i & SEL_BRANCH_Q) == 0) {
			q = Q_UNSPECIFIED;
		}
		if ((s->params[SEL_POS].v.i & (SEL_BRANCH_URI|SEL_BRANCH_DST_URI)) && q != Q_UNSPECIFIED) {

			res->s = get_static_buffer(l + 1 + Q_PARAM_LEN + len_q(q));
			if (!res->s) return -1;
			res->len = 1;
			res->s[0] = '<';
			memcpy(res->s+res->len, c, l);
			res->len += l; 
			memcpy(res->s+res->len, Q_PARAM, Q_PARAM_LEN);
			res->len += Q_PARAM_LEN;
			c = q2str(q, &l2); l = l2;
			memcpy(res->s+res->len, c, l);
			res->len += l; 
		}
		else if (s->params[SEL_POS].v.i & (SEL_BRANCH_URI|SEL_BRANCH_DST_URI)) {
			res->s = c;  /* not necessary to copy to static buffer */
			res->len = l;
		} 
		else if (q != Q_UNSPECIFIED) {
			c = q2str(q, &l2);
			res->len = l2;
			res->s = get_static_buffer(res->len);
			if (!res->s) return -1;
			memcpy(res->s, c, res->len);
		}
		else {
			res->len = 0;
		}
	}
	return 0;
#undef SEL_POS
}

int select_branch_uriq(str* res, select_t* s, struct sip_msg* msg) {
	return select_branch_uri(res, s, msg);
}

int select_branch_q(str* res, select_t* s, struct sip_msg* msg) {
	return select_branch_uri(res, s, msg);
}

int select_branch_dst_uri(str* res, select_t* s, struct sip_msg* msg) {
	return select_branch_uri(res, s, msg);
}
