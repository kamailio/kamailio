/*
 * $Id$
 *
 * Digest Authentication Module
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */


#include <string.h>
#include "../../dprint.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../data_lump_rpl.h"
#include "auth_mod.h"
#include "common.h"


/* 
 * Return parsed To or From, host part of the parsed uri is realm
 */
int get_realm(struct sip_msg* _m, int _hftype, struct sip_uri* _u)
{
	str uri;

	if ((REQ_LINE(_m).method.len == 8) 
	    && !memcmp(REQ_LINE(_m).method.s, "REGISTER", 8) 
	    && (_hftype == HDR_AUTHORIZATION)
	   ) {
		if (!_m->to && ((parse_headers(_m, HDR_TO, 0) == -1) || (!_m->to))) {
			LOG(L_ERR, "get_realm(): Error while parsing headers\n");
			return -1;
		}
		
		     /* Body of To header field is parsed automatically */
		uri = get_to(_m)->uri; 
	} else {
		if (parse_from_header(_m) < 0) {
			LOG(L_ERR, "get_realm(): Error while parsing headers\n");
			return -2;
		}

		uri = get_from(_m)->uri;
	}

	if (parse_uri(uri.s, uri.len, _u) < 0) {
		LOG(L_ERR, "get_realm(): Error while parsing URI\n");
		return -3;
	}
	
	return 0;
}


/*
 * Create a response with given code and reason phrase
 * Optionaly add new headers specified in _hdr
 */
int send_resp(struct sip_msg* _m, int _code, char* _reason,
					char* _hdr, int _hdr_len)
{
	struct lump_rpl* ptr;
	
	     /* Add new headers if there are any */
	if ((_hdr) && (_hdr_len)) {
		ptr = build_lump_rpl(_hdr, _hdr_len);
		add_lump_rpl(_m, ptr);
	}
	
	return sl_reply(_m, (char*)(long)_code, _reason);
}
