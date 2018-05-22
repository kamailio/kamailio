/**
 *
 * Copyright (C) 2009 SIP-Router.org
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief SIP-router topoh ::
 * \ingroup topoh
 * Module: \ref topoh
 */

#include <string.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/data_lump.h"
#include "../../core/forward.h"
#include "../../core/trim.h"
#include "../../core/msg_translator.h"
#include "../../core/parser/parse_rr.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_via.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_refer_to.h"
#include "th_mask.h"
#include "th_msg.h"

extern str th_cookie_name;
extern str th_cookie_value;
extern str th_via_prefix;
extern str th_uri_prefix;
extern str th_callid_prefix;

extern str th_ip;
extern str th_uparam_name;
extern str th_uparam_prefix;
extern str th_vparam_name;
extern str th_vparam_prefix;

extern int th_param_mask_callid;
extern int th_mask_addr_myself;
extern int th_uri_prefix_checks;

int th_skip_rw(char *s, int len)
{
	while(len>0)
	{
		if(s[len-1]==' ' || s[len-1]=='\t' || s[len-1]=='\n' || s[len-1]=='\r'
				|| s[len-1]==',')
			len--;
		else return len;
	}
	return 0;
}

struct via_param *th_get_via_param(struct via_body *via, str *name)
{
	struct via_param *p;
	for(p=via->param_lst; p; p=p->next)
	{
		if(p->name.len==name->len
				&& strncasecmp(p->name.s, name->s, name->len)==0)
			return p;
	}
	return NULL;
}

int th_get_param_value(str *in, str *name, str *value)
{
	param_t* params = NULL;
	param_t* p = NULL;
	param_hooks_t phooks;
	if (parse_params(in, CLASS_ANY, &phooks, &params)<0)
		return -1;
	for (p = params; p; p=p->next)
	{
		if (p->name.len==name->len
				&& strncasecmp(p->name.s, name->s, name->len)==0)
		{
			*value = p->body;
			free_params(params);
			return 0;
		}
	}

	if(params) free_params(params);
	return 1;

}

int th_get_uri_param_value(str *uri, str *name, str *value)
{
	struct sip_uri puri;

	memset(value, 0, sizeof(str));
	if(parse_uri(uri->s, uri->len, &puri)<0)
		return -1;
	return th_get_param_value(&puri.params, name, value);
}

int th_get_uri_type(str *uri, int *mode, str *value)
{
	struct sip_uri puri;
	int ret;
	str r2 = {"r2", 2};

	memset(value, 0, sizeof(str));
	*mode = 0;
	if(parse_uri(uri->s, uri->len, &puri)<0)
		return -1;

	LM_DBG("PARAMS [%.*s]\n", puri.params.len, puri.params.s);
	if(puri.host.len==th_ip.len
			&& strncasecmp(puri.host.s, th_ip.s, th_ip.len)==0)
	{
		/* host matches TH ip */
		ret = th_get_param_value(&puri.params, &th_uparam_name, value);
		if(ret<0)
			return -1; /* eroor parsing parameters */
		if(ret==0)
			return 2; /* param found - decode */
		if(th_mask_addr_myself==0)
			return 0; /* param not found - skip */
	}

	if(check_self(&puri.host, puri.port_no, 0)==1)
	{
		/* myself -- matched on all protos */
		ret = th_get_param_value(&puri.params, &r2, value);
		if(ret<0)
			return -1;
		if(ret==1) /* not found */
			return 0; /* skip */
		LM_DBG("VALUE [%.*s]\n",
				value->len, value->s);
		if(value->len==2 && strncasecmp(value->s, "on", 2)==0)
			*mode = 1;
		memset(value, 0, sizeof(str));
		return 0; /* skip */
	}
	/* not myself & not mask ip */
	return 1; /* encode */
}

int th_mask_via(sip_msg_t *msg)
{
	hdr_field_t *hdr;
	struct via_body *via;
	struct lump* l;
	int i;
	str out;
	int vlen;

	i=0;
	for(hdr=msg->h_via1; hdr; hdr=next_sibling_hdr(hdr))
	{
		for(via=(struct via_body*)hdr->parsed; via; via=via->next)
		{
			i++;
			LM_DBG("=======via[%d]\n", i);
			LM_DBG("hdr: [%.*s]\n", via->hdr.len, via->hdr.s);
			vlen = th_skip_rw(via->name.s, via->bsize);
			LM_DBG("body: %d: [%.*s]\n", vlen, vlen, via->name.s);
			if(i!=1)
			{
				out.s = th_mask_encode(via->name.s, vlen, &th_via_prefix,
						&out.len);
				if(out.s==NULL)
				{
					LM_ERR("cannot encode via %d\n", i);
					return -1;
				}

				LM_DBG("+body: %d: [%.*s]\n", out.len, out.len, out.s);
				l=del_lump(msg, via->name.s-msg->buf, vlen, 0);
				if (l==0)
				{
					LM_ERR("failed deleting via [%d]\n", i);
					pkg_free(out.s);
					return -1;
				}
				if (insert_new_lump_after(l, out.s, out.len, 0)==0){
					LM_ERR("could not insert new lump\n");
					pkg_free(out.s);
					return -1;
				}
			}
		}
	}
	return 0;
}

int th_mask_callid(sip_msg_t *msg)
{
	struct lump* l;
	str out;

	if(th_param_mask_callid==0)
		return 0;

	if(msg->callid==NULL)
	{
		LM_ERR("cannot get Call-Id header\n");
		return -1;
	}

	out.s = th_mask_encode(msg->callid->body.s, msg->callid->body.len,
				&th_callid_prefix, &out.len);
	if(out.s==NULL)
	{
		LM_ERR("cannot encode callid\n");
		return -1;
	}

	l=del_lump(msg, msg->callid->body.s-msg->buf, msg->callid->body.len, 0);
	if (l==0)
	{
		LM_ERR("failed deleting callid\n");
		pkg_free(out.s);
		return -1;
	}
	if (insert_new_lump_after(l, out.s, out.len, 0)==0) {
		LM_ERR("could not insert new lump\n");
		pkg_free(out.s);
		return -1;
	}

	return 0;
}

int th_mask_contact(sip_msg_t *msg)
{
	struct lump* l;
	str out;
	str in;
	char *p;
	contact_t *c;

	if(msg->contact==NULL)
	{
		LM_DBG("no contact header\n");
		return 0;
	}

	if(parse_contact(msg->contact) < 0)
	{
		LM_ERR("failed parsing contact header\n");
		return -1;
	}

	c = ((contact_body_t*)msg->contact->parsed)->contacts;
	in = c->uri;

	out.s = th_mask_encode(in.s, in.len, &th_uri_prefix, &out.len);
	if(out.s==NULL)
	{
		LM_ERR("cannot encode contact uri\n");
		return -1;
	}
	if(*(in.s-1)!='<')
	{
		/* add < > around contact uri if not there */
		p = (char*)pkg_malloc(out.len+3);
		if(p==NULL)
		{
			LM_ERR("failed to get more pkg\n");
			pkg_free(out.s);
			return -1;
		}
		*p = '<';
		strncpy(p+1, out.s, out.len);
		p[out.len+1] = '>';
		p[out.len+2] = '\0';
		pkg_free(out.s);
		out.s = p;
		out.len += 2;
	}

	l=del_lump(msg, in.s-msg->buf, in.len, 0);
	if (l==0)
	{
		LM_ERR("failed deleting contact uri\n");
		pkg_free(out.s);
		return -1;
	}
	if (insert_new_lump_after(l, out.s, out.len, 0)==0) {
		LM_ERR("could not insert new lump\n");
		pkg_free(out.s);
		return -1;
	}

	return 0;
}

int th_mask_record_route(sip_msg_t *msg)
{
	hdr_field_t *hdr;
	struct lump* l;
	int i;
	rr_t *rr;
	str out;

	if(msg->record_route==NULL)
	{
		LM_DBG("no record route header\n");
		return 0;
	}
	hdr = msg->record_route;
	i = 0;
	while(hdr!=NULL)
	{
		if (parse_rr(hdr) < 0)
		{
			LM_ERR("failed to parse RR\n");
			return -1;
		}

		rr =(rr_t*)hdr->parsed;
		while(rr)
		{
			i++;
			if(i!=1)
			{
				out.s = th_mask_encode(rr->nameaddr.uri.s, rr->nameaddr.uri.len,
						&th_uri_prefix, &out.len);
				if(out.s==NULL)
				{
					LM_ERR("cannot encode r-r %d\n", i);
					return -1;
				}
				l=del_lump(msg, rr->nameaddr.uri.s-msg->buf,
						rr->nameaddr.uri.len, 0);
				if (l==0)
				{
					LM_ERR("failed deleting r-r [%d]\n", i);
					pkg_free(out.s);
					return -1;
				}
				if (insert_new_lump_after(l, out.s, out.len, 0)==0){
					LM_ERR("could not insert new lump\n");
					pkg_free(out.s);
					return -1;
				}
			}
			rr = rr->next;
		}
		hdr = next_sibling_hdr(hdr);
	}

	return 0;
}

int th_unmask_via(sip_msg_t *msg, str *cookie)
{
	hdr_field_t *hdr;
	struct via_body *via;
	struct via_body *via2;
	struct via_param *vp;
	struct lump* l;
	int i;
	str out;
	int vlen;

	i=0;
	for(hdr=msg->h_via1; hdr; hdr=next_sibling_hdr(hdr))
	{
		for(via=(struct via_body*)hdr->parsed; via; via=via->next)
		{
			i++;
			LM_DBG("=======via[%d]\n", i);
			LM_DBG("hdr: [%.*s]\n", via->hdr.len, via->hdr.s);
			vlen = th_skip_rw(via->name.s, via->bsize);
			LM_DBG("body: %d: [%.*s]\n", vlen, vlen, via->name.s);
			if(i!=1)
			{
				/* Skip if via is not encoded */
				if (th_uri_prefix_checks && (via->host.len!=th_ip.len
						|| strncasecmp(via->host.s, th_ip.s, th_ip.len)!=0))
				{
					LM_DBG("via %d is not encoded",i);
					continue;
				}

				vp = th_get_via_param(via, &th_vparam_name);
				if(vp==NULL)
				{
					LM_ERR("cannot find param in via %d\n", i);
					return -1;
				}
				if(i==2)
					out.s = th_mask_decode(vp->value.s, vp->value.len,
							&th_vparam_prefix, CRLF_LEN+1, &out.len);
				else
					out.s = th_mask_decode(vp->value.s, vp->value.len,
							&th_vparam_prefix, 0, &out.len);
				if(out.s==NULL)
				{
					LM_ERR("cannot decode via %d\n", i);
					return -1;
				}

				LM_DBG("+body: %d: [%.*s]\n", out.len, out.len, out.s);
				if(i==2)
				{
					via2=pkg_malloc(sizeof(struct via_body));
					if (via2==0)
					{
						LM_ERR("out of memory\n");
						pkg_free(out.s);
						return -1;

					}

					memset(via2, 0, sizeof(struct via_body));
					memcpy(out.s+out.len, CRLF, CRLF_LEN);
					out.s[out.len+CRLF_LEN]='X';
					if(parse_via(out.s, out.s+out.len+CRLF_LEN+1, via2)==NULL)
					{
						LM_ERR("error parsing decoded via2\n");
						free_via_list(via2);
						pkg_free(out.s);
						return -1;
					}
					out.s[out.len] = '\0';
					vp = th_get_via_param(via2, &th_cookie_name);
					if(vp==NULL)
					{
						LM_ERR("cannot find cookie in via2\n");
						free_via_list(via2);
						pkg_free(out.s);
						return -1;
					}
					*cookie = vp->value;
					free_via_list(via2);
				}
				l=del_lump(msg, via->name.s-msg->buf, vlen, 0);
				if (l==0)
				{
					LM_ERR("failed deleting via [%d]\n", i);
					pkg_free(out.s);
					return -1;
				}
				if (insert_new_lump_after(l, out.s, out.len, 0)==0)
				{
					LM_ERR("could not insert new lump\n");
					pkg_free(out.s);
					return -1;
				}
			}
		}
	}

	return 0;
}

int th_unmask_callid(sip_msg_t *msg)
{
	struct lump* l;
	str out;

	if(th_param_mask_callid==0)
		return 0;

	if(msg->callid==NULL)
	{
		LM_ERR("cannot get Call-Id header\n");
		return -1;
	}

	/* Do nothing if call-id is not encoded */
	if ((msg->callid->body.len<th_callid_prefix.len) ||
			(strncasecmp(msg->callid->body.s,th_callid_prefix.s,th_callid_prefix.len)!=0))
	{
		LM_DBG("call-id [%.*s] not encoded",msg->callid->body.len,msg->callid->body.s);
		return 0;
	}

	out.s = th_mask_decode(msg->callid->body.s, msg->callid->body.len,
					&th_callid_prefix, 0, &out.len);
	if(out.s==NULL)
	{
		LM_ERR("cannot decode callid\n");
		return -1;
	}

	l=del_lump(msg, msg->callid->body.s-msg->buf, msg->callid->body.len, 0);
	if (l==0)
	{
		LM_ERR("failed deleting callid\n");
		pkg_free(out.s);
		return -1;
	}
	if (insert_new_lump_after(l, out.s, out.len, 0)==0) {
		LM_ERR("could not insert new lump\n");
		pkg_free(out.s);
		return -1;
	}

	return 0;
}

#define TH_CALLID_SIZE	256
int th_unmask_callid_str(str *icallid, str *ocallid)
{
	static char th_callid_buf[TH_CALLID_SIZE];
	str out;

	if(th_param_mask_callid==0)
		return 0;

	if(icallid->s==NULL) {
		LM_ERR("invalid Call-Id value\n");
		return -1;
	}

	if(th_callid_prefix.len>0) {
		if(th_callid_prefix.len >= icallid->len) {
			return 1;
		}
		if(strncmp(icallid->s, th_callid_prefix.s, th_callid_prefix.len)!=0) {
			return 1;
		}
	}
	out.s = th_mask_decode(icallid->s, icallid->len,
					&th_callid_prefix, 0, &out.len);
	if(out.len>=TH_CALLID_SIZE) {
		pkg_free(out.s);
		LM_ERR("not enough callid buf size (needed %d)\n", out.len);
		return -2;
	}

	memcpy(th_callid_buf, out.s, out.len);
	th_callid_buf[out.len] = '\0';

	pkg_free(out.s);

	ocallid->s = th_callid_buf;
	ocallid->len = out.len;

	return 0;
}

int th_flip_record_route(sip_msg_t *msg, int mode)
{
	hdr_field_t *hdr;
	struct lump* l;
	int i;
	rr_t *rr;
	str out;
	int utype;
	str pval;
	int r2;
	int act;

	if(msg->record_route==NULL)
	{
		LM_DBG("no record route header\n");
		return 0;
	}
	hdr = msg->record_route;
	i = 0;
	act = 0;
	if(mode==1)
		act = 2;
	while(hdr!=NULL)
	{
		if (parse_rr(hdr) < 0)
		{
			LM_ERR("failed to parse RR\n");
			return -1;
		}

		rr =(rr_t*)hdr->parsed;
		while(rr)
		{
			i++;
			r2 = 0;
			utype = th_get_uri_type(&rr->nameaddr.uri, &r2, &pval);
			if(utype==0 && mode==1)
			{
				if(r2==1)
				{
					act--;
					if(act==0)
						return 0;
					utype = 1;
				} else {
					return 0;
				}
			}
			out.s = NULL;
			switch(utype) {
				case 1: /* encode */
					if(act!=0 && mode==1)
					{
						out.s = th_mask_encode(rr->nameaddr.uri.s,
							rr->nameaddr.uri.len, &th_uri_prefix, &out.len);
						if(out.s==NULL)
						{
							LM_ERR("cannot encode r-r %d\n", i);
							return -1;
						}
					}
				break;
				case 2: /* decode */
					if(mode==0)
					{
						out.s = th_mask_decode(pval.s,
							pval.len, &th_uparam_prefix, 0, &out.len);
						if(out.s==NULL)
						{
							LM_ERR("cannot decode r-r %d\n", i);
							return -1;
						}
					}
				break;
			}
			if(out.s!=NULL)
			{
				l=del_lump(msg, rr->nameaddr.uri.s-msg->buf,
						rr->nameaddr.uri.len, 0);
				if (l==0)
				{
					LM_ERR("failed deleting r-r [%d]\n", i);
					pkg_free(out.s);
					return -1;
				}
				if (insert_new_lump_after(l, out.s, out.len, 0)==0){
					LM_ERR("could not insert new lump\n");
					pkg_free(out.s);
					return -1;
				}
			}
			rr = rr->next;
		}
		hdr = next_sibling_hdr(hdr);
	}

	return 0;
}

int th_unmask_route(sip_msg_t *msg)
{
	hdr_field_t *hdr;
	struct lump* l;
	int i;
	rr_t *rr;
	str out;
	str eval;

	if(msg->route==NULL)
	{
		LM_DBG("no record route header\n");
		return 0;
	}
	hdr = msg->route;
	i = 0;
	while(hdr!=NULL)
	{
		if (parse_rr(hdr) < 0)
		{
			LM_ERR("failed to parse RR\n");
			return -1;
		}

		rr =(rr_t*)hdr->parsed;
		while(rr)
		{
			i++;
			if(i!=1)
			{
				/* Skip if route is not encoded */
				if (th_uri_prefix_checks
						&& ((rr->nameaddr.uri.len<th_uri_prefix.len) ||
						(strncasecmp(rr->nameaddr.uri.s,th_uri_prefix.s,
									th_uri_prefix.len)!=0)))
				{
					LM_DBG("rr %d is not encoded: [%.*s]", i,
							rr->nameaddr.uri.len, rr->nameaddr.uri.s);
					rr = rr->next;
					continue;
				}

				if(th_get_uri_param_value(&rr->nameaddr.uri, &th_uparam_name,
							&eval)<0 || eval.len<=0)
					return -1;

				out.s = th_mask_decode(eval.s, eval.len,
							&th_uparam_prefix, 0, &out.len);

				if(out.s==NULL)
				{
					LM_ERR("cannot decode R %d\n", i);
					return -1;
				}
				l=del_lump(msg, rr->nameaddr.uri.s-msg->buf,
						rr->nameaddr.uri.len, 0);
				if (l==0)
				{
					LM_ERR("failed deleting R [%d]\n", i);
					pkg_free(out.s);
					return -1;
				}
				if (insert_new_lump_after(l, out.s, out.len, 0)==0){
					LM_ERR("could not insert new lump\n");
					pkg_free(out.s);
					return -1;
				}
			}
			rr = rr->next;
		}
		hdr = next_sibling_hdr(hdr);
	}

	return 0;
}

int th_unmask_ruri(sip_msg_t *msg)
{
	str eval;
	struct lump* l;
	str out;

	/* Do nothing if ruri is not encoded */
	if (th_uri_prefix_checks && ((REQ_LINE(msg).uri.len<th_uri_prefix.len) ||
			(strncasecmp(REQ_LINE(msg).uri.s, th_uri_prefix.s,
						th_uri_prefix.len)!=0)))
	{
		LM_DBG("ruri [%.*s] is not encoded",REQ_LINE(msg).uri.len,REQ_LINE(msg).uri.s);
		return 0;
	}

	if(th_get_uri_param_value(&REQ_LINE(msg).uri, &th_uparam_name, &eval)<0
			|| eval.len<=0) {
		LM_DBG("no uri param [%.*s] in [%.*s]\n",
				th_uparam_name.len, th_uparam_name.s,
				REQ_LINE(msg).uri.len,REQ_LINE(msg).uri.s);
		return -1;
	}

	out.s = th_mask_decode(eval.s, eval.len,
				&th_uparam_prefix, 0, &out.len);
	if(out.s==NULL)
	{
		LM_ERR("cannot decode r-uri [%.*s]\n",
				REQ_LINE(msg).uri.len,REQ_LINE(msg).uri.s);
		return -1;
	}

	LM_DBG("+decoded: %d: [%.*s]\n", out.len, out.len, out.s);
	l=del_lump(msg, REQ_LINE(msg).uri.s-msg->buf, REQ_LINE(msg).uri.len, 0);
	if (l==0)
	{
		LM_ERR("failed deleting r-uri\n");
		pkg_free(out.s);
		return -1;
	}
	if (insert_new_lump_after(l, out.s, out.len, 0)==0)
	{
		LM_ERR("could not insert new lump\n");
		pkg_free(out.s);
		return -1;
	}

	return 0;
}

int th_unmask_refer_to(sip_msg_t *msg)
{
	str eval;
	str *uri;
	int ulen;
	struct lump* l;
	str out;

	if(!((get_cseq(msg)->method_id)&(METHOD_REFER)))
		return 0;

	if(parse_refer_to_header(msg)==-1)
	{
		LM_DBG("no Refer-To header\n");
		return 0;
	}
	if(msg->refer_to==NULL || get_refer_to(msg)==NULL)
	{
		LM_DBG("Refer-To header not found\n");
		return 0;
	}

	uri = &(get_refer_to(msg)->uri);

	/* Do nothing if refer_to is not encoded */
	if (th_uri_prefix_checks && ((uri->len<th_uri_prefix.len)
			|| (strncasecmp(uri->s, th_uri_prefix.s, th_uri_prefix.len)!=0)))
	{
		LM_DBG("refer-to [%.*s] is not encoded",uri->len,uri->s);
		return 0;
	}

	if(th_get_uri_param_value(uri, &th_uparam_name, &eval)<0
			|| eval.len<=0)
		return -1;

	out.s = th_mask_decode(eval.s, eval.len,
				&th_uparam_prefix, 0, &out.len);
	if(out.s==NULL)
	{
		LM_ERR("cannot decode r-uri\n");
		return -1;
	}

	LM_DBG("+decoded: %d: [%.*s]\n", out.len, out.len, out.s);
	for(ulen=0; ulen<uri->len; ulen++)
	{
		if(uri->s[ulen]=='?')
			break;
	}

	l=del_lump(msg, uri->s-msg->buf, ulen, 0);
	if (l==0)
	{
		LM_ERR("failed deleting r-uri\n");
		pkg_free(out.s);
		return -1;
	}
	if (insert_new_lump_after(l, out.s, out.len, 0)==0)
	{
		LM_ERR("could not insert new lump\n");
		pkg_free(out.s);
		return -1;
	}

	return 0;
}

int th_update_hdr_replaces(sip_msg_t *msg)
{
	struct hdr_field *hf = NULL;
	str replaces;
	str rcallid;
	struct lump* l;
	str out;

	LM_DBG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	if(th_param_mask_callid==0)
		return 0;

	if(!((get_cseq(msg)->method_id)&(METHOD_INVITE)))
		return 0;

	for (hf=msg->headers; hf; hf=hf->next)
	{
		if (hf->name.len==8 && strncasecmp(hf->name.s, "Replaces", 8)==0)
			break;
	}

	if(hf==NULL)
		return 0;

	replaces = hf->body;
	trim(&replaces);
	rcallid.s = replaces.s;
	for(rcallid.len=0; rcallid.len<replaces.len; rcallid.len++)
	{
		if(rcallid.s[rcallid.len]==';')
			break;
	}

	if(rcallid.len>th_callid_prefix.len
			&& strncmp(rcallid.s, th_callid_prefix.s, th_callid_prefix.len)==0)
	{
		/* value encoded - decode it */
		out.s = th_mask_decode(rcallid.s, rcallid.len,
					&th_callid_prefix, 0, &out.len);
	} else {
		/* value decoded - encode it */
		out.s = th_mask_encode(rcallid.s, rcallid.len,
				&th_callid_prefix, &out.len);
	}
	if(out.s==NULL)
	{
		LM_ERR("cannot update Replaces callid\n");
		return -1;
	}

	l=del_lump(msg, rcallid.s-msg->buf, rcallid.len, 0);
	if (l==0)
	{
		LM_ERR("failed deleting Replaces callid\n");
		pkg_free(out.s);
		return -1;
	}
	if (insert_new_lump_after(l, out.s, out.len, 0)==0) {
		LM_ERR("could not insert new lump\n");
		pkg_free(out.s);
		return -1;
	}

	return 0;
}

char* th_msg_update(sip_msg_t *msg, unsigned int *olen)
{
	struct dest_info dst;

	init_dest_info(&dst);
	dst.proto = PROTO_UDP;
	return build_req_buf_from_sip_req(msg,
			olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
}

int th_add_via_cookie(sip_msg_t *msg, struct via_body *via)
{
	struct lump* l;
	int viap;
	str out;

	if (via->params.s) {
		viap = via->params.s - via->hdr.s - 1;
	} else {
		viap = via->host.s - via->hdr.s + via->host.len;
		if (via->port!=0)
			viap += via->port_str.len + 1; /* +1 for ':'*/
	}
	l = anchor_lump(msg, via->hdr.s - msg->buf + viap, 0, 0);
	if (l==0)
	{
		LM_ERR("failed adding cookie to via [%p]\n", via);
		return -1;
	}

	out.len = 1+th_cookie_name.len+1+th_cookie_value.len+1;
	out.s = (char*)pkg_malloc(out.len+1);
	if(out.s==0)
	{
		LM_ERR("no pkg memory\n");
		return -1;
	}
	out.s[0] = ';';
	memcpy(out.s+1, th_cookie_name.s, th_cookie_name.len);
	out.s[th_cookie_name.len+1]='=';
	memcpy(out.s+th_cookie_name.len+2, th_cookie_value.s, th_cookie_value.len);
	out.s[out.len-1] = 'v';
	out.s[out.len] = '\0';
	if (insert_new_lump_after(l, out.s, out.len, 0)==0){
		LM_ERR("could not insert new lump!\n");
		pkg_free(out.s);
		return -1;
	}
	return 0;
}

int th_add_hdr_cookie(sip_msg_t *msg)
{
	struct lump* anchor;
	str h;

	h.len = th_cookie_name.len + 2 + th_cookie_value.len + 1 + CRLF_LEN;
	h.s = (char*)pkg_malloc(h.len+1);
	if(h.s == 0)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if(anchor == 0)
	{
		LM_ERR("can't get anchor\n");
		pkg_free(h.s);
		return -1;
	}
	memcpy(h.s, th_cookie_name.s, th_cookie_name.len);
	memcpy(h.s+th_cookie_name.len, ": ", 2);
	memcpy(h.s+th_cookie_name.len+2, th_cookie_value.s, th_cookie_value.len);
	memcpy(h.s+th_cookie_name.len+2+th_cookie_value.len+1, CRLF, CRLF_LEN);
	h.s[h.len-1-CRLF_LEN] = 'h';
	h.s[h.len] = '\0';
	if (insert_new_lump_before(anchor, h.s, h.len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(h.s);
		return -1;
	}
	LM_DBG("added cookie header [%s]\n", h.s);
	return 0;
}

struct via_param *th_get_via_cookie(sip_msg_t *msg, struct via_body *via)
{
	struct via_param *p;

	if (!via) {
		return NULL;
	}
	for(p=via->param_lst; p; p=p->next)
	{
		if(p->name.len==th_cookie_name.len
				&& strncasecmp(p->name.s, th_cookie_name.s,
					th_cookie_name.len)==0)
			return p;
	}
	return NULL;
}

hdr_field_t *th_get_hdr_cookie(sip_msg_t *msg)
{
	hdr_field_t *hf;
	for (hf=msg->headers; hf; hf=hf->next)
	{
		if (hf->name.len==th_cookie_name.len
				&& strncasecmp(hf->name.s, th_cookie_name.s,
					th_cookie_name.len)==0)
			return hf;
	}
	return NULL;
}

int th_add_cookie(sip_msg_t *msg)
{
	if(th_cookie_value.len<=0)
		return 0;
	th_add_hdr_cookie(msg);
	th_add_via_cookie(msg, msg->via1);
	return 0;
}

int th_del_hdr_cookie(sip_msg_t *msg)
{
	hdr_field_t *hf;
	struct lump* l;
	for (hf=msg->headers; hf; hf=hf->next)
	{
		if (hf->name.len==th_cookie_name.len
				&& strncasecmp(hf->name.s, th_cookie_name.s,
					th_cookie_name.len)==0)
		{
			l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
			if (l==0) {
				LM_ERR("unable to delete cookie header\n");
				return -1;
			}
			return 0;
		}
	}
	return 0;
}

int th_del_via_cookie(sip_msg_t *msg, struct via_body *via)
{
	struct via_param *p;
	struct lump* l;

	if(via==NULL) {
		LM_DBG("no via header\n");
		return 0;
	}
	for(p=via->param_lst; p; p=p->next)
	{
		if(p->name.len==th_cookie_name.len
				&& strncasecmp(p->name.s, th_cookie_name.s,
					th_cookie_name.len)==0)
		{
			l=del_lump(msg, p->start-msg->buf-1, p->size+1, 0);
			if (l==0) {
				LM_ERR("unable to delete cookie header\n");
				return -1;
			}
			return 0;
		}
	}
	return 0;
}

int th_del_cookie(sip_msg_t *msg)
{
	th_del_hdr_cookie(msg);
	if(msg->first_line.type==SIP_REPLY)
		th_del_via_cookie(msg, msg->via1);
	return 0;
}


/**
 * return the special topoh cookie
 * - TH header of TH Via parame
 * - value is 3 chars
 *   [0] - direction:    d - downstream; u - upstream
 *   [1] - request type: i - initial; c - in-dialog; l - local in-dialog
 *   [2] - location:     h - header; v - via param
 * - if not found, returns 'xxx'
 */
char* th_get_cookie(sip_msg_t *msg, int *clen)
{
	hdr_field_t *hf;
	struct via_param *p;

	hf = th_get_hdr_cookie(msg);
	if(hf!=NULL)
	{
		*clen = hf->body.len;
		return hf->body.s;
	}
	p = th_get_via_cookie(msg, msg->via1);
	if(p!=NULL)
	{
		*clen = p->value.len;
		return p->value.s;
	}

	*clen = 3;
	return "xxx";
}

int th_route_direction(sip_msg_t *msg)
{
	rr_t *rr;
	struct sip_uri puri;
	str ftn = {"ftag", 4};
	str ftv = {0, 0};

	if(get_from(msg)->tag_value.len<=0)
	{
		LM_ERR("failed to get from header tag\n");
		return -1;
	}
	if(msg->route==NULL)
	{
		LM_DBG("no route header - downstream\n");
		return 0;
	}
	if (parse_rr(msg->route) < 0)
	{
		LM_ERR("failed to parse route header\n");
		return -1;
	}

	rr =(rr_t*)msg->route->parsed;

	if (parse_uri(rr->nameaddr.uri.s, rr->nameaddr.uri.len, &puri) < 0) {
		LM_ERR("failed to parse the first route URI\n");
		return -1;
	}
	if(th_get_param_value(&puri.params, &ftn, &ftv)!=0)
		return 0;

	if(get_from(msg)->tag_value.len!=ftv.len
			|| strncmp(get_from(msg)->tag_value.s, ftv.s, ftv.len)!=0)
	{
		LM_DBG("ftag mismatch\n");
		return 1;
	}
	LM_DBG("ftag match\n");
	return 0;
}

int th_skip_msg(sip_msg_t *msg)
{
	if (msg->cseq==NULL || get_cseq(msg)==NULL) {
		LM_WARN("Invalid/Unparsed CSeq in message. Skipping.");
		return 1;
	}

	if((get_cseq(msg)->method_id)&(METHOD_REGISTER|METHOD_PUBLISH))
		return 1;

	return 0;
}

