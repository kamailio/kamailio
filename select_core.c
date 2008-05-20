/*
 * $Id$
 *
 * Copyright (C) 2005-2006 iptelorg GmbH
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
 *
 * History:
 * --------
 *  2005-12-19  select framework, basic core functions (mma)
 *  2006-01-19  multiple nested calls, IS_ALIAS -> NESTED flag renamed (mma)
 *  2006-02-17  fixup call for select_anyhdr (mma)
 */

 
#include "select.h"
#include "select_core.h"
#include "select_buf.h"
#include "dprint.h"
#include "trim.h"
#include "ut.h"
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
#include "sr_module.h"

#define RETURN0_res(x) {*res=x;return 0;}
#define TRIM_RET0_res(x) {*res=x;trim(res);return 0;} 
#define TEST_RET_res_body(x) if (x){*res=x->body;return 0;}else return 1;
#define TEST_RET_res_value(x) if (x){*res=x->value;return 0;}else return 1;

int select_method(str* res, select_t* s, struct sip_msg* msg)
{
	if (msg->first_line.type==SIP_REQUEST) {
		RETURN0_res(msg->first_line.u.request.method);
	} else return -1;
}

int select_ruri(str* res, select_t* s, struct sip_msg* msg)
{
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
			RETURN0_res(msg->new_uri);
		}
		else {
			RETURN0_res(msg->first_line.u.request.uri);
		}
	}
	return -1;
}

int select_from(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(msg->from->body);
}

int select_from_uri(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(get_from(msg)->uri);
}

int select_from_tag(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(get_from(msg)->tag_value);
}

int select_from_name(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(get_from(msg)->display);
}

int select_from_params(str* res, select_t* s, struct sip_msg* msg)
{
	struct to_param* p;
	if (parse_from_header(msg)<0)
		return -1;
	
	p = get_from(msg)->param_lst;
	while (p) {
		if ((p->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) {
			RETURN0_res(p->value);
		}
		p = p->next;
	}
	return 1;
}

int parse_to_header(struct sip_msg *msg)
{
	if ( !msg->to && ( parse_headers(msg,HDR_TO_F,0)==-1 || !msg->to)) {
		ERR("bad msg or missing TO header\n");
		return -1;
	}

	// HDR_TO_T is automatically parsed (get_hdr_field in parser/msg_parser.c)
	// so check only ptr validity
	if (msg->to->parsed)
		return 0;
	else
		return -1;
}

int select_to(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1; 
	RETURN0_res(msg->to->body);
}

int select_to_uri(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1;
	RETURN0_res(get_to(msg)->uri);
}

int select_to_tag(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1;
	RETURN0_res(get_to(msg)->tag_value);
}

int select_to_name(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1;
	RETURN0_res(get_to(msg)->display);
}

int select_to_params(str* res, select_t* s, struct sip_msg* msg)
{
	struct to_param* p;

	if (parse_to_header(msg)<0)
		return -1;
	
	p = get_to(msg)->param_lst;
	while (p) {
		if ((p->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) {
			RETURN0_res(p->value);
		}
		p = p->next;
	}
	return 1;
}

int parse_contact_header( struct sip_msg *msg)
{
        if ( !msg->contact && ( parse_headers(msg,HDR_CONTACT_F,0)==-1 || !msg->contact)) {
                DBG("bad msg or missing CONTACT header\n");
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
	case SEL_PARAM_METHOD:
		TEST_RET_res_body(c->method);
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

int parse_via_header( struct sip_msg *msg, int n, struct via_body** q)
{
	struct hdr_field *p;
	struct via_body *pp = NULL;
	int i;
	
	switch (n) {
	case 1:
	case 2:
		if (!msg->h_via1 && (parse_headers(msg,HDR_VIA_F,0)==-1 || !msg->h_via1)) {
                        DBG("bad msg or missing VIA1 header \n");
                        return -1;
                }
		pp = msg->h_via1->parsed;
		if (n==1) break;
		pp = pp->next;
		if (pp) break;
		
                if (!msg->h_via2 && (parse_headers(msg,HDR_VIA2_F,0)==-1 || !msg->h_via2)) {
                        DBG("bad msg or missing VIA2 header \n");
                        return -1;
                }
                pp = msg->h_via2->parsed;
                break;
	default:	
	        if (!msg->eoh && (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->eoh)) {
        	        ERR("bad msg while parsing to EOH \n");
	                return -1;
		}
		p = msg->h_via1;
		i = n;
		while (i && p) {
		        if (p->type == HDR_VIA_T) {
		        	i--;
		        	pp = p->parsed;
		        	while (i && (pp->next)) {
		        		i--;
		        		pp = pp->next;
		        	}
		        }
			p = p->next;
		}
		if (i > 0) {
			DBG("missing VIA[%d] header\n", n);
			return -1;
		}
	}
	if (pp) {
		*q = pp;
		return 0;
	} else
		return -1;
}

int select_via(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	if (((s->n == 1) || (s->params[1].type == SEL_PARAM_STR)) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
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
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->name);
}

int select_via_version(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->version);
}

int select_via_transport(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->transport);
}

int select_via_host(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->host);
}

int select_via_port(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->port_str);
}

int select_via_comment(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->comment);
}

int select_via_params(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	struct via_param *q;

	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
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
	if ((s->params[1].type == SEL_PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
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

//ABSTRACT_F(select_msgheader)
// Instead of ABSTRACT_F(select_msgheader)
// use function which uses select_core_table
// to avoid gcc warning about not used 
int select_msgheader(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_core_table.next)
		return -1;
	else
		return -1;
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
		if (s->n <2) return -1;

		if (s->params[1].type==SEL_PARAM_STR) {
				/* replace _ with - (for P-xxx headers) */
			for (hi=s->params[1].v.s.len-1; hi>0; hi--)
				if (s->params[1].v.s.s[hi]=='_')
					s->params[1].v.s.s[hi]='-';
				/* if header name is parseable, parse it and set SEL_PARAM_DIV */
			c=s->params[1].v.s.s[s->params[1].v.s.len];
			s->params[1].v.s.s[s->params[1].v.s.len]=':';
			if (parse_hname2(s->params[1].v.s.s,s->params[1].v.s.s+(s->params[1].v.s.len<3?4:s->params[1].v.s.len+1),
						&hdr)==0) {
				ERR("select_anyhdr:fixup_call:parse error\n");
				return -1;
			}
			s->params[1].v.s.s[s->params[1].v.s.len]=c;
			
			if (hdr.type!=HDR_OTHER_T && hdr.type!=HDR_ERROR_T) {
				/* pkg_free(s->params[1].v.s.s); */
				/* don't free it (the mem can leak only once at startup)
				 * the parsed string can live inside larger string block
				 * e.g. when xlog's select is parsed
				 */
				s->params[1].type = SEL_PARAM_DIV;
				s->params[1].v.i = hdr.type;
			}
		}
		return 1;
	}

	hf0 = NULL;

	/* extract header index if present */
	if (s->param_offset[select_level+1] == 3) {
		if (s->params[2].type == SEL_PARAM_INT) {
			hi = s->params[2].v.i;
		} else {
			hi = -1;
		}
	} else {
		hi = 1;
	}

	/* we need to be sure we have parsed all headers */
	if (!msg->eoh && (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->eoh)) {
		ERR("bad msg while parsing to EOH \n");
		return -1;
	}
	for (hf=msg->headers; hf; hf=hf->next) {
		if(s->params[1].type==SEL_PARAM_DIV) {
			if (s->params[1].v.i!=hf->type)	continue;
		} else if(s->params[1].type==SEL_PARAM_STR) {
			if (s->params[1].v.s.len!=hf->name.len)	continue;
			if (strncasecmp(s->params[1].v.s.s, hf->name.s, hf->name.len)!=0) continue;
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

ABSTRACT_F(select_anyheader_params)
ABSTRACT_F(select_any_uri)

static struct sip_uri uri;

int select_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	trim(res);
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	switch (uri.type) {
	case SIPS_URI_T:
	case TELS_URI_T:
		res->len=4;
		break;
	case SIP_URI_T:
	case TEL_URI_T:
		res->len=3;
		break;
	case ERROR_URI_T:
		return -1;
	}
	return 0;
}

int select_uri_user(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.user);
}

int select_uri_pwd(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.passwd);
}

int select_uri_host(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.host);
}

int select_uri_port(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.port);
}

int select_uri_hostport(str* res, select_t* s, struct sip_msg* msg)
{
	char* p;
	int size;
	
	if (parse_uri(res->s,res->len, &uri)<0)
		return -1;

	if (!uri.host.len)
		return -1;
	
	if (uri.port.len) {
		res->s=uri.host.s;
		res->len=uri.host.len+uri.port.len+1;
		return 0;
	}
	
	size=uri.host.len+5;
	if (!(p = get_static_buffer(size)))
		return -1;
			
	strncpy(p, uri.host.s, uri.host.len);
	switch (uri.type) {
		case SIPS_URI_T:
		case TELS_URI_T:
			strncpy(p+uri.host.len, ":5061", 5); 
			break;
		case SIP_URI_T:
		case TEL_URI_T:
			strncpy(p+uri.host.len, ":5060", 5);
			break;
		case ERROR_URI_T:
			return -1;
	}
	res->s = p;
	res->len = size;
	return 0;
}

int select_uri_params(str* res, select_t* s, struct sip_msg* msg)
{
	if (!msg || !res) {
		return select_any_params(res, s, msg);
	}

	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;
	
	if (s->param_offset[select_level+1]-s->param_offset[select_level]==1)
		RETURN0_res(uri.params);

	*res=uri.params;
	return select_any_params(res, s, msg);
}

int select_any_params(str* res, select_t* s, struct sip_msg* msg)
{
	param_hooks_t h;
	param_t *p, *list=NULL;
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
	if (parse_params(res, CLASS_ANY, &h, &list)<0) return -1;
	
	for (p = list; p; p=p->next) {
		if ((p->name.len==wanted->len) && 
			 !strncasecmp(p->name.s, wanted->s,wanted->len)) {
			*res=p->body;
			free_params(list);
			return (res->len ? 0 : 1);
		}
	}
	free_params(list);

	DBG("SELECT ...uri.params.%s NOT FOUND !\n", wanted->s);
	return -1;
}

int select_event(str* res, select_t* s, struct sip_msg* msg)
{
	if (!msg->event && parse_headers(msg, HDR_EVENT_F, 0) == -1) {
		ERR("Error while searching Event header field\n");
		return -1;
	}

	if (!msg->event) {
		DBG("Event header field not found\n");
		return -1;
	}

	if (parse_event(msg->event) < 0) {
		ERR("Error while parsing Event header field\n");
		return -1;
	}

	*res = ((event_t*)msg->event->parsed)->text;
	return 0;
}



static int parse_rr_header(struct sip_msg *msg)
{
        if ( !msg->record_route && ( parse_headers(msg,HDR_RECORDROUTE_F,0) == -1)) {
                ERR("bad msg or missing Record-Route header\n");
                return -1;
        }

	if (!msg->record_route) {
		DBG("No Record-Route header field found\n");
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
                ERR("Unable to parse CSeq header\n");
                return 0;
        }

	if (!msg->cseq) {
		DBG("No CSeqheader field found\n");
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
		DBG("select_nameaddr_name: no < found, whole string is uri\n");
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
		DBG("select_nameaddr_uri: no < found, string up to first semicolon is uri\n");
		p = q_memchr(res->s, ';', res->len);
		if (p)
			res->len = p-res->s;
		return 0;
	}

	res->len=res->len - (p-res->s) -1;
	res->s=p +1;
	
	p=find_not_quoted(res, '>');
	if (!p) {
		ERR("select_nameaddr_uri: no > found, invalid nameaddr value\n");
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
			ERR("select_nameaddr_params: no > found, invalid nameaddr value\n");
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
				ERR("BUG: select_ip_port: Unknown transport protocol\n");
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

