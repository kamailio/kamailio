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

#include "../../dprint.h"
#include "../../ut.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../socket_info.h"
#include "../../dset.h"
#include "../../modules/tm/dlg.h"
#include "../../modules/tm/tm_load.h"
#include "../../lib/kmi/tree.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "dlg_timer.h"
#include "dlg_hash.h"
#include "dlg_handlers.h"
#include "dlg_req_within.h"
#include "dlg_db_handler.h"


#define MAX_FWD_HDR        "Max-Forwards: " MAX_FWD CRLF
#define MAX_FWD_HDR_LEN    (sizeof(MAX_FWD_HDR) - 1)

extern str dlg_extra_hdrs;
extern str dlg_lreq_callee_headers;


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
	str nuri;
	str duri;
	size_t sz;
	char *p;

	/*remote target--- Request URI*/
	if(cell->contact[dir].s==0 || cell->contact[dir].len==0){
		LM_ERR("no contact available\n");
		goto error;
	}
	/*restore alias parameter*/
	nuri.s = nbuf;
	nuri.len = MAX_URI_SIZE;
	duri.s = dbuf;
	duri.len = 80;
	if(uri_restore_rcv_alias(&cell->contact[dir], &nuri, &duri)<0) {
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
void dlg_ka_cb(struct cell* t, int type, struct tmcb_params* ps){

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
		if(update_dlg_timer(&dlg->tl, 10)<0) {
			LM_ERR("failed to update dialog lifetime\n");
			goto done;
		}
		dlg->lifetime = 10;
		dlg->dflags |= DLG_FLAG_CHANGED;
	}

done:
	dlg_unref(dlg, 1);
	dlg_iuid_sfree(iuid);
}


static inline int build_extra_hdr(struct dlg_cell * cell, str *extra_hdrs,
		str *str_hdr)
{
	char *p;
	int blen;

	str_hdr->len = MAX_FWD_HDR_LEN + dlg_extra_hdrs.len;
	if(extra_hdrs && extra_hdrs->len>0)
		str_hdr->len += extra_hdrs->len;

	blen = str_hdr->len + 1 /* '\0' */;

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

	/* do not send BYE request for non-confirmed dialogs (not supported) */
	if (cell->state != DLG_STATE_CONFIRMED_NA && cell->state != DLG_STATE_CONFIRMED) {
		LM_ERR("terminating non-confirmed dialogs not supported\n");
		return -1;
	}

	/*verify direction*/

	if ((dialog_info = build_dlg_t(cell, dir)) == 0){
		LM_ERR("failed to create dlg_t\n");
		goto err;
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
			strncpy(lhdrs.s+lhdrs.len, CRLF, CRLF_LEN);
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
				TMCB_LOCAL_COMPLETED, dlg_ka_cb, (void*)iuid);
	} else {
		set_uac_req(&uac_r, &met, NULL, NULL, di, TMCB_LOCAL_COMPLETED,
				dlg_ka_cb, (void*)iuid);
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



/*parameters from MI: h_entry, h_id of the requested dialog*/
struct mi_root * mi_terminate_dlg(struct mi_root *cmd_tree, void *param ){

	struct mi_node* node;
	unsigned int h_entry, h_id;
	struct dlg_cell * dlg = NULL;
	str mi_extra_hdrs = {NULL,0};
	int status, msg_len;
	char *msg;


	if( d_table ==NULL)
		goto end;

	node = cmd_tree->node.kids;
	h_entry = h_id = 0;

	if (node==NULL || node->next==NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if (!node->value.s|| !node->value.len|| strno2int(&node->value,&h_entry)<0)
		goto error;

	node = node->next;
	if ( !node->value.s || !node->value.len || strno2int(&node->value,&h_id)<0)
		goto error;

	if (node->next) {
		node = node->next;
		if (node->value.len && node->value.s)
			mi_extra_hdrs = node->value;
	}

	LM_DBG("h_entry %u h_id %u\n", h_entry, h_id);

	dlg = dlg_lookup(h_entry, h_id);

	// lookup_dlg has incremented the reference count

	if(dlg){
		if(dlg_bye_all(dlg,(mi_extra_hdrs.len>0)?&mi_extra_hdrs:NULL)<0) {
			status = 500;
			msg = MI_DLG_OPERATION_ERR;
			msg_len = MI_DLG_OPERATION_ERR_LEN;
		} else {
			status = 200;
			msg = MI_OK_S;
			msg_len = MI_OK_LEN;
		}

		dlg_release(dlg);

		return init_mi_tree(status, msg, msg_len);
	}

end:
	return init_mi_tree(404, MI_DIALOG_NOT_FOUND, MI_DIALOG_NOT_FOUND_LEN);
	
error:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);

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
	return ret;

}

