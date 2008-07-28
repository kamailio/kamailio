/*
 * $Id$
 *
 * Digest Authentication Module
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
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../data_lump_rpl.h"
#include "auth_mod.h"
#include "common.h"


/* 
 * Return parsed To or From, host part of the parsed uri is realm
 */
int get_realm(struct sip_msg* _m, hdr_types_t _hftype, struct sip_uri** _u)
{

	if(_u==NULL)
		return -1;
	if ((REQ_LINE(_m).method.len == 8) 
	    && !memcmp(REQ_LINE(_m).method.s, "REGISTER", 8) 
	    && (_hftype == HDR_AUTHORIZATION_T)
	   ) {
		if (!_m->to && ((parse_headers(_m, HDR_TO_F, 0)==-1) || (!_m->to))) {
			LM_ERR("failed to parse TO headers\n");
			return -1;
		}
		
		/* Body of To header field is parsed automatically */
		if((*_u = parse_to_uri(_m))==NULL)
			return -1;
	} else {
		if (parse_from_header(_m) < 0) {
			LM_ERR("failed to parse FROM headers\n");
			return -2;
		}
		if((*_u = parse_from_uri(_m))==NULL)
			return -1;
	}
	
	return 0;
}


/*
 * Create a response with given code and reason phrase
 * Optionally add new headers specified in _hdr
 */
int send_resp(struct sip_msg* _m, int _code, str* _reason,
					char* _hdr, int _hdr_len)
{
	/* Add new headers if there are any */
	if ((_hdr) && (_hdr_len)) {
		if (add_lump_rpl( _m, _hdr, _hdr_len, LUMP_RPL_HDR)==0) {
			LM_ERR("unable to append hdr\n");
			return -1;
		}
	}

	return slb.reply(_m, _code, _reason);
}
