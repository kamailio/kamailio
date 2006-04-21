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
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 */


#include "string.h"

#include "../../trim.h"
#include "../../items.h"
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

extern struct tm_binds d_tmb;
extern struct rr_binds d_rrb;

/* statistic variables */
extern int dlg_enable_stats;
extern stat_var *active_dlgs;
extern stat_var *processed_dlgs;
extern stat_var *expired_dlgs;


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
	if (dlg==0)
		return;

	if (type==TMCB_TRANS_DELETED) {
		if (dlg->state == DLG_STATE_UNCONFIRMED) {
			DBG("DEBUG:dialog:dlg_onreply: destroying unused dialog %p\n",dlg);
			unref_dlg(dlg,1,1);
			if_update_stat( dlg_enable_stats, active_dlgs, -1);
		}
		return;
	}

	rpl = param->rpl;

	if (type==TMCB_RESPONSE_FWDED) {
	  /* The state does not change, but the msg is mutable in this callback */
	  run_dlg_callbacks(DLGCB_RESPONSE_FWDED, dlg, rpl);
	  return;
	}

	if (param->code<200) {
		DBG("DEBUG:dialog:dlg_onreply: dialog %p goes into Early state "
			"with code %d\n", dlg, param->code);
		dlg->state = DLG_STATE_EARLY;
		/* Early state, the message is immutable here */
		run_dlg_callbacks(DLGCB_EARLY, dlg, rpl);
		return;
	}

	if (param->code>=300) {
		DBG("DEBUG:dialog:dlg_onreply: destroying unconfirmed dialog "
			"with code %d (%p)\n", param->code, dlg);

		/* dialog setup not completed (3456XX) */
		run_dlg_callbacks( DLGCB_FAILED, dlg, rpl);

		unref_dlg(dlg,1,1);
		if_update_stat( dlg_enable_stats, active_dlgs, -1);
		return;
	}

	DBG("DEBUG:dialog:dlg_onreply: dialog %p confirmed\n",dlg);
	dlg->state = DLG_STATE_CONFIRMED;

	if ( (!rpl->to && parse_headers(rpl, HDR_TO_F,0)<0) || !rpl->to ) {
		LOG(L_ERR, "ERROR:dialog:dlg_onreply: bad reply or "
			"missing TO hdr :-/\n");
	} else {
		tag = get_to(rpl)->tag_value;
		if (tag.s!=0 && tag.len!=0)
			dlg_set_totag( dlg, &tag);
	}

	/* dialog confirmed */
	run_dlg_callbacks( DLGCB_CONFIRMED, dlg, rpl);

	insert_dlg_timer( &dlg->tl, dlg->lifetime );

	*(param->param) = 0;

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

	link_dlg( dlg );

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
	unref_dlg(dlg,1,1);
	return;
}



static inline int parse_dlg_rr_param(char *p, char *end,
													int *h_entry, int *h_id)
{
	char *s;

	for ( s=p ; p<end && *p!=DLG_SEPARATOR ; p++ );
	if (*p!=DLG_SEPARATOR) {
		LOG(L_ERR,"ERROR:dialog:parse_dlg_rr_param: malformed rr param "
			"'%.*s'\n", end-s, s);
		return -1;
	}

	if ( (*h_entry=reverse_hex2int( s, p-s))<0 ) {
		LOG(L_ERR,"ERROR:dialog:parse_dlg_rr_param: invalid hash entry "
			"'%.*s'\n", p-s, s);
		return -1;
	}

	if ( (*h_id=reverse_hex2int( p+1, end-(p+1)))<0 ) {
		LOG(L_ERR,"ERROR:dialog:parse_dlg_rr_param: invalid hash id "
			"'%.*s'\n", end-(p+1), p+1 );
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

	if (req->first_line.u.request.method_value==METHOD_BYE) {
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
	}

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

