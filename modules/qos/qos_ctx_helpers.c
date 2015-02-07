/*
 * Copyright (C) 2007 SOMA Networks, Inc.
 * Written by Ovidiu Sas (osas)
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

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/sdp/sdp_cloner.h"
#include "../dialog/dlg_hash.h"
#include "qos_ctx_helpers.h"

#define ERROR_MATCH           -1
#define NO_INVITE_REQ_MATCH    0
#define NO_INVITE_RESP_MATCH   1
#define PENDING_MATCH          2
#define NEGOTIATED_MATCH       3
#define NO_ACK_REQ_MATCH       4
#define NO_UPDATE_REQ_MATCH    7
#define NO_UPDATE_RESP_MATCH   8


#define N_UNKNOWN       0
/* INVITE/200_ok */
#define N_INVITE_200OK  1
/* 200_ok/ACK */
#define N_200OK_ACK     2
/* early media (http://www.ietf.org/rfc/rfc3959.txt) */
/* 183_early_media/PRACK */
#define N_183_PRACK     3

qos_ctx_t *build_new_qos_ctx(void) {
	qos_ctx_t *ctx = NULL;

	ctx = (qos_ctx_t *)shm_malloc(sizeof(qos_ctx_t));
	if (ctx!=NULL) {
		memset(ctx, 0, sizeof(qos_ctx_t));
	} else {
		LM_ERR("No enough shared memory\n");
		return NULL;
	}
	if (!lock_init(&ctx->lock)) {
		shm_free(ctx);
		return NULL;
	}
	return ctx;
}

void destroy_qos(qos_sdp_t *qos_sdp)
{
	free_cloned_sdp_session(qos_sdp->sdp_session[0]);
	free_cloned_sdp_session(qos_sdp->sdp_session[1]);
	shm_free(qos_sdp);

	return;
}


void print_qos_sdp(qos_sdp_t *qos_sdp)
{
	if (qos_sdp == NULL) {
		return;
	}
	LM_DBG("[%p] prev->%p next->%p method_dir=%d method_id=%d method='%.*s' cseq='%.*s' negotiation=%d sdp[0:QOS_CALLER]=%p sdp[1:QOS_CALLEE]=%p\n",
		qos_sdp, qos_sdp->prev, qos_sdp->next, qos_sdp->method_dir, qos_sdp->method_id,
		qos_sdp->method.len ,qos_sdp->method.s, qos_sdp->cseq.len, qos_sdp->cseq.s,
		qos_sdp->negotiation, qos_sdp->sdp_session[0], qos_sdp->sdp_session[1]);
	/* print_sdp_session(qos_sdp->sdp_session[0]); */
	/* print_sdp_session(qos_sdp->sdp_session[1]); */
}



/*
 * Find a matching sdp inside the local qos_ctx
 * for the given session received via message _m with the given direction, cseq and method
 * and return the type of the match and a pointer to the matched qos_sdp so we can properly insert the given session into the qos_ctx->qos_sdp.
 */
int find_qos_sdp(qos_ctx_t *qos_ctx, unsigned int dir, unsigned int other_role, str *cseq_number, int cseq_method_id, sdp_session_cell_t *session, struct sip_msg *_m, qos_sdp_t **_qos_sdp)
{
	qos_sdp_t *qos_sdp;
	str *received_cnt_disp, *local_cnt_disp;

	LM_DBG("received session: %p and other_role: %s\n", session, (other_role==QOS_CALLER)?"QOS_CALLER":"QOS_CALLEE");

	switch (_m->first_line.type) {
		case SIP_REQUEST:
			switch (cseq_method_id) {
				case METHOD_INVITE:
					return NO_INVITE_REQ_MATCH;
					break;
				case METHOD_ACK:
					/* searching into the pending_sdp list */
					qos_sdp = qos_ctx->pending_sdp;
					LM_DBG("searching the negotiated_sdp: %p\n", qos_sdp);
					while (qos_sdp) {
						if (METHOD_INVITE == qos_sdp->method_id && dir != qos_sdp->method_dir && qos_sdp->negotiation == N_200OK_ACK &&
							cseq_number->len == qos_sdp->cseq.len && 0 == strncmp(cseq_number->s, qos_sdp->cseq.s, cseq_number->len)) {
							LM_DBG("method_id, dir and cseq match with previous session %p->%p\n",
								qos_sdp, qos_sdp->sdp_session[other_role]);
							/* print_sdp_session(qos_sdp->sdp_session[other_role]); */
							if (qos_sdp->sdp_session[other_role] != NULL) {
								local_cnt_disp = &(qos_sdp->sdp_session[other_role]->cnt_disp);
								received_cnt_disp = &(session->cnt_disp);
								if (local_cnt_disp->len == received_cnt_disp->len) {
									if (local_cnt_disp->len == 0) {
										LM_DBG("no cnt disp header ... => %p\n", qos_sdp);
										*_qos_sdp = qos_sdp;
										return PENDING_MATCH;
									} else if (0==strncmp(local_cnt_disp->s, received_cnt_disp->s, local_cnt_disp->len)) {
										LM_DBG("'%.*s' => %p\n", local_cnt_disp->len, local_cnt_disp->s, qos_sdp);
										*_qos_sdp = qos_sdp;
										return PENDING_MATCH;
									}
								} else if (received_cnt_disp->len == 0 && local_cnt_disp->len == 7 &&
									0==strncmp(local_cnt_disp->s, "session", 7)) {
									/* We may have an offer with cnt_disp='session' and an answer with cnt_disp='' */
									*_qos_sdp = qos_sdp;
									return PENDING_MATCH;
								}
							} else {
								LM_ERR("skipping search for null sdp for %s\n", (other_role==QOS_CALLER)?"QOS_CALLER":"QOS_CALLEE");
							}
						}
						qos_sdp = qos_sdp->next;
					}
					return NO_ACK_REQ_MATCH;
					break;
				case METHOD_UPDATE:
					return NO_UPDATE_REQ_MATCH;
					break;
				case METHOD_PRACK:
					LM_ERR("PRACK not implemented yet\n");
					return ERROR_MATCH;
					break;
				default:
					LM_ERR("Unexpected method id %d\n", cseq_method_id);
					return ERROR_MATCH;
			}
			break;
		case SIP_REPLY:
			switch (cseq_method_id) {
				case METHOD_INVITE:
					/* searching into the pending_sdp list */
					qos_sdp = qos_ctx->pending_sdp;
					while (qos_sdp) {
						//print_qos_sdp(qos_sdp);
						if (cseq_method_id == qos_sdp->method_id && dir != qos_sdp->method_dir &&
							qos_sdp->negotiation == N_INVITE_200OK && cseq_number->len == qos_sdp->cseq.len &&
							0 == strncmp(cseq_number->s, qos_sdp->cseq.s, cseq_number->len)) {
							LM_DBG("method_id, dir and cseq match with previous session %p->%p\n",
								qos_sdp, qos_sdp->sdp_session[other_role]);
							/* print_sdp_session(qos_sdp->sdp_session[other_role]); */
							if (qos_sdp->sdp_session[other_role] != NULL) {
								local_cnt_disp = &(qos_sdp->sdp_session[other_role]->cnt_disp);
								received_cnt_disp = &(session->cnt_disp);
								if (local_cnt_disp->len == received_cnt_disp->len) {
									if (local_cnt_disp->len == 0) {
										LM_DBG("no cnt disp header ... => %p\n", qos_sdp);
										*_qos_sdp = qos_sdp;
										return PENDING_MATCH;
									} else if (0==strncmp(local_cnt_disp->s, received_cnt_disp->s, local_cnt_disp->len)) {
										LM_DBG("'%.*s' => %p\n", local_cnt_disp->len, local_cnt_disp->s, qos_sdp);
										*_qos_sdp = qos_sdp;
										return PENDING_MATCH;
									}
								} else if (received_cnt_disp->len == 0 && local_cnt_disp->len == 7 &&
									0==strncmp(local_cnt_disp->s, "session", 7)) {
									/* We have an offer with cnt_disp='session' and an answer with cnt_disp='' */
									*_qos_sdp = qos_sdp;
									return PENDING_MATCH;
								}
							} else {
								LM_ERR("skipping search for null sdp for %s\n", (other_role==QOS_CALLER)?"QOS_CALLER":"QOS_CALLEE");
							}
						}
						qos_sdp = qos_sdp->next;
					}
					/* searching into the negotiated_sdp list */
					qos_sdp = qos_ctx->negotiated_sdp;
					LM_DBG("searching the negotiated_sdp: %p\n", qos_sdp);
					while (qos_sdp) {
						//print_qos_sdp(qos_sdp);
						if (cseq_method_id == qos_sdp->method_id && dir != qos_sdp->method_dir &&
							qos_sdp->negotiation == N_INVITE_200OK && cseq_number->len == qos_sdp->cseq.len &&
							0 == strncmp(cseq_number->s, qos_sdp->cseq.s, cseq_number->len)) {
							LM_DBG("method_id, dir and cseq match with previous session %p\n", qos_sdp->sdp_session[other_role]);
							if (qos_sdp->sdp_session[other_role] != NULL) {
								local_cnt_disp = &(qos_sdp->sdp_session[other_role]->cnt_disp);
								received_cnt_disp = &(session->cnt_disp);
								if (local_cnt_disp->len == received_cnt_disp->len) {
									if (local_cnt_disp->len == 0) {
										LM_DBG("no cnt disp header ... => %p\n", qos_sdp);
										*_qos_sdp = qos_sdp;
										return NEGOTIATED_MATCH;
									} else if (0==strncmp(local_cnt_disp->s, received_cnt_disp->s, local_cnt_disp->len)) {
										LM_DBG("'%.*s' => %p\n", local_cnt_disp->len, local_cnt_disp->s, qos_sdp);
										*_qos_sdp = qos_sdp;
										return NEGOTIATED_MATCH;
									}
								} else if (received_cnt_disp->len == 0 && local_cnt_disp->len == 7 &&
									0==strncasecmp(local_cnt_disp->s, "session", 7)) {
									/* We have an offer with cnt_disp='session' and an answer with cnt_disp='' */
									*_qos_sdp = qos_sdp;
									return NEGOTIATED_MATCH;
								}
							} else {
								LM_ERR("skipping search for null sdp for %s\n", (other_role==QOS_CALLER)?"QOS_CALLER":"QOS_CALLEE");
							}
						}
						qos_sdp = qos_sdp->next;
					}
					return NO_INVITE_RESP_MATCH;
					break;
				case METHOD_UPDATE:
					LM_ERR("FIXME\n");
					return NO_UPDATE_RESP_MATCH;
					break;
				default:
					LM_ERR("Unexpected reply for method id %d\n", cseq_method_id);
					return ERROR_MATCH;
			}
			break;
		default:
			LM_ERR("Unknown SIP message type: %d\n", _m->first_line.type);
			return ERROR_MATCH;
	}
	LM_ERR("FIXME: out of case\n");
	return ERROR_MATCH;
}

void link_pending_qos_sdp(qos_ctx_t *qos_ctx, qos_sdp_t *qos_sdp)
{
	if (qos_sdp->prev != NULL) LM_ERR("got qos_sdp->prev=%p\n", qos_sdp->prev);
	if (qos_sdp->next != NULL) LM_ERR("got qos_sdp->next=%p\n", qos_sdp->next);

	if (qos_ctx->pending_sdp) {
		LM_DBG("Adding pending qos_sdp: %p\n", qos_sdp);
		if (qos_ctx->pending_sdp->prev != NULL) LM_ERR("got qos_ctx->pending_sdp->prev=%p\n", qos_ctx->pending_sdp->prev);
		qos_sdp->next = qos_ctx->pending_sdp;
		qos_ctx->pending_sdp->prev = qos_sdp;
		qos_ctx->pending_sdp = qos_sdp;
	} else {
		LM_DBG("Inserting pending qos_sdp: %p\n", qos_sdp);
		qos_ctx->pending_sdp = qos_sdp;
	}
}

void unlink_pending_qos_sdp(qos_ctx_t *qos_ctx, qos_sdp_t *qos_sdp)
{
	if (qos_sdp->next)
		qos_sdp->next->prev = qos_sdp->prev;

	if (qos_sdp->prev)
		qos_sdp->prev->next = qos_sdp->next;
	else
		qos_ctx->pending_sdp = qos_sdp->next;

	qos_sdp->next = qos_sdp->prev = NULL;
}
void unlink_negotiated_qos_sdp(qos_ctx_t *qos_ctx, qos_sdp_t *qos_sdp)
{
	if (qos_sdp->next)
		qos_sdp->next->prev = qos_sdp->prev;

	if (qos_sdp->prev)
		qos_sdp->prev->next = qos_sdp->next;
	else
		qos_ctx->negotiated_sdp = qos_sdp->next;

	qos_sdp->next = qos_sdp->prev = NULL;
}


void link_negotiated_qos_sdp_and_run_cb(qos_ctx_t *qos_ctx, qos_sdp_t *qos_sdp, unsigned int role, struct sip_msg *_m)
{
	qos_sdp_t *next_qos_sdp;
	qos_sdp_t *temp_qos_sdp = qos_ctx->negotiated_sdp;

	if (qos_sdp->prev != NULL) LM_ERR("got qos_sdp->prev=%p\n", qos_sdp->prev);
	if (qos_sdp->next != NULL) LM_ERR("got qos_sdp->next=%p\n", qos_sdp->next);

	if (temp_qos_sdp) {
		while (temp_qos_sdp) {
			next_qos_sdp = temp_qos_sdp->next;
			if (qos_sdp->negotiation == temp_qos_sdp->negotiation) {
				LM_DBG("run_qos_callbacks(QOSCB_REMOVE_SDP, qos_ctx=%p, temp_qos_sdp=%p, role=%d, _m=%p)\n",
					qos_ctx, temp_qos_sdp, role, _m);
				run_qos_callbacks(QOSCB_REMOVE_SDP, qos_ctx, temp_qos_sdp, role, _m);

				unlink_negotiated_qos_sdp(qos_ctx, temp_qos_sdp);
				destroy_qos(temp_qos_sdp);
				break;
			}
			temp_qos_sdp = next_qos_sdp;
		}
		if (qos_ctx->negotiated_sdp) {
			LM_DBG("Adding negotiated qos_sdp: %p\n", qos_sdp);
			if (qos_ctx->negotiated_sdp->prev != NULL) LM_ERR("got qos_ctx->negotiated_sdp->prev=%p\n", qos_ctx->negotiated_sdp->prev);
			qos_sdp->next = qos_ctx->negotiated_sdp;
			qos_ctx->negotiated_sdp->prev = qos_sdp;
			qos_ctx->negotiated_sdp = qos_sdp;
		} else {
			LM_DBG("Inserting negotiated qos_sdp: %p\n", qos_sdp);
			qos_ctx->negotiated_sdp = qos_sdp;
		}
	} else {
		LM_DBG("Inserting first negotiated qos_sdp: %p\n", qos_sdp);
		qos_ctx->negotiated_sdp = qos_sdp;
	}

	LM_DBG("run_qos_callbacks(QOSCB_UPDATE_SDP, qos_ctx=%p, qos_sdp=%p, role=%d, _m=%p)\n",
		qos_ctx, qos_sdp, role, _m);
	run_qos_callbacks(QOSCB_UPDATE_SDP, qos_ctx, qos_sdp, role, _m);
}

int add_pending_sdp_session(qos_ctx_t *qos_ctx, unsigned int dir, str *cseq_number, str *cseq_method, int cseq_method_id,
				unsigned int role, unsigned int negotiation, sdp_session_cell_t *session, struct sip_msg *_m)
{
	unsigned int len;
	sdp_session_cell_t *cloned_session;
	qos_sdp_t *qos_sdp;
	char *p;

	len = sizeof(qos_sdp_t) + cseq_method->len + cseq_number->len;
	qos_sdp = (qos_sdp_t *)shm_malloc(len);
	LM_DBG("alloc qos_sdp: %p\n", qos_sdp);
	if (qos_sdp==NULL) {
		LM_ERR("oom %d\n", len);
		return -1;
	} else {
		memset(qos_sdp, 0, len);
		LM_DBG("Allocated memory for qos_sdp: %p\n", qos_sdp);

		cloned_session = clone_sdp_session_cell(session);
		if (cloned_session==NULL) {
			shm_free(qos_sdp);
			LM_DBG("free qos_sdp: %p\n", qos_sdp);
			return -1;
		}
		qos_sdp->sdp_session[role] = cloned_session;

		LM_DBG("qos_sdp->sdp_session[%d]=%p\n", role, qos_sdp->sdp_session[role]);

		if (_m->first_line.type == SIP_REQUEST) {
			qos_sdp->method_dir = dir;
		} else {
			/* This is a SIP_REPLY and we need to set
			 * the direction for the SIP_REQUEST */
			if (dir==DLG_DIR_UPSTREAM)
				qos_sdp->method_dir = DLG_DIR_DOWNSTREAM;
			else
				qos_sdp->method_dir = DLG_DIR_UPSTREAM;
		}
		qos_sdp->method_id = cseq_method_id;
		qos_sdp->negotiation = negotiation;
		p = (char*)(qos_sdp+1);

		qos_sdp->method.s = p;
		qos_sdp->method.len = cseq_method->len;
		memcpy( p, cseq_method->s, cseq_method->len);
		p += cseq_method->len;

		qos_sdp->cseq.s = p;
		qos_sdp->cseq.len = cseq_number->len;
		memcpy( p,cseq_number->s, cseq_number->len);
		/* p += cseq_number->len; */

		link_pending_qos_sdp(qos_ctx, qos_sdp);

		LM_DBG("run_qos_callbacks(QOSCB_ADD_SDP, qos_ctx=%p, qos_sdp=%p, role=%d, _m=%p)\n",
			qos_ctx, qos_sdp, role, _m);
		run_qos_callbacks(QOSCB_ADD_SDP, qos_ctx, qos_sdp, role, _m);
	}
	return 0;
}




/*
 * Add the sdp carried by the given SIP message into the qos context.
 */
void add_sdp(qos_ctx_t *qos_ctx, unsigned int dir, struct sip_msg *_m, unsigned int role, unsigned int other_role)
{
	qos_sdp_t *qos_sdp;
	sdp_session_cell_t *recv_session;
	str *cseq_number, *cseq_method;
	int cseq_method_id, sdp_match;

	if ( (!_m->cseq && parse_headers(_m,HDR_CSEQ_F,0)<0) || !_m->cseq || !_m->cseq->parsed) {
		LM_ERR("bad sip message or missing CSeq hdr\n");
		return;
	}

	cseq_number = &((get_cseq(_m))->number);
	cseq_method = &((get_cseq(_m))->method);
	cseq_method_id = (get_cseq(_m))->method_id;

	LM_DBG("cseq=`%.*s' `%.*s' and dir=%d\n",
		cseq_number->len, cseq_number->s,
		cseq_method->len, cseq_method->s, dir);

	/* Let's iterate through all the received sessions */
	recv_session = ((sdp_info_t*)_m->body)->sessions;
	while(recv_session) {
		qos_sdp = NULL;
		sdp_match = find_qos_sdp(qos_ctx, dir, other_role, cseq_number, cseq_method_id, recv_session, _m, &qos_sdp);

		switch (sdp_match) {
			case NO_INVITE_REQ_MATCH:
				if (0!=add_pending_sdp_session( qos_ctx, dir, cseq_number, cseq_method, cseq_method_id, role, N_INVITE_200OK, recv_session, _m)) {
					LM_ERR("Unable to add new sdp session\n");
					goto error;
				}

				break;
			case NO_INVITE_RESP_MATCH:
				if (0!=add_pending_sdp_session( qos_ctx, dir, cseq_number, cseq_method, cseq_method_id, role, N_200OK_ACK, recv_session, _m)) {
					LM_ERR("Unable to add new sdp session\n");
					goto error;
				}

				break;
			case ERROR_MATCH:
			case NO_ACK_REQ_MATCH:
			case NO_UPDATE_REQ_MATCH:
			case NO_UPDATE_RESP_MATCH:
				LM_ERR("error match: %d\n", sdp_match);
				break;
			case PENDING_MATCH:
				LM_DBG("we have a pending match: %p\n", qos_sdp);
				/* Let's save the received session */
				qos_sdp->sdp_session[role] = clone_sdp_session_cell(recv_session);
				if (qos_sdp->sdp_session[role] == NULL) {
					LM_ERR("PENDING_MATCH:oom: Unable to add new sdp session\n");
					return;
				}

				/* Negotiation completed, need to move the established SDP into the negotiated_sdp */

				/* removing qos_sdp from qos_ctx->pending_sdp list */
				unlink_pending_qos_sdp(qos_ctx, qos_sdp);
				/* inserting qos_sdp into the qos_ctx->negotiated_sdp list */
				link_negotiated_qos_sdp_and_run_cb(qos_ctx, qos_sdp, role, _m);

				break;
			case NEGOTIATED_MATCH:
				LM_DBG("we have a negotiated match: %p\n", qos_sdp);
				/* some sanity checks */
				if (qos_sdp->sdp_session[role]) {
					free_cloned_sdp_session(qos_sdp->sdp_session[role]);
				} else {
					LM_ERR("missing sdp_session for %s\n", (role==QOS_CALLER)?"QOS_CALLER":"QOS_CALLEE");
				}
				/* Let's save the received session */
				qos_sdp->sdp_session[role] = clone_sdp_session_cell(recv_session);
				if (qos_sdp->sdp_session[role]  == NULL) {
					LM_ERR("NEGOTIATED_MATCH:oom: Unable to add new sdp session\n");
					return;
				}

				LM_DBG("run_qos_callbacks(QOSCB_UPDATE_SDP, qos_ctx=%p, qos_sdp=%p, role=%d, _m=%p)\n",
					qos_ctx, qos_sdp, role, _m);
				run_qos_callbacks(QOSCB_UPDATE_SDP, qos_ctx, qos_sdp, role, _m);

				break;
			default:
				LM_CRIT("Undefined return code from find_qos_sdp(): %d\n", sdp_match);
		}
		recv_session = recv_session->next;
	}

	return;
error:
	shm_free(qos_sdp);
	LM_DBG("free qos_sdp: %p\n", qos_sdp);
	return;
}

/*
 * Remove the sdp previously added.
 */
void remove_sdp(qos_ctx_t *qos_ctx, unsigned int dir, struct sip_msg *_m, unsigned int role, unsigned int other_role)
{
	str *cseq_number;
	int cseq_method_id;
	qos_sdp_t *qos_sdp;

	if ( (!_m->cseq && parse_headers(_m,HDR_CSEQ_F,0)<0) || !_m->cseq || !_m->cseq->parsed) {
		LM_ERR("bad sip message or missing CSeq hdr\n");
		return;
	}

	cseq_number = &((get_cseq(_m))->number);
	cseq_method_id = (get_cseq(_m))->method_id;

	if (_m->first_line.type == SIP_REPLY) {
		switch (cseq_method_id) {
			case METHOD_INVITE:
			case METHOD_UPDATE:
					/* searching into the pending_sdp list only */
					qos_sdp = qos_ctx->pending_sdp;
					while (qos_sdp) {
						qos_sdp = qos_sdp->next;
						if (cseq_method_id == qos_sdp->method_id && dir != qos_sdp->method_dir &&
							qos_sdp->negotiation == N_INVITE_200OK && cseq_number->len == qos_sdp->cseq.len &&
							0 == strncmp(cseq_number->s, qos_sdp->cseq.s, cseq_number->len)) {
							LM_DBG("method_id, dir and cseq match with previous session %p->%p\n",
								qos_sdp, qos_sdp->sdp_session[other_role]);
							/* print_sdp_session(qos_sdp->sdp_session[other_role]); */
							if (qos_sdp->sdp_session[other_role] != NULL) {
								LM_DBG("run_qos_callbacks(QOSCB_REMOVE_SDP, qos_ctx=%p, qos_sdp=%p, role=%d, _m=%p)\n",
									qos_ctx, qos_sdp, role, _m);
								run_qos_callbacks(QOSCB_REMOVE_SDP, qos_ctx, qos_sdp, role, _m);
								unlink_negotiated_qos_sdp(qos_ctx, qos_sdp);
								/* Here we free up the pending qos_sdp */
								destroy_qos(qos_sdp);
								continue;
							} else {
								LM_ERR("skipping search for null sdp for %s\n", (other_role==QOS_CALLER)?"QOS_CALLER":"QOS_CALLEE");
							}
						}
					} /* end while (qos_sdp) */
				break;
			default:
				LM_ERR("Unexpected method id %d\n", cseq_method_id);
		}
	} else {
		LM_ERR("we remove sdp only for a SIP_REPLY, not for a %d\n",
			_m->first_line.type);
	}

	return;
}

void destroy_qos_ctx(qos_ctx_t *qos_ctx)
{
	qos_sdp_t * qos_sdp, * next_qos_sdp;

	lock_get(&qos_ctx->lock);

	qos_sdp = qos_ctx->pending_sdp;
	while (qos_sdp) {
		next_qos_sdp = qos_sdp->next;
		destroy_qos(qos_sdp);
		qos_sdp = next_qos_sdp;
	}
	qos_sdp = qos_ctx->negotiated_sdp;
	while (qos_sdp) {
		next_qos_sdp = qos_sdp->next;
		destroy_qos(qos_sdp);
		qos_sdp = next_qos_sdp;
	}

	lock_release(&qos_ctx->lock);

	lock_destroy(&qos_ctx->lock);

	LM_DBG("free qos_ctx: %p\n", qos_ctx);
	shm_free(qos_ctx);

	return;
}

