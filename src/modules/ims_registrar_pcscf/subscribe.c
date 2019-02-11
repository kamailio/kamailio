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
 * 
 * * History:
 * ========
 * 
 * Nov 2013 Richard Good migrated pua_reginfo funtionality to ims_registrar_pcscf
 */

#include "subscribe.h"

#include "../pua/send_subscribe.h"
#include "../pua/pua.h"

#include "../pua/pua_bind.h"
#include "async_reginfo.h"


extern pua_api_t pua;
extern str pcscf_uri;
extern str force_icscf_uri;

#define P_ASSERTED_IDENTITY_HDR_PREFIX	"P-Asserted-Identity: <"

int reginfo_subscribe_real(struct sip_msg* msg, pv_elem_t* uri, str* service_routes, int expires) {
	str uri_str = {0, 0};
	char uri_buf[512];
	int uri_buf_len = 512;
	//subs_info_t subs;
	str p_asserted_identity_header;
	reginfo_event_t *new_event;
	str *subs_outbound_proxy = 0;
	
	int len = strlen(P_ASSERTED_IDENTITY_HDR_PREFIX) + pcscf_uri.len + 1 + CRLF_LEN;
	p_asserted_identity_header.s = (char *)pkg_malloc( len );
	if ( p_asserted_identity_header.s == NULL ) {
	    LM_ERR( "insert_asserted_identity: pkg_malloc %d bytes failed", len );
	    goto error;
	}

	memcpy(p_asserted_identity_header.s, P_ASSERTED_IDENTITY_HDR_PREFIX, strlen(P_ASSERTED_IDENTITY_HDR_PREFIX));
	p_asserted_identity_header.len = strlen(P_ASSERTED_IDENTITY_HDR_PREFIX);
	memcpy(p_asserted_identity_header.s + p_asserted_identity_header.len, pcscf_uri.s, pcscf_uri.len);
	p_asserted_identity_header.len += pcscf_uri.len;
	*(p_asserted_identity_header.s + p_asserted_identity_header.len) = '>';
	p_asserted_identity_header.len ++;
	memcpy( p_asserted_identity_header.s + p_asserted_identity_header.len, CRLF, CRLF_LEN );
	p_asserted_identity_header.len += CRLF_LEN;
	
	if (pv_printf(msg, uri, uri_buf, &uri_buf_len) < 0) {
	    LM_ERR("cannot print uri into the format\n");
	    goto error;
	}
	uri_str.s = uri_buf;
	uri_str.len = uri_buf_len;
	
	LM_DBG("p_asserted_identity_header: [%.*s]", p_asserted_identity_header.len, p_asserted_identity_header.s);

	LM_DBG("Subscribing to %.*s\n", uri_str.len, uri_str.s);

	if(force_icscf_uri.s && force_icscf_uri.len) {
	    subs_outbound_proxy= &force_icscf_uri;
	}
	
	new_event = new_reginfo_event(REG_EVENT_SUBSCRIBE, 0, 0, 0, &uri_str, &pcscf_uri, &pcscf_uri,
	subs_outbound_proxy, expires, UPDATE_TYPE, REGINFO_SUBSCRIBE, REGINFO_EVENT, &p_asserted_identity_header, &uri_str);
	
	if (!new_event) {
            LM_ERR("Unable to create event for cdp callback\n");
            goto error;
        }
        //push the new event onto the stack (FIFO)
        push_reginfo_event(new_event);
	
	if (p_asserted_identity_header.s) {
		pkg_free(p_asserted_identity_header.s);
	}

	return 1;
	
    error:
    
	if (p_asserted_identity_header.s) {
		pkg_free(p_asserted_identity_header.s);
	}
	return -1;
	
}
