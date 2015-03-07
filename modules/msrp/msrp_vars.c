/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../pvapi.h"
#include "../../sr_module.h"

#include "msrp_parser.h"
#include "msrp_vars.h"

extern int msrp_tls_module_loaded;

/**
 *
 */
int pv_parse_msrp_name(pv_spec_t *sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3: 
			if(strncmp(in->s, "buf", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 4: 
			if(strncmp(in->s, "body", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "code", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else if(strncmp(in->s, "hdrs", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "msgid", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "conid", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 21;
			else goto error;
		case 6:
			if(strncmp(in->s, "method", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else if(strncmp(in->s, "buflen", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else if(strncmp(in->s, "sessid", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
			else if(strncmp(in->s, "reason", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 9;
			else if(strncmp(in->s, "crthop", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 12;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "bodylen", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 10;
			else if(strncmp(in->s, "transid", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 11;
			else if(strncmp(in->s, "prevhop", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 13;
			else if(strncmp(in->s, "nexthop", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 14;
			else if(strncmp(in->s, "lasthop", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 15;
			else if(strncmp(in->s, "srcaddr", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 19;
			else if(strncmp(in->s, "srcsock", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 20;
			else goto error;
		break;
		case 8: 
			if(strncmp(in->s, "firsthop", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 16;
			else if(strncmp(in->s, "prevhops", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 17;
			else if(strncmp(in->s, "nexthops", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 18;
			else goto error;
		break;
		default:
			/* max is 21 */
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV msrp name %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
int pv_get_msrp(sip_msg_t *msg,  pv_param_t *param, pv_value_t *res)
{
	msrp_frame_t *mf;
	msrp_hdr_t *hdr;
	str_array_t *sar;
	msrp_uri_t uri;
	str s;
	char *p;

	mf = msrp_get_current_frame();
	if(mf==NULL || param==NULL)
		return -1;

	sar = NULL;
	hdr = NULL;

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			s.s   = mf->buf.s;
			s.len = mf->buf.len;
			return pv_get_strval(msg, param, res, &s);
		case 2:
			if(mf->mbody.s==NULL)
				return pv_get_null(msg, param, res);
			s.s   = mf->mbody.s;
			s.len = mf->mbody.len;
			return pv_get_strval(msg, param, res, &s);
		case 3:
			if(mf->fline.msgtypeid==MSRP_REQUEST)
				return pv_get_null(msg, param, res);
			return pv_get_intstrval(msg, param, res,
					MSRP_RPL_CODE(mf->fline.rtypeid),
					&mf->fline.rtype);
		case 4:
			if(mf->hbody.s==NULL)
				return pv_get_null(msg, param, res);
			s.s   = mf->hbody.s;
			s.len = mf->hbody.len;
			return pv_get_strval(msg, param, res, &s);
		case 5:
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_MESSAGE_ID);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			s.s   = hdr->body.s;
			s.len = hdr->body.len;
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 6:
			if(mf->fline.msgtypeid==MSRP_REPLY)
				return pv_get_null(msg, param, res);
			return pv_get_strintval(msg, param, res, &mf->fline.rtype,
					mf->fline.rtypeid);
		case 7:
			return pv_get_uintval(msg, param, res, mf->buf.len);
		case 8:
			if(msrp_parse_hdr_to_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			s = sar->list[0];
			trim(&s);
			if(msrp_parse_uri(s.s, s.len, &uri)<0 || uri.session.len<=0)
				return pv_get_null(msg, param, res);
			s = uri.session;
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 9:
			if(mf->fline.msgtypeid==MSRP_REQUEST || mf->fline.rtext.s==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &mf->fline.rtext);
		case 10:
			return pv_get_uintval(msg, param, res, mf->mbody.len);
		case 11:
			s = mf->fline.transaction;
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 12:
			if(msrp_parse_hdr_to_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			s = sar->list[0];
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 13:
			if(msrp_parse_hdr_from_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			s = sar->list[0];
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 14:
			if(msrp_parse_hdr_to_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			if(sar->size<2)
				return pv_get_null(msg, param, res);
			s = sar->list[1];
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 15:
			if(msrp_parse_hdr_to_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			s = sar->list[sar->size-1];
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 16:
			if(msrp_parse_hdr_from_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			s = sar->list[sar->size-1];
			trim(&s);
			return pv_get_strval(msg, param, res, &s);
		case 17:
			if(msrp_parse_hdr_from_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			return pv_get_uintval(msg, param, res, sar->size);
		case 18:
			if(msrp_parse_hdr_to_path(mf)<0)
				return pv_get_null(msg, param, res);
			hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
			if(hdr==NULL)
				return pv_get_null(msg, param, res);
			sar = (str_array_t*)hdr->parsed.data;
			return pv_get_uintval(msg, param, res, sar->size-1);
		case 19:
			if(pv_get_buffer_size()<100)
				return pv_get_null(msg, param, res);
			s.s = pv_get_buffer();
			p = s.s;
			if (msrp_tls_module_loaded)
			{
				memcpy(p, "msrps://", 8);
				p+=8;
			} else {
				memcpy(p, "msrp://", 7);
				p+=7;
			}
			strcpy(p, ip_addr2a(&mf->tcpinfo->rcv->src_ip));
			strcat(p, ":");
			strcat(p, int2str(mf->tcpinfo->rcv->src_port, NULL));
			s.len = strlen(s.s);
			return pv_get_strval(msg, param, res, &s);
		case 20:
			return pv_get_strval(msg, param, res,
					&mf->tcpinfo->rcv->bind_address->sock_str);
		case 21:
			if(mf->tcpinfo->con==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_sintval(msg, param, res, mf->tcpinfo->con->id);
		default:
			return pv_get_null(msg, param, res);
	}

	return 0;
}

/**
 *
 */
int pv_set_msrp(sip_msg_t *msg, pv_param_t *param, int op,
		pv_value_t *val)
{
	return 0;
}

enum _tr_msrp_type { TR_MSRP_NONE=0, TR_MSRPURI };
enum _tr_msrpuri_subtype { 
	TR_MSRPURI_NONE=0, TR_MSRPURI_USER, TR_MSRPURI_HOST, TR_MSRPURI_PORT,
	TR_MSRPURI_SESSION, TR_MSRPURI_PROTO, TR_MSRPURI_USERINFO,
	TR_MSRPURI_PARAMS, TR_MSRPURI_SCHEME};

static str _tr_empty = { "", 0 };
static str _tr_msrpuri = {0, 0};
static msrp_uri_t _tr_parsed_msrpuri;

/**
 *
 */
int tr_msrp_eval_msrpuri(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	if(val==NULL || (!(val->flags&PV_VAL_STR)) || val->rs.len<=0)
		return -1;

	if(_tr_msrpuri.len==0 || _tr_msrpuri.len!=val->rs.len ||
			strncmp(_tr_msrpuri.s, val->rs.s, val->rs.len)!=0)
	{
		if(val->rs.len>_tr_msrpuri.len)
		{
			if(_tr_msrpuri.s) pkg_free(_tr_msrpuri.s);
			_tr_msrpuri.s = (char*)pkg_malloc((val->rs.len+1)*sizeof(char));
			if(_tr_msrpuri.s==NULL)
			{
				LM_ERR("no more private memory\n");
				memset(&_tr_msrpuri, 0, sizeof(str));
				memset(&_tr_parsed_msrpuri, 0, sizeof(msrp_uri_t));
				return -1;
			}
		}
		_tr_msrpuri.len = val->rs.len;
		memcpy(_tr_msrpuri.s, val->rs.s, val->rs.len);
		_tr_msrpuri.s[_tr_msrpuri.len] = '\0';
		/* reset old values */
		memset(&_tr_parsed_msrpuri, 0, sizeof(msrp_uri_t));
		/* parse uri */
		if(msrp_parse_uri(_tr_msrpuri.s, _tr_msrpuri.len,
					&_tr_parsed_msrpuri)!=0)
		{
			LM_ERR("invalid uri [%.*s]\n", val->rs.len,
					val->rs.s);
			pkg_free(_tr_msrpuri.s);
			memset(&_tr_msrpuri, 0, sizeof(str));
			memset(&_tr_parsed_msrpuri, 0, sizeof(msrp_uri_t));
			return -1;
		}
	}
	memset(val, 0, sizeof(pv_value_t));
	val->flags = PV_VAL_STR;

	switch(subtype)
	{
		case TR_MSRPURI_USER:
			val->rs = (_tr_parsed_msrpuri.user.s)?_tr_parsed_msrpuri.user:_tr_empty;
			break;
		case TR_MSRPURI_USERINFO:
			val->rs = (_tr_parsed_msrpuri.userinfo.s)?_tr_parsed_msrpuri.userinfo:_tr_empty;
			break;
		case TR_MSRPURI_HOST:
			val->rs = (_tr_parsed_msrpuri.host.s)?_tr_parsed_msrpuri.host:_tr_empty;
			break;
		case TR_MSRPURI_PORT:
			val->rs = (_tr_parsed_msrpuri.port.s)?_tr_parsed_msrpuri.port:_tr_empty;
			break;
		case TR_MSRPURI_SESSION:
			val->rs = (_tr_parsed_msrpuri.session.s)?_tr_parsed_msrpuri.session:_tr_empty;
			break;
		case TR_MSRPURI_PROTO:
			val->rs = (_tr_parsed_msrpuri.proto.s)?_tr_parsed_msrpuri.proto:_tr_empty;
			break;
		case TR_MSRPURI_PARAMS:
			val->rs = (_tr_parsed_msrpuri.params.s)?_tr_parsed_msrpuri.params:_tr_empty;
			break;
		case TR_MSRPURI_SCHEME:
			val->rs = (_tr_parsed_msrpuri.scheme.s)?_tr_parsed_msrpuri.scheme:_tr_empty;
			break;
		default:
			LM_ERR("unknown subtype %d\n",
					subtype);
			return -1;
	}
	return 0;
}

/**
 *
 */
char* tr_parse_msrpuri(str* in, trans_t *t)
{
	str name;
	char *p;

	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_MSRPURI;
	t->trf = tr_msrp_eval_msrpuri;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n",
				in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==4 && strncasecmp(name.s, "user", 4)==0)
	{
		t->subtype = TR_MSRPURI_USER;
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "host", 4)==0) {
		t->subtype = TR_MSRPURI_HOST;
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "port", 4)==0) {
		t->subtype = TR_MSRPURI_PORT;
		goto done;
	} else if(name.len==7 && strncasecmp(name.s, "session", 7)==0) {
		t->subtype = TR_MSRPURI_SESSION;
		goto done;
	} else if(name.len==5 && strncasecmp(name.s, "proto", 5)==0) {
		t->subtype = TR_MSRPURI_PROTO;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "scheme", 6)==0) {
		t->subtype = TR_MSRPURI_SCHEME;
		goto done;
	} else if(name.len==6 && strncasecmp(name.s, "params", 6)==0) {
		t->subtype = TR_MSRPURI_PARAMS;
		goto done;
	} else if(name.len==8 && strncasecmp(name.s, "userinfo", 8)==0) {
		t->subtype = TR_MSRPURI_USERINFO;
		goto done;
	}


	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	return NULL;
done:
	t->name = name;
	return p;

}
