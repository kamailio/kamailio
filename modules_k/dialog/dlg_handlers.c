/*
 * $Id$
 *
 * Copyright (C) 2006 Voice System SRL
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
 * 2007-07-06  support for saving additional dialog info : cseq, contact,
 *             route_set and socket_info for both caller and callee (ancuta)
 * 2007-07-10  Optimized dlg_match_mode 2 (DID_NONE), it now employs a proper
 *             hash table lookup and isn't dependant on the is_direction
 *             function (which requires an RR param like dlg_match_mode 0
 *             anyways.. ;) ; based on a patch from
 *             Tavis Paquette <tavis@galaxytelecom.net>
 *             and Peter Baer <pbaer@galaxytelecom.net>  (bogdan)
 * 2008-04-04  added direction reporting in dlg callbacks (bogdan)
 */


/*!
 * \file
 * \brief Functions related to dialog handling
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <string.h>
#include <time.h>

#include "../../trim.h"
#include "../../pvar.h"
#include "../../timer.h"
#include "../../lib/kcore/statistics.h"
#include "../../action.h"
#include "../../script_cb.h"
#include "../../lib/kcore/faked_msg.h"
#include "../../lib/kcore/parser_helpers.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../modules/tm/tm_load.h"
#include "../rr/api.h"
#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_cb.h"
#include "dlg_handlers.h"
#include "dlg_req_within.h"
#include "dlg_db_handler.h"
#include "dlg_profile.h"
#include "dlg_var.h"

static str       rr_param;		/*!< record-route parameter for matching */
static int       dlg_flag;		/*!< flag for dialog tracking */
static pv_spec_t *timeout_avp;		/*!< AVP for timeout setting */
static int       default_timeout;	/*!< default dialog timeout */
static int       shutdown_done = 0;	/*!< 1 when destroy_dlg_handlers was called */
extern int       seq_match_mode;	/*!< dlg_match mode */ 
extern int       detect_spirals;

extern struct rr_binds d_rrb;		/*!< binding to record-routing module */

/* statistic variables */
extern stat_var *early_dlgs; 		/*!< number of early dialogs */
extern stat_var *processed_dlgs;	/*!< number of processed dialogs */
extern stat_var *expired_dlgs;		/*!< number of expired dialogs */
extern stat_var *failed_dlgs;		/*!< number of failed dialogs */

extern pv_elem_t *ruri_param_model;	/*!< pv-string to get r-uri */

static unsigned int CURR_DLG_LIFETIME = 0;	/*!< current dialog lifetime */
static unsigned int CURR_DLG_STATUS = 0;	/*!< current dialog state */
static unsigned int CURR_DLG_ID  = 0xffffffff;	/*!< current dialog id */


/*! size of the dialog record-route parameter */
#define RR_DLG_PARAM_SIZE  (2*2*sizeof(int)+3+MAX_DLG_RR_PARAM_NAME)
/*! separator inside the record-route paramter */
#define DLG_SEPARATOR      '.'


/*!
 * \brief Initialize the dialog handlers
 * \param rr_param_p added record-route parameter
 * \param dlg_flag_p dialog flag
 * \param timeout_avp_p AVP for timeout setting
 * \param default_timeout_p default timeout
 */
void init_dlg_handlers(char *rr_param_p, int dlg_flag_p,
		pv_spec_t *timeout_avp_p ,int default_timeout_p)
{
	rr_param.s = rr_param_p;
	rr_param.len = strlen(rr_param.s);

	dlg_flag = 1<<dlg_flag_p;

	timeout_avp = timeout_avp_p;
	default_timeout = default_timeout_p;
}


/*!
 * \brief Shutdown operation of the module
 */
void destroy_dlg_handlers(void)
{
	shutdown_done = 1;
}


/*!
 * \brief Add record-route parameter for dialog tracking
 * \param req SIP request
 * \param entry dialog hash entry
 * \param id dialog hash id
 * \return 0 on success, -1 on failure
 */
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
		LM_ERR("failed to add rr param\n");
		return -1;
	}

	return 0;
}


/*!
 * \brief Parse SIP message and populate leg informations
 *
 * Parse SIP message and populate leg informations. 
 * \param dlg the dialog to add cseq, contact & record_route
 * \param msg sip message
 * \param flag  0-for a request(INVITE), 1- for a reply(200 ok)
 * \return 0 on success, -1 on failure
 * \note for a request: get record route in normal order, for a reply get
 * in reverse order, skipping the ones from the request and the proxies' own
 */
int populate_leg_info( struct dlg_cell *dlg, struct sip_msg *msg,
	struct cell* t, unsigned int leg, str *tag)
{
	unsigned int skip_recs;
	str cseq;
	str contact;
	str rr_set;

	dlg->bind_addr[leg] = msg->rcv.bind_address;

	/* extract the cseq number as string */
	if (leg==DLG_CALLER_LEG) {
		if((!msg->cseq && (parse_headers(msg,HDR_CSEQ_F,0)<0 || !msg->cseq))
			|| !msg->cseq->parsed){
			LM_ERR("bad sip message or missing CSeq hdr :-/\n");
			goto error0;
		}
		cseq = (get_cseq(msg))->number;
	} else {
		/* use the same as in request */
		cseq = dlg->cseq[DLG_CALLER_LEG];
	}

	/* extract the contact address */
	if (!msg->contact&&(parse_headers(msg,HDR_CONTACT_F,0)<0||!msg->contact)){
		LM_ERR("bad sip message or missing Contact hdr\n");
		goto error0;
	}
	if ( parse_contact(msg->contact)<0 ||
	((contact_body_t *)msg->contact->parsed)->contacts==NULL ||
	((contact_body_t *)msg->contact->parsed)->contacts->next!=NULL ) {
		LM_ERR("bad Contact HDR\n");
		goto error0;
	}
	contact = ((contact_body_t *)msg->contact->parsed)->contacts->uri;

	/* extract the RR parts */
	if(!msg->record_route && (parse_headers(msg,HDR_EOH_F,0)<0)  ){
		LM_ERR("failed to parse record route header\n");
		goto error0;
	}

	if (leg==DLG_CALLER_LEG) {
		skip_recs = 0;
	} else {
		/* was the 200 OK received or local generated */
		skip_recs = dlg->from_rr_nb +
			((t->relayed_reply_branch>=0)?
				((t->uac[t->relayed_reply_branch].flags&TM_UAC_FLAG_R2)?2:
				 ((t->uac[t->relayed_reply_branch].flags&TM_UAC_FLAG_RR)?1:0))
				:0);
	}

	if(msg->record_route){
		if( print_rr_body(msg->record_route, &rr_set, leg,
							&skip_recs) != 0 ){
			LM_ERR("failed to print route records \n");
			goto error0;
		}
	} else {
		rr_set.s = 0;
		rr_set.len = 0;
	}

	if(leg==DLG_CALLER_LEG)
		dlg->from_rr_nb = skip_recs;

	LM_DBG("route_set %.*s, contact %.*s, cseq %.*s and bind_addr %.*s\n",
		rr_set.len, rr_set.s, contact.len, contact.s,
		cseq.len, cseq.s,
		msg->rcv.bind_address->sock_str.len,
		msg->rcv.bind_address->sock_str.s);

	if (dlg_set_leg_info( dlg, tag, &rr_set, &contact, &cseq, leg)!=0) {
		LM_ERR("dlg_set_leg_info failed\n");
		if (rr_set.s) pkg_free(rr_set.s);
		goto error0;
	}

	if (rr_set.s) pkg_free(rr_set.s);

	return 0;
error0:
	return -1;
}


/*!
 * \brief Function that is registered as TM callback and called on replies
 *
 * Function that is registered as TM callback and called on replies. It
 * parses the reply and set the appropriate event. This is then used to
 * update the dialog state, run eventual dialog callbacks and save or
 * update the necessary informations about the dialog.
 * \see next_state_dlg
 * \param t transaction, unused
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
static void dlg_onreply(struct cell* t, int type, struct tmcb_params *param)
{
	struct sip_msg *rpl;
	struct dlg_cell *dlg;
	int new_state, old_state, unref, event;
	str tag;

	dlg = (struct dlg_cell *)(*param->param);
	if (shutdown_done || dlg==0)
		return;

	rpl = param->rpl;

	if (type==TMCB_RESPONSE_FWDED) {
		/* The state does not change, but the msg is mutable in this callback*/
		run_dlg_callbacks(DLGCB_RESPONSE_FWDED, dlg, rpl, DLG_DIR_UPSTREAM, 0);
		return;
	}

	if (type==TMCB_DESTROY)
		event = DLG_EVENT_TDEL;
	else if (param->code<200)
		event = DLG_EVENT_RPL1xx;
	else if (param->code<300)
		event = DLG_EVENT_RPL2xx;
	else
		event = DLG_EVENT_RPL3xx;

	next_state_dlg( dlg, event, &old_state, &new_state, &unref);

	if (new_state==DLG_STATE_EARLY) {
		run_dlg_callbacks(DLGCB_EARLY, dlg, rpl, DLG_DIR_UPSTREAM, 0);
		if (old_state!=DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, 1);
		return;
	}

	if (new_state==DLG_STATE_CONFIRMED_NA &&
	old_state!=DLG_STATE_CONFIRMED_NA && old_state!=DLG_STATE_CONFIRMED ) {
		LM_DBG("dialog %p confirmed\n",dlg);

		 if (rpl != FAKED_REPLY) {
			/* get to tag*/
			if ( !rpl->to && ((parse_headers(rpl, HDR_TO_F,0)<0)
						|| !rpl->to) ) {
				LM_ERR("bad reply or missing TO hdr :-/\n");
				tag.s = 0;
				tag.len = 0;
			} else {
				tag = get_to(rpl)->tag_value;
				if (tag.s==0 || tag.len==0) {
					LM_ERR("missing TAG param in TO hdr :-/\n");
					tag.s = 0;
					tag.len = 0;
				}
			}

			/* save callee's tag, cseq, contact and record route*/
			if (populate_leg_info( dlg, rpl, t, DLG_CALLEE_LEG, &tag) !=0) {
				LM_ERR("could not add further info to the dialog\n");
			}
		 } else {
			 LM_ERR("Faked reply!\n");
		 }

		/* set start time */
		dlg->start_ts = (unsigned int)(time(0));

		/* save the settings to the database,
		 * if realtime saving mode configured- save dialog now
		 * else: the next time the timer will fire the update*/
		dlg->dflags |= DLG_FLAG_NEW;
		if ( dlg_db_mode==DB_MODE_REALTIME )
			update_dialog_dbinfo(dlg);

		if (0 != insert_dlg_timer( &dlg->tl, dlg->lifetime )) {
			LM_CRIT("Unable to insert dlg %p [%u:%u] on event %d [%d->%d] "
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",
				dlg, dlg->h_entry, dlg->h_id, event, old_state, new_state,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
		} else {
			ref_dlg(dlg,1);
		}

		/* dialog confirmed */
		run_dlg_callbacks( DLGCB_CONFIRMED, dlg, rpl, DLG_DIR_UPSTREAM, 0);

		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);

		if (unref) unref_dlg(dlg,unref);
		if_update_stat(dlg_enable_stats, active_dlgs, 1);
		return;
	}

	if ( old_state!=DLG_STATE_DELETED && new_state==DLG_STATE_DELETED ) {
		LM_DBG("dialog %p failed (negative reply)\n", dlg);
		/* dialog setup not completed (3456XX) */
		run_dlg_callbacks( DLGCB_FAILED, dlg, rpl, DLG_DIR_UPSTREAM, 0);
		/* do unref */
		if (unref)
			unref_dlg(dlg,unref);
		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);

		if_update_stat(dlg_enable_stats, failed_dlgs, 1);

		return;
	}

	if (unref) unref_dlg(dlg,unref);

	return;
}


/*!
 * \brief Helper function that run dialog callbacks on forwarded requests
 * \see dlg_seq_up_onreply
 * \see dlg_seq_down_onreply
 * \param t transaction, unused
 * \param type type of the callback, should be TMCB_RESPONSE_FWDED
 * \param param saved dialog structure inside the callback
 * \param direction direction of the request
 */
static void dlg_seq_onreply_helper(struct cell* t, int type,
		struct tmcb_params *param, const int direction)
{
	struct dlg_cell *dlg;

	dlg = (struct dlg_cell *)(*param->param);
	if (shutdown_done || dlg==0)
		return;

	if (type==TMCB_RESPONSE_FWDED) {
		run_dlg_callbacks(DLGCB_RESPONSE_WITHIN, dlg, param->rpl,
			direction, 0);
		return;
	}

	return;
}


/*!
 * \brief Run dialog callbacks on forwarded requests in upstream direction
 * \see dlg_seq_onreply_helper
 * \param t transaction, unused
 * \param type type of the callback, should be TMCB_RESPONSE_FWDED
 * \param param saved dialog structure inside the callback
 */
static void dlg_seq_up_onreply(struct cell* t, int type, struct tmcb_params *param)
{
	return dlg_seq_onreply_helper(t, type, param, DLG_DIR_UPSTREAM);
}


/*!
 * \brief Run dialog callbacks on forwarded requests in downstream direction
 * \see dlg_seq_onreply_helper
 * \param t transaction, unused
 * \param type type of the callback, should be TMCB_RESPONSE_FWDED
 * \param param saved dialog structure inside the callback
 */
static void dlg_seq_down_onreply(struct cell* t, int type, struct tmcb_params *param)
{
	return dlg_seq_onreply_helper(t, type, param, DLG_DIR_DOWNSTREAM);
}


/*!
 * \brief Return the timeout for a dialog
 * \param req SIP message
 * \return value from timeout AVP if present or default timeout
 */
inline static int get_dlg_timeout(struct sip_msg *req)
{
	pv_value_t pv_val;

	if( timeout_avp ) {
		if ( pv_get_spec_value( req, timeout_avp, &pv_val)==0 &&
				pv_val.flags&PV_VAL_INT && pv_val.ri>0 ) {
			return pv_val.ri;
		}
		LM_DBG("invalid AVP value, using default timeout\n");
	}
	return default_timeout;
}


/*!
 * \brief Helper function to get the necessary content from SIP message
 * \param req SIP request
 * \param callid found callid
 * \param ftag found from tag
 * \param ttag found to tag
 * \param with_ttag flag set if to tag must be found for success
 * \return 0 on success, -1 on failure
 */
static inline int pre_match_parse( struct sip_msg *req, str *callid,
		str *ftag, str *ttag, int with_ttag)
{
	if (parse_headers(req,HDR_CALLID_F|HDR_TO_F,0)<0 || !req->callid ||
			!req->to ) {
		LM_ERR("bad request or missing CALLID/TO hdr :-/\n");
		return -1;
	}

	if (get_to(req)->tag_value.len==0) {
		if (with_ttag == 1) {
			/* out of dialog request with preloaded Route headers; ignore. */
			return -1;
		} else {
			ttag->s = NULL;
			ttag->len = 0;
		}
	} else {
		*ttag = get_to(req)->tag_value;
	}

	if (parse_from_header(req)<0 || get_from(req)->tag_value.len==0) {
		LM_ERR("failed to get From header\n");
		return -1;
	}

	/* callid */
	*callid = req->callid->body;
	trim(callid);
	/* from tag */
	*ftag = get_from(req)->tag_value;
	return 0;
}


/*!
 * \brief Function that is registered as TM callback and called on requests
 * \see dlg_new_dialog
 * \param t transaction, used to created the dialog
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
void dlg_onreq(struct cell* t, int type, struct tmcb_params *param)
{
	struct sip_msg *req = param->req;

	if((req->flags&dlg_flag)!=dlg_flag)
		return;
	if (current_dlg_pointer!=NULL)
		return;
	dlg_new_dialog(req, t);
}


/*!
 * \brief Unreference a new dialog, helper function for dlg_onreq
 * \see dlg_onreq
 * \param dialog unreferenced dialog
 */
static void unref_new_dialog(void *dialog)
{
	struct tmcb_params p;

	memset(&p, 0, sizeof(struct tmcb_params));
	p.param = (void*)&dialog;
	dlg_onreply(0, TMCB_DESTROY, &p);
}

/*!
 * \brief Unreference a dialog (small wrapper to take care of shutdown)
 * \see unref_dlg
 * \param dialog unreferenced dialog
 */
static void unreference_dialog(void *dialog)
{
    // if the dialog table is gone, it means the system is shutting down.
    if (!d_table)
        return;
    unref_dlg((struct dlg_cell*)dialog, 1);
}

/*!
 * \brief Dummy callback just to keep the compiler happy
 * \param t unused
 * \param type unused
 * \param param unused
 */
void dlg_tmcb_dummy(struct cell* t, int type, struct tmcb_params *param)
{
	return;
}

/*!
 * \brief Release a transaction from a dialog
 * \param t transaction
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
static void release_dlg_from_tm(struct cell* t,
                                int type,
                                struct tmcb_params *param)
{
    struct dlg_cell *dlg = get_dialog_from_tm(t);

    if (!dlg)
    {
        LM_ERR("Failed to get and unref dialog from transaction!");
        return;
    }

    unreference_dialog(dlg);
}

/*!
 * \brief Register a transaction on a dialog
 * \param t transaction
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
static void store_dlg_in_tm(struct sip_msg* msg,
                            struct cell* t,
                            struct dlg_cell *dlg)
{
    if( !msg || msg == FAKED_REPLY || !t || !dlg)
    {
        LM_ERR("invalid parameter msg(%p), t(%p), dlg(%p)\n", msg, t, dlg);
        return;
    }

    if(get_dialog_from_tm(t))
    {
        LM_NOTICE("dialog %p is already set for this transaction!\n",dlg);
        return;
    }

    if( d_tmb.register_tmcb (msg,
                             t,
                             TMCB_MAX,
                             dlg_tmcb_dummy,
                             (void*)dlg, 0)<0 )
    {
        LM_ERR("failed cache in T the shortcut to dlg %p\n",dlg);
        return;
    }

    ref_dlg(dlg, 1);

    if (d_tmb.register_tmcb (msg,
                             t,
                             TMCB_DESTROY,
                             release_dlg_from_tm,
                             (void*)dlg, NULL)<0 )
    {
        LM_ERR("failed to register unref tm for handling dialog-termination\n");
    }
}

/*!
 * \brief Callback to register a transaction on a dialog
 * \param t transaction, unused
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
static void store_dlg_in_tm_cb (struct cell* t,
                                int type,
                                struct tmcb_params *param)
{
    struct dlg_cell *dlg = (struct dlg_cell *)(*param->param);

    struct sip_msg* msg = param->rpl;
    if (msg == NULL || msg == FAKED_REPLY)
    {
        msg = param->req;
    }

    store_dlg_in_tm (msg, t, dlg);
}

/*!
 * \brief Create a new dialog from a sip message
 *
 * Create a new dialog from a SIP message, register a callback
 * to keep track of the dialog with help of the tm module.
 * This function is either called from the request callback, or
 * from the dlg_manage function in the configuration script.
 * \see dlg_onreq
 * \see w_dlg_manage
 * \param msg SIP message
 * \param t transaction
 * \return 0 on success, -1 on failure
 */ 
int dlg_new_dialog(struct sip_msg *msg, struct cell *t)
{
	struct dlg_cell *dlg;
	str s;
	str callid;
	str ftag;
	str ttag;
	str req_uri;
	unsigned int dir;
	unsigned int del;

	if(current_dlg_pointer != NULL)
		return -1;

	if(msg->first_line.u.request.method_value==METHOD_CANCEL)
		return -1;

	if(pre_match_parse( msg, &callid, &ftag, &ttag, 0)<0) {
		LM_WARN("pre-matching failed\n");
		return -1;
	}

	if(ttag.s!=0 && ttag.len!=0)
		return -1;

	if (pv_printf_s(msg, ruri_param_model, &req_uri)<0) {
		LM_ERR("error - cannot print the r-uri format\n");
		return -1;
	}
	trim(&req_uri);

	if (detect_spirals)
	{
		dir = DLG_DIR_NONE;

		dlg = get_dlg(&callid, &ftag, &ttag, &dir, &del);
		if (del == 1)
		{
			LM_WARN("Failed to get dialog (callid: '%.*s') because it is marked for deletion!\n",
					callid.len, callid.s);
			unref_dlg(dlg, 1);
			return 0;
		}
		if (dlg)
		{
			LM_DBG("Callid '%.*s' found, must be a spiraled request\n",
					callid.len, callid.s);

			run_dlg_callbacks( DLGCB_SPIRALED, dlg, msg, DLG_DIR_DOWNSTREAM, 0);

			// get_dlg with del==0 has incremented the ref count by 1
			unref_dlg(dlg, 1);
			goto finish;
		}
	}

	dlg = build_new_dlg (&callid /*callid*/,
			&(get_from(msg)->uri) /*from uri*/,
			&(get_to(msg)->uri) /*to uri*/,
			&ftag/*from_tag*/,
			&req_uri /*r-uri*/ );

	if (dlg==0)
	{
		LM_ERR("failed to create new dialog\n");
		return -1;
	}

	/* save caller's tag, cseq, contact and record route*/
	if (populate_leg_info(dlg, msg, t, DLG_CALLER_LEG,
			&(get_from(msg)->tag_value)) !=0)
	{
		LM_ERR("could not add further info to the dialog\n");
		shm_free(dlg);
		return -1;
	}


	link_dlg(dlg,0);

	run_create_callbacks(dlg, msg);

	/* first INVITE seen (dialog created, unconfirmed) */
	if ( seq_match_mode!=SEQ_MATCH_NO_ID &&
			add_dlg_rr_param( msg, dlg->h_entry, dlg->h_id)<0 ) {
		LM_ERR("failed to add RR param\n");
		goto error;
	}

	if ( d_tmb.register_tmcb( msg, t,
				TMCB_RESPONSE_READY|TMCB_RESPONSE_FWDED,
				dlg_onreply, (void*)dlg, unref_new_dialog)<0 ) {
		LM_ERR("failed to register TMCB\n");
		goto error;
	}
	// increase reference counter because of registered callback
	ref_dlg(dlg, 1);

	dlg->lifetime = get_dlg_timeout(msg);
	s.s   = _dlg_ctx.to_route_name;
	s.len = strlen(s.s);
	dlg_set_toroute(dlg, &s);
	dlg->sflags |= _dlg_ctx.flags;

	if (_dlg_ctx.to_bye!=0)
		dlg->dflags |= DLG_FLAG_TOBYE;

	if_update_stat( dlg_enable_stats, processed_dlgs, 1);

finish:
	set_current_dialog(msg, dlg);
	_dlg_ctx.dlg = dlg;
	ref_dlg(dlg, 1);

	if (t) {
		// transaction exists ==> keep ref counter large enough to
		// avoid premature cleanup and ensure proper dialog referencing
		store_dlg_in_tm( msg, t, dlg);
	}
	else
	{
		// no transaction exists ==> postpone work until we see the
		// request being forwarded statefully
		if ( d_tmb.register_tmcb( msg, NULL, TMCB_REQUEST_FWDED,
					store_dlg_in_tm_cb, (void*)dlg, NULL)<0 ) {
			LM_ERR("failed to store dialog in transaction during dialog creation for later reference\n");
		}
	}

	return 0;
error:
	unref_dlg(dlg,1);
	profile_cleanup(msg, 0, NULL);
	return -1;
}


/*!
 * \brief Parse the record-route parameter, to get dialog information back
 * \param p start of parameter string
 * \param end end of parameter string
 * \param h_entry found dialog hash entry
 * \param h_id found dialog hash id
 * \return 0 on success, -1 on failure
 */
static inline int parse_dlg_rr_param(char *p, char *end, int *h_entry, int *h_id)
{
	char *s;

	for ( s=p ; p<end && *p!=DLG_SEPARATOR ; p++ );
	if (*p!=DLG_SEPARATOR) {
		LM_ERR("malformed rr param '%.*s'\n", (int)(long)(end-s), s);
		return -1;
	}

	if ( reverse_hex2int( s, p-s, (unsigned int*)h_entry)<0 ) {
		LM_ERR("invalid hash entry '%.*s'\n", (int)(long)(p-s), s);
		return -1;
	}

	if ( reverse_hex2int( p+1, end-(p+1), (unsigned int*)h_id)<0 ) {
		LM_ERR("invalid hash id '%.*s'\n", (int)(long)(end-(p+1)), p+1 );
		return -1;
	}

	return 0;
}


/*!
 * \brief Update the saved CSEQ information in dialog from SIP message
 * \param dlg updated dialog
 * \param req SIP request
 * \param dir direction of request, must DLG_DIR_UPSTREAM or DLG_DIR_DOWNSTREAM
 * \return 0 on success, -1 on failure
 */
static inline int update_cseqs(struct dlg_cell *dlg, struct sip_msg *req,
		unsigned int dir)
{
	if ( (!req->cseq && parse_headers(req,HDR_CSEQ_F,0)<0) || !req->cseq ||
	!req->cseq->parsed) {
		LM_ERR("bad sip message or missing CSeq hdr :-/\n");
		return -1;
	}

	if ( dir==DLG_DIR_UPSTREAM) {
		return dlg_update_cseq(dlg, DLG_CALLEE_LEG,&((get_cseq(req))->number));
	} else if ( dir==DLG_DIR_DOWNSTREAM) {
		return dlg_update_cseq(dlg, DLG_CALLER_LEG,&((get_cseq(req))->number));
	} else {
		LM_CRIT("dir is not set!\n");
		return -1;
	}
}

/*!
 * \brief Unreference a dialog from tm callback (another wrapper)
 * \param t transaction, unused
 * \param type type of the entered callback
 * \param param saved dialog structure in the callback
 */
static void unref_dlg_from_cb(struct cell* t, int type, struct tmcb_params *param)
{
	struct dlg_cell *dlg = (struct dlg_cell *)(*param->param);

	if (!dlg)
		return;

	/* destroy dialog */
	unreference_dialog(dlg);
}


/*!
 * \brief Function that is registered as RR callback for dialog tracking
 * 
 * Function that is registered as RR callback for dialog tracking. It
 * sets the appropriate events after the SIP method and run the state
 * machine to update the dialog state. It updates then the saved
 * dialogs and also the statistics.
 * \param req SIP request
 * \param route_params record-route parameter
 * \param param unused
 */
void dlg_onroute(struct sip_msg* req, str *route_params, void *param)
{
	struct dlg_cell *dlg;
	str val, callid, ftag, ttag;
	int h_entry, h_id, new_state, old_state, unref, event, timeout;
	unsigned int dir;
	unsigned int del;
	int ret = 0;

	if (current_dlg_pointer!=NULL)
		return;

	/* skip initial requests - they may end up here because of the
	 * preloaded route */
	if ( (!req->to && parse_headers(req, HDR_TO_F,0)<0) || !req->to ) {
		LM_ERR("bad request or missing TO hdr :-/\n");
		return;
	}
	if ( get_to(req)->tag_value.len==0 )
		return;

	dlg = 0;
	dir = DLG_DIR_NONE;

	if ( seq_match_mode!=SEQ_MATCH_NO_ID ) {
		if( d_rrb.get_route_param( req, &rr_param, &val)!=0) {
			LM_DBG("Route param '%.*s' not found\n", rr_param.len,rr_param.s);
			if (seq_match_mode==SEQ_MATCH_STRICT_ID )
				return;
		} else {
			LM_DBG("route param is '%.*s' (len=%d)\n",val.len,val.s,val.len);

			if ( parse_dlg_rr_param( val.s, val.s+val.len, &h_entry, &h_id)<0 )
				return;

			dlg = lookup_dlg( h_entry, h_id, &del);
			if (del == 1) {
				LM_DBG("dialog marked for deletion, ignoring\n");
				return;
			}
			if (dlg==0) {
				LM_WARN("unable to find dialog for %.*s "
					"with route param '%.*s' [%u:%u]\n",
					req->first_line.u.request.method.len,
					req->first_line.u.request.method.s,
					val.len,val.s, h_entry, h_id);
				if (seq_match_mode==SEQ_MATCH_STRICT_ID )
					return;
			} else {
				// lookup_dlg has incremented the ref count by 1
				if (pre_match_parse( req, &callid, &ftag, &ttag, 1)<0) {
					unref_dlg(dlg, 1);
					return;
				}
				if (match_dialog( dlg, &callid, &ftag, &ttag, &dir )==0) {
					LM_WARN("tight matching failed for %.*s with callid='%.*s'/%d, "
							"ftag='%.*s'/%d, ttag='%.*s'/%d and direction=%d\n",
							req->first_line.u.request.method.len,
							req->first_line.u.request.method.s,
							callid.len, callid.s, callid.len,
							ftag.len, ftag.s, ftag.len,
							ttag.len, ttag.s, ttag.len, dir);
					LM_WARN("dialog identification elements are callid='%.*s'/%d, "
							"caller tag='%.*s'/%d, callee tag='%.*s'/%d\n",
							dlg->callid.len, dlg->callid.s, dlg->callid.len,
							dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
							dlg->tag[DLG_CALLER_LEG].len,
							dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s,
							dlg->tag[DLG_CALLEE_LEG].len);
					unref_dlg(dlg, 1);

					// Reset variables in order to do a lookup based on SIP-Elements.
					dlg = 0;
					dir = DLG_DIR_NONE;

					if (seq_match_mode==SEQ_MATCH_STRICT_ID )
						return;
				}
			}
		}
	}

	if (dlg==0) {
		if (pre_match_parse( req, &callid, &ftag, &ttag, 1)<0)
			return;
		/* TODO - try to use the RR dir detection to speed up here the
		 * search -bogdan */
		dlg = get_dlg(&callid, &ftag, &ttag, &dir, &del);
		if (del == 1) {
			LM_DBG("dialog marked for deletion, ignoring\n");
			return;
		}
		if (!dlg){
			LM_DBG("Callid '%.*s' not found\n",
				req->callid->body.len, req->callid->body.s);
			return;
		}
	}

	/* set current dialog - it will keep a ref! */
	set_current_dialog( req, dlg);
	_dlg_ctx.dlg = dlg;

	if ( d_tmb.register_tmcb( req, NULL, TMCB_REQUEST_FWDED,
				store_dlg_in_tm_cb, (void*)dlg, NULL)<0 ) {
		LM_ERR("failed to store dialog in transaction during dialog creation for later reference\n");
	}

	if (del == 1) {
		LM_DBG( "Use the dialog (callid: '%.*s') without further handling because it is marked for deletion\n",
				callid.len, callid.s);
		return;
	}

	/* run state machine */
	switch ( req->first_line.u.request.method_value ) {
		case METHOD_PRACK:
			event = DLG_EVENT_REQPRACK; break;
		case METHOD_ACK:
			event = DLG_EVENT_REQACK; break;
		case METHOD_BYE:
			event = DLG_EVENT_REQBYE; break;
		default:
			event = DLG_EVENT_REQ;
	}

	next_state_dlg( dlg, event, &old_state, &new_state, &unref);

	CURR_DLG_ID = req->id;
	CURR_DLG_LIFETIME = (unsigned int)(time(0))-dlg->start_ts;
	CURR_DLG_STATUS = new_state;

	/* delay deletion of dialog until transaction has died off in order
	 * to absorb in-air messages */
	if (new_state==DLG_STATE_DELETED && old_state!=DLG_STATE_DELETED) {
		if ( d_tmb.register_tmcb(req, NULL, TMCB_DESTROY,
					unref_dlg_from_cb, (void*)dlg, NULL)<0 ) {
			LM_ERR("failed to register deletion delay function\n");
		} else {
			ref_dlg(dlg, 1);
		}
	}

	/* run actions for the transition */
	if (event==DLG_EVENT_REQBYE && new_state==DLG_STATE_DELETED &&
	old_state!=DLG_STATE_DELETED) {
		LM_DBG("BYE successfully processed\n");
		/* remove from timer */
		ret = remove_dialog_timer(&dlg->tl);
		if (ret < 0) {
			LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",
				dlg, dlg->h_entry, dlg->h_id,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
		} else if (ret > 0) {
			LM_WARN("inconsitent dlg timer data on dlg %p [%u:%u] "
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",
				dlg, dlg->h_entry, dlg->h_id,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
		} else {
			unref++;
		}
		/* dialog terminated (BYE) */
		unref_dlg(dlg, unref);

		if_update_stat( dlg_enable_stats, active_dlgs, -1);
		return;
	}

	if ( (event==DLG_EVENT_REQ || event==DLG_EVENT_REQACK)
	&& new_state==DLG_STATE_CONFIRMED) {
		LM_DBG("sequential request successfully processed\n");
		timeout = get_dlg_timeout(req);
		if (timeout!=default_timeout) {
			dlg->lifetime = timeout;
		}
		if (update_dlg_timer( &dlg->tl, dlg->lifetime )==-1) {
			LM_ERR("failed to update dialog lifetime\n");
		}
		if (update_cseqs(dlg, req, dir)!=0) {
			LM_ERR("cseqs update failed\n");
		} else {
			dlg->dflags |= DLG_FLAG_CHANGED;
			if ( dlg_db_mode==DB_MODE_REALTIME )
				update_dialog_dbinfo(dlg);
		}

		/* within dialog request */
		run_dlg_callbacks( DLGCB_REQ_WITHIN, dlg, req, dir, 0);

		if ( (event!=DLG_EVENT_REQACK) &&
		(dlg->cbs.types)&DLGCB_RESPONSE_WITHIN ) {
			/* ref the dialog as registered into the transaction callback.
			 * unref will be done when the callback will be destroyed */
			ref_dlg( dlg , 1);
			/* register callback for the replies of this request */
			if ( d_tmb.register_tmcb( req, 0, TMCB_RESPONSE_FWDED,
			(dir==DLG_DIR_UPSTREAM)?dlg_seq_down_onreply:dlg_seq_up_onreply,
			(void*)dlg, unreference_dialog)<0 ) {
				LM_ERR("failed to register TMCB (2)\n");
					unref_dlg( dlg , 1);
			}
		}
	}

	if(new_state==DLG_STATE_CONFIRMED && old_state==DLG_STATE_CONFIRMED_NA){
		dlg->dflags |= DLG_FLAG_CHANGED;
		if(dlg_db_mode == DB_MODE_REALTIME)
			update_dialog_dbinfo(dlg);
	}

	return;
}


/*!
 * \brief Timer function that removes expired dialogs, run timeout route
 * \param tl dialog timer list
 */
void dlg_ontimeout( struct dlg_tl *tl)
{
	struct dlg_cell *dlg;
	int new_state, old_state, unref;
	struct sip_msg *fmsg;

	/* get the dialog tl payload */
	dlg = ((struct dlg_cell*)((char *)(tl) -
		(unsigned long)(&((struct dlg_cell*)0)->tl)));

	if(dlg->toroute>0 && dlg->toroute<main_rt.entries
			&& main_rt.rlist[dlg->toroute]!=NULL)
	{
		fmsg = faked_msg_next();
		if (exec_pre_script_cb(fmsg, REQUEST_CB_TYPE)>0)
		{
			dlg_set_ctx_dialog(dlg);
			LM_DBG("executing route %d on timeout\n", dlg->toroute);
			set_route_type(REQUEST_ROUTE);
			run_top_route(main_rt.rlist[dlg->toroute], fmsg, 0);
			dlg_set_ctx_dialog(0);
			exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
		}
	}

	if ((dlg->dflags&DLG_FLAG_TOBYE)
			&& (dlg->state==DLG_STATE_CONFIRMED_NA
				|| dlg->state==DLG_STATE_CONFIRMED))
	{
		dlg_bye_all(dlg, NULL);
		unref_dlg(dlg, 1);
		if_update_stat(dlg_enable_stats, expired_dlgs, 1);
		return;
	}

	next_state_dlg( dlg, DLG_EVENT_REQBYE, &old_state, &new_state, &unref);

	if (new_state==DLG_STATE_DELETED && old_state!=DLG_STATE_DELETED) {
		LM_WARN("timeout for dlg with CallID '%.*s' and tags '%.*s' '%.*s'\n",
			dlg->callid.len, dlg->callid.s,
			dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
			dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);

		/* dialog timeout */
		run_dlg_callbacks( DLGCB_EXPIRED, dlg, NULL, DLG_DIR_NONE, 0);

		unref_dlg(dlg, unref+1);

		if_update_stat( dlg_enable_stats, expired_dlgs, 1);
		if_update_stat( dlg_enable_stats, active_dlgs, -1);
	} else {
		unref_dlg(dlg, 1);
	}

	return;
}


/*!
 * \brief Function that returns the dialog lifetime as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_lifetime(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if (CURR_DLG_ID!=msg->id)
		return pv_get_null( msg, param, res);

	res->ri = CURR_DLG_LIFETIME;
	ch = int2str( (unsigned long)res->ri, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;

	return 0;
}


/*!
 * \brief Function that returns the dialog state as pseudo-variable
 * \param msg SIP message
 * \param param pseudo-variable parameter
 * \param res pseudo-variable result
 * \return 0 on success, -1 on failure
 */
int pv_get_dlg_status(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;

	if (CURR_DLG_ID!=msg->id)
		return pv_get_null( msg, param, res);

	res->ri = CURR_DLG_STATUS;
	ch = int2str( (unsigned long)res->ri, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;

	return 0;
}
