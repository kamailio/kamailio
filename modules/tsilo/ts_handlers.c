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

#include <string.h>

#include "ts_hash.h"
#include "ts_handlers.h"

extern struct tm_binds _tmb;

/*!
 * \brief add transaction structure to tm callbacks
 * \param t current transaction
 * \param req current sip request
 * \param tma_t current transaction
 * \return 0 on success, -1 on failure
 */
int ts_set_tm_callbacks(struct cell *t, sip_msg_t *req, ts_transaction_t *ts)
{
	ts_transaction_t* ts_clone;

	if(t==NULL)
		return -1;

	if ( (ts_clone=clone_ts_transaction(ts)) < 0 ) {
		LM_ERR("failed to clone transaction\n");
		return -1;
	}

	if (ts_clone == NULL) {
		LM_ERR("transaction clone null\n");
	}
	if ( _tmb.register_tmcb( req, t,TMCB_DESTROY,
			ts_onreply, (void*)ts_clone, free_ts_transaction)<0 ) {
		LM_ERR("failed to register TMCB for transaction %d:%d\n", t->hash_index, t->label);
		return -1;
	}
	LM_DBG("registered TMCB for transaction %d:%d\n", ts_clone->tindex, ts_clone->tlabel);

	return 0;
}

void ts_onreply(struct cell* t, int type, struct tmcb_params *param)
{
	ts_urecord_t* _r;
	ts_entry_t* _e;
	ts_transaction_t *cb_ptr, *ptr;

	cb_ptr = (ts_transaction_t*)(*param->param);
	if (cb_ptr == NULL) {
		LM_DBG("NULL param for type %d\n", type);
		return;
	}

	if (type &(TMCB_DESTROY)) {
		LM_DBG("TMCB_DESTROY called for transaction %u:%u\n", cb_ptr->tindex, cb_ptr->tlabel);
		_r = cb_ptr->urecord;
		_e = _r->entry;
		lock_entry(_e);
		ptr = _r->transactions;
		while(ptr) {
			if ((ptr->tindex == cb_ptr->tindex) && (ptr->tlabel == cb_ptr->tlabel)) {
				remove_ts_transaction(ptr);

				if (_r->transactions == NULL) {
					LM_DBG("last transaction for %.*s, removing urecord\n", _r->ruri.len, _r->ruri.s);
					remove_ts_urecord(_r);
				}
				unlock_entry(_e);
				return;
			}
			ptr = ptr->next;
		}
		LM_DBG("transaction %u:%u not found\n",ptr->tindex, ptr->tlabel);
		unlock_entry(_e);
	} else {
		LM_DBG("called with uknown type %d\n", type);
	}

	return;
}
