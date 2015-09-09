/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
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
#include "../../pt.h"
#include "../../lib/kcore/faked_msg.h"
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
static int       seq_match_mode;	/*!< dlg_match mode */ 
static int       shutdown_done = 0;	/*!< 1 when destroy_dlg_handlers was called */
extern int       detect_spirals;
extern int       dlg_timeout_noreset;
extern int       initial_cbs_inscript;
extern int       dlg_send_bye;
extern int       dlg_event_rt[DLG_EVENTRT_MAX];
extern int       dlg_wait_ack;
int              spiral_detected = -1;

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

int dlg_set_tm_callbacks(tm_cell_t *t, sip_msg_t *req, dlg_cell_t *dlg,
		int mode);
int dlg_set_tm_waitack(tm_cell_t *t, dlg_cell_t *dlg);

/*!
 * \brief Initialize the dialog handlers
 * \param rr_param_p added record-route parameter
 * \param dlg_flag_p dialog flag
 * \param timeout_avp_p AVP for timeout setting
 * \param default_timeout_p default timeout
 * \param seq_match_mode_p matching mode
 */
void init_dlg_handlers(char *rr_param_p, int dlg_flag_p,
		pv_spec_t *timeout_avp_p ,int default_timeout_p,
		int seq_match_mode_p)
{
	rr_param.s = rr_param_p;
	rr_param.len = strlen(rr_param.s);

	dlg_flag = 1<<dlg_flag_p;

	timeout_avp = timeout_avp_p;
	default_timeout = default_timeout_p;
	seq_match_mode = seq_match_mode_p;
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
 * \param t transaction
 * \param leg type of the call leg
 * \param tag SIP To tag
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
		cseq = dlg->cseq[DLG_CALLEE_LEG];
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
 * \brief Clone dialog internal unique id to shared memory
 */
dlg_iuid_t *dlg_get_iuid_shm_clone(dlg_cell_t *dlg)
{
	dlg_iuid_t *iuid = NULL;

	if(dlg==NULL)
		return NULL;

	iuid = (dlg_iuid_t*)shm_malloc(sizeof(dlg_iuid_t));
	if(iuid==NULL)
	{
		LM_ERR("failed to clone dialog iuid\n");
		return NULL;
	}

	memset(iuid, 0, sizeof(dlg_iuid_t));
	iuid->h_entry = dlg->h_entry;
	iuid->h_id = dlg->h_id;

	return iuid;
}


/*!
 * \brief Free dialog internal unique id stored in shared memory
 */
void dlg_iuid_sfree(void *iuid)
{
    if(iuid) {
		LM_DBG("freeing dlg iuid [%u:%u] (%p)\n",
				((dlg_iuid_t*)iuid)->h_entry,
				((dlg_iuid_t*)iuid)->h_id, iuid);
		shm_free(iuid);
	}
}


/*!
 * \brief Function that executes BYE reply callbacks
 * \param t transaction, unused
 * \param type type of the callback, should be TMCB_RESPONSE_FWDED
 * \param params saved dialog structure inside the callback
 */
static void dlg_terminated_confirmed(tm_cell_t *t, int type,
                                     struct tmcb_params* params)
{
    dlg_cell_t *dlg = NULL;
	dlg_iuid_t *iuid = NULL;

    if(!params || !params->req || !params->param)
    {
        LM_ERR("invalid parameters!\n");
        return;
    }

	iuid = (dlg_iuid_t*)*params->param;
	if(iuid==NULL)
		return;

    dlg = dlg_get_by_iuid(iuid);

    if(dlg==NULL)
    {
        LM_ERR("failed to get dialog from params!\n");
        return;
    }
    /* dialog termination confirmed (BYE reply) */
    run_dlg_callbacks(DLGCB_TERMINATED_CONFIRMED,
                      dlg,
                      params->req,
                      params->rpl,
                      DLG_DIR_UPSTREAM,
                      0);
	dlg_release(dlg);
}

/*!
 * \brief Execute callback for the BYE request and register callback for the BYE reply
 * \param req request message
 * \param dlg corresponding dialog
 * \param dir message direction
 */
static void dlg_terminated(sip_msg_t *req, dlg_cell_t *dlg, unsigned int dir)
{
	dlg_iuid_t *iuid = NULL;

    if(!req) {
        LM_ERR("request is empty!");
        return;
    }

    if(!dlg) {
        LM_ERR("dialog is empty!");
        return;
    }

    /* dialog terminated (BYE) */
    run_dlg_callbacks(DLGCB_TERMINATED, dlg, req, NULL, dir, 0);

	iuid = dlg_get_iuid_shm_clone(dlg);
	if(iuid==NULL)
		return;

    /* register callback for the coresponding reply */
    if (d_tmb.register_tmcb(req,
                            0,
                            TMCB_RESPONSE_OUT,
                            dlg_terminated_confirmed,
                            (void*)iuid,
                            dlg_iuid_sfree) <= 0 ) {
        LM_ERR("cannot register response callback for BYE request\n");
        return;
    }
}

/*!
 * \brief Function that is registered as TM callback and called on T destroy
 *
 * - happens when wait_ack==1
 *
 */
static void dlg_ontdestroy(struct cell* t, int type, struct tmcb_params *param)
{
	dlg_cell_t *dlg = NULL;
	dlg_iuid_t *iuid = NULL;

	iuid = (dlg_iuid_t*)(*param->param);
	dlg = dlg_get_by_iuid(iuid);
	if(dlg==0)
		return;
	/* 1 for callback and 1 for dlg lookup */
	dlg_unref(dlg, 2);
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
    dlg_cell_t *dlg = NULL;
	dlg_iuid_t *iuid = NULL;
    int new_state, old_state, unref, event;
    str tag;
    sip_msg_t *req = param->req;
	sip_msg_t *rpl = param->rpl;

	if (shutdown_done)
		return;
	iuid = (dlg_iuid_t*)(*param->param);
	dlg = dlg_get_by_iuid(iuid);
	if(dlg==0)
		return;

	unref = 0;
	if (type & (TMCB_RESPONSE_IN|TMCB_ON_FAILURE)) {
		/* Set the dialog context so it is available in onreply_route and failure_route*/
		set_current_dialog(req, dlg);
		dlg_set_ctx_iuid(dlg);
		goto done;
	}

	if (type==TMCB_RESPONSE_FWDED) {
		/* The state does not change, but the msg is mutable in this callback*/
		run_dlg_callbacks(DLGCB_RESPONSE_FWDED, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
		goto done;
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
	dlg_run_event_route(dlg, (rpl==FAKED_REPLY)?NULL:rpl, old_state, new_state);

	if (new_state==DLG_STATE_EARLY) {
		run_dlg_callbacks(DLGCB_EARLY, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
		if (old_state!=DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, 1);
		goto done;
	}

	if (new_state==DLG_STATE_CONFIRMED_NA &&
	old_state!=DLG_STATE_CONFIRMED_NA && old_state!=DLG_STATE_CONFIRMED ) {
		LM_DBG("dialog %p confirmed (ACK pending)\n",dlg);

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
			/* dialog pointer inserted in timer list */
			dlg_ref(dlg, 1);
		}

		/* dialog confirmed (ACK pending) */
		run_dlg_callbacks( DLGCB_CONFIRMED_NA, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);

		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);

		if (unref) dlg_unref(dlg, unref);
		if_update_stat(dlg_enable_stats, active_dlgs, 1);
		goto done;
	}

	if ( new_state==DLG_STATE_DELETED
				&& (old_state==DLG_STATE_UNCONFIRMED
					|| old_state==DLG_STATE_EARLY) ) {
		LM_DBG("dialog %p failed (negative reply)\n", dlg);
		/* dialog setup not completed (3456XX) */
		run_dlg_callbacks( DLGCB_FAILED, dlg, req, rpl, DLG_DIR_UPSTREAM, 0);
		if(dlg_wait_ack==1)
			dlg_set_tm_waitack(t, dlg);
		/* do unref */
		if (unref)
			dlg_unref(dlg, unref);
		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);

		if_update_stat(dlg_enable_stats, failed_dlgs, 1);

		goto done;
	}

	if (unref) dlg_unref(dlg, unref);

done:
	/* unref due to dlg_get_by_iuid() */
	dlg_release(dlg);
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
	dlg_cell_t *dlg = NULL;
	dlg_iuid_t *iuid = NULL;

	if (shutdown_done)
		return;
	iuid = (dlg_iuid_t*)(*param->param);
	dlg = dlg_get_by_iuid(iuid);
	if (dlg==0)
		return;

	if (type==TMCB_RESPONSE_FWDED)
	{
		run_dlg_callbacks( DLGCB_RESPONSE_WITHIN,
		                   dlg,
		                   param->req,
		                   param->rpl,
		                   direction,
		                   0);
	}
	dlg_release(dlg);

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
	sip_msg_t *req = param->req;
	dlg_cell_t *dlg = NULL;

	if(req->first_line.u.request.method_value == METHOD_BYE) {
		_dlg_ctx.t = 1;
		return;
	}

	if(req->first_line.u.request.method_value != METHOD_INVITE)
		return;

	dlg = dlg_get_ctx_dialog();

	if (dlg!=NULL) {
		if (!initial_cbs_inscript) {
			if (spiral_detected == 1)
				run_dlg_callbacks( DLGCB_SPIRALED, dlg,
						req, NULL, DLG_DIR_DOWNSTREAM, 0);
			else if (spiral_detected == 0)
				run_create_callbacks(dlg, req);
		}
	}
	if (dlg==NULL) {
		if((req->flags&dlg_flag)!=dlg_flag)
			return;
		LM_DBG("dialog creation on config flag\n");
		dlg_new_dialog(req, t, 1);
		dlg = dlg_get_ctx_dialog();
	}
	if (dlg!=NULL) {
		LM_DBG("dialog added to tm callbacks\n");
		dlg_set_tm_callbacks(t, req, dlg, spiral_detected);
		_dlg_ctx.t = 1;
		dlg_release(dlg);
	}
}


/*!
 * \brief Unreference a new dialog, helper function for dlg_onreq
 * \see dlg_onreq
 * \param dialog unreferenced dialog
 */
#if 0
static void unref_new_dialog(void *iuid)
{
	struct tmcb_params p;

	memset(&p, 0, sizeof(struct tmcb_params));
	p.param = (void*)&iuid;
	dlg_onreply(0, TMCB_DESTROY, &p);
}
#endif


/*!
 * \brief Create a new dialog from a sip message
 *
 * Create a new dialog from a SIP message, register a callback
 * to keep track of the dialog with help of the tm module.
 * This function is either called from the request callback, or
 * from the dlg_manage function in the configuration script.
 * \see dlg_onreq
 * \see w_dlg_manage
 * \param req SIP message
 * \param t transaction
 * \param run_initial_cbs if set zero, initial callbacks are not executed
 * \return 0 on success, -1 on failure
 */ 
int dlg_new_dialog(sip_msg_t *req, struct cell *t, const int run_initial_cbs)
{
	dlg_cell_t *dlg;
	str s;
	str callid;
    str ftag;
    str ttag;
    str req_uri;
    unsigned int dir;
    int mlock;

	dlg = dlg_get_ctx_dialog();
    if(dlg != NULL) {
		dlg_release(dlg);
        return -1;
	}

	if(req->first_line.u.request.method_value != METHOD_INVITE)
		return -1;

    if(pre_match_parse( req, &callid, &ftag, &ttag, 0)<0) {
        LM_WARN("pre-matching failed\n");
        return -1;
    }

    if(ttag.s!=0 && ttag.len!=0)
        return -1;

    if(pv_printf_s(req, ruri_param_model, &req_uri)<0) {
        LM_ERR("error - cannot print the r-uri format\n");
        return -1;
    }
    trim(&req_uri);

	dir = DLG_DIR_NONE;
	mlock = 1;
	/* search dialog by SIP attributes
	 * - if not found, hash table slot is left locked, to avoid races
	 *   to add 'same' dialog on parallel forking or not-handled-yet
	 *   retransmissions. Release slot after linking new dialog */
	dlg = search_dlg(&callid, &ftag, &ttag, &dir);
	if(dlg) {
		mlock = 0;
		if (detect_spirals) {
			if (spiral_detected == 1)
				return 0;

			if ( dlg->state != DLG_STATE_DELETED ) {
				LM_DBG("Callid '%.*s' found, must be a spiraled request\n",
					callid.len, callid.s);
				spiral_detected = 1;

				if (run_initial_cbs)
					run_dlg_callbacks( DLGCB_SPIRALED, dlg, req, NULL,
							DLG_DIR_DOWNSTREAM, 0);
				/* set ctx dlg id shortcuts */
				_dlg_ctx.iuid.h_entry = dlg->h_entry;
				_dlg_ctx.iuid.h_id = dlg->h_id;
				/* search_dlg() has incremented the ref count by 1 */
				dlg_release(dlg);
				return 0;
			}
			dlg_release(dlg);
		}
		/* lock the slot - dlg found, but in dlg_state_deleted, do a new one */
		dlg_hash_lock(&callid);
    }
    spiral_detected = 0;

    dlg = build_new_dlg (&callid /*callid*/,
                         &(get_from(req)->uri) /*from uri*/,
                         &(get_to(req)->uri) /*to uri*/,
                         &ftag/*from_tag*/,
                         &req_uri /*r-uri*/ );

	if (dlg==0) {
		if(likely(mlock==1)) dlg_hash_release(&callid);
		LM_ERR("failed to create new dialog\n");
		return -1;
	}

	/* save caller's tag, cseq, contact and record route*/
	if (populate_leg_info(dlg, req, t, DLG_CALLER_LEG,
			&(get_from(req)->tag_value)) !=0) {
		if(likely(mlock==1)) dlg_hash_release(&callid);
		LM_ERR("could not add further info to the dialog\n");
		shm_free(dlg);
		return -1;
	}

	/* Populate initial varlist: */
	dlg->vars = get_local_varlist_pointer(req, 1);

	/* if search_dlg() returned NULL, slot was kept locked */
	link_dlg(dlg, 0, mlock);
	if(likely(mlock==1)) dlg_hash_release(&callid);

	dlg->lifetime = get_dlg_timeout(req);
	s.s   = _dlg_ctx.to_route_name;
	s.len = strlen(s.s);
	dlg_set_toroute(dlg, &s);
	dlg->sflags |= _dlg_ctx.flags;
	dlg->iflags |= _dlg_ctx.iflags;

	if (dlg_send_bye!=0 || _dlg_ctx.to_bye!=0)
		dlg->iflags |= DLG_IFLAG_TIMEOUTBYE;

    if (run_initial_cbs)  run_create_callbacks( dlg, req);

	/* first INVITE seen (dialog created, unconfirmed) */
	if ( seq_match_mode!=SEQ_MATCH_NO_ID &&
			add_dlg_rr_param( req, dlg->h_entry, dlg->h_id)<0 ) {
		LM_ERR("failed to add RR param\n");
		goto error;
	}

    if_update_stat( dlg_enable_stats, processed_dlgs, 1);

	_dlg_ctx.cpid = my_pid();
    _dlg_ctx.iuid.h_entry = dlg->h_entry;
    _dlg_ctx.iuid.h_id = dlg->h_id;
    set_current_dialog(req, dlg);

	return 0;

error:
	if (!spiral_detected)
		dlg_unref(dlg, 1);               // undo ref regarding linking
	return -1;
}


/*!
 * \brief add dlg structure to tm callbacks
 * \param t current transaction
 * \param req current sip request
 * \param dlg current dialog
 * \param smode if the sip request was spiraled
 * \return 0 on success, -1 on failure
 */
int dlg_set_tm_callbacks(tm_cell_t *t, sip_msg_t *req, dlg_cell_t *dlg,
		int smode)
{
	dlg_iuid_t *iuid = NULL;
	if(t==NULL)
		return -1;

	if(smode==0) {
		iuid = dlg_get_iuid_shm_clone(dlg);
		if(iuid==NULL)
		{
			LM_ERR("failed to create dialog unique id clone\n");
			goto error;
		}
		if ( d_tmb.register_tmcb( req, t,
				TMCB_RESPONSE_IN|TMCB_RESPONSE_READY|TMCB_RESPONSE_FWDED|TMCB_ON_FAILURE,
				dlg_onreply, (void*)iuid, dlg_iuid_sfree)<0 ) {
			LM_ERR("failed to register TMCB\n");
			goto error;
		}
	}

	dlg->dflags |= DLG_FLAG_TM;

	return 0;
error:
	dlg_iuid_sfree(iuid);
	return -1;
}

/*!
 * \brief add dlg structure to tm callbacks to wait for negative ACK
 * \param t current transaction
 * \param dlg current dialog
 * \return 0 on success, -1 on failure
 */
int dlg_set_tm_waitack(tm_cell_t *t, dlg_cell_t *dlg)
{
	dlg_iuid_t *iuid = NULL;
	if(t==NULL)
		return -1;

	LM_DBG("registering TMCB to wait for negative ACK\n");
	iuid = dlg_get_iuid_shm_clone(dlg);
	if(iuid==NULL)
	{
		LM_ERR("failed to create dialog unique id clone\n");
		goto error;
	}
	dlg_ref(dlg, 1);
	if ( d_tmb.register_tmcb( NULL, t,
			TMCB_DESTROY,
			dlg_ontdestroy, (void*)iuid, dlg_iuid_sfree)<0 ) {
		LM_ERR("failed to register TMCB to wait for negative ACK\n");
		dlg_unref(dlg, 1);
		goto error;
	}

	return 0;
error:
	dlg_iuid_sfree(iuid);
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
	dlg_cell_t *dlg = NULL;
	dlg_iuid_t *iuid = NULL;

	iuid = (dlg_iuid_t*)(*param->param);
	if (iuid==NULL)
		return;

	dlg = dlg_get_by_iuid(iuid);
	if(dlg==NULL)
		return;
	/* unref by 2: 1 set when adding in tm cb, 1 sent by dlg_get_by_iuid() */
	dlg_unref(dlg, 2);
}

/*!
 *
 */
dlg_cell_t *dlg_lookup_msg_dialog(sip_msg_t *msg, unsigned int *dir)
{
	dlg_cell_t *dlg = NULL;
	str callid;
	str ftag;
	str ttag;
	unsigned int vdir;

	/* Retrieve the current dialog */
	dlg = dlg_get_ctx_dialog();
	if(dlg!=NULL) {
		if(dir) {
			if (pre_match_parse(msg, &callid, &ftag, &ttag, 0)<0) {
				dlg_release(dlg);
				return NULL;
			}
			if (dlg->tag[DLG_CALLER_LEG].len == ftag.len &&
					   strncmp(dlg->tag[DLG_CALLER_LEG].s, ftag.s, ftag.len)==0 &&
					   strncmp(dlg->callid.s, callid.s, callid.len)==0) {
				*dir = DLG_DIR_DOWNSTREAM;
			} else {
				if (ttag.len>0 && dlg->tag[DLG_CALLER_LEG].len == ttag.len &&
						   strncmp(dlg->tag[DLG_CALLER_LEG].s, ttag.s, ttag.len)==0 &&
						   strncmp(dlg->callid.s, callid.s, callid.len)==0) {
					*dir = DLG_DIR_UPSTREAM;
				}
			}
		}
		return dlg;
	}
	
	if (pre_match_parse(msg, &callid, &ftag, &ttag, 0)<0)
		return NULL;
	vdir = DLG_DIR_NONE;
	dlg = get_dlg(&callid, &ftag, &ttag, &vdir);
	if (dlg==NULL){
		LM_DBG("dlg with callid '%.*s' not found\n",
				msg->callid->body.len, msg->callid->body.s);
		return NULL;
	}
	if(dir) *dir = vdir;
	return dlg;
}

/*!
 *
 */
dlg_cell_t *dlg_get_msg_dialog(sip_msg_t *msg)
{
	return dlg_lookup_msg_dialog(msg, NULL);
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
	dlg_cell_t *dlg;
	dlg_iuid_t *iuid;
	str val, callid, ftag, ttag;
	int h_entry, h_id, new_state, old_state, unref, event, timeout, reset;
	unsigned int dir;
	int ret = 0;

	dlg = dlg_get_ctx_dialog();
	if (dlg!=NULL) {
		dlg_release(dlg);
		return;
	}

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

			dlg = dlg_lookup(h_entry, h_id);
			if (dlg==0) {
				LM_WARN("unable to find dialog for %.*s "
					"with route param '%.*s' [%u:%u]\n",
					req->first_line.u.request.method.len,
					req->first_line.u.request.method.s,
					val.len,val.s, h_entry, h_id);
				if (seq_match_mode==SEQ_MATCH_STRICT_ID )
					return;
			} else {
				if (pre_match_parse( req, &callid, &ftag, &ttag, 1)<0) {
					// lookup_dlg has incremented the ref count by 1
					dlg_release(dlg);
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
					// lookup_dlg has incremented the ref count by 1
					dlg_release(dlg);

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
		dlg = get_dlg(&callid, &ftag, &ttag, &dir);
		if (dlg==0){
			LM_DBG("Callid '%.*s' not found\n",
				req->callid->body.len, req->callid->body.s);
			return;
		}
	}

    /* set current dialog - re-use ref increment from dlg_get() above */
    set_current_dialog( req, dlg);
    _dlg_ctx.iuid.h_entry = dlg->h_entry;
    _dlg_ctx.iuid.h_id = dlg->h_id;

	if (req->first_line.u.request.method_value != METHOD_ACK) {
		iuid = dlg_get_iuid_shm_clone(dlg);
		if(iuid!=NULL)
		{
			/* register callback for the replies of this request */
			if ( d_tmb.register_tmcb( req, 0, TMCB_RESPONSE_IN|TMCB_ON_FAILURE,
					dlg_onreply, (void*)iuid, dlg_iuid_sfree)<0 ) {
				LM_ERR("failed to register TMCB (3)\n");
				shm_free(iuid);
			}
			iuid = NULL;
		}
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

	dlg_run_event_route(dlg, req, old_state, new_state);

	/* delay deletion of dialog until transaction has died off in order
	 * to absorb in-air messages */
	if (new_state==DLG_STATE_DELETED && old_state!=DLG_STATE_DELETED) {
		iuid = dlg_get_iuid_shm_clone(dlg);
		if(iuid!=NULL) {
			if ( d_tmb.register_tmcb(req, NULL, TMCB_DESTROY,
					unref_dlg_from_cb, (void*)iuid, dlg_iuid_sfree)<0 ) {
				LM_ERR("failed to register deletion delay function\n");
				shm_free(iuid);
			} else {
				dlg_ref(dlg, 1);
			}
		}
	}

	if (new_state==DLG_STATE_CONFIRMED && old_state!=DLG_STATE_CONFIRMED)
		dlg_ka_add(dlg);

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
			/* one extra unref due to removal from timer list */
			unref++;
		}
		/* dialog terminated (BYE) */
        dlg_terminated( req, dlg, dir);

		dlg_unref(dlg, unref);

		_dlg_ctx.cpid = my_pid();
		_dlg_ctx.expect_t = 1;
		dlg_set_ctx_iuid(dlg);

		if_update_stat( dlg_enable_stats, active_dlgs, -1);
		goto done;
	}

	if ( (event==DLG_EVENT_REQ || event==DLG_EVENT_REQACK)
	&& (new_state==DLG_STATE_CONFIRMED || new_state==DLG_STATE_EARLY)) {

		timeout = get_dlg_timeout(req);
		if (timeout!=default_timeout) {
			dlg->lifetime = timeout;
		}
		reset = !((dlg->iflags & DLG_IFLAG_TIMER_NORESET) || dlg_timeout_noreset);

		if ((new_state!=DLG_STATE_EARLY) && (old_state!=DLG_STATE_CONFIRMED || reset)) {
			if (update_dlg_timer( &dlg->tl, dlg->lifetime )==-1) {
				LM_ERR("failed to update dialog lifetime\n");
			} else {
				dlg->dflags |= DLG_FLAG_CHANGED;
			}
		}
		if(event != DLG_EVENT_REQACK) {
			if(update_cseqs(dlg, req, dir)!=0) {
				LM_ERR("cseqs update failed\n");
			} else {
				dlg->dflags |= DLG_FLAG_CHANGED;
			}
		}
		if(dlg_db_mode==DB_MODE_REALTIME && (dlg->dflags&DLG_FLAG_CHANGED)) {
			update_dialog_dbinfo(dlg);
		}

		if (old_state==DLG_STATE_CONFIRMED_NA) {
			LM_DBG("confirming ACK successfully processed\n");

			/* confirming ACK request */
			run_dlg_callbacks( DLGCB_CONFIRMED, dlg, req, NULL, dir, 0);
		} else {
			LM_DBG("sequential request successfully processed\n");

			/* within dialog request */
			run_dlg_callbacks( DLGCB_REQ_WITHIN, dlg, req, NULL, dir, 0);

			if ( (event!=DLG_EVENT_REQACK) &&
					(dlg->cbs.types)&DLGCB_RESPONSE_WITHIN ) {
				iuid = dlg_get_iuid_shm_clone(dlg);
				if(iuid!=NULL)
				{
					/* register callback for the replies of this request */
					if ( d_tmb.register_tmcb( req, 0, TMCB_RESPONSE_FWDED,
							(dir==DLG_DIR_UPSTREAM)?dlg_seq_down_onreply:
														dlg_seq_up_onreply,
							(void*)iuid, dlg_iuid_sfree)<0 ) {
						LM_ERR("failed to register TMCB (2)\n");
						shm_free(iuid);
					}
				}
			}
		}
	}

	if(new_state==DLG_STATE_CONFIRMED && old_state==DLG_STATE_CONFIRMED_NA){
		dlg->dflags |= DLG_FLAG_CHANGED;
		if(dlg_db_mode == DB_MODE_REALTIME)
			update_dialog_dbinfo(dlg);
	}

done:
	dlg_release(dlg);
	return;
}


/*!
 * \brief Timer function that removes expired dialogs, run timeout route
 * \param tl dialog timer list
 */
void dlg_ontimeout(struct dlg_tl *tl)
{
	dlg_cell_t *dlg;
	int new_state, old_state, unref;
	sip_msg_t *fmsg;
	void* timeout_cb = 0;

	/* get the dialog tl payload */
	dlg = ((struct dlg_cell*)((char *)(tl) -
			(unsigned long)(&((struct dlg_cell*)0)->tl)));

	/* mark dialog as expired */
	dlg->dflags |= DLG_FLAG_EXPIRED;

	if(dlg->state==DLG_STATE_CONFIRMED_NA
				|| dlg->state==DLG_STATE_CONFIRMED)
	{
		if(dlg->toroute>0 && dlg->toroute<main_rt.entries
			&& main_rt.rlist[dlg->toroute]!=NULL)
		{
			fmsg = faked_msg_next();
			if (exec_pre_script_cb(fmsg, REQUEST_CB_TYPE)>0)
			{
				dlg_ref(dlg, 1);
				dlg_set_ctx_iuid(dlg);
				LM_DBG("executing route %d on timeout\n", dlg->toroute);
				set_route_type(REQUEST_ROUTE);
				run_top_route(main_rt.rlist[dlg->toroute], fmsg, 0);
				dlg_reset_ctx_iuid();
				exec_post_script_cb(fmsg, REQUEST_CB_TYPE);
				dlg_unref(dlg, 1);
			}
		}

		if(dlg->iflags&DLG_IFLAG_TIMEOUTBYE)
		{
			/* set the dialog context so that it's available in
			 * tm:local-request event route */
			dlg_set_ctx_iuid(dlg);
			if(dlg_bye_all(dlg, NULL)<0)
				dlg_unref(dlg, 1);
			dlg_reset_ctx_iuid();	

			/* run event route for end of dlg */
			dlg_run_event_route(dlg, NULL, dlg->state, DLG_STATE_DELETED);

			dlg_unref(dlg, 1);
			if_update_stat(dlg_enable_stats, expired_dlgs, 1);
			return;
		}
	}

	next_state_dlg( dlg, DLG_EVENT_REQBYE, &old_state, &new_state, &unref);
    /* used for computing duration for timed out acknowledged dialog */
	if (DLG_STATE_CONFIRMED == old_state) {
		timeout_cb = (void *)CONFIRMED_DIALOG_STATE;
	}	

	dlg_run_event_route(dlg, NULL, old_state, new_state);

	if (new_state==DLG_STATE_DELETED && old_state!=DLG_STATE_DELETED) {
		LM_WARN("timeout for dlg with CallID '%.*s' and tags '%.*s' '%.*s'\n",
			dlg->callid.len, dlg->callid.s,
			dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
			dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);

		/* dialog timeout */
		run_dlg_callbacks( DLGCB_EXPIRED, dlg, NULL, NULL, DLG_DIR_NONE, timeout_cb);

		dlg_unref(dlg, unref+1);

		if_update_stat( dlg_enable_stats, expired_dlgs, 1);
		if_update_stat( dlg_enable_stats, active_dlgs, -1);
	} else {
		dlg_unref(dlg, 1);
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

/*!
 * \brief Execute event routes based on new state
 *
 */
void dlg_run_event_route(dlg_cell_t *dlg, sip_msg_t *msg, int ostate, int nstate)
{
	sip_msg_t *fmsg;
	int rt;
	int bkroute;

	if(dlg==NULL)
		return;
	if(ostate==nstate)
		return;

	rt = -1;
	if(nstate==DLG_STATE_CONFIRMED_NA) {
		rt = dlg_event_rt[DLG_EVENTRT_START];
	} else if(nstate==DLG_STATE_DELETED) {
		if(ostate==DLG_STATE_CONFIRMED || ostate==DLG_STATE_CONFIRMED_NA)
			rt = dlg_event_rt[DLG_EVENTRT_END];
		else if(ostate==DLG_STATE_UNCONFIRMED || ostate==DLG_STATE_EARLY)
			rt = dlg_event_rt[DLG_EVENTRT_FAILED];
	}

	if(rt==-1 || event_rt.rlist[rt]==NULL)
		return;

	if(msg==NULL)
		fmsg = faked_msg_next();
	else
		fmsg = msg;

	if (exec_pre_script_cb(fmsg, LOCAL_CB_TYPE)>0)
	{
		dlg_ref(dlg, 1);
		dlg_set_ctx_iuid(dlg);
		LM_DBG("executing event_route %d on state %d\n", rt, nstate);
		bkroute = get_route_type();
		set_route_type(LOCAL_ROUTE);
		run_top_route(event_rt.rlist[rt], fmsg, 0);
		dlg_reset_ctx_iuid();
		exec_post_script_cb(fmsg, LOCAL_CB_TYPE);
		dlg_unref(dlg, 1);
		set_route_type(bkroute);
	}
}

int dlg_manage(sip_msg_t *msg)
{
	str tag;
	int backup_mode;
	dlg_cell_t *dlg = NULL;
	tm_cell_t *t = NULL;

	if( (msg->to==NULL && parse_headers(msg, HDR_TO_F,0)<0) || msg->to==NULL )
	{
		LM_ERR("bad TO header\n");
		return -1;
	}
	tag = get_to(msg)->tag_value;
	if(tag.s!=0 && tag.len!=0)
	{
		backup_mode = seq_match_mode;
		seq_match_mode = SEQ_MATCH_NO_ID;
		dlg_onroute(msg, NULL, NULL);
		seq_match_mode = backup_mode;
	} else {
		t = d_tmb.t_gett();
		if(t==T_UNDEFINED)
			t = NULL;
		if(dlg_new_dialog(msg, t, initial_cbs_inscript)!=0)
			return -1;
		dlg = dlg_get_ctx_dialog();
		if(dlg==NULL)
			return -1;
		if(t!=NULL) {
			dlg_set_tm_callbacks(t, msg, dlg, spiral_detected);
			_dlg_ctx.t = 1;
			LM_DBG("dialog created on existing transaction\n");
		} else {
			LM_DBG("dialog created before transaction\n");
		}
		dlg_release(dlg);
	}
	return 1;
}
