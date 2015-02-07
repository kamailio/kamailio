/*
 * Copyright (C) 2007 SOMA Networks, Inc.
 * Written by Ovidiu Sas (osas)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 */


#include "../../ut.h"
#include "../../dprint.h"
#include "../../lib/kmi/mi.h"
#include "qos_handlers.h"
#include "qos_ctx_helpers.h"


int add_mi_sdp_payload_nodes(struct mi_node* node, int index, sdp_payload_attr_t* sdp_payload)
{
	struct mi_node* node1;
	struct mi_attr* attr;
	char* p;
	int len;

	p = int2str((unsigned long)(index), &len);
	node1 = add_mi_node_child( node, MI_DUP_VALUE, "payload", 7, p, len);
	if (node1==NULL)
		return 1;

	attr = add_mi_attr(node1, MI_DUP_VALUE, "rtpmap", 6, sdp_payload->rtp_payload.s, sdp_payload->rtp_payload.len);
	if (attr==NULL)
		return 1;

	if (sdp_payload->rtp_enc.s!=NULL && sdp_payload->rtp_enc.len!=0) {
		attr = add_mi_attr(node1, MI_DUP_VALUE, "codec", 5, sdp_payload->rtp_enc.s, sdp_payload->rtp_enc.len);
		if(attr == NULL)
			return 1;
	}

	return 0;
}

int add_mi_stream_nodes(struct mi_node* node, int index, sdp_stream_cell_t* stream)
{
	struct mi_node* node1;
	struct mi_attr* attr;
	sdp_payload_attr_t* sdp_payload;
	char* p;
	int i, len;

	p = int2str((unsigned long)(index), &len);
	node1 = add_mi_node_child( node, MI_DUP_VALUE, "stream", 6, p, len);
	if (node1==NULL)
		return 1;
	
	attr = add_mi_attr(node1, MI_DUP_VALUE, "media", 5, stream->media.s, stream->media.len);
	if(attr == NULL) 
		return 1; 

	attr = add_mi_attr(node1, MI_DUP_VALUE, "IP", 2, stream->ip_addr.s, stream->ip_addr.len);
	if(attr == NULL) 
		return 1;

	attr = add_mi_attr(node1, MI_DUP_VALUE, "port", 4, stream->port.s, stream->port.len);
	if(attr == NULL)
		return 1;

	attr = add_mi_attr(node1, MI_DUP_VALUE, "transport", 9, stream->transport.s, stream->transport.len);
	if(attr == NULL)
		return 1;

	if (stream->sendrecv_mode.s!=NULL && stream->sendrecv_mode.len!=0) {
		attr = add_mi_attr(node1, MI_DUP_VALUE, "sendrecv", 8, stream->sendrecv_mode.s, stream->sendrecv_mode.len);
		if(attr == NULL)
			return 1;
	}

	if (stream->ptime.s!=NULL && stream->ptime.len!=0) {
		attr = add_mi_attr(node1, MI_DUP_VALUE, "ptime", 5, stream->ptime.s, stream->ptime.len);
		if(attr == NULL)
			return 1;
	}

	p = int2str((unsigned long)(stream->payloads_num), &len);
	attr = add_mi_attr(node1, MI_DUP_VALUE, "payloads_num", 12, p, len);
	if(attr == NULL)
		return 1;

	sdp_payload = stream->payload_attr;
	for(i=stream->payloads_num-1;i>=0;i--){
		if (!sdp_payload) {
			LM_ERR("got NULL sdp_payload\n");
			return 1;
		}
		if (0!=add_mi_sdp_payload_nodes(node1, i, sdp_payload)){
			return 1;
		}
		sdp_payload = sdp_payload->next;
	}

	return 0;
}

int add_mi_session_nodes(struct mi_node* node, int index, sdp_session_cell_t* session)
{
	struct mi_node* node1;
	struct mi_attr* attr;
	sdp_stream_cell_t* stream;
	char* p;
	int i, len;

	switch (index) {
		case 0:
			node1 = add_mi_node_child( node, MI_DUP_VALUE, "session", 7, "caller", 6);
			if (node1==NULL)
				return 1;
			break;
		case 1:
			node1 = add_mi_node_child( node, MI_DUP_VALUE, "session", 7, "callee", 6);
			if (node1==NULL)
				return 1;
			break;
		default:
			return 1;
	}

	attr = add_mi_attr(node1, MI_DUP_VALUE, "cnt_disp", 8, session->cnt_disp.s, session->cnt_disp.len);
	if(attr == NULL)
		return 1;

	attr = add_mi_attr(node1, MI_DUP_VALUE, "bw_type", 7, session->bw_type.s, session->bw_type.len);
	if(attr == NULL)
		return 1;

	attr = add_mi_attr(node1, MI_DUP_VALUE, "bw_width", 8, session->bw_width.s, session->bw_width.len);
	if(attr == NULL)
		return 1;

	p = int2str((unsigned long)(session->streams_num), &len); 
	attr = add_mi_attr(node1, MI_DUP_VALUE, "streams", 7, p, len);
	if(attr == NULL)
		return 1;

	stream = session->streams;
	for(i=session->streams_num-1;i>=0;i--){
		if (!stream) {
			LM_ERR("got NULL stream\n");
			return 1;
		}
		if (0!=add_mi_stream_nodes(node1, i, stream)){
			return 1;
		}
		stream = stream->next;
	}

	return 0;
}

int add_mi_sdp_nodes(struct mi_node* node, qos_sdp_t* qos_sdp)
{
	struct mi_node* node1;
	struct mi_attr* attr;
	char* p;
	int i, len;
	sdp_session_cell_t* session;

	if ( qos_sdp->prev != NULL ) LM_ERR("got qos_sdp->prev=%p\n", qos_sdp->prev);

	while (qos_sdp) {
		node1 = add_mi_node_child( node, MI_DUP_VALUE, "sdp", 3, NULL, 0);
		if (node1==NULL)
			return 1;

		p = int2str((unsigned long)(qos_sdp->method_dir), &len);
		attr = add_mi_attr(node1, MI_DUP_VALUE, "m_dir", 5, p, len);
		if(attr == NULL)
			return 1;

		p = int2str((unsigned long)(qos_sdp->method_id), &len);
		attr = add_mi_attr(node1, MI_DUP_VALUE, "m_id", 4, p, len);
		if(attr == NULL)
			return 1;

		attr = add_mi_attr(node1, MI_DUP_VALUE, "method", 6, qos_sdp->method.s, qos_sdp->method.len);
		if(attr == NULL)
			return 1;

		attr = add_mi_attr(node1, MI_DUP_VALUE, "cseq", 4, qos_sdp->cseq.s, qos_sdp->cseq.len);
		if(attr == NULL)
			return 1;

		p = int2str((unsigned long)(qos_sdp->negotiation), &len);
		attr = add_mi_attr(node1, MI_DUP_VALUE, "negotiation", 11, p, len);
		if(attr == NULL)
			return 1;

		for (i=1;i>=0;i--){
			session = qos_sdp->sdp_session[i];
			if (session) {
				if (0 != add_mi_session_nodes(node1, i, session))
					return 1;
			}
		}

		qos_sdp = qos_sdp->next;
	}
	return 0;
}

void qos_dialog_mi_context_CB(struct dlg_cell* did, int type, struct dlg_cb_params * params)
{
	struct mi_node* parent_node = (struct mi_node*)(params->dlg_data);
	struct mi_node* node;
	qos_ctx_t* qos_ctx = (qos_ctx_t*)*(params->param);
	qos_sdp_t* qos_sdp;

	qos_sdp = qos_ctx->pending_sdp;
	if (qos_sdp) {
		node = add_mi_node_child(parent_node, MI_DUP_VALUE, "qos", 3, "pending_sdp", 11);
		if (node==NULL) {
			LM_ERR("oom\n");
			return;
		}

		if (0 != add_mi_sdp_nodes( node, qos_sdp))
			return;
	}


	qos_sdp = qos_ctx->negotiated_sdp;
	if (qos_sdp) {
		node = add_mi_node_child(parent_node, MI_DUP_VALUE, "qos", 3, "negotiated_sdp", 14);
		if (node==NULL) {
			LM_ERR("oom\n");
			return;
		}

		if (0 != add_mi_sdp_nodes( node, qos_sdp))
			return;
	}


	return;
}

