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
 * 2003-11-04  multidomain support for mysql introduced (jiri)
 * 2004-06-06  updated to the new DB api, cleanup: acc_db_{bind, init,close)
 *              added (andrei)
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

#ifdef DIAM_ACC
#include "diam_dict.h"
#include "diam_message.h"
#include "diam_tcp.h"

#define AA_REQUEST 265
#define AA_ANSWER  265

#define ACCOUNTING_REQUEST 271
#define ACCOUNTING_ANSWER  271

#define M_NAME	"acc"
#endif

#define ATR(atr)  atr_arr[cnt].s=A_##atr;\
				atr_arr[cnt].len=A_##atr##_LEN;

static str na={NA, NA_LEN};

#ifdef RAD_ACC
/* caution: keep these aligned to RAD_ACC_FMT !! */
static int rad_attr[] = { A_CALLING_STATION_ID, A_CALLED_STATION_ID,
	A_SIP_TRANSLATED_REQUEST_URI, A_ACCT_SESSION_ID, A_SIP_TO_TAG,
	A_SIP_FROM_TAG, A_SIP_CSEQ };
#endif

#ifdef DIAM_ACC
extern char *diameter_client_host;
extern int diameter_client_port;

/* caution: keep these aligned to DIAM_ACC_FMT !! */
static int diam_attr[] = { AVP_SIP_FROM_URI, AVP_SIP_TO_URI, AVP_SIP_OURI, 
	AVP_SIP_CALLID, AVP_SIP_TO_TAG, AVP_SIP_FROM_TAG, AVP_SIP_CSEQ };
#endif

#ifdef SQL_ACC

static char* acc_db_url=0;
static db_func_t acc_dbf;
static db_con_t* db_handle=0;

#endif


static inline struct hdr_field *valid_to( struct cell *t, 
				struct sip_msg *reply)
{
	if (reply==FAKED_REPLY || !reply || !reply->to) 
		return t->uas.request->to;
	return reply->to;
}

static inline str *cred_user(struct sip_msg *rq)
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

static inline str *cred_realm(struct sip_msg *rq)
{
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred=(auth_body_t*)(h->parsed);
	if (!cred || !cred->digest.realm.len) 
			return 0;
	return &cred->digest.realm;
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
				cr=cred_user(rq);
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
			case 'X': /* from user */
				val_arr[cnt]=&na;
				if (rq->from && (from=get_from(rq))
						&& from->uri.len) {
					parse_uri(from->uri.s, from->uri.len, &from_uri);
					if (from_uri.host.len) 
							val_arr[cnt]=&from_uri.host;
				} 
				ATR(FROMDOMAIN);
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
				cr=cred_user(rq);
				val_arr[cnt]=cr?cr:&na;
				ATR(UID);
				break;
			case 'p': /* user part of request-uri */
				val_arr[cnt]=rq->parsed_orig_ruri.user.len ?
					& rq->parsed_orig_ruri.user : &na;
				ATR(UP_IURI);
				break;
			case 'D': /* domain part of request-uri */
				val_arr[cnt]=rq->parsed_orig_ruri.host.len ?
					& rq->parsed_orig_ruri.host : &na;
				ATR(RURI_DOMAIN);
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


/* binds to the corresponding database module
 * returns 0 on success, -1 on error */
int acc_db_bind(char* db_url)
{
	acc_db_url=db_url;
	if (bind_dbmod(acc_db_url, &acc_dbf)<0){
		LOG(L_ERR, "ERROR: acc_db_init: bind_db failed\n");
		return -1;
	}
	
	return 0;
}



/* initialize the database connection
 * returns 0 on success, -1 on error */
int acc_db_init()
{
	if (acc_db_url){
		db_handle=acc_dbf.init(acc_db_url);
		if (db_handle==0){
			LOG(L_ERR, "ERROR: acc_db_init: unable to connect to the "
					"database\n");
			return -1;
		}
		return 0;
	}else{
		LOG(L_CRIT, "BUG: acc_db_init: null db url\n");
		return -1;
	}
}



/* close a db connection */
void acc_db_close()
{
	if (db_handle && acc_dbf.close)	acc_dbf.close(db_handle);
}



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
		acc_totag_col, acc_fromtag_col, acc_domain_col, acc_time_col };

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

	if (!acc_db_url) {
		LOG(L_ERR, "ERROR: can't log -- no db_url set\n");
		return -1;
	}

	timep = time(NULL);
	tm = db_localtime ? localtime(&timep) : gmtime(&timep);
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

	acc_dbf.use_table(db_handle, table);
	if (acc_dbf.insert(db_handle, keys, vals, i+1) < 0) {
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
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR: acc_db_missed_report: "
						"get_reply_status failed\n" );
		return;
	}
	acc_db_request(t->uas.request, valid_to(t,reply), &acc_text,
				db_table_mc, SQL_MC_FMT );
	pkg_free(acc_text.s);
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
		return vals[V_STATUS_FAILED].v;
	if ((rq->REQ_METHOD==METHOD_INVITE || rq->REQ_METHOD==METHOD_ACK)
				&& code>=200 && code<300) 
		return vals[V_STATUS_START].v;
	if ((rq->REQ_METHOD==METHOD_BYE 
					|| rq->REQ_METHOD==METHOD_CANCEL)) 
		return vals[V_STATUS_STOP].v;
	return vals[V_STATUS_FAILED].v;
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
	str* user;
	str* realm;
	str user_name;
	struct sip_uri puri;
	struct to_body* from;
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
	if (!rc_avpair_add(rh, &send, attrs[A_ACCT_STATUS_TYPE].v, &av_type, -1, 0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add STATUS_TYPE\n");
		goto error;
	}
	av_type=vals[V_SIP_SESSION].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &av_type, -1, 0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add STATUS_TYPE\n");
		goto error;
	}
	av_type=phrase2code(phrase); /* status=integer */
	/* if (phrase.len<3) c=nullcode;
	else { memcpy(ccode, phrase.s, 3); ccode[3]=0;c=nullcode;} */
	if (!rc_avpair_add(rh, &send, attrs[A_SIP_RESPONSE_CODE].v, &av_type, -1, 0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add RESPONSE_CODE\n");
		goto error;
	}
	av_type=rq->REQ_METHOD;
	if (!rc_avpair_add(rh, &send, attrs[A_SIP_METHOD].v, &av_type, -1, 0)) {
		LOG(L_ERR, "ERROR: acc_rad_request: add SIP_METHOD\n");
		goto error;
	}
        /* Handle User-Name as a special case */
	user=cred_user(rq);  /* try to take it from credentials */
	if (user) {
		realm = cred_realm(rq);
		if (realm) {
			user_name.len = user->len+1+realm->len;
			user_name.s = pkg_malloc(user_name.len);
			if (!user_name.s) {
				LOG(L_ERR, "ERROR: acc_rad_request: no memory\n");
				goto error;
			}
			memcpy(user_name.s, user->s, user->len);
			user_name.s[user->len] = '@';
			memcpy(user_name.s+user->len+1, realm->s, realm->len);
			if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, 
					   user_name.s, user_name.len, 0)) {
				LOG(L_ERR, "ERROR: acc_rad_request: rc_avpaid_add "
				    "failed for %d\n", attrs[A_USER_NAME].v );
				pkg_free(user_name.s);
				goto error;
			}
			pkg_free(user_name.s);
		} else {
			user_name.len = user->len;
			user_name.s = user->s;
			if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, 
					   user_name.s, user_name.len, 0)) {
				LOG(L_ERR, "ERROR: acc_rad_request: rc_avpaid_add "
				    "failed for %d\n", attrs[A_USER_NAME].v );
				goto error;
			}
		}
	} else {  /* from from uri */
		if (rq->from && (from=get_from(rq)) && from->uri.len) {
			if (parse_uri(from->uri.s, from->uri.len, &puri) < 0 ) {
				LOG(L_ERR, "ERROR: acc_rad_request: Bad From URI\n");
				goto error;
			}
			user_name.len = puri.user.len+1+puri.host.len;
			user_name.s = pkg_malloc(user_name.len);
			if (!user_name.s) {
				LOG(L_ERR, "ERROR: acc_rad_request: no memory\n");
				goto error;
			}
			memcpy(user_name.s, puri.user.s, puri.user.len);
			user_name.s[puri.user.len] = '@';
			memcpy(user_name.s+puri.user.len+1, puri.host.s, puri.host.len);
			if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, 
					   user_name.s, user_name.len, 0)) {
				LOG(L_ERR, "ERROR: acc_rad_request: rc_avpaid_add "
				    "failed for %d\n", attrs[A_USER_NAME].v );
				pkg_free(user_name.s);
				goto error;
			}
			pkg_free(user_name.s);
		} else {
			user_name.len = na.len;
			user_name.s = na.s;
			if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, 
					   user_name.s, user_name.len, 0)) {
				LOG(L_ERR, "ERROR: acc_rad_request: rc_avpaid_add "
				    "failed for %d\n", attrs[A_USER_NAME].v );
				goto error;
			}
		}
	}
        /* Remaining attributes from rad_attr vector */
	for(i=0; i<attr_cnt; i++) {
		if (!rc_avpair_add(rh, &send, attrs[rad_attr[i]].v, 
				   val_arr[i]->s,val_arr[i]->len, 0)) {
			LOG(L_ERR, "ERROR: acc_rad_request: rc_avpaid_add "
			    "failed for %s\n", attrs[rad_attr[i]].n );
			goto error;
		}
	}
		
	if (rc_acct(rh, SIP_PORT, send)!=OK_RC) {
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
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR: acc_rad_missed_report: "
						"get_reply_status failed\n" );
		return;
	}
	acc_rad_request(t->uas.request, valid_to(t,reply), &acc_text);
	pkg_free(acc_text.s);
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

/**************** DIAMETER Support *************************/
#ifdef DIAM_ACC
#ifndef RAD_ACC
inline static unsigned long phrase2code(str *phrase)
{
	unsigned long code;
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
#endif

inline unsigned long diam_status(struct sip_msg *rq, str *phrase)
{
	int code;

	code=phrase2code(phrase);
	if (code==0)
		return -1;

	if ((rq->REQ_METHOD==METHOD_INVITE || rq->REQ_METHOD==METHOD_ACK)
				&& code>=200 && code<300) 
		return AAA_ACCT_START;
	
	if ((rq->REQ_METHOD==METHOD_BYE 
					|| rq->REQ_METHOD==METHOD_CANCEL)) 
		return AAA_ACCT_STOP;
	
	if (rq->REQ_METHOD==METHOD_OTHER/*MESSAGE */ && code>=200 && code <=300)  
		return AAA_ACCT_EVENT;
	
	return -1;
}

int acc_diam_request( struct sip_msg *rq, struct hdr_field *to, str *phrase )
{
	str* val_arr[ALL_LOG_FMT_LEN+1];
	str atr_arr[ALL_LOG_FMT_LEN+1];
	int attr_cnt;
	AAAMessage *send = NULL;
	AAA_AVP *avp;
	int i;
	int dummy_len;
	str* user;
	str* realm;
	str user_name;
	str value;
	str *uri;
	struct sip_uri puri;
	struct to_body* from;
	int ret, free_user_name;
	int status;
	char tmp[2];
	unsigned int mid;


	if (skip_cancel(rq)) return 1;

	attr_cnt=fmt2strar( DIAM_ACC_FMT, rq, to, phrase, 
					&dummy_len, &dummy_len, val_arr, atr_arr);
	
	if (attr_cnt!=(sizeof(DIAM_ACC_FMT)-1)) 
	{
		LOG(L_ERR, "ERROR: acc_diam_request: fmt2strar failed\n");
		return -1;
	}
	
	if ( (send=AAAInMessage(ACCOUNTING_REQUEST, AAA_APP_NASREQ))==NULL)
	{
		LOG(L_ERR, "ERROR: acc_diam_reqest: new AAA message not created\n");
		return -1;
	}


	/* AVP_ACCOUNTIG_RECORD_TYPE */
	if( (status = diam_status(rq, phrase))<0)
	{
		LOG(L_ERR, "ERROR: acc_diam_request: status unknown\n");
		goto error;
	}
	tmp[0] = status+'0';
	tmp[1] = 0;
	if( (avp=AAACreateAVP(AVP_Accounting_Record_Type, 0, 0, tmp, 
						1, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR,"ERROR: acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, "ERROR: acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}
	/* SIP_MSGID AVP */
	DBG("**ACC***** m_id=%d\n", rq->id);
	mid = rq->id;
	if( (avp=AAACreateAVP(AVP_SIP_MSGID, 0, 0, (char*)(&mid), 
				sizeof(mid), AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* SIP Service AVP */
	if( (avp=AAACreateAVP(AVP_Service_Type, 0, 0, SIP_ACCOUNTING, 
				SERVICE_LEN, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR,"ERROR: acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, "ERROR: acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* SIP_STATUS avp */
	if( (avp=AAACreateAVP(AVP_SIP_STATUS, 0, 0, phrase->s, 
						phrase->len, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR,"ERROR: acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, "ERROR: acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* SIP_METHOD avp */
	value = rq->first_line.u.request.method;
	if( (avp=AAACreateAVP(AVP_SIP_METHOD, 0, 0, value.s, 
						value.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR,"ERROR: acc_diam_request: no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, "ERROR: acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	/* Handle AVP_USER_NAME as a special case */
	free_user_name = 0;
	user=cred_user(rq);  /* try to take it from credentials */
	if (user) 
	{
		realm = cred_realm(rq);
		if (realm) 
		{
			user_name.len = user->len+1+realm->len;
			user_name.s = pkg_malloc(user_name.len);
			if (!user_name.s) 
			{
				LOG(L_ERR, "ERROR: acc_diam_request: no memory\n");
				goto error;
			}
			memcpy(user_name.s, user->s, user->len);
			user_name.s[user->len] = '@';
			memcpy(user_name.s+user->len+1, realm->s, realm->len);
			free_user_name = 1;
		} 
		else 
		{
			user_name.len = user->len;
			user_name.s = user->s;
		}
	} 
	else 
	{  /* from from uri */
		if (rq->from && (from=get_from(rq)) && from->uri.len) 
		{
			if (parse_uri(from->uri.s, from->uri.len, &puri) < 0 ) 
			{
				LOG(L_ERR, "ERROR: acc_diam_request: Bad From URI\n");
				goto error;
			}
			user_name.len = puri.user.len+1+puri.host.len;
			user_name.s = pkg_malloc(user_name.len);
			if (!user_name.s) {
				LOG(L_ERR, "ERROR: acc_diam_request: no memory\n");
				goto error;
			}
			memcpy(user_name.s, puri.user.s, puri.user.len);
			user_name.s[puri.user.len] = '@';
			memcpy(user_name.s+puri.user.len+1, puri.host.s, puri.host.len);
			free_user_name = 1;
		} 
		else 
		{
			user_name.len = na.len;
			user_name.s = na.s;
		}
	}

	if( (avp=AAACreateAVP(AVP_User_Name, 0, 0, user_name.s, user_name.len, 
					free_user_name?AVP_FREE_DATA:AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR,"ERROR: acc_diam_request: no more free memory!\n");
		if(free_user_name)
			pkg_free(user_name.s);
		goto error;
	}
	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, "ERROR: acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}

	
    /* Remaining attributes from diam_attr vector */
	for(i=0; i<attr_cnt; i++) 
	{
		if((avp=AAACreateAVP(diam_attr[i], 0,0, val_arr[i]->s, val_arr[i]->len, 
					AVP_DUPLICATE_DATA)) == 0)
		{
			LOG(L_ERR,"ERROR: acc_diam_request: no more free memory!\n");
			goto error;
		}
		if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
		{
			LOG(L_ERR, "ERROR: acc_diam_request: avp not added \n");
			AAAFreeAVP(&avp);
			goto error;
		}
	}
		
	if (get_uri(rq, &uri) < 0) 
	{
		LOG(L_ERR, "ERROR: acc_diam_request: From/To URI not found\n");
		goto error;
	}
	
	if (parse_uri(uri->s, uri->len, &puri) < 0) 
	{
		LOG(L_ERR, "ERROR: acc_diam_request: Error parsing From/To URI\n");
		goto error;
	}

	
	/* Destination-Realm AVP */
	if( (avp=AAACreateAVP(AVP_Destination_Realm, 0, 0, puri.host.s,
						puri.host.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR,"acc_diam_request: no more free memory!\n");
		goto error;
	}

	if( AAAAddAVPToMessage(send, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, "acc_diam_request: avp not added \n");
		AAAFreeAVP(&avp);
		goto error;
	}


	/* prepare the message to be sent over the network */
	if(AAABuildMsgBuffer(send) != AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, "ERROR: acc_diam_request: message buffer not created\n");
		goto error;
	}

	if(sockfd==AAA_NO_CONNECTION)
	{
		sockfd = init_mytcp(diameter_client_host, diameter_client_port);
		if(sockfd==AAA_NO_CONNECTION)
		{
			LOG(L_ERR, M_NAME":acc_diam_request: failed to reconnect"
								" to Diameter client\n");
			goto error;
		}
	}
		
	/* send the message to the DIAMETER client */
	ret = tcp_send_recv(sockfd, send->buf.s, send->buf.len, rb, rq->id);

	if(ret == AAA_CONN_CLOSED)
	{
		LOG(L_NOTICE, M_NAME":acc_diam_request: connection to Diameter"
					" client closed.It will be reopened by the next request\n");
		close(sockfd);
		sockfd = AAA_NO_CONNECTION;
		goto error;
	}

	if(ret != ACC_SUCCESS) /* a transmission error occured */
	{
		LOG(L_ERR, M_NAME":acc_diam_request: message sending to the" 
					" DIAMETER backend authorization server failed\n");
		goto error;
	}

	AAAFreeMessage(&send);
	return 1;

error:
	AAAFreeMessage(&send);
	return -1;
}


void acc_diam_missed( struct cell* t, struct sip_msg *reply, unsigned int code )
{
	str acc_text;

	get_reply_status(&acc_text, reply, code);
	acc_diam_request(t->uas.request, valid_to(t,reply), &acc_text);
}
void acc_diam_ack( struct cell* t, struct sip_msg *ack )
{
	str code_str;

	code_str.s=int2str(t->uas.status, &code_str.len);
	acc_diam_request(ack, ack->to ? ack->to : t->uas.request->to,
			&code_str);
}

void acc_diam_reply( struct cell* t , struct sip_msg *reply, unsigned int code )
{
	str code_str;

	code_str.s=int2str(code, &code_str.len);
	acc_diam_request(t->uas.request, valid_to(t, reply), &code_str);
}

#endif
