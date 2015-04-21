/*
 * Copyright (C) 2014 Federico Cabiddu (federico.cabiddu@gmail.com)
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../route.h"
#include "../../data_lump.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../dset.h"
#include "../../script_cb.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "tsilo.h"
#include "ts_hash.h"
#include "ts_store.h"

extern int use_domain;

int ts_store(struct sip_msg* msg) {
	struct cell		*t;
	str aor;
	struct sip_uri ruri;

	ts_urecord_t* r;
	int res;



	if (use_domain) {
		aor = msg->first_line.u.request.uri;
	}
	else {
		if (parse_uri(msg->first_line.u.request.uri.s, msg->first_line.u.request.uri.len, &ruri)!=0)
		{
			LM_ERR("bad uri [%.*s]\n",
					msg->first_line.u.request.uri.len,
					msg->first_line.u.request.uri.s);
			return -1;
		}
		aor = ruri.user;
	}

	t = _tmb.t_gett();
	if (!t || t==T_UNDEFINED) {
		LM_ERR("no transaction defined for %.*s\n", aor.len, aor.s);
		return -1;
	}

	LM_DBG("storing transaction %u:%u for r-uri: %.*s\n", t->hash_index, t->label, aor.len, aor.s);

	lock_entry_by_ruri(&aor);

	res = get_ts_urecord(&aor, &r);

	if (res < 0) {
		LM_ERR("failed to retrieve record for %.*s\n", aor.len, aor.s);
		unlock_entry_by_ruri(&aor);
		return -1;
	}

	if (res != 0) { /* entry not found for the ruri */
		if (insert_ts_urecord(&aor, &r) < 0) {
			LM_ERR("failed to insert new record structure\n");
			unlock_entry_by_ruri(&aor);
			return -1;
		}
	}

	insert_ts_transaction(t, msg, r);
	unlock_entry_by_ruri(&aor);

	LM_DBG("transaction %u:%u (ruri: %.*s) inserted\n", t->hash_index, t->label, aor.len, aor.s);

	return 1;
}
