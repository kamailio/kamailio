/*
 * Copyright (C) 2007 Voice System SRL
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
 * \brief Requests
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../lib/srdb1/db.h"
#include "../../core/config.h"
#include "../../core/socket_info.h"
#include "../../core/dset.h"
#include "../../modules/tm/dlg.h"
#include "../../modules/tm/tm_load.h"
#include "../../core/counters.h"
#include "../../core/parser/contact/parse_contact.h"
#include "dlg_timer.h"
#include "dlg_hash.h"
#include "dlg_handlers.h"
#include "dlg_req_within.h"
#include "dlg_db_handler.h"


#define MAX_FWD_HDR        "Max-Forwards: " MAX_FWD CRLF
#define MAX_FWD_HDR_LEN    (sizeof(MAX_FWD_HDR) - 1)

extern str dlg_extra_hdrs;
extern str dlg_lreq_callee_headers;
extern int dlg_ka_failed_limit;
extern int dlg_filter_mode;

extern int bye_early_code;
extern str bye_early_reason;

/**
 *
 */
int free_tm_dlg(dlg_t *td)
{
	if(td)
	{
		if(td->route_set)
			free_rr(&td->route_set);
		pkg_free(td);
	}
	return 0;
}



dlg_t * build_dlg_t(struct dlg_cell * cell, int dir){

	dlg_t* td = NULL;
	str cseq;
	unsigned int loc_seq;
	char nbuf[MAX_URI_SIZE];
	char dbuf[80];
	str nuri = STR_NULL;
	str duri = STR_NULL;
	size_t sz;
	char *p;

	/*remote target--- Request URI*/
	if(cell->contact[dir].s==0 || cell->contact[dir].len==0){
		LM_ERR("no contact available\n");
		goto error;
	}
	if(cell->route_set[dir].s==NULL || cell->route_set[dir].len<=0){
		/*try to restore alias parameter if no route set */
		nuri.s = nbuf;
		nuri.len = MAX_URI_SIZE;
		duri.s = dbuf;
		duri.len = 80;
		if(uri_restore_rcv_alias(&cell->contact[dir], &nuri, &duri)<0) {
			nuri.len = 0;
			duri.len = 0;
		}
	}
	if(nuri.len>0 && duri.len>0) {
		sz = sizeof(dlg_t) + (nuri.len+duri.len+2)*sizeof(char);
	} else {
		sz = sizeof(dlg_t);
	}
	td = (dlg_t*)pkg_malloc(sz);
	if(!td){
		LM_ERR("out of pkg memory\n");
		return NULL;
	}
	memset(td, 0, sz);

	/*local sequence number*/
	cseq = (dir == DLG_CALLER_LEG) ?	cell->cseq[DLG_CALLEE_LEG]:
										cell->cseq[DLG_CALLER_LEG];
	if(str2int(&cseq, &loc_seq) != 0){
		LM_ERR("invalid cseq\n");
		goto error;
	}
	/*we don not increase here the cseq as this will be done by TM*/
	td->loc_seq.value = loc_seq;
	td->loc_seq.is_set = 1;

	/*route set*/
	if( cell->route_set[dir].s && cell->route_set[dir].len){
		if( parse_rr_body(cell->route_set[dir].s, cell->route_set[dir].len,
						&td->route_set) !=0){
		 	LM_ERR("failed to parse route set\n");
			goto error;
		}
	}

	if(nuri.len>0 && duri.len>0) {
		/* req uri */
		p = (char*)td + sizeof(dlg_t);
		strncpy(p, nuri.s, nuri.len);
		p[nuri.len] = '\0';
		td->rem_target.s = p;
		td->rem_target.len = nuri.len;
		/* dst uri */
		p += nuri.len + 1;
		strncpy(p, duri.s, duri.len);
		p[duri.len] = '\0';
		td->dst_uri.s = p;
		td->dst_uri.len = duri.len;
	} else {
		td->rem_target = cell->contact[dir];
	}

	td->rem_uri	=   (dir == DLG_CALLER_LEG)?	cell->from_uri: cell->to_uri;
	td->loc_uri	=	(dir == DLG_CALLER_LEG)?	cell->to_uri: cell->from_uri;
	td->id.call_id = cell->callid;
	td->id.rem_tag = cell->tag[dir];
	td->id.loc_tag = (dir == DLG_CALLER_LEG) ? 	cell->tag[DLG_CALLEE_LEG]:
												cell->tag[DLG_CALLER_LEG];

	td->state= DLG_CONFIRMED;
	td->send_sock = cell->bind_addr[dir];

	return td;

error:
	free_tm_dlg(td);
	return NULL;
}



/* callback function to handle responses to the BYE request */
void bye_reply_cb(struct cell* t, int type, struct tmcb_params* ps){

	struct dlg_cell* dlg;
	int event, old_state, new_state, unref, ret;
	dlg_iuid_t *iuid = NULL;

	if(ps->param == NULL || *ps->param == NULL){
		LM_ERR("invalid parameter\n");
		return;
	}

	if(ps->code < 200){
		LM_DBG("receiving a provisional reply\n");
		return;
	}

	LM_DBG("receiving a final reply %d\n",ps->code);

	iuid = (dlg_iuid_t*)(*ps->param);
	dlg = dlg_get_by_iuid(iuid);
	if(dlg==0)
		return;

	event = DLG_EVENT_REQBYE;
	next_state_dlg(dlg, event, &old_state, &new_state, &unref);

	if(new_state == DLG_STATE_DELETED && old_state != DLG_STATE_DELETED){

		LM_DBG("removing dialog with h_entry %u and h_id %u\n", 
			dlg->h_entry, dlg->h_id);

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
			LM_WARN("inconsistent dlg timer data on dlg %p [%u:%u] "
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",
				dlg, dlg->h_entry, dlg->h_id,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, dlg->tag[DLG_CALLEE_LEG].s);
		} else {
			unref++;
		}
		/* dialog terminated (BYE) */
		run_dlg_callbacks( DLGCB_TERMINATED_CONFIRMED, dlg, ps->req, ps->rpl, DLG_DIR_NONE, 0);

		LM_DBG("first final reply\n");
		/* derefering the dialog */
		dlg_unref(dlg, unref+1);

		if_update_stat( dlg_enable_stats, active_dlgs, -1);
	}

	if(new_state == DLG_STATE_DELETED && old_state == DLG_STATE_DELETED ) {
		/* trash the dialog from DB and memory */
		LM_DBG("second final reply\n");
		/* delete the dialog from DB */
		if (dlg_db_mode)
			remove_dialog_from_db(dlg);
		/* force delete from mem */
		dlg_unref(dlg, 1);
	}
	dlg_iuid_sfree(iuid);
}


/* callback function to handle responses to the keep-alive request */
void dlg_ka_cb_all(struct cell* t, int type, struct tmcb_params* ps, int dir)
{
	int tend;
	dlg_cell_t* dlg;
	dlg_iuid_t *iuid = NULL;

	if(ps->param == NULL || *ps->param == NULL) {
		LM_ERR("invalid parameter\n");
		return;
	}

	if(ps->code < 200) {
		LM_DBG("receiving a provisional reply\n");
		return;
	}

	LM_DBG("receiving a final reply %d\n",ps->code);

	iuid = (dlg_iuid_t*)(*ps->param);
	dlg = dlg_get_by_iuid(iuid);
	if(dlg==0) {
		dlg_iuid_sfree(iuid);
		return;
	}

	if(ps->code==408 || ps->code==481) {
		if (dlg->state != DLG_STATE_CONFIRMED) {
			LM_DBG("skip updating non-confirmed dialogs\n");
			goto done;
		}
		if(ps->code==408 && (dlg->cseq[dir].len==0
					|| (dlg->cseq[dir].len==1 && dlg->cseq[dir].s[0]=='\0'))) {
			LM_DBG("ignore 408 for %s cseq 0\n",
					((dir==DLG_CALLER_LEG)?"caller":"callee"));
			goto done;
		}
		tend = 0;
		if(dir==DLG_CALLER_LEG) {
			dlg->ka_src_counter++;
			if(dlg->ka_src_counter>=dlg_ka_failed_limit) {
				tend = 1;
			}
		} else {
			dlg->ka_dst_counter++;
			if(dlg->ka_dst_counter>=dlg_ka_failed_limit) {
				tend = 1;
			}
		}
		if(tend) {
			if(update_dlg_timer(&dlg->tl, 10)<0) {
				LM_ERR("failed to update dialog lifetime\n");
				goto done;
			}
			dlg->lifetime = 10;
			dlg->dflags |= DLG_FLAG_CHANGED;
		}
	} else {
		if (dlg->state == DLG_STATE_CONFIRMED) {
			if(dir==DLG_CALLER_LEG) {
				dlg->ka_src_counter = 0;
			} else {
				dlg->ka_dst_counter = 0;
			}
		}
	}

done:
	dlg_unref(dlg, 1);
	dlg_iuid_sfree(iuid);
}

/* callback function to handle responses to the keep-alive request to src */
void dlg_ka_cb_src(struct cell* t, int type, struct tmcb_params* ps)
{
	dlg_ka_cb_all(t, type, ps, DLG_CALLER_LEG);
}

/* callback function to handle responses to the keep-alive request to dst */
void dlg_ka_cb_dst(struct cell* t, int type, struct tmcb_params* ps)
{
	dlg_ka_cb_all(t, type, ps, DLG_CALLEE_LEG);
}

static inline int build_extra_hdr(struct dlg_cell * cell, str *extra_hdrs,
		str *str_hdr)
{
	char *p;
	int blen;

	str_hdr->len = MAX_FWD_HDR_LEN + dlg_extra_hdrs.len;
	if(extra_hdrs && extra_hdrs->len>0)
		str_hdr->len += extra_hdrs->len;

	blen = str_hdr->len + 3 /* '\r\n\0' */;

	/* reserve space for callee headers in local requests */
	if(dlg_lreq_callee_headers.len>0)
		blen += dlg_lreq_callee_headers.len + 2 /* '\r\n' */;

	str_hdr->s = (char*)pkg_malloc( blen * sizeof(char) );
	if(!str_hdr->s){
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	memcpy(str_hdr->s , MAX_FWD_HDR, MAX_FWD_HDR_LEN );
	p = str_hdr->s + MAX_FWD_HDR_LEN;
	if (dlg_extra_hdrs.len) {
		memcpy( p, dlg_extra_hdrs.s, dlg_extra_hdrs.len);
		p += dlg_extra_hdrs.len;
	}
	if (extra_hdrs && extra_hdrs->len>0)
		memcpy( p, extra_hdrs->s, extra_hdrs->len);

	return 0;

error: 
	return -1;
}



/* cell- pointer to a struct dlg_cell
 * dir- direction: the request will be sent to:
 * 		DLG_CALLER_LEG (0): caller
 * 		DLG_CALLEE_LEG (1): callee
 */
static inline int send_bye(struct dlg_cell * cell, int dir, str *hdrs)
{
	uac_req_t uac_r;
	dlg_t* dialog_info;
	str met = {"BYE", 3};
	int result;
	dlg_iuid_t *iuid = NULL;
	str lhdrs;

	/* Send Cancel or final response for non-confirmed dialogs */
	if (cell->state != DLG_STATE_CONFIRMED_NA && cell->state != DLG_STATE_CONFIRMED) {
		if (cell->t) {
			if (dir == DLG_CALLER_LEG) {
				if(d_tmb.t_reply(cell->t->uas.request, bye_early_code, bye_early_reason.s)< 0) {
					LM_ERR("Failed to send reply to caller\n");
					return -1;
				}
				LM_DBG("\"%d %.*s\" sent to caller\n", bye_early_code, bye_early_reason.len, bye_early_reason.s);
			} else {
				d_tmb.cancel_all_uacs(cell->t, 0);
				LM_DBG("CANCEL sent to callee(s)\n");
			}
			return 0;
		} else {
			LM_ERR("terminating non-confirmed dialog not possible, transaction not longer available.\n");
			return -1;
		}
	}

	/*verify direction*/

	if ((dialog_info = build_dlg_t(cell, dir)) == 0){
		LM_ERR("failed to create dlg_t\n");
		goto err;
	}

	/* safety bump of cseq if prack was involved in call setup */
	if(cell->iflags & DLG_IFLAG_PRACK) {
		dialog_info->loc_seq.value += 80;
	}

	LM_DBG("sending BYE to %s\n", (dir==DLG_CALLER_LEG)?"caller":"callee");

	iuid = dlg_get_iuid_shm_clone(cell);
	if(iuid==NULL)
	{
		LM_ERR("failed to create dialog unique id clone\n");
		goto err;
	}

	lhdrs = *hdrs;

	if(dir==DLG_CALLEE_LEG && dlg_lreq_callee_headers.len>0) {
		/* space allocated in hdrs->s by build_extra_hdrs() */
		memcpy(lhdrs.s+lhdrs.len, dlg_lreq_callee_headers.s,
				dlg_lreq_callee_headers.len);
		lhdrs.len += dlg_lreq_callee_headers.len;
		if(dlg_lreq_callee_headers.s[dlg_lreq_callee_headers.len-1]!='\n') {
			memcpy(lhdrs.s+lhdrs.len, CRLF, CRLF_LEN);
			lhdrs.len += CRLF_LEN;
		}
	}

	set_uac_req(&uac_r, &met, &lhdrs, NULL, dialog_info, TMCB_LOCAL_COMPLETED,
				bye_reply_cb, (void*)iuid);
	result = d_tmb.t_request_within(&uac_r);

	if(result < 0){
		LM_ERR("failed to send the BYE request\n");
		goto err;
	}

	free_tm_dlg(dialog_info);

	LM_DBG("BYE sent to %s\n", (dir==0)?"caller":"callee");
	return 0;

err:
	if(dialog_info)
		free_tm_dlg(dialog_info);
	return -1;
}

dlg_t * build_dlg_t_early(struct sip_msg *msg, struct dlg_cell * cell,
		int branch_id, str * rr_set)
{

	dlg_t* td = NULL;
	str cseq;
	unsigned int loc_seq;
	char nbuf[MAX_URI_SIZE];
	char dbuf[80];
	str nuri = STR_NULL;
	str duri = STR_NULL;
	size_t sz;
	char *p;
	unsigned int own_rr = 0, skip_recs = 0;

	if (cell->state != DLG_STATE_UNCONFIRMED && cell->state != DLG_STATE_EARLY) {
		LM_ERR("invalid state for build_dlg_state: %d"
				" (only working for unconfirmed or early dialogs)\n", cell->state);
		goto error;
	}

	if (msg == NULL || msg->first_line.type != SIP_REPLY) {
		if (!cell->t) {
			LM_ERR("no transaction associated\n");
			goto error;
		}

		if (branch_id <= 0 || branch_id > cell->t->nr_of_outgoings) {
			LM_ERR("invalid branch %d (%d branches in transaction)\n",
					branch_id, cell->t->nr_of_outgoings);
			goto error;
		}
	}

	if (!msg->contact && (parse_headers(msg,HDR_CONTACT_F,0)<0
				|| !msg->contact)) {
		LM_ERR("bad sip message or missing Contact hdr\n");
		goto error;
	}

	if ( parse_contact(msg->contact)<0 ||
			((contact_body_t *)msg->contact->parsed)->contacts==NULL) {
		LM_ERR("bad Contact HDR\n");
		goto error;
	}

	/*try to restore alias parameter if no route set */
	nuri.s = nbuf;
	nuri.len = MAX_URI_SIZE;
	duri.s = dbuf;
	duri.len = 80;
	if(uri_restore_rcv_alias(&((contact_body_t *)msg->contact->parsed)->contacts->uri,
				&nuri, &duri)<0) {
		nuri.len = 0;
		duri.len = 0;
	}

	if(nuri.len>0 && duri.len>0) {
		sz = sizeof(dlg_t) + (nuri.len+duri.len+2)*sizeof(char);
	} else {
		sz = sizeof(dlg_t);
	}

	td = (dlg_t*)pkg_malloc(sz);
	if(!td){
		LM_ERR("out of pkg memory\n");
		return NULL;
	}
	memset(td, 0, sz);

	/*route set*/
	if (msg->record_route) {
		if (cell->t) {
			LM_DBG("transaction exists\n");
			own_rr = (cell->t->flags&TM_UAC_FLAG_R2)?2:
				(cell->t->flags&TM_UAC_FLAG_RR)?1:0;
		} else {
			own_rr = (msg->flags&TM_UAC_FLAG_R2)?2:
				(msg->flags&TM_UAC_FLAG_RR)?1:0;
		}
		skip_recs = cell->from_rr_nb + own_rr;

		LM_DBG("skipping %u records, %u of myself\n", skip_recs, own_rr);

		if( print_rr_body(msg->record_route, rr_set, DLG_CALLEE_LEG,
					&skip_recs) != 0 ){
			LM_ERR("failed to print route records \n");
			goto error;
		}
		LM_DBG("new route set: %.*s\n", STR_FMT(rr_set));

		if( parse_rr_body(rr_set->s, rr_set->len,
					&td->route_set) !=0){
			LM_ERR("failed to parse route set\n");
			goto error;
		}
	}

	/*local sequence number*/
	cseq = cell->cseq[DLG_CALLER_LEG];

	if (cseq.len > 0) {
		LM_DBG("CSeq is %.*s\n", cseq.len, cseq.s);
		if(str2int(&cseq, &loc_seq) != 0){
			LM_ERR("invalid cseq\n");
			goto error;
		}
	} else {
		LM_DBG("CSeq not set yet, assuming 1\n");
		loc_seq = 1;
	}

	/*we don not increase here the cseq as this will be done by TM*/
	td->loc_seq.value = loc_seq;
	td->loc_seq.is_set = 1;

	LM_DBG("nuri: %.*s\n", STR_FMT(&nuri));
	LM_DBG("duri: %.*s\n", STR_FMT(&duri));

	if(nuri.len>0 && duri.len>0) {
		/* req uri */
		p = (char*)td + sizeof(dlg_t);
		strncpy(p, nuri.s, nuri.len);
		p[nuri.len] = '\0';
		td->rem_target.s = p;
		td->rem_target.len = nuri.len;
		/* dst uri */
		p += nuri.len + 1;
		strncpy(p, duri.s, duri.len);
		p[duri.len] = '\0';
		td->dst_uri.s = p;
		td->dst_uri.len = duri.len;
	} else {
		td->rem_target = ((contact_body_t *)msg->contact->parsed)->contacts->uri;
	}

	td->rem_uri	= cell->from_uri;
	td->loc_uri	= cell->to_uri;
	LM_DBG("rem_uri: %.*s\n", STR_FMT(&td->rem_uri));
	LM_DBG("loc_uri: %.*s\n", STR_FMT(&td->loc_uri));

	LM_DBG("rem_target: %.*s\n", STR_FMT(&td->rem_target));
	LM_DBG("dst_uri: %.*s\n", STR_FMT(&td->dst_uri));

	td->id.call_id = cell->callid;
	td->id.rem_tag = cell->tag[DLG_CALLER_LEG];
	td->id.loc_tag = cell->tag[DLG_CALLEE_LEG];

	td->state= DLG_EARLY;
	td->send_sock = cell->bind_addr[DLG_CALLER_LEG];

	return td;

error:
	LM_ERR("Error occurred creating early dialog\n");
	free_tm_dlg(td);
	return NULL;
}

int dlg_request_within(struct sip_msg *msg, struct dlg_cell *dlg, int side,
		str * method, str * hdrs, str * content_type, str * content)
{
	uac_req_t uac_r;
	dlg_t* dialog_info;
	int result;
	dlg_iuid_t *iuid = NULL;
	char rr_set_s[MAX_URI_SIZE];
	str rr_set = {rr_set_s, 0};
	str allheaders = {0, 0};
	str content_type_hdr = {"Content-Type: ", 14};
	int idx = 0;
	memset(rr_set_s, 0, 500);

	/* Special treatment for callee in early state*/
	if (dlg->state != DLG_STATE_CONFIRMED_NA
			&& dlg->state != DLG_STATE_CONFIRMED && side == DLG_CALLEE_LEG) {
		LM_DBG("Send request to callee in early state...\n");

		if (dlg->t == NULL && d_tmb.t_gett) {
			dlg->t = d_tmb.t_gett();
			if (dlg->t && dlg->t != T_UNDEFINED)
				idx = dlg->t->nr_of_outgoings;
		}
		LM_DBG("Branch %i\n", idx);

		/*verify direction*/
		if ((dialog_info = build_dlg_t_early(msg, dlg, idx, &rr_set)) == 0){
			LM_ERR("failed to create dlg_t\n");
			goto err;
		}
	} else {
		LM_DBG("Send request to caller or in confirmed state...\n");
		/*verify direction*/
		if ((dialog_info = build_dlg_t(dlg, side)) == 0){
			LM_ERR("failed to create dlg_t\n");
			goto err;
		}
	}

	LM_DBG("sending %.*s to %s\n", method->len, method->s,
			(side==DLG_CALLER_LEG)?"caller":"callee");

	iuid = dlg_get_iuid_shm_clone(dlg);
	if(iuid==NULL)
	{
		LM_ERR("failed to create dialog unique id clone\n");
		goto err;
	}

	if (hdrs && hdrs->len > 0) {
		LM_DBG("Extra headers: %.*s\n", STR_FMT(hdrs));
		allheaders.len += hdrs->len;
	}

	if (content_type && content_type->s && content && content->s) {
		LM_DBG("Content-Type: %.*s\n", STR_FMT(content_type));
		allheaders.len += content_type_hdr.len + content_type->len + 2;
	}
	if (allheaders.len > 0) {
		allheaders.s = (char*)pkg_malloc(allheaders.len);
		if (allheaders.s == NULL) {
			PKG_MEM_ERROR;
			goto err;
		}
		allheaders.len = 0;
		if (hdrs && hdrs->len > 0) {
			memcpy(allheaders.s, hdrs->s, hdrs->len);
			allheaders.len += hdrs->len;
		}
		if (content_type && content_type->s && content && content->s) {
			memcpy(allheaders.s + allheaders.len, content_type_hdr.s, content_type_hdr.len);
			allheaders.len += content_type_hdr.len;
			memcpy(allheaders.s + allheaders.len, content_type->s, content_type->len);
			allheaders.len += content_type->len;
			memcpy(allheaders.s + allheaders.len, "\r\n", 2);
			allheaders.len += 2;
		}
		LM_DBG("All headers: %.*s\n", STR_FMT(&allheaders));
	}

	set_uac_req(&uac_r, method, allheaders.len?&allheaders:NULL,
			(content && content->len)?content:NULL, dialog_info, TMCB_LOCAL_COMPLETED,
				bye_reply_cb, (void*)iuid);

	result = d_tmb.t_request_within(&uac_r);

	if (allheaders.s)
		pkg_free(allheaders.s);

	if(result < 0){
		LM_ERR("failed to send request\n");
		goto err;
	}

	free_tm_dlg(dialog_info);

	LM_DBG("%.*s sent to %s\n", method->len, method->s,
			(side==DLG_CALLER_LEG)?"caller":"callee");

	return 0;
err:
	if(dialog_info)
		free_tm_dlg(dialog_info);
	return -1;
}

/* send keep-alive
 * dlg - pointer to a struct dlg_cell
 * dir - direction: the request will be sent to:
 * 		DLG_CALLER_LEG (0): caller
 * 		DLG_CALLEE_LEG (1): callee
 */
int dlg_send_ka(dlg_cell_t *dlg, int dir)
{
	uac_req_t uac_r;
	dlg_t* di;
	str met = {"OPTIONS", 7};
	int result;
	dlg_iuid_t *iuid = NULL;

	if (dlg_filter_mode & DLG_FILTER_LOCALONLY) {
		if (dlg->bind_addr[dir] == NULL) {
			LM_DBG("skipping dialog without bind address\n");
			return 0;
		}

		if (lookup_local_socket(&(dlg->bind_addr[dir]->sock_str)) == NULL) {
			LM_DBG("skipping non local dialog\n");
			return 0;
		}
	}

	/* do not send KA request for non-confirmed dialogs (not supported) */
	if (dlg->state != DLG_STATE_CONFIRMED) {
		LM_DBG("skipping non-confirmed dialogs\n");
		return 0;
	}

	/* build tm dlg by direction */
	if ((di = build_dlg_t(dlg, dir)) == 0){
		LM_ERR("failed to create dlg_t\n");
		goto err;
	}

	/* tm increases cseq value, decrease it no to make it invalid
	 * - dialog is ended on timeout (408) or C/L does not exist (481) */
	if(di->loc_seq.value>1)
		di->loc_seq.value -= 2;
	else
		di->loc_seq.value -= 1;

	LM_DBG("sending OPTIONS to %s\n", (dir==DLG_CALLER_LEG)?"caller":"callee");

	iuid = dlg_get_iuid_shm_clone(dlg);
	if(iuid==NULL)
	{
		LM_ERR("failed to create dialog unique id clone\n");
		goto err;
	}

	if(dir==DLG_CALLEE_LEG && dlg_lreq_callee_headers.len>0) {
		set_uac_req(&uac_r, &met, &dlg_lreq_callee_headers, NULL, di,
				TMCB_LOCAL_COMPLETED, dlg_ka_cb_dst, (void*)iuid);
	} else {
		set_uac_req(&uac_r, &met, NULL, NULL, di, TMCB_LOCAL_COMPLETED,
				(dir==DLG_CALLEE_LEG)?dlg_ka_cb_dst:dlg_ka_cb_src, (void*)iuid);
	}
	result = d_tmb.t_request_within(&uac_r);

	if(result < 0){
		LM_ERR("failed to send the OPTIONS request\n");
		goto err;
	}

	free_tm_dlg(di);

	LM_DBG("keep-alive sent to %s\n", (dir==0)?"caller":"callee");
	return 0;

err:
	if(di)
		free_tm_dlg(di);
	return -1;
}


int dlg_bye(struct dlg_cell *dlg, str *hdrs, int side)
{
	str all_hdrs = { 0, 0 };
	int ret;

	if(side==DLG_CALLER_LEG)
	{
		if(dlg->dflags&DLG_FLAG_CALLERBYE)
			return -1;
		dlg->dflags |= DLG_FLAG_CALLERBYE;
	} else {
		if(dlg->dflags&DLG_FLAG_CALLEEBYE)
			return -1;
		dlg->dflags |= DLG_FLAG_CALLEEBYE;
	}
	if ((build_extra_hdr(dlg, hdrs, &all_hdrs)) != 0)
	{
		LM_ERR("failed to build dlg headers\n");
		return -1;
	}
	ret = send_bye(dlg, side, &all_hdrs);
	pkg_free(all_hdrs.s);

	dlg_run_event_route(dlg, NULL, dlg->state, DLG_STATE_DELETED);

	return ret;
}

int dlg_bye_all(struct dlg_cell *dlg, str *hdrs)
{
	str all_hdrs = { 0, 0 };
	int ret;

	/* run dialog terminated callbacks */
	run_dlg_callbacks( DLGCB_TERMINATED, dlg, NULL, NULL, DLG_DIR_NONE, 0);

	if ((build_extra_hdr(dlg, hdrs, &all_hdrs)) != 0)
	{
		LM_ERR("failed to build dlg headers\n");
		return -1;
	}

	ret = send_bye(dlg, DLG_CALLER_LEG, &all_hdrs);
	ret |= send_bye(dlg, DLG_CALLEE_LEG, &all_hdrs);
	
	pkg_free(all_hdrs.s);

	dlg_run_event_route(dlg, NULL, dlg->state, DLG_STATE_DELETED);

	return ret;

}

