/*
 * $Id$
 *
 * Digest Authentication Module
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-15: In case of HDR_PROXYAUTH we always extract realm from From,
 *             even for REGISTERS
 * 2003-09-11: updated to new build_lump_rpl() interface (bogdan)
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 */


#include <string.h>
#include "../../dprint.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../data_lump_rpl.h"
#include "../../usr_avp.h"
#include "auth_mod.h"
#include "common.h"


/* 
 * Find out the digest realm to be used in challenge in the following order:
 *
 * 1) "realm" avp
 * 2) To/From URI host part
 */
int get_realm(struct sip_msg* msg, hdr_types_t hftype, str* realm)
{
	static struct sip_uri puri;
	int_str name, val;
	str u;
	static str n = STR_STATIC_INIT(AVP_REALM);
	
	name.s = n;
	if (search_first_avp(AVP_NAME_STR, name, &val, 0)) {
		*realm = val.s;
		return 0;
	}

	if ((REQ_LINE(msg).method.len == 8) 
	    && !memcmp(REQ_LINE(msg).method.s, "REGISTER", 8) 
	    && (hftype == HDR_AUTHORIZATION_T)
	   ) {
		if (!msg->to && ((parse_headers(msg, HDR_TO_F, 0) == -1) || (!msg->to))) {
			LOG(L_ERR, "auth:get_realm: Error while parsing headers\n");
			return -1;
		}
		
		     /* Body of To header field is parsed automatically */
		u = get_to(msg)->uri; 
	} else {
		if (parse_from_header(msg) < 0) {
			LOG(L_ERR, "auth:get_realm: Error while parsing headers\n");
			return -2;
		}

		u = get_from(msg)->uri;
	}

	if (parse_uri(u.s, u.len, &puri) < 0) {
		LOG(L_ERR, "auth:get_realm: Error while parsing URI\n");
		return -3;
	}

	*realm = puri.host;
	return 0;
}


/*
 * Create a response with given code and reason phrase
 * Optionally add new headers specified in _hdr
 */
int send_resp(struct sip_msg* msg, int code, char* reason,
					char* hdr, int hdr_len)
{
	/* Add new headers if there are any */
	if ((hdr) && (hdr_len)) {
		if (add_lump_rpl(msg, hdr, hdr_len, LUMP_RPL_HDR)==0) {
			LOG(L_ERR,"ERROR:auth:send_resp: unable to append hdr\n");
			return -1;
		}
	}

	return sl.reply(msg, code, reason);
}
