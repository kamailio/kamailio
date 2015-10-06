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
#include "../../lib/kcore/statistics.h"
#include "../../dset.h"
#include "../../script_cb.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "tsilo.h"
#include "ts_hash.h"
#include "ts_append.h"

int ts_append(struct sip_msg* msg, str *ruri, char *table) {
	ts_urecord_t* _r;
	ts_transaction_t* ptr;

	int res;
	int appended;

	lock_entry_by_ruri(ruri);

	res = get_ts_urecord(ruri, &_r);

	if (res != 0) {
		LM_ERR("failed to retrieve record for %.*s\n", ruri->len, ruri->s);
		unlock_entry_by_ruri(ruri);
		return -1;
	}

	ptr = _r->transactions;

	while(ptr) {
		LM_DBG("transaction %u:%u found for %.*s, going to append branches\n",ptr->tindex, ptr->tlabel, ruri->len, ruri->s);

		appended = ts_append_to(msg, ptr->tindex, ptr->tlabel, table, ruri);
		if (appended > 0)
			update_stat(added_branches, appended);
		ptr = ptr->next;
	}

	unlock_entry_by_ruri(ruri);

	return 1;
}

int ts_append_to(struct sip_msg* msg, int tindex, int tlabel, char *table, str *uri) {
	struct cell     *t;
	struct sip_msg *orig_msg;
	int ret;

	if(_tmb.t_lookup_ident(&t, tindex, tlabel) < 0)
	{
		LM_ERR("transaction [%u:%u] not found\n",
				tindex, tlabel);
		return -1;
	}
	if (t->flags & T_CANCELED) {
		LM_DBG("trasaction [%u:%u] was cancelled\n",
				tindex, tlabel);
		return -2;
	}
	if (t->uas.status >= 200) {
		LM_DBG("trasaction [%u:%u] sent out a final response already - %d\n",
				tindex, tlabel, t->uas.status);
		return -3;
	}

	orig_msg = t->uas.request;

	if(uri==NULL || uri->s==NULL || uri->len<=0) {
		ret = _regapi.lookup_to_dset(orig_msg, table, NULL);
	} else {
		ret = _regapi.lookup_to_dset(orig_msg, table, uri);
	}
	if(ret != 1) {
		LM_DBG("transaction %u:%u: error updating dset (%d)\n", tindex, tlabel, ret);
		return -4;
	}

	return _tmb.t_append_branches();
}
