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
#include "../../core/dset.h"
#include "../../core/script_cb.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/contact/parse_contact.h"
#include "tsilo.h"
#include "ts_hash.h"
#include "ts_append.h"

int ts_append(struct sip_msg* msg, str *ruri, char *table) {
	ts_urecord_t* _r;
	ts_transaction_t* ptr;

	struct sip_uri p_uri;
	str *t_uri;

	int res;
	int appended;
	
	if (use_domain) {
		t_uri = ruri;
	} else {
		if(parse_uri(ruri->s, ruri->len, &p_uri)<0) {
			LM_ERR("failed to parse uri %.*s\n", ruri->len, ruri->s);
			return -1;
		}
		t_uri = &p_uri.user;
	}

	lock_entry_by_ruri(t_uri);

	res = get_ts_urecord(t_uri, &_r);

	if (res != 0) {
		LM_ERR("failed to retrieve record for %.*s\n", t_uri->len, t_uri->s);
		unlock_entry_by_ruri(t_uri);
		return -1;
	}

	ptr = _r->transactions;

	while(ptr) {
		LM_DBG("transaction %u:%u found for %.*s, going to append branches\n",ptr->tindex, ptr->tlabel, t_uri->len, t_uri->s);

		appended = ts_append_to(msg, ptr->tindex, ptr->tlabel, table, ruri);
		if (appended > 0)
			update_stat(added_branches, appended);
		ptr = ptr->next;
	}

	unlock_entry_by_ruri(t_uri);

	return 1;
}

int ts_append_to(struct sip_msg* msg, int tindex, int tlabel, char *table, str *uri) {
	struct cell     *t=0;
	struct cell     *orig_t;
	struct sip_msg *orig_msg;
	int ret;
	str stable;

	orig_t = _tmb.t_gett();

	if(_tmb.t_lookup_ident(&t, tindex, tlabel) < 0)
	{
		LM_ERR("transaction [%u:%u] not found\n",
				tindex, tlabel);
		ret = -1;
		goto done;
	}

	if (t->flags & T_CANCELED) {
		LM_DBG("trasaction [%u:%u] was cancelled\n",
				tindex, tlabel);
		ret = -2;
		goto done;
	}

	if (t->uas.status >= 200) {
		LM_DBG("trasaction [%u:%u] sent out a final response already - %d\n",
				tindex, tlabel, t->uas.status);
		ret = -3;
		goto done;
	}

	orig_msg = t->uas.request;

	stable.s = table;
	stable.len = strlen(stable.s);
	if(uri==NULL || uri->s==NULL || uri->len<=0) {
		ret = _regapi.lookup_to_dset(orig_msg, &stable, NULL);
	} else {
		ret = _regapi.lookup_to_dset(orig_msg, &stable, uri);
	}

	if(ret != 1) {
		LM_DBG("transaction %u:%u: error updating dset (%d)\n", tindex, tlabel, ret);
		ret = -4;
		goto done;
	}

	ret = _tmb.t_append_branches();

done:
	/* unref the transaction which had been referred by t_lookup_ident() call.
	 * Restore the original transaction (if any) */
	if(t) _tmb.unref_cell(t);
	_tmb.t_sett(orig_t, T_BR_UNDEFINED);

	return ret;
}

int ts_append_by_contact(struct sip_msg* msg, str *ruri, str *contact, char *table) {
	ts_urecord_t* _r;
	ts_transaction_t* ptr;

	struct sip_uri p_uri;
	struct sip_uri c_uri;
	str *t_uri;

	int res;
	int appended;

	/* parse R-URI */
	if (use_domain) {
		t_uri = ruri;
	} else {
		if (parse_uri(ruri->s, ruri->len, &p_uri) < 0) {
			LM_ERR("tsilo: failed to parse uri %.*s\n", ruri->len, ruri->s);
			return -1;
		}
		t_uri = &p_uri.user;
	}

	/* parse contact */
	if (parse_uri(contact->s, contact->len, &c_uri) < 0) {
		LM_ERR("tsilo: failed to parse contact %.*s\n", ruri->len, ruri->s);
		return -1;
	}

	/* find urecord in TSILO cache */
	lock_entry_by_ruri(t_uri);
	res = get_ts_urecord(t_uri, &_r);

	if (res != 0) {
		LM_ERR("tsilo: failed to retrieve record for %.*s\n", t_uri->len, t_uri->s);
		unlock_entry_by_ruri(t_uri);
		return -1;
	}

	/* cycle through existing transactions */
	ptr = _r->transactions;
	while(ptr) {
		LM_DBG("tsilo: transaction %u:%u found for %.*s, going to append branches\n",
						ptr->tindex, ptr->tlabel, t_uri->len, t_uri->s);
		/* append only if the desired contact has been found in locations */
		appended = ts_append_by_contact_to(msg, ptr->tindex, ptr->tlabel, table, ruri, contact);
		if (appended > 0)
			update_stat(added_branches, appended);
		ptr = ptr->next;
	}

	unlock_entry_by_ruri(t_uri);

	return 1;
}

int ts_append_by_contact_to(struct sip_msg* msg, int tindex, int tlabel, char *table, str *uri, str *contact) {
	struct cell     *t=0;
	struct cell     *orig_t;	/* a pointer to an existing transaction or 0 if lookup fails*/
	struct sip_msg *orig_msg;
	int ret;
	str stable;

	LM_DBG("tsilo: trying to append based on contact <%.*s>\n", contact->len, contact->s);

	/* lookup a transaction based on its identifier (hash_index:label) */
	orig_t = _tmb.t_gett();
	if(_tmb.t_lookup_ident(&t, tindex, tlabel) < 0)
	{
		LM_ERR("tsilo: transaction [%u:%u] not found\n", tindex, tlabel);
		ret = -1;
		goto done;
	}

	/* check if the dialog is still in the early stage */
	if (t->flags & T_CANCELED) {
		LM_DBG("tsilo: trasaction [%u:%u] was cancelled\n", tindex, tlabel);
		ret = -2;
		goto done;
	}
	if (t->uas.status >= 200) {
		LM_DBG("tsilo: trasaction [%u:%u] sent out a final response already - %d\n",
					tindex, tlabel, t->uas.status);
		ret = -3;
		goto done;
	}

	/* get original (very first) request of the transaction */
	orig_msg = t->uas.request;
	stable.s = table;
	stable.len = strlen(stable.s);

	if(uri==NULL || uri->s==NULL || uri->len<=0) {
		ret = _regapi.lookup_to_dset(orig_msg, &stable, NULL);
	} else {
		ret = _regapi.lookup_to_dset(orig_msg, &stable, uri);
	}

	if(ret != 1) {
		LM_ERR("tsilo: transaction %u:%u: error updating dset (%d)\n", tindex, tlabel, ret);
		ret = -4;
		goto done;
	}

	/* start the transaction only for the desired contact
		contact must be of syntax: sip:<user>@<host>:<port> with no parameters list*/
	ret = _tmb.t_append_branch_by_contact(contact);

done:
	/* unref the transaction which had been referred by t_lookup_ident() call.
	 * Restore the original transaction (if any) */
	if(t) _tmb.unref_cell(t);
	_tmb.t_sett(orig_t, T_BR_UNDEFINED);

	return ret;
}
