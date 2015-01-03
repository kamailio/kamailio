/*
 * Copyright (C) 2001-2003 FhG Fokus
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


#include <stdio.h>
#include "select.h"
#include "h_table.h"
#include "t_lookup.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../select.h"
#include "../../select_buf.h"
#include "../../parser/msg_parser.h"

#define RETURN0_res(x) {*res=(x);return 0;}

inline static int select_tm_get_cell(struct sip_msg* msg, int *branch, struct cell **t) {

	/* make sure we know the associated transaction ... */
	if (t_check( msg  , branch )==-1)  /* it's not necessary whan calling from script because already done */ 
		return -1;

	/*... if there is none, tell the core router to fwd statelessly */
	*t = get_t();
	if ( (*t==0)||(*t==T_UNDEFINED)) return -1;

	return 0;
}	

static int select_tm(str* res, select_t* s, struct sip_msg* msg) {
	int branch;
	struct cell *t;
	if (select_tm_get_cell(msg, &branch, &t) < 0) {
		res->s = "0";
	}
	else {
		res->s = "1";
	}
	res->len = 1;
	return 0;
}

#define SEL_BRANCH_POS 2
#define BRANCH_NO(_s_) (_s_->params[SEL_BRANCH_POS].v.i)

#define SELECT_check(msg) \
	int branch; \
	struct cell *t; \
	if (select_tm_get_cell(msg, &branch, &t) < 0) return -1;

#define SELECT_check_branch(_s_, _msg_) \
	SELECT_check(_msg_); \
	if (BRANCH_NO(_s_) >=t->nr_of_outgoings) return -1;
 

/* string resides in shared memory but I think it's not worth copying to
 * static buffer (str_to_static_buffer) as minimal probability that string 
 * is changed by other process (or cell is already locked ?)
 */
 
static int select_tm_method(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check(msg);
	RETURN0_res(t->method);
}

static ABSTRACT_F(select_tm_uas);

static int select_tm_uas_status(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check(msg);
	return int_to_static_buffer(res, t->uas.status);
}

/* transaction cell has request in sip_msg structure which brings idea to
 * use selects from select_core.c (e.g. to avoid copy&paste automatically 
 * generate select definitions in tm_init_selects()). But it's not so easy
 * as select may perform any parsing which is stored in sip_msg structure.
 * But transaction cell resides in shared memory while parsing is done in
 * private memory. Therefore we support currently only complete request */
 
static int select_tm_uas_request(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check(msg);
	res->s = t->uas.request->buf;
	res->len = t->uas.request->len;
	return 0; 
}

static int select_tm_uas_local_to_tag(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check(msg);
	RETURN0_res(t->uas.local_totag);
}

static int select_tm_uas_response(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check(msg);
	res->s = t->uas.response.buffer;	
	res->len = t->uas.response.buffer_len;
	return 0;	
}

/* TODO: implement a general select function that works with any
 * kind of requests not only with negative ACKs
 */
static int select_tm_uas_request_neg_ack_retransmission(str* res, select_t* s, struct sip_msg* msg) {
        int rv;

	SELECT_check(msg);
        rv = ((msg->REQ_METHOD == METHOD_ACK)
		&& (t->uas.status >= 300)
		/* Misuse the timer flag of the 200 retransmission buffer
		 * to check whether or not this is an ACK retransmission.
		 * Warning: this check is not 100% fail-safe because two
		 * ACKs can be processed in parallel and none of them
		 * may be considered a retransmission - Miklos */
		&& (t->uas.response.t_active == 0)) ? 1 : -1;

        return int_to_static_buffer(res, rv);
}


static ABSTRACT_F(select_tm_uac);

static int select_tm_uac_count(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check(msg);
	return int_to_static_buffer(res, t->nr_of_outgoings);
}

static int select_tm_uac_relayed(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check(msg);
	return int_to_static_buffer(res, t->relayed_reply_branch);
}

static ABSTRACT_F(select_tm_uac_branch);

/**
 * Get last_status from current branch. Helper function.
 * @see selects select_tm_uac_last_status
 * @see select_tm_uac_response_retransmission
 */
static int get_last_status(struct sip_msg* msg, int *last_status)
{
	unsigned int branch;
	char *bptr;
	int blen;
	struct cell *t;

/*	DBG("select_tm_uac_last_status: branch param name: '%.*s', value: '%.*s'\n",
			msg->via1->branch->name.len,
			msg->via1->branch->name.s,
			msg->via1->branch->value.len,
			msg->via1->branch->value.s);
*/
	
	/* branch ID consist of MAGIC '.' HASHID '.'  BRANCH_ID */
	blen = 0;
	for (bptr = msg->via1->branch->value.s + msg->via1->branch->value.len - 1;
	     bptr != msg->via1->branch->value.s;
	     bptr--, blen++)
	{
		if (*bptr == '.') break;
	}
	bptr++;
	/* we have a pointer to the branch number */
/*	DBG("branch number: '%.*s'\n", blen, bptr); */
	if (reverse_hex2int(bptr, blen, &branch) < 0) {
		ERR("Wrong branch number in Via1 branch param\n");
		return -1;
	}
	
	t = get_t();
	if ( (t == NULL) || (t == T_UNDEFINED) ) {
		ERR("get_last_status: no transaction\n");
		return -1;
	}

/*	DBG("select_tm_uac_last_status: branch = %d\n", branch); */
	*last_status = t->uac[branch].last_received;
	return 1;
}
/**
 * Get last status in current branch.
 */
static int select_tm_uac_last_status(str* res, select_t* s, struct sip_msg* msg) {
	int last_status;
	if (get_last_status(msg, &last_status) < 0) return -1;
	return int_to_static_buffer(res, last_status);
}
/**
 * Comparison function which compares current status and last status
 * in current branch.
 * It is a simplified method how to detect incoming retransmited responses.
 * @return  1 if @status is less or equal @tm.uac.last_status (retransmited response)
 *          otherwise returns -1 (not retransmited response).
 * @see get_last_status
 */
static int select_tm_uac_response_retransmission(str* res, select_t* s, struct sip_msg* msg) {
	int last_status, rv;
	if (get_last_status(msg, &last_status) < 0) return -1;
	rv = msg->first_line.u.reply.statuscode <= last_status ? 1 : -1;

/*	DBG("select_tm_uac_response_retransmission: %d\n", rv); */
	return int_to_static_buffer(res, rv);
}

static int select_tm_uac_status(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check_branch(s, msg);
	return int_to_static_buffer(res, t->uac[BRANCH_NO(s)].last_received);
}

static int select_tm_uac_uri(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check_branch(s, msg);
	RETURN0_res(t->uac[BRANCH_NO(s)].uri);
}

/* see select_tm_uas_request comments */
static int select_tm_uac_response(str* res, select_t* s, struct sip_msg* msg) {  // struct
	SELECT_check_branch(s, msg);
	if (t->uac[BRANCH_NO(s)].reply) {
		res->s = t->uac[BRANCH_NO(s)].reply->buf;
		res->len = t->uac[BRANCH_NO(s)].reply->len;
		return 0;
	}
	else
		return -1;
}

static int select_tm_uac_branch_request(str* res, select_t* s, struct sip_msg* msg) {
	SELECT_check_branch(s, msg);
	res->s = t->uac[BRANCH_NO(s)].request.buffer;	
	res->len = t->uac[BRANCH_NO(s)].request.buffer_len;
	return 0;	
}

static select_row_t select_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("tm"), select_tm, 0},
	{ select_tm, SEL_PARAM_STR, STR_STATIC_INIT("method"), select_tm_method, 0},
	{ select_tm, SEL_PARAM_STR, STR_STATIC_INIT("uas"), select_tm_uas, SEL_PARAM_EXPECTED},
	{ select_tm_uas, SEL_PARAM_STR, STR_STATIC_INIT("status"), select_tm_uas_status, 0},
	{ select_tm_uas, SEL_PARAM_STR, STR_STATIC_INIT("request"), select_tm_uas_request, 0},
	{ select_tm_uas, SEL_PARAM_STR, STR_STATIC_INIT("req"), select_tm_uas_request, 0},
	{ select_tm_uas, SEL_PARAM_STR, STR_STATIC_INIT("local_to_tag"), select_tm_uas_local_to_tag, 0},
	{ select_tm_uas, SEL_PARAM_STR, STR_STATIC_INIT("response"), select_tm_uas_response, 0},
	{ select_tm_uas, SEL_PARAM_STR, STR_STATIC_INIT("resp"), select_tm_uas_response, 0},
	{ select_tm_uas_request, SEL_PARAM_STR, STR_STATIC_INIT("neg_ack_retransmission"), select_tm_uas_request_neg_ack_retransmission, 0},

	{ select_tm, SEL_PARAM_STR, STR_STATIC_INIT("uac"), select_tm_uac, SEL_PARAM_EXPECTED},
	{ select_tm_uac, SEL_PARAM_STR, STR_STATIC_INIT("count"), select_tm_uac_count, 0},
	{ select_tm_uac, SEL_PARAM_STR, STR_STATIC_INIT("relayed"), select_tm_uac_relayed, 0},
	{ select_tm_uac, SEL_PARAM_STR, STR_STATIC_INIT("last_status"), select_tm_uac_last_status, 0},  /* last status of current branch */
	{ select_tm_uac, SEL_PARAM_STR, STR_STATIC_INIT("response_retransmission"), select_tm_uac_response_retransmission, 0},  /* last status of current branch */
	{ select_tm_uac, SEL_PARAM_INT, STR_NULL, select_tm_uac_branch, SEL_PARAM_EXPECTED},
	{ select_tm_uac_branch, SEL_PARAM_STR, STR_STATIC_INIT("status"), select_tm_uac_status, 0},
	{ select_tm_uac_branch, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_tm_uac_uri, 0},
	{ select_tm_uac_branch, SEL_PARAM_STR, STR_STATIC_INIT("response"), select_tm_uac_response, 0},
	{ select_tm_uac_branch, SEL_PARAM_STR, STR_STATIC_INIT("resp"), select_tm_uac_response, 0},
	{ select_tm_uac_branch, SEL_PARAM_STR, STR_STATIC_INIT("request"), select_tm_uac_branch_request, 0},
	{ select_tm_uac_branch, SEL_PARAM_STR, STR_STATIC_INIT("req"), select_tm_uac_branch_request, 0},

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

int tm_init_selects() {
	register_select_table(select_declaration);
	return 0;
}
