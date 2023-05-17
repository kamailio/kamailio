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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/route.h"
#include "../../core/data_lump.h"
#include "../../core/counters.h"
#include "../../core/dset.h"
#include "../../core/script_cb.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/contact/parse_contact.h"
#include "tsilo.h"
#include "ts_hash.h"
#include "ts_store.h"

int ts_store(struct sip_msg *msg, str *puri)
{
	struct cell *t;
	str aor;
	struct sip_uri ruri;
	str suri;

	ts_urecord_t *r;
	int res;

	if(puri && puri->s && puri->len > 0) {
		suri = *puri;
	} else {
		if(msg->new_uri.s != NULL) {
			/* incoming r-uri was chaged by cfg or other component */
			suri = msg->new_uri;
		} else {
			/* no changes to incoming r-uri */
			suri = msg->first_line.u.request.uri;
		}
	}

	if(parse_uri(suri.s, suri.len, &ruri) != 0) {
		LM_ERR("bad uri [%.*s]\n", suri.len, suri.s);
		return -1;
	}

	if(use_domain)
		aor = suri;
	else
		aor = ruri.user;

	if(aor.s == NULL) {
		LM_ERR("malformed aor from uri[%.*s]\n", suri.len, suri.s);
		return -1;
	}

	t = _tmb.t_gett();
	if(!t || t == T_UNDEFINED) {
		LM_ERR("no transaction defined for %.*s\n", aor.len, aor.s);
		return -1;
	}

	LM_DBG("storing transaction %u:%u for r-uri: %.*s\n", t->hash_index,
			t->label, aor.len, aor.s);

	lock_entry_by_ruri(&aor);

	res = get_ts_urecord(&aor, &r);

	if(res < 0) {
		LM_ERR("failed to retrieve record for %.*s\n", aor.len, aor.s);
		unlock_entry_by_ruri(&aor);
		return -1;
	}

	if(res != 0) { /* entry not found for the ruri */
		if(insert_ts_urecord(&aor, &r) < 0) {
			LM_ERR("failed to insert new record structure\n");
			unlock_entry_by_ruri(&aor);
			return -1;
		}
	}

	insert_ts_transaction(t, msg, r);
	unlock_entry_by_ruri(&aor);

	LM_DBG("transaction %u:%u (ruri: %.*s) inserted\n", t->hash_index, t->label,
			aor.len, aor.s);

	return 1;
}
