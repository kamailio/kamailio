/*
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


#include "../../action.h"
#include "../../dset.h"
#include "../tm/tm_load.h"
#include "loc_set.h"
#include "cpl_sig.h"

extern struct tm_binds cpl_tmb;


int cpl_proxy_to_loc_set( struct cpl_interpreter *inter )
{
	struct location *loc;
	struct action act;

	loc = inter->loc_set;
	if (!loc) {
		LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: empty loc set!!\n");
		goto runtime_error;
	}

	/* use the first addr in loc_set to rewrite the RURI */
	DBG("DEBUG:cpl_c:cpl_proxy_to_loc_set: rewriting Request-URI with "
		"<%s>\n",loc->addr.uri.s);
	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = loc->addr.uri.s;
	act.next = 0;

	if (do_action(&act, inter->msg) < 0) {
		LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: do_action failed :-(\n");
		goto runtime_error;
	}
	loc = loc->next;

	/* add the rest of the locations as branches */
	while(loc) {
		if (append_branch(inter->msg,loc->addr.uri.s,loc->addr.uri.len-1)==-1){
			LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: failed when "
				"appending branch <%s>\n",loc->addr.uri.s);
			goto runtime_error;
		}
		loc = loc->next;
	}

	/* do t_relay*/
	if (cpl_tmb.t_relay( inter->msg, 0, 0)==-1) {
		LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: t_relay failed\n");
		goto runtime_error;
	}

	return SCRIPT_END;
runtime_error:
	return SCRIPT_RUN_ERROR;
}


