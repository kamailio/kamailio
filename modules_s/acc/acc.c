/*
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
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
 * 2003-04-04  grand acc cleanup (jiri)
 */


#include <stdio.h>
#include <time.h>

#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"      /* q_memchr */
#include "../../mem/mem.h"
#include "../../parser/hf.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/digest/digest.h"
#include "../tm/t_funcs.h"
#include "acc_mod.h"
#include "acc.h"
#include "dict.h"
#ifdef RAD_ACC
#include <radiusclient.h>
#endif


#define ATR(atr)  atr_arr[cnt].s=A_##atr;\
				atr_arr[cnt].len=A_##atr##_LEN;

static str na={NA, NA_LEN};

#ifdef RAD_ACC
/* caution: keep these aligned to RAD_ACC_FMT !! */
static int rad_attr[] = {PW_USER_NAME, 
	PW_CALLING_STATION_ID, PW_CALLED_STATION_ID,
	PW_SIP_TRANSLATED_REQ_URI, PW_ACCT_SESSION_ID, PW_SIP_TO_TAG, 
	PW_SIP_FROM_TAG, PW_SIP_CSEQ };
#endif


static inline struct hdr_field *valid_to( struct cell *t, 
				struct sip_msg *reply)
{
	if (reply==FAKED_REPLY || !reply || !reply->to) 
		return t->uas.request->to;
	return reply->to;
}

static inline str *cred(struct sip_msg *rq)
{
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred=(auth_body_t*)(h->parsed);
	if (!cred || !cred->digest.username.user.len) 
			return 0;
	return &cred->digest.username.user;
}

/* create an array of str's for accounting using a formatting string;
 * this is the heart of the accounting module -- it prints whatever
 * requested in a way, that can be used for syslog, radius, 
 * sql, whatsoever */
static int fmt2strar( char *fmt, /* what would you like to account ? */
		struct sip_msg *rq, /* accounted message */
		struct hdr_field *to, 
		str *phrase, 
		int *total_len, /* total length of accounted values */
		int *attr_len,  /* total length of accounted attribtue names */
		str **val_arr, /* that's the output -- must have MAX_ACC_COLUMNS */
		str *atr_arr)
{
	int cnt, tl, al;
	struct to_body* from, *pto;
	static struct sip_uri from_uri, to_uri;
	static str mycode;
	str *cr;
	struct cseq_body *cseq;

	cnt=tl=al=0;

	/* we don't care about parsing here; either the function
	 * was called from script, in which case the wrapping function
	 * is supposed to parse, or from reply processing in which case
	 * TM should have preparsed from REQUEST_IN callback; what's not
	 * here is replaced with NA
	 */


	while(*fmt) {
		if (cnt==ALL_LOG_FMT_LEN) {
			LOG(L_ERR, "ERROR: fmt2strar: too long formatting string\n");
			return 0;
		}
		switch(*fmt) {
			case 'n': /* CSeq number */
				if (rq->cseq && (cseq=get_cseq(rq)) && cseq->number.len) 
					val_arr[cnt]=&cseq->number;
				else val_arr[cnt]=&na;
				ATR(CSEQ);
				break;
			case 'c':	/* Callid */
				val_arr[cnt]=rq->callid && rq->callid->body.len
						? &rq->callid->body : &na;
				ATR(CALLID);
				break;
			case 'i': /* incoming uri */
				val_arr[cnt]=&rq->first_line.u.request.uri;
				ATR(IURI);
				break;
			case 'm': /* method */
				val_arr[cnt]=&rq->first_line.u.request.method;
				ATR(METHOD);
				break;
			case 'o':
				if (rq->new_uri.len) val_arr[cnt]=&rq->new_uri;
				else val_arr[cnt]=&rq->first_line.u.request.uri;
				ATR(OURI);
				break;
			case 'f':
				val_arr[cnt]=(rq->from && rq->from->body.len) 
					? &rq->from->body : &na;
				ATR(FROM);
				break;
			case 'r': /* from-tag */
				if (rq->from && (from=get_from(rq))
							&& from->tag_value.len) {
						val_arr[cnt]=&from->tag_value;
				} else val_arr[cnt]=&na;
				ATR(FROMTAG);
				break;
			case 'U': /* digest, from-uri otherwise */
				cr=cred(rq);
				if (cr) {
					ATR(UID);
					val_arr[cnt]=cr;
					break;
				}
				/* fallback to from-uri if digest unavailable ... */
			case 'F': /* from-uri */
				if (rq->from && (from=get_from(rq))
							&& from->uri.len) {
						val_arr[cnt]=&from->uri;
				} else val_arr[cnt]=&na;
				ATR(FROMURI);
				break;
			case '0': /* from user */
				val_arr[cnt]=&na;
				if (rq->from && (from=get_from(rq))
						&& from->uri.len) {
					parse_uri(from->uri.s, from->uri.len, &from_uri);
					if (from_uri.user.len) 
							val_arr[cnt]=&from_uri.user;
				} 
				ATR(FROMUSER);
				break;
			case 't':
				val_arr[cnt]=(to && to->body.len) ? &to->body : &na;
				ATR(TO);
				break;
			case 'd':	
				val_arr[cnt]=(to && (pto=(struct to_body*)(to->parsed))
					&& pto->tag_value.len) ? 
					& pto->tag_value : &na;
				ATR(TOTAG);
				break;
			case 'T': /* to-uri */
				if (rq->to && (pto=get_to(rq))
							&& pto->uri.len) {
						val_arr[cnt]=&pto->uri;
				} else val_arr[cnt]=&na;
				ATR(TOURI);
				break;
			case '1': /* to user */ 
				val_arr[cnt]=&na;
				if (rq->to && (pto=get_to(rq))
							&& pto->uri.len) {
					parse_uri(pto->uri.s, pto->uri.len, &to_uri);
					if (to_uri.user.len)
						val_arr[cnt]=&to_uri.user;
				} 
				ATR(TOUSER);
				break;
			case 'S':
				if (phrase->len>=3) {
					mycode.s=phrase->s;mycode.len=3;
					val_arr[cnt]=&mycode;
				} else val_arr[cnt]=&na;
				ATR(CODE);
				break;
			case 's':
				val_arr[cnt]=phrase;
				ATR(STATUS);
				break;
			case 'u':
				cr=cred(rq);
				val_arr[cnt]=cr?cr:&na;
				ATR(UID);
				break;
			case 'p':
				val_arr[cnt]=rq->parsed_orig_ruri.user.len ?
					& rq->parsed_orig_ruri.user : &na;
				ATR(UP_IURI);
				break;
			default:
				LOG(L_CRIT, "BUG: acc_log_request: uknown char: %c\n",
					*fmt);
				return 0;
		} /* switch (*fmt) */
		tl+=val_arr[cnt]->len;
		al+=atr_arr[cnt].len;
		fmt++;
		cnt++;
	} /* while (*fmt) */
	*total_len=tl;
	*attr_len=al;
	return cnt;
}


	/* skip leading text and begin with first item's
	 * separator ", " which will be overwritten by the
	 * leading text later 
	 * */
/********************************************
 *        acc_request
 ********************************************/
int acc_log_request( struct sip_msg *rq, struct hdr_field *to, 
				str *txt, str *phrase)
{
	int len;
	char *log_msg;
	char *p;
	int attr_cnt;
	int attr_len;
	str* val_arr[ALL_LOG_FMT_LEN];
	str atr_arr[ALL_LOG_FMT_LEN];
	int i;

	if (skip_cancel(rq)) return 1;

	attr_cnt=fmt2strar( log_fmt, rq, to, phrase, 
					&len, &attr_len, val_arr, atr_arr);
	if (!attr_cnt) {
		LOG(L_ERR, "ERROR: acc_log_request: fmt2strar failed\n");
		return -1;
	}
	len+=attr_len+ACC_LEN+txt->len+A_EOL_LEN 
		+attr_cnt*(A_SEPARATOR_LEN+A_EQ_LEN)-A_SEPARATOR_LEN;
	log_msg=pkg_malloc(len);
	if (!log_msg) {
		LOG(L_ERR, "ERROR: acc_log_request: no mem\n");
		return -1;
	}

	/* skip leading text and begin with first item's
	 * separator ", " which will be overwritten by the
	 * leading text later 
	 * */
	p=log_msg+(ACC_LEN+txt->len-A_SEPARATOR_LEN);
	for (i=0; i<attr_cnt; i++) {
		memcpy(p, A_SEPARATOR, A_SEPARATOR_LEN );
		p+=A_SEPARATOR_LEN;
		memcpy(p, atr_arr[i].s, atr_arr[i].len);
		p+=atr_arr[i].len;
		memcpy(p, A_EQ, A_EQ_LEN);
		p+=A_EQ_LEN;
		memcpy(p, val_arr[i]->s, val_arr[i]->len);
		p+=val_arr[i]->len;
	}

	/* terminating text */
	memcpy(p, A_EOL, A_EOL_LEN); p+=A_EOL_LEN;
	/* leading text */
	p=log_msg;
	memcpy(p, ACC, ACC_LEN ); p+=ACC_LEN;
	memcpy(p, txt->s, txt->len); p+=txt->len;

	LOG(log_level, "%s", log_msg );

	pkg_free(log_msg);
	return 1;
}



/********************************************
 *        acc_missed_report
 ********************************************/


void acc_log_missed( struct cell* t, struct sip_msg *reply,
	unsigned int code )
{
	str acc_text;
	static str leading_text={ACC_MISSED, ACC_MISSED_LEN};

	get_reply_status(&acc_text, reply, code);
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR: acc_missed_report: "
						"get_reply_status failed\n" );
		return;
	}

	acc_log_request(t->uas.request, 
			valid_to(t, reply), &leading_text, &acc_text);
	pkg_free(acc_text.s);
}


/********************************************
 *        acc_reply_report
 ********************************************/

void acc_log_reply(  struct cell* t , struct sip_msg *reply,
	unsigned int code )
{
	str code_str;
	static str lead={ACC_ANSWERED, ACC_ANSWERED_LEN};

	code_str.s=int2str(code, &code_str.len);
	acc_log_request(t->uas.request, 
			valid_to(t,reply), &lead, &code_str );
}

/********************************************
 *        reports for e2e ACKs
 ********************************************/
void acc_log_ack(  struct cell* t , struct sip_msg *ack )
{

	struct sip_msg *rq;
	struct hdr_field *to;
	static str lead={ACC_ACKED, ACC_ACKED_LEN};
	str code_str;

	rq =  t->uas.request;

	if (ack->to) to=ack->to; else to=rq->to;
	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_log_request(ack, to, &lead, &code_str );
}

/**************** SQL Support *************************/

#ifdef SQL_ACC

int acc_db_request( struct sip_msg *rq, struct hdr_field *to, 
				str *phrase, char *table, char *fmt)
{
	db_val_t vals[ALL_LOG_FMT_LEN+1];
	str* val_arr[ALL_LOG_FMT_LEN+1];
	str atr_arr[ALL_LOG_FMT_LEN+1];
	/* caution: keys need to be aligned to formatting strings */
	db_key_t keys[] = {acc_from_uri, acc_to_uri,
		acc_sip_method_col, acc_i_uri_col, 
		acc_o_uri_col, acc_sip_from_col, acc_sip_callid_col,
   		acc_sip_to_col, acc_sip_status_col, acc_user_col, 
		acc_time_col, acc_totag_col, acc_fromtag_col};

	struct tm *tm;
	time_t timep;
	char time_s[20];
	int attr_cnt;

	int i;
	int dummy_len;

	if (skip_cancel(rq)) return 1;

	/* database columns:
	 * "sip_method", "i_uri", "o_uri", "sip_from", "sip_callid", 
	 * "sip_to", "sip_status", "user", "time"
	 */
	attr_cnt=fmt2strar( fmt, rq, to, phrase, 
					&dummy_len, &dummy_len, val_arr, atr_arr);
	if (!attr_cnt) {
		LOG(L_ERR, "ERROR: acc_db_request: fmt2strar failed\n");
		return -1;
	}

	if (!db_url) {
		LOG(L_ERR, "ERROR: can't log -- no db_url set\n");
		return -1;
	}

	timep = time(NULL);
	tm = gmtime(&timep);
	strftime(time_s, 20, "%Y-%m-%d %H:%M:%S", tm);


	for(i=0; i<attr_cnt; i++) {
		VAL_TYPE(vals+i)=DB_STR;
		VAL_NULL(vals+i)=0;
		VAL_STR(vals+i)=*val_arr[i];
	}
	/* time */
	VAL_TYPE(vals+i)=DB_STRING;
	VAL_NULL(vals+i)=0;
	VAL_STRING(vals+i)=time_s;

	db_use_table(db_handle, table);
	if (db_insert(db_handle, keys, vals, i+1) < 0) {
		LOG(L_ERR, "ERROR: acc_request: "
				"Error while inserting to database\n");
		return -1;;
	}

	return 1;
}

void acc_db_missed( struct cell* t, struct sip_msg *reply,
	unsigned int code )
{
	str acc_text;

	get_reply_status(&acc_text, reply, code);
	acc_db_request(t->uas.request, valid_to(t,reply), &acc_text,
				db_table_mc, SQL_MC_FMT );
}
void acc_db_ack(  struct cell* t , struct sip_msg *ack )
{
	str code_str;

	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_db_request(ack, ack->to ? ack->to : t->uas.request->to,
			&code_str, db_table_acc, SQL_ACC_FMT);
}



void acc_db_reply(  struct cell* t , struct sip_msg *reply,
	unsigned int code )
{
	str code_str;

	code_str.s=int2str(code, &code_str.len);
	acc_db_request(t->uas.request, valid_to(t,reply), &code_str,
				db_table_acc, SQL_ACC_FMT);
}
#endif

/**************** RADIUS Support *************************/

#ifdef RAD_ACC
inline static UINT4 phrase2code(str *phrase)
{
	UINT4 code;
	int i;

	if (phrase->len<3) return 0;
	code=0;
	for (i=0;i<3;i++) {
		if (!(phrase->s[i]>='0' && phrase->s[i]<'9'))
				return 0;
		code=code*10+phrase->s[i]-'0';
	}
	return code;
}

inline UINT4 rad_status(struct sip_msg *rq, str *phrase)
{
	int code;

	code=phrase2code(phrase);
	if (code==0)
		return PW_STATUS_FAILED;
	if ((rq->REQ_METHOD==METHOD_INVITE || rq->REQ_METHOD==METHOD_ACK)
				&& code>=200 && code<300) 
		return PW_STATUS_START;
	if ((rq->REQ_METHOD==METHOD_BYE 
					|| rq->REQ_METHOD==METHOD_CANCEL)) 
		return PW_STATUS_STOP;
	return PW_STATUS_FAILED;
}

int acc_rad_request( struct sip_msg *rq, struct hdr_field *to, 
				str *phrase )
{
	str* val_arr[ALL_LOG_FMT_LEN+1];
	str atr_arr[ALL_LOG_FMT_LEN+1];
	int attr_cnt;
	VALUE_PAIR *send;
	UINT4 av_type;
	int i;
	int dummy_len;
#ifdef _OBSO
	char nullcode="00000";
	char ccode[6];
	char *c;
#endif

	send=NULL;

	if (skip_cancel(rq)) return 1;

	attr_cnt=fmt2strar( RAD_ACC_FMT, rq, to, phrase, 
					&dummy_len, &dummy_len, val_arr, atr_arr);
	if (attr_cnt!=(sizeof(RAD_ACC_FMT)-1)) {
		LOG(L_ERR, "ERROR: acc_rad_request: fmt2strar failed\n");
		goto error;
	}

	av_type=rad_status(rq, phrase);
	if (!rc_avpair_add(&send, PW_ACCT_STATUS_TYPE, &av_type,0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add STATUS_TYPE\n");
		goto error;
	}
	av_type=SIP_SERVICE_TYPE;
	if (!rc_avpair_add(&send, PW_SERVICE_TYPE, &av_type,0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add STATUS_TYPE\n");
		goto error;
	}
	av_type=phrase2code(phrase); /* status=integer */
	/* if (phrase.len<3) c=nullcode;
	else { memcpy(ccode, phrase.s, 3); ccode[3]=0;c=nullcode;} */
	if (!rc_avpair_add(&send, PW_SIP_RESPONSE_CODE, &av_type,0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add RESPONSE_CODE\n");
		goto error;
	}
	av_type=rq->REQ_METHOD;
	if (!rc_avpair_add(&send, PW_SIP_METHOD, &av_type,0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add SIP_METHOD\n");
		goto error;
	}

	for(i=0; i<attr_cnt; i++) {
		if (!rc_avpair_add(&send, rad_attr[i], 
					val_arr[i]->s,val_arr[i]->len)) {
			LOG(L_ERR, "ERROR: acc_rad_request: rc_avpaid_add "
					"failed for %d\n", rad_attr[i] );
			goto error;
		}
	}
		
	if (rc_acct(SIP_PORT, send)!=OK_RC) {
		LOG(L_ERR, "ERROR: acc_rad_request: radius-ing failed\n");
		goto error;
	}
	rc_avpair_free(send);
	return 1;

error:
	rc_avpair_free(send);
	return -1;
}

void acc_rad_missed( struct cell* t, struct sip_msg *reply,
	unsigned int code )
{
	str acc_text;

	get_reply_status(&acc_text, reply, code);
	acc_rad_request(t->uas.request, valid_to(t,reply), &acc_text);
}
void acc_rad_ack(  struct cell* t , struct sip_msg *ack )
{
	str code_str;

	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_rad_request(ack, ack->to ? ack->to : t->uas.request->to,
			&code_str);
}

void acc_rad_reply(  struct cell* t , struct sip_msg *reply,
	unsigned int code )
{
	str code_str;

	code_str.s=int2str(code, &code_str.len);
	acc_rad_request(t->uas.request, valid_to(t,reply), &code_str);
}
#endif

