/*
 * $Id $
 *
 * Copyright (C) 2006 Voice System SRL
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
 * TODO - several races to be fixed:
 *   1) race between multiple 200 OK (parallel execution)
 *   2) race between 200 OK and BYE - BYE received before going into CONFIRMED
 *       state (rpl callback is actually called after sending out the reply)
 *   3) race between non-200 OK and 200 OK replies - 200 Ok may be pushed after
 *       (or in parallel) with a negative reply
 *
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 * 2006-11-28  Added support for tracking the number of early dialogs, and the
 *             number of failed dialogs. This involved updates to dlg_onreply().
 *             (Jeffrey Magder - SOMA Networks)
 */


#include "string.h"

#include "../../trim.h"
#include "../../items.h"
#include "../../timer.h"
#include "../../statistics.h"
#include "../../parser/parse_from.h"
#include "../tm/tm_load.h"
#include "../rr/api.h"
#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_cb.h"
#include "dlg_handlers.h"

static str       rr_param;
static int       dlg_flag;
static xl_spec_t *timeout_avp;
static int       default_timeout;
static int       use_tight_match;
static int       shutdown_done = 0;

extern struct tm_binds d_tmb;
extern struct rr_binds d_rrb;

/* statistic variables */
extern int dlg_enable_stats;
extern stat_var *active_dlgs;
extern stat_var *early_dlgs;
extern stat_var *processed_dlgs;
extern stat_var *expired_dlgs;
extern stat_var *failed_dlgs;


static unsigned int CURR_DLG_LIFETIME = 0;
static unsigned int CURR_DLG_STATUS = 0;
static unsigned int CURR_DLG_ID  = 0xffffffff;

#define RR_DLG_PARAM_SIZE  (2*2*sizeof(int)+3+MAX_DLG_RR_PARAM_NAME)
#define DLG_SEPARATOR      '.'


void init_dlg_handlers(char *rr_param_p, int dlg_flag_p,
		xl_spec_t *timeout_avp_p ,int default_timeout_p, int use_tight_match_p)
{
	rr_param.s = rr_param_p;
	rr_param.len = strlen(rr_param.s);

	dlg_flag = 1<<dlg_flag_p;

	timeout_avp = timeout_avp_p;
	default_timeout = default_timeout_p;
	use_tight_match = use_tight_match_p;
}


void destroy_dlg_handlers()
{
	shutdown_done = 1;
}


static inline int add_dlg_rr_param(struct sip_msg *req, unsigned int entry,
													unsigned int id)
{
	static char buf[RR_DLG_PARAM_SIZE];
	str s;
	int n;
	char *p;

	s.s = p = buf;

	*(p++) = ';';
	memcpy(p, rr_param.s, rr_param.len);
	p += rr_param.len;
	*(p++) = '=';

	n = RR_DLG_PARAM_SIZE - (p-buf);
	if (int2reverse_hex( &p, &n, entry)==-1)
		return -1;

	*(p++) = DLG_SEPARATOR;

	n = RR_DLG_PARAM_SIZE - (p-buf);
	if (int2reverse_hex( &p, &n, id)==-1)
		return -1;

	s.len = p-buf;

	if (d_rrb.add_rr_param( req, &s)<0) {
		LOG(L_ERR,"ERROR:dialog:add_dlg_rr_param: failed to add rr param\n");
		return -1;
	}

	return 0;
}



static void dlg_onreply(struct cell* t, int type, struct tmcb_params *param)
{
	struct sip_msg *rpl;
	struct dlg_cell *dlg;
	str tag;

	dlg = (struct dlg_cell *)(*param->param);
	if (shutdown_done || dlg==0)
		return;

	if (type==TMCB_TRANS_DELETED) {
		if (dlg->state == DLG_STATE_UNCONFIRMED) {
			DBG("DEBUG:dialog:dlg_onreply: destroying unused dialog %p\n",dlg);
			unref_dlg(dlg,2,1);
			if_update_stat( dlg_enable_stats, active_dlgs, -1);
			return;
		}
		unref_dlg(dlg,1,0);
		return;
	}

	rpl = param->rpl;

	if (type==TMCB_RESPONSE_FWDED) {
		/* The state does not change, but the msg is mutable in this callback*/
		run_dlg_callbacks(DLGCB_RESPONSE_FWDED, dlg, rpl);
		return;
	}

	if (param->code<200) {
		DBG("DEBUG:dialog:dlg_onreply: dialog %p goes into Early state "
			"with code %d\n", dlg, param->code);

		/* We only want to update the 'early' statistics if this is the
		* first provisional response. */
		if (dlg->state != DLG_STATE_EARLY) {
		
			/* We have received a provisional response, so we are
			* officially in an 'early state'.  Update the stats
			* accordingly. */
			if_update_stat(dlg_enable_stats, early_dlgs, 1);

			/* We need to keep track of the number of 1xx responses.
			* If there is at least one, we'll need to decrement the
			* counter later when we get a 2xx response. */
			dlg->num_100s++;

		}

		dlg->state = DLG_STATE_EARLY;

		/* Early state, the message is immutable here */
		run_dlg_callbacks(DLGCB_EARLY, dlg, rpl);
		return;
	}

	/* for this point we deal only with final requests that trigger dialog
	 * ending, so prevent any sequential callbacks to be executed */
	if ( (dlg->state & (DLG_STATE_UNCONFIRMED|DLG_STATE_EARLY))==0 )
		return;

	DBG("DEBUG:dialog:dlg_onreply: dialog %p confirmed\n",dlg);

	/* We need to handle the case that the dialog received a provisional
	 * response at some point, but then that there was an error at some
	 * other point before the call could receive a 2xx response.  This means
	 * we have to decrement the early_dlgs counter. */
	if (dlg->state == DLG_STATE_EARLY) {
		if (param->code >= 300)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);
		dlg->state = DLG_STATE_CONFIRMED_NA;
	}


	if (param->code>=300) {
		DBG("DEBUG:dialog:dlg_onreply: destroying dialog "
			"with code %d (%p)\n", param->code, dlg);

		/* dialog setup not completed (3456XX) */
		run_dlg_callbacks( DLGCB_FAILED, dlg, rpl);

		unref_dlg(dlg,1,1);

		if_update_stat( dlg_enable_stats, active_dlgs, -1);
		return;
	}

	if ( (!rpl->to && parse_headers(rpl, HDR_TO_F,0)<0) || !rpl->to ) {
		LOG(L_ERR, "ERROR:dialog:dlg_onreply: bad reply or "
			"missing TO hdr :-/\n");
	} else {
		tag = get_to(rpl)->tag_value;
		if (tag.s!=0 && tag.len!=0)
			/* FIXME: this should be sincronized since multiple 200 can be
			 * sent out */
			dlg_set_totag( dlg, &tag);
	}

	dlg->start_ts = get_ticks();

	/* We have received a non-provisional response, so the dialog is no
	 * longer in the early state.  However, we need to check two things:
	 *
	 * 1) We can get more than one 2xx response.  We only want to decrement
	 *    for the first one.
	 * 
	 * 2) It is possible to receive a 2xx response without ever receiving a
	 *    1xx response.  If this has happened, we don't want to decrement.
	 */

	if ((dlg->num_200s == 0) && (dlg->num_100s > 0)) {
		if_update_stat(dlg_enable_stats, early_dlgs, -1);
	}

	/* Increment the number of 2xx's we've gotten so we don't update our
	* counters more than once */
	dlg->num_200s++;


	/* dialog confirmed */
	run_dlg_callbacks( DLGCB_CONFIRMED, dlg, rpl);

	insert_dlg_timer( &dlg->tl, dlg->lifetime );

	return;
}



inline static int get_dlg_timeout(struct sip_msg *req)
{
	xl_value_t xl_val;

	if( timeout_avp && xl_get_spec_value( req, timeout_avp, &xl_val, 0)==0
	&& xl_val.flags&XL_VAL_INT && xl_val.ri>0 ) {
		return xl_val.ri;
	}
	return default_timeout;
}



void dlg_onreq(struct cell* t, int type, struct tmcb_params *param)
{
	struct dlg_cell *dlg;
	struct sip_msg *req;
	str s;

	req = param->req;

	if ( (!req->to && parse_headers(req, HDR_TO_F,0)<0) || !req->to ) {
		LOG(L_ERR, "ERROR:dialog:dlg_onreq: bad request or "
			"missing TO hdr :-/\n");
		return;
	}
	s = get_to(req)->tag_value;
	if (s.s!=0 && s.len!=0)
		return;

	if (req->first_line.u.request.method_value==METHOD_CANCEL)
		return;

	if ( (req->flags & dlg_flag) != dlg_flag)
		return;

	if ( parse_from_header(req)) {
		LOG(L_ERR, "ERROR:dialog:dlg_onreq: bad request or "
			"missing FROM hdr :-/\n");
		return;
	}
	if ((!req->callid && parse_headers(req,HDR_CALLID_F,0)<0) || !req->callid){
		LOG(L_ERR, "ERROR:dialog:dlg_onreq: bad request or "
			"missing CALLID hdr :-/\n");
		return;
	}
	s = req->callid->body;
	trim(&s);

	dlg = build_new_dlg( &s /*callid*/, &(get_from(req)->uri) /*from uri*/,
		&(get_to(req)->uri) /*to uri*/,
		&(get_from(req)->tag_value)/*from_tag*/ );
	if (dlg==0) {
		LOG(L_ERR,"ERROR:dialog:dlg_onreq: failed to create new dialog\n");
		return;
	}

	/* first INVITE seen (dialog created, unconfirmed) */
	run_create_callbacks( dlg, req);

	link_dlg( dlg , 1/* one extra ref for the callback*/);

	if ( add_dlg_rr_param( req, dlg->h_entry, dlg->h_id)<0 ) {
		LOG(L_ERR,"ERROR:dialog:dlg_onreq: failed to add RR param\n");
		goto error;
	}

	if ( d_tmb.register_tmcb( 0, t,
				  TMCB_RESPONSE_OUT|TMCB_TRANS_DELETED|TMCB_RESPONSE_FWDED,
				  dlg_onreply, (void*)dlg)<0 ) {
		LOG(L_ERR,"ERROR:dialog:dlg_onreq: failed to register TMCB\n");
		goto error;
	}

	dlg->lifetime = get_dlg_timeout(req);

	if_update_stat( dlg_enable_stats, processed_dlgs, 1);
	if_update_stat( dlg_enable_stats, active_dlgs, 1);

	return;
error:
	update_stat(failed_dlgs, 1);
	unref_dlg(dlg,2,1);
	return;
}



static inline int parse_dlg_rr_param(char *p, char *end,
													int *h_entry, int *h_id)
{
	char *s;

	for ( s=p ; p<end && *p!=DLG_SEPARATOR ; p++ );
	if (*p!=DLG_SEPARATOR) {
		LOG(L_ERR,"ERROR:dialog:parse_dlg_rr_param: malformed rr param "
			"'%.*s'\n", (int)(long)(end-s), s);
		return -1;
	}

	if ( (*h_entry=reverse_hex2int( s, p-s))<0 ) {
		LOG(L_ERR,"ERROR:dialog:parse_dlg_rr_param: invalid hash entry "
			"'%.*s'\n", (int)(long)(p-s), s);
		return -1;
	}

	if ( (*h_id=reverse_hex2int( p+1, end-(p+1)))<0 ) {
		LOG(L_ERR,"ERROR:dialog:parse_dlg_rr_param: invalid hash id "
			"'%.*s'\n", (int)(long)(end-(p+1)), p+1 );
		return -1;
	}

	return 0;
}



void dlg_onroute(struct sip_msg* req, str *route_params, void *param)
{
	struct dlg_cell *dlg;
	str val;
	str callid;
	int h_entry;
	int h_id;


	if (d_rrb.get_route_param( req, &rr_param, &val)!=0) {
		DBG("DEBUG:dialog:dlg_onroute: Route param '%.*s' not found\n",
			rr_param.len,rr_param.s);
		return;
	}
	DBG("DEBUG:dialog:dlg_onroute: route param is '%.*s' (len=%d)\n",
		val.len, val.s, val.len);

	if ( parse_dlg_rr_param( val.s, val.s+val.len, &h_entry, &h_id)<0 )
		return;

	dlg = lookup_dlg( h_entry, h_id);
	if (dlg==0) {
		LOG(L_WARN,"WARNING:dialog:dlg_onroute: unable to find dialog\n");
		return;
	}
	if (use_tight_match) {
		if ((!req->callid && parse_headers(req,HDR_CALLID_F,0)<0) ||
		!req->callid) {
			LOG(L_ERR, "ERROR:dialog:dlg_onroute: bad request or "
				"missing CALLID hdr :-/\n");
			return;
		}
		callid = req->callid->body;
		trim(&callid);
		if (dlg->callid.len!=callid.len ||
		strncmp(dlg->callid.s,callid.s,callid.len)!=0) {
			LOG(L_WARN,"WARNING:dialog:dlg_onroute: tight matching failed\n");
			return;
		}
	}

	CURR_DLG_ID = req->id;
	CURR_DLG_LIFETIME = get_ticks()-dlg->start_ts;

	if (req->first_line.u.request.method_value==METHOD_BYE) {
		CURR_DLG_STATUS = DLG_STATE_DELETED;
		if (remove_dlg_timer(&dlg->tl)!=0) {
			unref_dlg( dlg , 1, 0);
			return;
		}
		if (dlg->state!=DLG_STATE_CONFIRMED)
			LOG(L_WARN, "WARNING:dialog:dlg_onroute: BYE for "
				"unconfirmed dialog ?!\n");

		/* dialog terminated (BYE) */
		run_dlg_callbacks( DLGCB_TERMINATED, dlg, req);

		unref_dlg(dlg, 2, 1);
		if_update_stat( dlg_enable_stats, active_dlgs, -1);
		return;
	} else {
		/* within dialog request */
		run_dlg_callbacks( DLGCB_REQ_WITHIN, dlg, req);
	}

	if (req->first_line.u.request.method_value!=METHOD_ACK) {
		dlg->lifetime = get_dlg_timeout(req);
		update_dlg_timer( &dlg->tl, dlg->lifetime );
	} else {
		if (dlg->state==DLG_STATE_CONFIRMED_NA ||
		dlg->state==DLG_STATE_CONFIRMED) {
			dlg->state=DLG_STATE_CONFIRMED;
		} else {
			LOG(L_WARN, "WARNING:dialog:dlg_onroute: ACK for "
				"unconfirmed or unconfirmed_NA dialog ?!\n");
		}
	}

	CURR_DLG_STATUS = dlg->state;

	unref_dlg( dlg , 1, 0);
	return;
}



#define get_dlg_tl_payload(_tl_)  ((struct dlg_cell*)((char *)(_tl_)- \
		(unsigned long)(&((struct dlg_cell*)0)->tl)))

void dlg_ontimeout( struct dlg_tl *tl)
{
	struct dlg_cell *dlg;

	dlg = get_dlg_tl_payload(tl);

	DBG("DEBUG:dialog:dlg_timeout: dlg %p timeout at %d\n",
		dlg, tl->timeout);

	/* dialog timeout */
	run_dlg_callbacks( DLGCB_EXPIRED, dlg, 0);

	unref_dlg(dlg, 1, 1);

	if_update_stat( dlg_enable_stats, expired_dlgs, 1);
	if_update_stat( dlg_enable_stats, active_dlgs, -1);

	return;
}


/* item/pseudo-variables functions */
int it_get_dlg_lifetime(struct sip_msg *msg, xl_value_t *res,
		xl_param_t *param, int flags)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if (CURR_DLG_ID!=msg->id)
		return -1;

	res->ri = CURR_DLG_LIFETIME;
	ch = int2str( (unsigned long)res->ri, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->flags = XL_VAL_STR|XL_VAL_INT|XL_TYPE_INT;

	return 0;
}


int it_get_dlg_status(struct sip_msg *msg, xl_value_t *res,
		xl_param_t *param, int flags)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if (CURR_DLG_ID!=msg->id)
		return -1;

	res->ri = CURR_DLG_STATUS;
	ch = int2str( (unsigned long)res->ri, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->flags = XL_VAL_STR|XL_VAL_INT|XL_TYPE_INT;

	return 0;
}

