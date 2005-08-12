/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-03-19  replaced all the mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-12-04  global callbacks moved into transaction callbacks;
 *              multiple events per callback added; single list per
 *              transaction for all its callbacks (bogdan)
 *  2004-08-23  user avp(attribute value pair) added -> making avp list
 *              available in callbacks (bogdan)
 */

#include "stdlib.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "t_hooks.h"
#include "t_lookup.h"
#include "t_funcs.h"


struct tmcb_head_list* req_in_tmcb_hl = 0;

struct tmcb_head_list tmcb_pending_hl = {0,0};
int tmcb_pending_id = -1;


int init_tmcb_lists()
{
	req_in_tmcb_hl = (struct tmcb_head_list*)shm_malloc
		( sizeof(struct tmcb_head_list) );
	if (req_in_tmcb_hl==0) {
		LOG(L_CRIT,"ERROR:tm:init_tmcb_lists: no more shared mem\n");
		return -1;
	}
	req_in_tmcb_hl->first = 0;
	req_in_tmcb_hl->reg_types = 0;
	return 1;
}


inline static void empty_tmcb_list(struct tmcb_head_list *head)
{
	struct tm_callback *cbp, *cbp_tmp;

	for( cbp=head->first; cbp ; ) {
		cbp_tmp = cbp;
		cbp = cbp->next;
		if (cbp_tmp->param) shm_free( cbp_tmp->param );
		shm_free( cbp_tmp );
	}
	head->first = 0 ;
	head->reg_types = 0;
}

void destroy_tmcb_lists()
{
	if (!req_in_tmcb_hl)
		return;

	empty_tmcb_list(req_in_tmcb_hl);

	shm_free(req_in_tmcb_hl);
}


int insert_tmcb(struct tmcb_head_list *cb_list, int types,
									transaction_cb f, void *param )
{
	struct tm_callback *cbp;

	/* build a new callback structure */
	if (!(cbp=shm_malloc( sizeof( struct tm_callback)))) {
		LOG(L_ERR, "ERROR:tm:insert_tmcb: out of shm. mem\n");
		return E_OUT_OF_MEM;
	}

	/* link it into the proper place... */
	cbp->next = cb_list->first;
	cb_list->first = cbp;
	cb_list->reg_types |= types;
	/* ... and fill it up */
	cbp->callback = f;
	cbp->param = param;
	cbp->types = types;
	if (cbp->next)
		cbp->id = cbp->next->id+1;
	else
		cbp->id = 0;

	return 1;
}



/* register a callback function 'f' for 'types' mask of events;
 * will be called back whenever one of the events occurs in transaction module
 * (global or per transaction, depending of event type)
*/
int register_tmcb( struct sip_msg* p_msg, struct cell *t, int types,
											transaction_cb f, void *param )
{
	struct tmcb_head_list *cb_list;

	/* are the callback types valid?... */
	if ( types<0 || types>TMCB_MAX ) {
		LOG(L_CRIT, "BUG:tm:register_tmcb: invalid callback types: mask=%d\n",
			types);
		return E_BUG;
	}
	/* we don't register null functions */
	if (f==0) {
		LOG(L_CRIT, "BUG:tm:register_tmcb: null callback function\n");
		return E_BUG;
	}

	if (types&TMCB_REQUEST_IN) {
		if (types!=TMCB_REQUEST_IN) {
			LOG(L_CRIT, "BUG:tm:register_tmcb: callback type TMCB_REQUEST_IN "
				"can't be register along with types\n");
			return E_BUG;
		}
		cb_list = req_in_tmcb_hl;
	} else {
		if (!t) {
			if (!p_msg) {
				LOG(L_CRIT,"BUG:tm:register_tmcb: no sip_msg, nor transaction"
					" given\n");
				return E_BUG;
			}
			/* look for the transaction */
			if ( t_check(p_msg,0)==1 ){
				if ( (t=get_t())==0 ) {
					LOG(L_CRIT,"BUG:tm:register_tmcb: transaction found "
						"is NULL\n");
					return E_BUG;
				}
				cb_list = &(t->tmcb_hl);
			} else {
				/* no transaction found -> link it to waitting list */
				if (p_msg->id!=tmcb_pending_id) {
					empty_tmcb_list(&tmcb_pending_hl);
					tmcb_pending_id = p_msg->id;
				}
				cb_list = &(tmcb_pending_hl);
			}
		} else {
			cb_list = &(t->tmcb_hl);
		}
	}

	return insert_tmcb( cb_list, types, f, param );
}


void run_trans_callbacks( int type , struct cell *trans,
						struct sip_msg *req, struct sip_msg *rpl, int code )
{
	static struct tmcb_params params = {0,0,0,0};
	struct tm_callback    *cbp;
	struct usr_avp **backup;

	params.req = req;
	params.rpl = rpl;
	params.code = code;

	if (trans->tmcb_hl.first==0 || ((trans->tmcb_hl.reg_types)&type)==0 )
		return;

	backup = set_avp_list( &trans->user_avps );
	for (cbp=trans->tmcb_hl.first; cbp; cbp=cbp->next)  {
		if ( (cbp->types)&type ) {
			DBG("DBG: trans=%p, callback type %d, id %d entered\n",
				trans, type, cbp->id );
			params.param = &(cbp->param);
			cbp->callback( trans, type, &params );
		}
	set_avp_list( backup );
	}
}



void run_reqin_callbacks( struct cell *trans, struct sip_msg *req, int code )
{
	static struct tmcb_params params = {0,0,0,0};
	struct tm_callback    *cbp;
	struct usr_avp **backup;

	params.req = req;
	params.code = code;

	if (req_in_tmcb_hl->first==0)
		return;

	backup = set_avp_list( &trans->user_avps );
	for (cbp=req_in_tmcb_hl->first; cbp; cbp=cbp->next)  {
		DBG("DBG: trans=%p, callback type %d, id %d entered\n",
			trans, cbp->types, cbp->id );
		params.param = &(cbp->param);
		cbp->callback( trans, cbp->types, &params );
	}
	set_avp_list( backup );
}

