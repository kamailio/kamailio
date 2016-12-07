/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
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
 */

#include "subscribe.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../pua/send_subscribe.h"
#include "../pua/pua.h"
#include "pua_reginfo.h"

int reginfo_subscribe_real(struct sip_msg* msg, pv_elem_t* uri, int expires) {
	str uri_str = {0, 0};
	char uri_buf[512];
	int uri_buf_len = 512;
	subs_info_t subs;
	
	if (pv_printf(msg, uri, uri_buf, &uri_buf_len) < 0) {
		LM_ERR("cannot print uri into the format\n");
		return -1;
	}
	uri_str.s = uri_buf;
	uri_str.len = uri_buf_len;

	LM_DBG("Subscribing to %.*s\n", uri_str.len, uri_str.s);

	memset(&subs, 0, sizeof(subs_info_t));

	subs.remote_target = &uri_str;
	subs.pres_uri= &uri_str;
	subs.watcher_uri= &server_address;
	subs.expires = expires;

	subs.source_flag= REGINFO_SUBSCRIBE;
	subs.event= REGINFO_EVENT;
	subs.contact= &server_address;
	
	if(outbound_proxy.s && outbound_proxy.len)
		subs.outbound_proxy= &outbound_proxy;

	subs.flag|= UPDATE_TYPE;

	if(pua.send_subscribe(&subs)< 0) {
		LM_ERR("while sending subscribe\n");
	}	

	return 1;
}

int reginfo_subscribe(struct sip_msg* msg, char* uri, char* s2) {
	return reginfo_subscribe_real(msg, (pv_elem_t*)uri, 3600);
}

int reginfo_subscribe2(struct sip_msg* msg, char* uri, char* param2) {
	int expires;
	if(fixup_get_ivalue(msg, (gparam_p)param2, &expires) != 0) {
		LM_ERR("No expires provided!\n");
		return -1;
	}
	return reginfo_subscribe_real(msg, (pv_elem_t*)uri, expires);
}

int fixup_subscribe(void** param, int param_no) {
	pv_elem_t *model;
	str s;
	if (param_no == 1) {
		if(*param) {
			s.s = (char*)(*param);
			s.len = strlen(s.s);
			if(pv_parse_format(&s, &model)<0) {
				LM_ERR("wrong format[%s]\n",(char*)(*param));
				return E_UNSPEC;
			}
			*param = (void*)model;
			return 1;
		}
		LM_ERR("null format\n");
		return E_UNSPEC;
	} else if (param_no == 2) {
		return fixup_igp_igp(param, param_no);
	} else return 1;
}

