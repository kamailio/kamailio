/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * * History:
 * ========
 *
 * Nov 2013 Richard Good migrated pua_reginfo functionality to ims_registrar_pcscf
 */

#include "subscribe.h"

#include "../pua/send_subscribe.h"
#include "../pua/pua.h"

#include "../pua/pua_bind.h"
#include "async_reginfo.h"


extern pua_api_t pua;
extern str pcscf_uri;
extern str force_icscf_uri;

#define P_ASSERTED_IDENTITY_HDR_PREFIX "P-Asserted-Identity: <"
#define ROUTE_HDR_PREFIX "Route: <"
#define ROUTE_HDR_SEPARATOR ">, <"
#define ROUTE_HDR_END ">" CRLF

int reginfo_subscribe_real(struct sip_msg *msg, pv_elem_t *uri,
		str *service_routes, int num_service_routes, int expires)
{
	str uri_str = {0, 0};
	char uri_buf[512];
	int uri_buf_len = 512;
	//subs_info_t subs;
	str extra_headers = {0};
	reginfo_event_t *new_event;
	str *subs_outbound_proxy = 0;
	int i = 0;

	int len = strlen(P_ASSERTED_IDENTITY_HDR_PREFIX) + pcscf_uri.len + 1
			  + CRLF_LEN;
	if(service_routes != NULL) {
		len += strlen(ROUTE_HDR_PREFIX) + strlen(ROUTE_HDR_END);
		for(i = 0; i < num_service_routes; i++) {
			len += service_routes[i].len + strlen(ROUTE_HDR_SEPARATOR);
		}
	}
	extra_headers.s = (char *)pkg_malloc(len);
	if(extra_headers.s == NULL) {
		SHM_MEM_ERROR_FMT("%d bytes failed", len);
		goto error;
	}

	// Add P-Asserted-Identity header with pscscf_uri, as configured
	memcpy(extra_headers.s, P_ASSERTED_IDENTITY_HDR_PREFIX,
			strlen(P_ASSERTED_IDENTITY_HDR_PREFIX));
	extra_headers.len += strlen(P_ASSERTED_IDENTITY_HDR_PREFIX);
	memcpy(extra_headers.s + extra_headers.len, pcscf_uri.s, pcscf_uri.len);
	extra_headers.len += pcscf_uri.len;
	*(extra_headers.s + extra_headers.len) = '>';
	extra_headers.len++;
	memcpy(extra_headers.s + extra_headers.len, CRLF, CRLF_LEN);
	extra_headers.len += CRLF_LEN;

	// Add Service-Routes as Routes - TS 24.229 5.2.3
	if(service_routes != NULL && num_service_routes > 0) {
		memcpy(extra_headers.s + extra_headers.len, ROUTE_HDR_PREFIX,
				strlen(ROUTE_HDR_PREFIX));
		extra_headers.len += strlen(ROUTE_HDR_PREFIX);
		for(i = 0; i < num_service_routes; i++) {
			memcpy(extra_headers.s + extra_headers.len, service_routes[i].s,
					service_routes[i].len);
			extra_headers.len += service_routes[i].len;
			if(i < num_service_routes - 1) {
				memcpy(extra_headers.s + extra_headers.len, ROUTE_HDR_SEPARATOR,
						strlen(ROUTE_HDR_SEPARATOR));
				extra_headers.len += strlen(ROUTE_HDR_SEPARATOR);
			}
		}
		memcpy(extra_headers.s + extra_headers.len, ROUTE_HDR_END,
				strlen(ROUTE_HDR_END));
		extra_headers.len += strlen(ROUTE_HDR_END);
	}

	if(pv_printf(msg, uri, uri_buf, &uri_buf_len) < 0) {
		LM_ERR("cannot print uri into the format\n");
		goto error;
	}
	uri_str.s = uri_buf;
	uri_str.len = uri_buf_len;

	LM_DBG("extra_headers: [%.*s]", extra_headers.len, extra_headers.s);

	LM_DBG("Subscribing to %.*s\n", uri_str.len, uri_str.s);

	if(force_icscf_uri.s && force_icscf_uri.len) {
		subs_outbound_proxy = &force_icscf_uri;
	} else if(service_routes != NULL && num_service_routes > 0) {
		subs_outbound_proxy = &service_routes[0];
	}

	new_event = new_reginfo_event(REG_EVENT_SUBSCRIBE, 0, 0, 0, &uri_str,
			&pcscf_uri, &pcscf_uri, subs_outbound_proxy, expires, UPDATE_TYPE,
			REGINFO_SUBSCRIBE, REGINFO_EVENT, &extra_headers, &uri_str);

	if(!new_event) {
		LM_ERR("Unable to create event for cdp callback\n");
		goto error;
	}
	//push the new event onto the stack (FIFO)
	push_reginfo_event(new_event);

	if(extra_headers.s) {
		pkg_free(extra_headers.s);
	}

	return 1;

error:

	if(extra_headers.s) {
		pkg_free(extra_headers.s);
	}
	return -1;
}
