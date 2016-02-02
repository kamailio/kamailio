/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 */

/*!
 * \file
 * \brief SIP-router topoh ::
 * \ingroup topoh
 * Module: \ref topoh
 */

#include <string.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../forward.h"
#include "../../trim.h"
#include "../../msg_translator.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_param.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_via.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_refer_to.h"
#include "tps_msg.h"

extern int _tps_param_mask_callid;

int tps_skip_rw(char *s, int len)
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

struct via_param *tps_get_via_param(struct via_body *via, str *name)
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

int tps_get_param_value(str *in, str *name, str *value)
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

int tps_get_uri_param_value(str *uri, str *name, str *value)
{
	struct sip_uri puri;

	memset(value, 0, sizeof(str));
	if(parse_uri(uri->s, uri->len, &puri)<0)
		return -1;
	return tps_get_param_value(&puri.params, name, value);
}

int tps_get_uri_type(str *uri, int *mode, str *value)
{
	struct sip_uri puri;
	int ret;
	str r2 = {"r2", 2};

	memset(value, 0, sizeof(str));
	*mode = 0;
	if(parse_uri(uri->s, uri->len, &puri)<0)
		return -1;

	LM_DBG("PARAMS [%.*s]\n", puri.params.len, puri.params.s);

	if(check_self(&puri.host, puri.port_no, 0)==1)
	{
		/* myself -- matched on all protos */
		ret = tps_get_param_value(&puri.params, &r2, value);
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

char* tps_msg_update(sip_msg_t *msg, unsigned int *olen)
{
	struct dest_info dst;

	init_dest_info(&dst);
	dst.proto = PROTO_UDP;
	return build_req_buf_from_sip_req(msg,
			olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
}

int tps_route_direction(sip_msg_t *msg)
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
	if(tps_get_param_value(&puri.params, &ftn, &ftv)!=0)
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

int tps_skip_msg(sip_msg_t *msg)
{
	if (msg->cseq==NULL || get_cseq(msg)==NULL) {
		LM_WARN("Invalid/Unparsed CSeq in message. Skipping.");
		return 1;
	}

	if((get_cseq(msg)->method_id)&(METHOD_REGISTER|METHOD_PUBLISH))
		return 1;

	return 0;
}

/**
 *
 */
int tps_request_received(sip_msg_t *msg, int dialog, int direction)
{
	return 0;
}

/**
 *
 */
int tps_response_received(sip_msg_t *msg)
{
	return 0;
}

/**
 *
 */
int tps_request_sent(sip_msg_t *msg, int dialog, int direction, int local)
{
	return 0;
}

/**
 *
 */
int tps_response_sent(sip_msg_t *msg)
{
	return 0;
}
