/*
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*!
 * \file
 * \brief Call transfer
 * \ingroup dialog
 * Module: \ref dialog
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../dprint.h"
#include "../../ut.h"
#include "../../trim.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_from.h"
#include "../../parser/msg_parser.h"

#include "../../modules/tm/tm_load.h"

#include "dlg_req_within.h"
#include "dlg_handlers.h"
#include "dlg_transfer.h"

#define DLG_HOLD_SDP "v=0\r\no=kamailio-bridge 0 0 IN IP4 0.0.0.0\r\ns=kamailio\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\nm=audio 9 RTP/AVP 8 0\r\na=rtpmap:8 PCMA/8000\r\na=rtpmap:0 PCMU/8000\r\n"
#define DLG_HOLD_SDP_LEN	(sizeof(DLG_HOLD_SDP)-1)

/*
#define DLG_HOLD_CT_HDR "Contact: <sip:kamailio.org:5060>\r\nContent-Type: application/sdp\r\n"
#define DLG_HOLD_CT_HDR_LEN	(sizeof(DLG_HOLD_CT_HDR)-1)
*/

extern str dlg_bridge_controller;
extern str dlg_bridge_contact;

static char *dlg_bridge_hdrs_buf = NULL;
static str dlg_bridge_inv_hdrs = {0};
static str dlg_bridge_ref_hdrs = {0};

int dlg_bridge_init_hdrs(void)
{
	if(dlg_bridge_hdrs_buf!=NULL)
		return 0;
	dlg_bridge_hdrs_buf = (char*)pkg_malloc((dlg_bridge_contact.len + 46)
													* sizeof(char));
	if(dlg_bridge_hdrs_buf==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	strncpy(dlg_bridge_hdrs_buf,
			"Contact: <", 10);
	strncpy(dlg_bridge_hdrs_buf + 10,
			dlg_bridge_contact.s, dlg_bridge_contact.len);
	strncpy(dlg_bridge_hdrs_buf + 10 + dlg_bridge_contact.len,
			">\r\nContent-Type: application/sdp\r\n", 34);
	dlg_bridge_hdrs_buf[dlg_bridge_contact.len+44] = '\0';
	dlg_bridge_inv_hdrs.s = dlg_bridge_hdrs_buf;
	dlg_bridge_inv_hdrs.len = dlg_bridge_contact.len + 44;
	dlg_bridge_ref_hdrs.s = dlg_bridge_hdrs_buf;
	dlg_bridge_ref_hdrs.len = dlg_bridge_contact.len + 13;
	return 0;
}

void dlg_bridge_destroy_hdrs(void)
{
	if(dlg_bridge_hdrs_buf!=NULL)
		pkg_free(dlg_bridge_hdrs_buf);
}

void dlg_transfer_ctx_free(dlg_transfer_ctx_t *dtc)
{
	struct dlg_cell *dlg;

	if(dtc==NULL)
		return;
	if(dtc->from.s!=NULL)
		shm_free(dtc->from.s);
	if(dtc->to.s!=NULL)
		shm_free(dtc->to.s);

	dlg = dtc->dlg;
	if(dlg!=NULL)
	{
		if (dlg->tag[DLG_CALLER_LEG].s)
			shm_free(dlg->tag[DLG_CALLER_LEG].s);

		if (dlg->tag[DLG_CALLEE_LEG].s)
			shm_free(dlg->tag[DLG_CALLEE_LEG].s);

		if (dlg->cseq[DLG_CALLER_LEG].s)
			shm_free(dlg->cseq[DLG_CALLER_LEG].s);

		if (dlg->cseq[DLG_CALLEE_LEG].s)
			shm_free(dlg->cseq[DLG_CALLEE_LEG].s);

		shm_free(dlg);
	}

	shm_free(dtc);
}

void dlg_refer_tm_callback(struct cell *t, int type, struct tmcb_params *ps)
{
	dlg_transfer_ctx_t *dtc = NULL;
	dlg_t* dialog_info = NULL;
	str met = {"BYE", 3};
	int result;
	struct dlg_cell *dlg;
	uac_req_t uac_r;

	if(ps->param==NULL || *ps->param==0)
	{
		LM_DBG("message id not received\n");
		return;
	}
	dtc = *((dlg_transfer_ctx_t**)ps->param);
	if(dtc==NULL)
		return;
	LM_DBG("REFER completed with status %d\n", ps->code);

	/* we send the BYE anyhow */
	dlg = dtc->dlg;
	if ((dialog_info = build_dlg_t(dlg, DLG_CALLEE_LEG)) == 0){
		LM_ERR("failed to create dlg_t\n");
		goto error;
	}

	/* after REFER, the CSeq must be increased */
	dialog_info->loc_seq.value++;

	set_uac_req(&uac_r, &met, NULL, NULL, dialog_info, 0, NULL, NULL);
	result = d_tmb.t_request_within(&uac_r);

	if(result < 0) {
		LM_ERR("failed to send the REFER request\n");
		/* todo: clean-up dtc */
		goto error;
	}

	free_tm_dlg(dialog_info);
	dlg_transfer_ctx_free(dtc);

	LM_DBG("BYE sent\n");
	return;

error:
	dlg_transfer_ctx_free(dtc);
	if(dialog_info)
		free_tm_dlg(dialog_info);
	return;

}

static int dlg_refer_callee(dlg_transfer_ctx_t *dtc)
{
	/*verify direction*/
	dlg_t* dialog_info = NULL;
	str met = {"REFER", 5};
	int result;
	str hdrs;
	struct dlg_cell *dlg;
	uac_req_t uac_r;

	dlg = dtc->dlg;

	if ((dialog_info = build_dlg_t(dlg, DLG_CALLEE_LEG)) == 0){
		LM_ERR("failed to create dlg_t\n");
		goto error;
	}

	hdrs.len = 23 + 2*CRLF_LEN + dlg_bridge_controller.len
		+ dtc->to.len + dlg_bridge_ref_hdrs.len;
	LM_DBG("sending REFER [%d] <%.*s>\n", hdrs.len, dtc->to.len, dtc->to.s);
	hdrs.s = (char*)pkg_malloc(hdrs.len*sizeof(char));
	if(hdrs.s == NULL)
		goto error;
	memcpy(hdrs.s, "Referred-By: ", 13);
	memcpy(hdrs.s+13, dlg_bridge_controller.s, dlg_bridge_controller.len);
	memcpy(hdrs.s+13+dlg_bridge_controller.len, CRLF, CRLF_LEN);
	memcpy(hdrs.s+13+dlg_bridge_controller.len+CRLF_LEN, "Refer-To: ", 10);
	memcpy(hdrs.s+23+dlg_bridge_controller.len+CRLF_LEN, dtc->to.s,
			dtc->to.len);
	memcpy(hdrs.s+23+dlg_bridge_controller.len+CRLF_LEN+dtc->to.len,
			CRLF, CRLF_LEN);
	memcpy(hdrs.s+23+dlg_bridge_controller.len+CRLF_LEN+dtc->to.len+CRLF_LEN,
			dlg_bridge_ref_hdrs.s, dlg_bridge_ref_hdrs.len);

	set_uac_req(&uac_r, &met, &hdrs, NULL, dialog_info, TMCB_LOCAL_COMPLETED,
				dlg_refer_tm_callback, (void*)dtc);
	result = d_tmb.t_request_within(&uac_r);

	pkg_free(hdrs.s);
	if(result < 0) {
		LM_ERR("failed to send the REFER request\n");
		/* todo: clean-up dtc */
		goto error;
	}

	free_tm_dlg(dialog_info);

	LM_DBG("REFER sent\n");
	return 0;

error:
	if(dialog_info)
		free_tm_dlg(dialog_info);
	return -1;
}


void dlg_bridge_tm_callback(struct cell *t, int type, struct tmcb_params *ps)
{
	struct sip_msg *msg = NULL;
	dlg_transfer_ctx_t *dtc = NULL;
	struct dlg_cell *dlg = NULL;
	str s;
	str cseq;
	str empty = {"", 0};

	if(ps->param==NULL || *ps->param==0)
	{
		LM_DBG("message id not received\n");
		return;
	}
	dtc = *((dlg_transfer_ctx_t**)ps->param);
	if(dtc==NULL)
		return;
	LM_DBG("completed with status %d\n", ps->code);
	if(ps->code>=300)
		goto error;

	/* 2xx - build dialog/send refer */
	msg = ps->rpl;
	if((msg->cseq==NULL || parse_headers(msg,HDR_CSEQ_F,0)<0)
			|| msg->cseq==NULL || msg->cseq->parsed==NULL)
	{
			LM_ERR("bad sip message or missing CSeq hdr :-/\n");
			goto error;
	}
	cseq = (get_cseq(msg))->number;

	if((msg->to==NULL && parse_headers(msg, HDR_TO_F,0)<0) || msg->to==NULL)
	{
		LM_ERR("bad request or missing TO hdr\n");
		goto error;
	}
	if(parse_from_header(msg))
	{
		LM_ERR("bad request or missing FROM hdr\n");
		goto error;
	}
	if((msg->callid==NULL && parse_headers(msg,HDR_CALLID_F,0)<0)
			|| msg->callid==NULL){
		LM_ERR("bad request or missing CALLID hdr\n");
		goto error;
	}
	s = msg->callid->body;
	trim(&s);

	/* some sanity checks */
	if (s.len==0 || get_from(msg)->tag_value.len==0) {
		LM_ERR("invalid request -> callid (%d) or from TAG (%d) empty\n",
			s.len, get_from(msg)->tag_value.len);
		goto error;
	}

	dlg = build_new_dlg(&s /*callid*/, &(get_from(msg)->uri) /*from uri*/,
		&(get_to(msg)->uri) /*to uri*/,
		&(get_from(msg)->tag_value)/*from_tag*/,
		&(get_to(msg)->uri) /*use to as r-uri*/ );
	if (dlg==0) {
		LM_ERR("failed to create new dialog\n");
		goto error;
	}
	dtc->dlg = dlg;
	if (dlg_set_leg_info(dlg, &(get_from(msg)->tag_value),
				&empty, &dlg_bridge_controller, &cseq, DLG_CALLER_LEG)!=0) {
		LM_ERR("dlg_set_leg_info failed\n");
		goto error;
	}

	if (populate_leg_info(dlg, msg, t, DLG_CALLEE_LEG,
			&(get_to(msg)->tag_value)) !=0)
	{
		LM_ERR("could not add further info to the dialog\n");
		shm_free(dlg);
		goto error;
	}

	if(dlg_refer_callee(dtc)!=0)
		goto error;
	return;

error:
	dlg_transfer_ctx_free(dtc);
	return;
}


int dlg_bridge(str *from, str *to, str *op, str *bd)
{
	dlg_transfer_ctx_t *dtc;
	int ret;
	str s_method = {"INVITE", 6};
	str s_body;
	uac_req_t uac_r;

	dtc = (dlg_transfer_ctx_t*)shm_malloc(sizeof(dlg_transfer_ctx_t));
	if(dtc==NULL)
	{
		LM_ERR("no shm\n");
		return -1;
	}
	memset(dtc, 0, sizeof(dlg_transfer_ctx_t));
	dtc->from.s = (char*)shm_malloc((from->len+1)*sizeof(char));
	if(dtc->from.s==NULL)
	{
		LM_ERR("no shm\n");
		shm_free(dtc);
		return -1;
	}
	dtc->to.s = (char*)shm_malloc((to->len+1)*sizeof(char));
	if(dtc->to.s==NULL)
	{
		LM_ERR("no shm\n");
		shm_free(dtc->from.s);
		shm_free(dtc);
		return -1;
	}
	memcpy(dtc->from.s, from->s, from->len);
	dtc->from.len = from->len;
	dtc->from.s[dtc->from.len] = '\0';
	memcpy(dtc->to.s, to->s, to->len);
	dtc->to.len = to->len;
	dtc->to.s[dtc->to.len] = '\0';

	LM_DBG("bridge <%.*s> to <%.*s>\n", dtc->from.len, dtc->from.s,
			dtc->to.len, dtc->to.s);
	if(bd!=NULL && bd->s!=NULL && bd->len>0) {
		s_body.s = bd->s;
		s_body.len = bd->len;
	} else {
		s_body.s   = DLG_HOLD_SDP;
		s_body.len = DLG_HOLD_SDP_LEN;
	}

	memset(&uac_r, '\0', sizeof(uac_req_t));
	uac_r.method = &s_method;
	uac_r.headers = &dlg_bridge_inv_hdrs;
	uac_r.body = &s_body;
	uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
	uac_r.cb = dlg_bridge_tm_callback;
	uac_r.cbp = (void*)(long)dtc;
	ret = d_tmb.t_request(&uac_r, /* UAC Req */
						  &dtc->from, /* Request-URI (To) */
						  &dtc->from, /* To */
						  &dlg_bridge_controller, /* From */
						  (op != NULL && op->len>0)?op:NULL /* Outbound-URI */
		);

	if(ret<0)
	{
		dlg_transfer_ctx_free(dtc);
		return -1;
	}
	return 0;
}

int dlg_transfer(struct dlg_cell *dlg, str *to, int side)
{
	dlg_transfer_ctx_t *dtc = NULL;
	struct dlg_cell *ndlg = NULL;
	str from;
	str empty = {"", 0};

	dtc = (dlg_transfer_ctx_t*)shm_malloc(sizeof(dlg_transfer_ctx_t));
	if(dtc==NULL)
	{
		LM_ERR("no shm\n");
		return -1;
	}
	if(side==DLG_CALLEE_LEG)
	{
		from = dlg->from_uri;
	} else {
		from = dlg->to_uri;
	}
	memset(dtc, 0, sizeof(dlg_transfer_ctx_t));
	dtc->from.s = (char*)shm_malloc((from.len+1)*sizeof(char));
	if(dtc->from.s==NULL)
	{
		LM_ERR("no shm\n");
		shm_free(dtc);
		return -1;
	}
	dtc->to.s = (char*)shm_malloc((to->len+1)*sizeof(char));
	if(dtc->to.s==NULL)
	{
		LM_ERR("no shm\n");
		shm_free(dtc->from.s);
		shm_free(dtc);
		return -1;
	}
	memcpy(dtc->from.s, from.s, from.len);
	dtc->from.len = from.len;
	dtc->from.s[dtc->from.len] = '\0';
	memcpy(dtc->to.s, to->s, to->len);
	dtc->to.len = to->len;
	dtc->to.s[dtc->to.len] = '\0';
	
	if(side==DLG_CALLER_LEG)
		ndlg = build_new_dlg(&dlg->callid /*callid*/,
				&dlg->to_uri /*from uri*/, &dlg->from_uri /*to uri*/,
				&dlg->tag[side]/*from_tag*/, &dlg->req_uri /*req uri */ );
	else
		ndlg = build_new_dlg(&dlg->callid /*callid*/,
				&dlg->from_uri /*from uri*/, &dlg->to_uri /*to uri*/,
				&dlg->tag[side]/*from_tag*/, &dlg->req_uri /*req uri */ );
	if (ndlg==0) {
		LM_ERR("failed to create new dialog\n");
		goto error;
	}
	dtc->dlg = ndlg;
	if (dlg_set_leg_info(ndlg, &dlg->tag[side], &empty,
			&dlg->contact[side], &dlg->cseq[side], DLG_CALLER_LEG)!=0)
	{
		LM_ERR("dlg_set_leg_info failed for caller\n");
		goto error;
	}
	if(side==DLG_CALLEE_LEG)
		side = DLG_CALLER_LEG;
	else
		side = DLG_CALLEE_LEG;
	if (dlg_set_leg_info(ndlg, &dlg->tag[side], &dlg->route_set[side],
			&dlg->contact[side], &dlg->cseq[side], DLG_CALLEE_LEG)!=0)
	{
		LM_ERR("dlg_set_leg_info failed for caller\n");
		goto error;
	}

	if(dlg_refer_callee(dtc)!=0)
		goto error;
	return 0;

error:
	dlg_transfer_ctx_free(dtc);
	return -1;
}

