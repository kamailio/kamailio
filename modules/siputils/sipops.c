/*
 * SIPUTILS mangler module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief SIP-utils :: Mangler Module
 * \ingroup siputils
 * - Module; \ref siputils
 */


#include "../../mod_fix.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_supported.h"
#include "../../parser/parse_rr.h"
#include "../../ip_addr.h"
#include "../../resolve.h"
#include "../../forward.h"
#include "../../lib/kcore/cmpapi.h"

#include "sipops.h"

int w_cmp_uri(struct sip_msg *msg, char *uri1, char *uri2)
{
	str s1;
	str s2;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)uri1, &s1)!=0)
	{
		LM_ERR("cannot get first parameter\n");
		return -8;
	}
	if(fixup_get_svalue(msg, (gparam_p)uri2, &s2)!=0)
	{
		LM_ERR("cannot get second parameter\n");
		return -8;
	}
	ret = cmp_uri_str(&s1, &s2);
	if(ret==0)
		return 1;
	if(ret>0)
		return -1;
	return -2;
}

int w_cmp_aor(struct sip_msg *msg, char *uri1, char *uri2)
{
	str s1;
	str s2;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)uri1, &s1)!=0)
	{
		LM_ERR("cannot get first parameter\n");
		return -8;
	}
	if(fixup_get_svalue(msg, (gparam_p)uri2, &s2)!=0)
	{
		LM_ERR("cannot get second parameter\n");
		return -8;
	}
	ret = cmp_aor_str(&s1, &s2);
	if(ret==0)
		return 1;
	if(ret>0)
		return -1;
	return -2;
}

int w_is_gruu(sip_msg_t *msg, char *uri1, char *p2)
{
        str s1, *s2;
	sip_uri_t turi;
	sip_uri_t *puri;

	if(uri1!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)uri1, &s1)!=0)
		{
			LM_ERR("cannot get first parameter\n");
			return -8;
		}
		if(parse_uri(s1.s, s1.len, &turi)!=0) {
		    LM_ERR("parsing of uri '%.*s' failed\n", s1.len, s1.s);
		    return -1;
		}
		puri = &turi;
	} else {
  	        if(parse_sip_msg_uri(msg)<0) {
		    s2 = GET_RURI(msg);
  		    LM_ERR("parsing of uri '%.*s' failed\n", s2->len, s2->s);
		    return -1;
		}
		puri = &msg->parsed_uri;
	}
	if(puri->gr.s!=NULL)
	{
		if(puri->gr_val.len>0)
			return 1;
		return 2;
	}
	return -1;
}

int w_is_supported(sip_msg_t *msg, char *_option, char *p2)
{
	unsigned long option;

	option = (unsigned long)_option;

	if (parse_supported(msg) < 0)
		return -1;

	if ((((struct option_tag_body*)msg->supported->parsed)->option_tags_all &
				option) == 0)
		return -1;
	else
		return 1;
}


int w_is_first_hop(sip_msg_t *msg, char *p1, char *p2)
{
	int ret;
	rr_t* r = NULL;
	sip_uri_t puri;
	struct ip_addr *ip;

	if(msg==NULL)
		return -1;

	if(msg->first_line.type == SIP_REQUEST) {
		if (parse_headers( msg, HDR_VIA2_F, 0 )<0
				|| (msg->via2==0) || (msg->via2->error!=PARSE_OK))
		{
			/* sip request: if more than one via, then not first hop */
			/* no second via or error */
			LM_DBG("no 2nd via found - first hop\n");
			return 1;
		}
		return -1;
	} else if(msg->first_line.type == SIP_REPLY) {
		/* sip reply: if top record-route is myself
		 * and not received from myself (loop), then is first hop */
		if (parse_headers( msg, HDR_EOH_F, 0 )<0) {
			LM_DBG("error parsing headers\n");
			return -1;
		}
		if(msg->record_route==NULL) {
			LM_DBG("no record-route header - first hop\n");
			return 1;
		}
		if(parse_rr(msg->record_route)<0) {
			LM_DBG("failed to parse first record-route header\n");
			return -1;
		}
		r = (rr_t*)msg->record_route->parsed;
		if(parse_uri(r->nameaddr.uri.s, r->nameaddr.uri.len, &puri)<0) {
			LM_DBG("failed to parse uri in first record-route header\n");
			return -1;
		}
		if (((ip = str2ip(&(puri.host))) == NULL)
				&& ((ip = str2ip6(&(puri.host))) == NULL)) {
			LM_DBG("uri host is not an ip address\n");
			return -1;
		}
		ret = check_self(&puri.host, (puri.port.s)?puri.port_no:0,
				(puri.transport_val.s)?puri.proto:0);
		if(ret!=1) {
			LM_DBG("top record route uri is not myself\n");
			return -1;
		}
		if (ip_addr_cmp(ip, &(msg->rcv.src_ip))
				&& ((msg->rcv.src_port == puri.port_no)
					|| ((puri.port.len == 0) && (msg->rcv.src_port == 5060)))
				&& (puri.proto==msg->rcv.proto
					|| (puri.proto==0 && msg->rcv.proto==PROTO_UDP)) ) {
			LM_DBG("source address matches top record route uri - loop\n");
			return -1;
		}
		/* todo - check spirals */
		return 1;
	} else {
		return -1;
	}
}
