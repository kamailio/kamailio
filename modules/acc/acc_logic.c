/*
 * $Id$
 * 
 * Accounting module logic
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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
 * History:
 * -------
 * 2006-09-19  forked from the acc_mod.c file during a big re-structuring
 *             of acc module (bogdan)
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Logic
 *
 * - Module: \ref acc
 */

#include <stdio.h>
#include <string.h>

#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../modules/tm/tm_load.h"
#include "../rr/api.h"
#include "../../flags.h"
#include "acc.h"
#include "acc_api.h"
#include "acc_mod.h"
#include "acc_logic.h"

extern struct tm_binds tmb;
extern struct rr_binds rrb;

struct acc_enviroment acc_env;


#define is_acc_flag_set(_rq,_flag)  (((_flag) != -1) && (isflagset((_rq), (_flag)) == 1))
#define reset_acc_flag(_rq,_flag)   (resetflag((_rq), (_flag)))

#define is_failed_acc_on(_rq)  is_acc_flag_set(_rq,failed_transaction_flag)

#define is_log_acc_on(_rq)     is_acc_flag_set(_rq,log_flag)
#define is_log_mc_on(_rq)      is_acc_flag_set(_rq,log_missed_flag)

#ifdef SQL_ACC
	#define is_db_acc_on(_rq)     is_acc_flag_set(_rq,db_flag)
	#define is_db_mc_on(_rq)      is_acc_flag_set(_rq,db_missed_flag)
#else
	#define is_db_acc_on(_rq)     (0)
	#define is_db_mc_on(_rq)      (0)
#endif

#ifdef RAD_ACC
	#define is_rad_acc_on(_rq)     is_acc_flag_set(_rq,radius_flag)
	#define is_rad_mc_on(_rq)      is_acc_flag_set(_rq,radius_missed_flag)
#else
	#define is_rad_acc_on(_rq)     (0)
	#define is_rad_mc_on(_rq)      (0)
#endif


#ifdef DIAM_ACC
	#define is_diam_acc_on(_rq)     is_acc_flag_set(_rq,diameter_flag)
	#define is_diam_mc_on(_rq)      is_acc_flag_set(_rq,diameter_missed_flag)
#else
	#define is_diam_acc_on(_rq)     (0)
	#define is_diam_mc_on(_rq)      (0)
#endif

#define is_acc_on(_rq) \
	( (is_log_acc_on(_rq)) || (is_db_acc_on(_rq)) \
	|| (is_rad_acc_on(_rq)) || (is_diam_acc_on(_rq)) )

#define is_mc_on(_rq) \
	( (is_log_mc_on(_rq)) || (is_db_mc_on(_rq)) \
	|| (is_rad_mc_on(_rq)) || (is_diam_mc_on(_rq)) )

#define skip_cancel(_rq) \
	(((_rq)->REQ_METHOD==METHOD_CANCEL) && report_cancels==0)

#define is_acc_prepare_on(_rq) \
	(is_acc_flag_set(_rq,acc_prepare_flag))

static void tmcb_func( struct cell* t, int type, struct tmcb_params *ps );


static inline struct hdr_field* get_rpl_to( struct cell *t,
														struct sip_msg *reply)
{
	if (reply==FAKED_REPLY || !reply || !reply->to)
		return t->uas.request->to;
	else
		return reply->to;
}


static inline void env_set_to(struct hdr_field *to)
{
	acc_env.to = to;
}


static inline void env_set_text(char *p, int len)
{
	acc_env.text.s = p;
	acc_env.text.len = len;
}


static inline void env_set_code_status( int code, struct sip_msg *reply)
{
	static char code_buf[INT2STR_MAX_LEN];
	str reason = {"Reason", 6};
	struct hdr_field *hf;

	acc_env.code = code;
	if (reply==FAKED_REPLY || reply==NULL) {
		/* code */
		acc_env.code_s.s =
			int2bstr((unsigned long)code, code_buf, &acc_env.code_s.len);
		/* reason */
		acc_env.reason.s = error_text(code);
		acc_env.reason.len = strlen(acc_env.reason.s);
	} else {
		acc_env.code_s = reply->first_line.u.reply.status;
		hf = NULL;
	        if (reason_from_hf) {
			/* TODO: take reason from all Reason headers */
			if(parse_headers(reply, HDR_EOH_F, 0) < 0) {
				LM_ERR("error parsing headers\n");
			} else {
				for (hf=reply->headers; hf; hf=hf->next) {
					if (cmp_hdrname_str(&hf->name, &reason)==0)
						break;
				}
			}
		}
		if (hf == NULL) {
			acc_env.reason = reply->first_line.u.reply.reason;
		} else {
			acc_env.reason = hf->body;
		}
	}
}


static inline void env_set_comment(struct acc_param *accp)
{
	acc_env.code = accp->code;
	acc_env.code_s = accp->code_s;
	acc_env.reason = accp->reason;
}


static inline int acc_preparse_req(struct sip_msg *req)
{
	if ( (parse_headers(req,HDR_CALLID_F|HDR_CSEQ_F|HDR_FROM_F|HDR_TO_F,0)<0)
	|| (parse_from_header(req)<0 ) ) {
		LM_ERR("failed to preparse request\n");
		return -1;
	}
	return 0;
}

int acc_parse_code(char *p, struct acc_param *param)
{
	if (p==NULL||param==NULL)
		return -1;

	/* any code? */
	if (param->reason.len>=3 && isdigit((int)p[0])
	&& isdigit((int)p[1]) && isdigit((int)p[2]) ) {
		param->code = (p[0]-'0')*100 + (p[1]-'0')*10 + (p[2]-'0');
		param->code_s.s = p;
		param->code_s.len = 3;
		param->reason.s += 3;
		for( ; isspace((int)param->reason.s[0]) ; param->reason.s++ );
		param->reason.len = strlen(param->reason.s);
	}
	return 0;
}

int acc_get_param_value(struct sip_msg *rq, struct acc_param *param)
{
	if(param->elem!=NULL) {
		if(pv_printf_s(rq, param->elem, &param->reason)==-1) {
			LM_ERR("Can't get value for %.*s\n", param->reason.len, param->reason.s);
			return -1;
		}
		if(acc_parse_code(param->reason.s, param)<0)
		{
			LM_ERR("Can't parse code\n");
			return -1;
		}
	}
	return 0;
}

int w_acc_log_request(struct sip_msg *rq, char *comment, char *foo)
{
	struct acc_param *param = (struct acc_param*)comment;
	if (acc_preparse_req(rq)<0)
		return -1;
	if(acc_get_param_value(rq, param)<0)
		return -1;
	env_set_to( rq->to );
	env_set_comment(param);
	env_set_text( ACC_REQUEST, ACC_REQUEST_LEN);
	return acc_log_request(rq);
}


#ifdef SQL_ACC
int acc_db_set_table_name(struct sip_msg *msg, void *param, str *table)
{
#define DB_TABLE_NAME_SIZE	64
	static char db_table_name_buf[DB_TABLE_NAME_SIZE];
	str dbtable;

	if(param!=NULL) {
		if(get_str_fparam(&dbtable, msg, (fparam_t*)param)<0) {
			LM_ERR("cannot get acc db table name\n");
			return -1;
		}
		if(dbtable.len>=DB_TABLE_NAME_SIZE) {
			LM_ERR("acc db table name too big [%.*s] max %d\n",
					dbtable.len, dbtable.s, DB_TABLE_NAME_SIZE);
			return -1;
		}
		strncpy(db_table_name_buf, dbtable.s, dbtable.len);
		db_table_name_buf[dbtable.len] = '\0';
		env_set_text(db_table_name_buf, dbtable.len);
	} else {
		if(table==NULL) {
			LM_ERR("no acc table name\n");
			return -1;
		}
		env_set_text(table->s, table->len);
	}
	return 0;
}


int w_acc_db_request(struct sip_msg *rq, char *comment, char *table)
{
	struct acc_param *param = (struct acc_param*)comment;
	if (!table) {
		LM_ERR("db support not configured\n");
		return -1;
	}
	if (acc_preparse_req(rq)<0)
		return -1;
	if(acc_db_set_table_name(rq, (void*)table, NULL)<0) {
		LM_ERR("cannot set table name\n");
		return -1;
	}
	if(acc_get_param_value(rq, param)<0)
		return -1;
	env_set_to( rq->to );
	env_set_comment(param);
	return acc_db_request(rq);
}
#endif


#ifdef RAD_ACC
int w_acc_rad_request(struct sip_msg *rq, char *comment, char *foo)
{
	struct acc_param *param = (struct acc_param*)comment;
	if (acc_preparse_req(rq)<0)
		return -1;
	if(acc_get_param_value(rq, param)<0)
		return -1;
	env_set_to( rq->to );
	env_set_comment(param);
	return acc_rad_request(rq);
}
#endif


#ifdef DIAM_ACC
int w_acc_diam_request(struct sip_msg *rq, char *comment, char *foo)
{
	struct acc_param *param = (struct acc_param*)comment;
	if (acc_preparse_req(rq)<0)
		return -1;
	if(acc_get_param_value(rq, param)<0)
		return -1;
	env_set_to( rq->to );
	env_set_comment(param);
	return acc_diam_request(rq);
}
#endif



/* prepare message and transaction context for later accounting */
void acc_onreq( struct cell* t, int type, struct tmcb_params *ps )
{
	int tmcb_types;
	int is_invite;

	if ( ps->req && !skip_cancel(ps->req) &&
			( is_acc_on(ps->req) || is_mc_on(ps->req)
				|| is_acc_prepare_on(ps->req) ) ) {
		/* do some parsing in advance */
		if (acc_preparse_req(ps->req)<0)
			return;
		is_invite = (ps->req->REQ_METHOD==METHOD_INVITE)?1:0;
		/* install additional handlers */
		tmcb_types =
			/* report on completed transactions */
			TMCB_RESPONSE_OUT |
			/* account e2e acks if configured to do so */
			((report_ack && is_acc_on(ps->req))?TMCB_E2EACK_IN:0) |
			/* get incoming replies ready for processing */
			TMCB_RESPONSE_IN |
			/* report on missed calls */
			((is_invite && (is_mc_on(ps->req)
					|| is_acc_prepare_on(ps->req)))?TMCB_ON_FAILURE:0);
		if (tmb.register_tmcb( 0, t, tmcb_types, tmcb_func, 0, 0 )<=0) {
			LM_ERR("cannot register additional callbacks\n");
			return;
		}
		/* if required, determine request direction */
		if( detect_direction && !rrb.is_direction(ps->req,RR_FLOW_UPSTREAM) ) {
			LM_DBG("detected an UPSTREAM req -> flaging it\n");
			ps->req->msg_flags |= FL_REQ_UPSTREAM;
		}
	}
}



/* is this reply of interest for accounting ? */
static inline int should_acc_reply(struct sip_msg *req, struct sip_msg *rpl,
				   int code)
{
    unsigned int i;

	/* negative transactions reported otherwise only if explicitly 
	 * demanded */

    if (code >= 300) {
	if (!is_failed_acc_on(req)) return 0;
	i = 0;
	while (failed_filter[i] != 0) {
	    if (failed_filter[i] == code) return 0;
	    i++;
	}
	return 1;
    }

    if ( !is_acc_on(req) )
	return 0;
	
    if ( code<200 && !(early_media &&
		       parse_headers(rpl,HDR_CONTENTLENGTH_F, 0) == 0 &&
		       rpl->content_length && get_content_length(rpl) > 0))
	return 0;

    return 1; /* seed is through, we will account this reply */
}



/* parse incoming replies before cloning */
static inline void acc_onreply_in(struct cell *t, struct sip_msg *req,
											struct sip_msg *reply, int code)
{
	/* don't parse replies in which we are not interested */
	/* missed calls enabled ? */
	if ( (reply && reply!=FAKED_REPLY) && (should_acc_reply(req,reply,code)
	|| (is_invite(t) && code>=300 && is_mc_on(req))) ) {
		parse_headers(reply, HDR_TO_F, 0 );
	}
}



/* initiate a report if we previously enabled MC accounting for this t */
static inline void on_missed(struct cell *t, struct sip_msg *req,
											struct sip_msg *reply, int code)
{
	str new_uri_bk = {0, 0};
	int flags_to_reset = 0;
	int br = -1;

	/* get winning branch index, if set */
	if (t->relayed_reply_branch>=0) {
		br = t->relayed_reply_branch;
	} else {
		if(code>=300) {
			br = tmb.t_get_picked_branch();
		}
	}
	/* set as new_uri the one from selected branch */
	if (br>=0) {
		new_uri_bk = req->new_uri;
		req->new_uri = t->uac[br].uri;
		req->parsed_uri_ok = 0;
	} else {
		new_uri_bk.len = -1;
		new_uri_bk.s = 0;
	}

	/* set env variables */
	env_set_to( get_rpl_to(t,reply) );
	env_set_code_status( code, reply);

	/* we report on missed calls when the first
	 * forwarding attempt fails; we do not wish to
	 * report on every attempt; so we clear the flags; 
	 */

	if (is_log_mc_on(req)) {
		env_set_text( ACC_MISSED, ACC_MISSED_LEN);
		acc_log_request( req );
		flags_to_reset |= log_missed_flag;
	}
#ifdef SQL_ACC
	if (is_db_mc_on(req)) {
		if(acc_db_set_table_name(req, db_table_mc_data, &db_table_mc)<0) {
			LM_ERR("cannot set missed call db table name\n");
			return;
		}
		acc_db_request( req );
		flags_to_reset |= db_missed_flag;
	}
#endif
#ifdef RAD_ACC
	if (is_rad_mc_on(req)) {
		acc_rad_request( req );
		flags_to_reset |= radius_missed_flag;
	}
#endif
/* DIAMETER */
#ifdef DIAM_ACC
	if (is_diam_mc_on(req)) {
		acc_diam_request( req );
		flags_to_reset |= diameter_missed_flag;
	}
#endif

	/* run extra acc engines */
	acc_run_engines(req, 1, &flags_to_reset);

	/* Reset the accounting missed_flags
	 * These can't be reset in the blocks above, because
	 * it would skip accounting if the flags are identical
	 */
	reset_acc_flag( req, flags_to_reset );

	if (new_uri_bk.len>=0) {
		req->new_uri = new_uri_bk;
		req->parsed_uri_ok = 0;
	}

}


extern int _acc_clone_msg;

/* initiate a report if we previously enabled accounting for this t */
static inline void acc_onreply( struct cell* t, struct sip_msg *req,
											struct sip_msg *reply, int code)
{
	str new_uri_bk;
	int br = -1;
	hdr_field_t *hdr;
	sip_msg_t tmsg;
	sip_msg_t *preq;

	/* acc_onreply is bound to TMCB_REPLY which may be called
	   from _reply, like when FR hits; we should not miss this
	   event for missed calls either */
	if (is_invite(t) && code>=300 && is_mc_on(req) )
		on_missed(t, req, reply, code);

	if (!should_acc_reply(req, reply, code))
		return;

	if(_acc_clone_msg==1) {
		memcpy(&tmsg, req, sizeof(sip_msg_t));
		preq = &tmsg;
	} else {
		preq = req;
	}

	/* get winning branch index, if set */
	if (t->relayed_reply_branch>=0) {
		br = t->relayed_reply_branch;
	} else {
		if(code>=300) {
			br = tmb.t_get_picked_branch();
		}
	}

	/* for reply processing, set as new_uri the one from selected branch */
	if (br>=0) {
		new_uri_bk = preq->new_uri;
		preq->new_uri = t->uac[br].uri;
		preq->parsed_uri_ok = 0;
	} else {
		new_uri_bk.len = -1;
		new_uri_bk.s = 0;
	}
	/* set env variables */
	env_set_to( get_rpl_to(t,reply) );
	env_set_code_status( code, reply);

	if ( is_log_acc_on(preq) ) {
		env_set_text( ACC_ANSWERED, ACC_ANSWERED_LEN);
		acc_log_request(preq);
	}
#ifdef SQL_ACC
	if (is_db_acc_on(preq)) {
		if(acc_db_set_table_name(preq, db_table_acc_data, &db_table_acc)<0) {
			LM_ERR("cannot set acc db table name\n");
		} else {
			acc_db_request(preq);
		}
	}
#endif
#ifdef RAD_ACC
	if (is_rad_acc_on(preq))
		acc_rad_request(preq);
#endif
/* DIAMETER */
#ifdef DIAM_ACC
	if (is_diam_acc_on(preq))
		acc_diam_request(preq);
#endif

	/* run extra acc engines */
	acc_run_engines(preq, 0, NULL);

	if (new_uri_bk.len>=0) {
		req->new_uri = new_uri_bk;
		req->parsed_uri_ok = 0;
	}

	/* free header's parsed structures that were added by resolving acc attributes */
	for( hdr=req->headers ; hdr ; hdr=hdr->next ) {
		if ( hdr->parsed && hdr_allocs_parse(hdr) &&
					(hdr->parsed<(void*)t->uas.request ||
					hdr->parsed>=(void*)t->uas.end_request)) {
			/* header parsed filed doesn't point inside uas.request memory
			 * chunck -> it was added by resolving acc attributes -> free it as pkg */
			DBG("removing hdr->parsed %d\n", hdr->type);
			clean_hdr_field(hdr);
			hdr->parsed = 0;
		}
	}
}



static inline void acc_onack( struct cell* t, struct sip_msg *req,
		struct sip_msg *ack, int code)
{
	if (acc_preparse_req(ack)<0)
		return;

	/* set env variables */
	env_set_to( ack->to?ack->to:req->to );
	env_set_code_status( t->uas.status, 0 );

	if (is_log_acc_on(req)) {
		env_set_text( ACC_ACKED, ACC_ACKED_LEN);
		acc_log_request( ack );
	}
#ifdef SQL_ACC
	if (is_db_acc_on(req)) {
		if(acc_db_set_table_name(ack, db_table_acc_data, &db_table_acc)<0) {
			LM_ERR("cannot set acc db table name\n");
			return;
		}
		acc_db_request( ack );
	}
#endif
#ifdef RAD_ACC
	if (is_rad_acc_on(req)) {
		acc_rad_request(ack);
	}
#endif
/* DIAMETER */
#ifdef DIAM_ACC
	if (is_diam_acc_on(req)) {
		acc_diam_request(ack);
	}
#endif

	/* run extra acc engines */
	acc_run_engines(ack, 0, NULL);
	
}


/**
 * @brief execute an acc event via a specific engine
 */
int acc_api_exec(struct sip_msg *rq, acc_engine_t *eng,
		acc_param_t* comment)
{
	acc_info_t inf;
	if (acc_preparse_req(rq)<0)
		return -1;
	env_set_to(rq->to);
	env_set_comment(comment);
	memset(&inf, 0, sizeof(acc_info_t));
	inf.env  = &acc_env;
	acc_api_set_arrays(&inf);
	return eng->acc_req(rq, &inf);
}


static void tmcb_func( struct cell* t, int type, struct tmcb_params *ps )
{
	LM_DBG("acc callback called for t(%p) event type %d, reply code %d\n",
			t, type, ps->code);
	if (type&TMCB_RESPONSE_OUT) {
		acc_onreply( t, ps->req, ps->rpl, ps->code);
	} else if (type&TMCB_E2EACK_IN) {
		acc_onack( t, t->uas.request, ps->req, ps->code);
	} else if (type&TMCB_ON_FAILURE) {
		on_missed( t, ps->req, ps->rpl, ps->code);
	} else if (type&TMCB_RESPONSE_IN) {
		acc_onreply_in( t, ps->req, ps->rpl, ps->code);
	}
}

