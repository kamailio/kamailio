/*
 * Route & Record-Route module, strict routing support
 *
 * $Id$
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
 */


#include "strict.h"
#include "common.h"
#include "../../dprint.h"
#include "../../parser/parse_rr.h"


/*
 * Do strict routing as defined in RFC2584
 */
int strict_route(struct sip_msg* _m, char* _s1, char* _s2)
{
	str* uri;
	int ret;
	
	ret = find_first_route(_m);
	if (ret < 0) {
		LOG(L_ERR, "strict_route(): Error in find_first_route\n");
		return -1;
	}

	if (!ret) {
		print_rr(_m->route->parsed);

		uri = &((rr_t*)_m->route->parsed)->nameaddr.uri;
		if (rewrite_RURI(_m, uri) < 0) {
			LOG(L_ERR, "strict_route(): Error while rewriting request URI\n");
			return -2;
		}
		if (remove_first_route(_m, _m->route) < 0) {
			LOG(L_ERR, "strict_route(): Error while removing the topmost Route URI\n");
			return -3;
		}
		return 1;
	}
	
	DBG("strict_route(): There is no Route HF\n");
	return -1;
}

