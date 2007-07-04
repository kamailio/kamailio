/*
 * $Id$
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
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 * 2006-11-28  Added support for tracking the number of early dialogs, and the
 *             number of failed dialogs. This involved updates to dlg_onreply()
 *             (Jeffrey Magder - SOMA Networks)
 * 2007-03-06  syncronized state machine added for dialog state. New tranzition
 *             design based on events; removed num_1xx and num_2xx (bogdan)
 * 2007-04-30  added dialog matching without DID (dialog ID), but based only
 *             on RFC3261 elements - based on an original patch submitted 
 *             by Michel Bensoussan <michel@extricom.com> (bogdan)
 * 2007-05-17  new feature: saving dialog info into a database if 
 *             realtime update is set(ancuta)

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
#include "dlg_db_handler.h"

static str       rr_param;
static int       dlg_flag;
static xl_spec_t *timeout_avp;
static int       default_timeout;
static int       use_tight_match;
static int       seq_match_mode;
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
		xl_spec_t *timeout_avp_p ,int default_timeout_p, 
		int use_tight_match_p, int seq_match_mode_p)
{
	rr_param.s = rr_param_p;
	rr_param.len = strlen(rr_param.s);

	dlg_flag = 1<<dlg_flag_p;

	timeout_avp = timeout_avp_p;
	default_timeout = default_timeout_p;
	use_tight_match = use_tight_match_p;
	seq_match_mode = seq_match_mode_p;
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
	int new_state;
	int old_state;
	int unref;
	int event;
	str tag;

	dlg = (struct dlg_cell *)(*param->param);
	if (shutdown_done || dlg==0)
		return;

	rpl = param->rpl;

	if (type==TMCB_RESPONSE_FWDED) {
		/* The state does not change, but the msg is mutable in this callback*/
		run_dlg_callbacks(DLGCB_RESPONSE_FWDED, dlg, rpl);
		return;
	}

	if (type==TMCB_TRANS_DELETED)
		event = DLG_EVENT_TDEL;
	else if (param->code<200)
		event = DLG_EVENT_RPL1xx;
	else if (param->code<300)
		event = DLG_EVENT_RPL2xx;
	else
		event = DLG_EVENT_RPL3xx;

	next_state_dlg( dlg, event, &old_state, &new_state, &unref);

	if (new_state==DLG_STATE_EARLY) {
		run_dlg_callbacks(DLGCB_EARLY, dlg, rpl);
		if (old_state!=DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, 1);
		return;
	}

	if (new_state==DLG_STATE_CONFIRMED_NA &&
	old_state!=DLG_STATE_CONFIRMED_NA && old_state!=DLG_STATE_CONFIRMED ) {
		DBG("DEBUG:dialog:dlg_onreply: dialog %p confirmed\n",dlg);
		/* set to tag*/
		if ( (!rpl->to && parse_headers(rpl, HDR_TO_F,0)<0) || !rpl->to ) {
			LOG(L_ERR, "ERROR:dialog:dlg_onreply: bad reply or "
				"missing TO hdr :-/\n");
		} else {
			tag = get_to(rpl)->tag_value;
			if (tag.s!=0 && tag.len!=0)
				dlg_set_totag( dlg, &tag);
		}

		/* set start time */
		dlg->start_ts = get_ticks();

		/* save the settings to the database, 
		 * if realtime saving mode configured- save dialog now
		 * else: the next the timer will fire the update*/
		dlg->flags |= DLG_FLAG_CHANGED;
		if ( dlg_db_mode==DB_MODE_REALTIME )
			update_dialog_dbinfo(dlg);

		insert_dlg_timer( &dlg->tl, dlg->lifetime );

		/* dialog confirmed */
		run_dlg_callbacks( DLGCB_CONFIRMED, dlg, rpl);

		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);

		if_update_stat(dlg_enable_stats, active_dlgs, 1);
		return;
	}

	if ( event==DLG_EVENT_RPL3xx && new_state==DLG_STATE_DELETED ) {
		DBG("DEBUG:dialog:dlg_onreply: dialog %p failed (negative reply)\n",
			dlg);
		/* dialog setup not completed (3456XX) */
		run_dlg_callbacks( DLGCB_FAILED, dlg, rpl);
		if (unref)
			unref_dlg(dlg,unref);
		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);
		return;
	}


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

	if ( seq_match_mode!=SEQ_MATCH_NO_ID &&
	add_dlg_rr_param( req, dlg->h_entry, dlg->h_id)<0 ) {
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

	return;
error:
	unref_dlg(dlg,2);
	update_stat(failed_dlgs, 1);
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


static inline int pre_match_parse( struct sip_msg *req, str *callid,
														str *ftag, str *ttag)
{
	if (parse_headers(req,HDR_CALLID_F|HDR_TO_F,0)<0 || !req->callid ||
	!req->to || get_to(req)->tag_value.len==0 ) {
		LOG(L_ERR, "ERROR:dialog:pre_match_parse: bad request or "
			"missing CALLID/TO hdr :-/\n");
		return -1;
	}

	if (parse_from_header(req)<0 || get_from(req)->tag_value.len==0) {
		LOG(L_ERR,"ERROR:dialog:pre_match_parse: failed to get From header\n");
		return -1;
	}

	/* callid */
	*callid = req->callid->body;
	trim(callid);

	if (d_rrb.is_direction(req,RR_FLOW_UPSTREAM)==0) {
		/* to tag */
		*ttag = get_from(req)->tag_value;
		/* from tag */
		*ftag = get_to(req)->tag_value;
	} else {
		/* to tag */
		*ttag = get_to(req)->tag_value;
		/* from tag */
		*ftag = get_from(req)->tag_value;
	}
	return 0;
}


void dlg_onroute(struct sip_msg* req, str *route_params, void *param)
{
	struct dlg_cell *dlg;
	str val;
	str callid;
	str ftag;
	str ttag;
	int h_entry;
	int h_id;
	int new_state;
	int old_state;
	int unref;
	int event;

	dlg = 0;
	if ( seq_match_mode!=SEQ_MATCH_NO_ID ) {
		if( d_rrb.get_route_param( req, &rr_param, &val)!=0) {
			DBG("DEBUG:dialog:dlg_onroute: Route param '%.*s' not found\n",
				rr_param.len,rr_param.s);
			if (seq_match_mode==SEQ_MATCH_STRICT_ID )
				return;
		} else {
			DBG("DEBUG:dialog:dlg_onroute: route param is '%.*s' (len=%d)\n",
				val.len, val.s, val.len);

			if ( parse_dlg_rr_param( val.s, val.s+val.len, &h_entry, &h_id)<0 )
				return;

			dlg = lookup_dlg( h_entry, h_id);
			if (dlg==0) {
				LOG(L_WARN, "WARNING:dialog:dlg_onroute: "
					"unable to find dialog\n");
				return;
			}

			if (use_tight_match) {
				if (pre_match_parse( req, &callid, &ftag, &ttag)<0)
					return;
				if (match_dialog( dlg, &callid, &ftag, &ttag )==0) {
					LOG(L_WARN,"WARNING:dialog:dlg_onroute: "
						"tight matching failed\n");
					return;
				}
			}
		}
	}

	if (dlg==0) {
		if (pre_match_parse( req, &callid, &ftag, &ttag)<0)
			return;
		dlg = get_dlg(&callid, &ftag, &ttag);
		if (!dlg){
			DBG("DEBUG:dialog:dlg_onroute: Callid '%.*s' not found\n",
				req->callid->body.len, req->callid->body.s);
			return;
		}
	}

	/* run state machine */
	if (req->first_line.u.request.method_value==METHOD_ACK)
		event = DLG_EVENT_REQACK;
	else if (req->first_line.u.request.method_value==METHOD_BYE)
		event = DLG_EVENT_REQBYE;
	else
		event = DLG_EVENT_REQ;

	next_state_dlg( dlg, event, &old_state, &new_state, &unref);

	CURR_DLG_ID = req->id;
	CURR_DLG_LIFETIME = get_ticks()-dlg->start_ts;
	CURR_DLG_STATUS = new_state;

	/* run actions for the transition */
	if (event==DLG_EVENT_REQBYE && new_state==DLG_STATE_DELETED &&
	old_state!=DLG_STATE_DELETED) {
		DBG("DEBUG:dialog:dlg_onroute: BYE successfully processed\n");
		/* remove from timer */
		remove_dlg_timer(&dlg->tl);
		/* dialog terminated (BYE) */
		run_dlg_callbacks( DLGCB_TERMINATED, dlg, req);
		/* destroy dialog */
		unref_dlg(dlg, unref+1);

		if_update_stat( dlg_enable_stats, active_dlgs, -1);
		return;
	}

	if (event==DLG_EVENT_REQ && new_state==DLG_STATE_CONFIRMED) {
		DBG("DEBUG:dialog:dlg_onroute: sequential request successfully "
			"processed\n");
		dlg->lifetime = get_dlg_timeout(req);
		if (update_dlg_timer( &dlg->tl, dlg->lifetime )!=-1) {
			dlg->flags |= DLG_FLAG_CHANGED;
			if ( dlg_db_mode==DB_MODE_REALTIME )
				update_dialog_dbinfo(dlg);
			/* within dialog request */
			run_dlg_callbacks( DLGCB_REQ_WITHIN, dlg, req);
		}
	}

	unref_dlg( dlg , 1);
	return;
}



#define get_dlg_tl_payload(_tl_)  ((struct dlg_cell*)((char *)(_tl_)- \
		(unsigned long)(&((struct dlg_cell*)0)->tl)))

void dlg_ontimeout( struct dlg_tl *tl)
{
	struct dlg_cell *dlg;
	int new_state;
	int old_state;
	int unref;

	dlg = get_dlg_tl_payload(tl);

	next_state_dlg( dlg, DLG_EVENT_REQBYE, &old_state, &new_state, &unref);

	if (new_state==DLG_STATE_DELETED && old_state!=DLG_STATE_DELETED) {
		DBG("DEBUG:dialog:dlg_timeout: dlg %p timeout at %d\n",
			dlg, tl->timeout);

		/* dialog timeout */
		run_dlg_callbacks( DLGCB_EXPIRED, dlg, 0);

		unref_dlg(dlg, unref);

		if_update_stat( dlg_enable_stats, expired_dlgs, 1);
		if_update_stat( dlg_enable_stats, active_dlgs, -1);
	}

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
		return xl_get_null( msg, res, param, flags);

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
		return xl_get_null( msg, res, param, flags);

	res->ri = CURR_DLG_STATUS;
	ch = int2str( (unsigned long)res->ri, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->flags = XL_VAL_STR|XL_VAL_INT|XL_TYPE_INT;

	return 0;
}

