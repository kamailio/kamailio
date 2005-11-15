/*
 * Accounting module
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2005 iptelorg GmbH
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
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../tm/t_hooks.h"
#include "../tm/tm_load.h"
#include "../tm/h_table.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/digest/digest.h"
#include "../../usr_avp.h"
#include "../tm/tm_load.h"
#include "../../db/db.h"

/*
 * TODO:
 * - Quote attribute values properly
 */

/*
 * a: attr
 * c: sip_callid
 * d: to_tag
 * f: sip_from
 * g: flags
 * i: inbound_ruri
 * m: sip_method
 * n: sip_cseq
 * o: outbound_ruri
 * p: source_ip
 * r: from_tag
 * t: sip_to
 * u: digest_username
 * x: request_timestamp
 * D: to_did
 * F: from_uri
 * I: from_uid
 * M: from_did
 * P: source_port
 * R: digest_realm
 * S: sip_status
 * T: to_uri
 * U: to_uid
 * X: response_timestamp
 */

#define ALL_LOG_FMT "acdfgimnoprtuxDFIMPRSTUX"
#define ALL_LOG_FMT_LEN (sizeof(ALL_LOG_FMT) - 1)

/* Default column names */
#define A_ATTRS        "attrs"
#define A_CALLID       "sip_callid"
#define A_TOTAG        "to_tag"
#define A_FROM         "sip_from"
#define A_FLAGS        "flags"
#define A_IURI         "in_ruri"
#define A_METHOD       "sip_method"
#define A_CSEQ         "sip_cseq"
#define A_OURI         "out_ruri"
#define A_FROMTAG      "from_tag"
#define A_TO           "sip_to"
#define A_DIGUSER      "digest_username"
#define A_REQTIMESTAMP "request_timestamp"
#define A_TODID        "to_did"
#define A_FROMURI      "from_uri"
#define A_FROMUID      "from_uid"
#define A_FROMDID      "from_did"
#define A_DIGREALM     "digest_realm"
#define A_STATUS       "sip_status"
#define A_TOURI        "to_uri"
#define A_TOUID        "to_uid"
#define A_RESTIMESTAMP "response_timestamp"
#define A_SRCIP        "src_ip"
#define A_SRCPORT      "src_port"

MODULE_VERSION

struct tm_binds tmb;

static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);

static int early_media = 0;         /* Enable/disable early media (183) accounting */
static int failed_transactions = 0; /* Enable/disable accounting of failed (>= 300) transactions */
static int report_cancels = 0;      /* Enable/disable CANCEL reporting */
static int report_ack = 0;          /* Enable/disable end-to-end ACK reports */
static int log_flag = 0;            /* Flag that marks transactions to be accounted */
static int log_missed_flag = 0;     /* Transaction having this flag set will be accounted in missed calls when fails */
static char* log_fmt = ALL_LOG_FMT; /* Formating string that controls what information will be collected and accounted */

#define DEFAULT_ACC_TABLE "acc"
#define DEFAULT_MC_TABLE "missed_calls"

static str db_url = {DEFAULT_DB_URL, DEFAULT_DB_URL_LEN};
static str acc_table = {DEFAULT_ACC_TABLE, sizeof(DEFAULT_ACC_TABLE) - 1};
static str mc_table = {DEFAULT_MC_TABLE, sizeof(DEFAULT_MC_TABLE) - 1};

static str attrs_col = {A_ATTRS, sizeof(A_ATTRS) - 1};
static str callid_col = {A_CALLID, sizeof(A_CALLID) - 1};
static str totag_col = {A_TOTAG, sizeof(A_TOTAG) - 1};
static str from_col = {A_FROM, sizeof(A_FROM) - 1};
static str flags_col = {A_FLAGS, sizeof(A_FLAGS) - 1};
static str iuri_col = {A_IURI, sizeof(A_IURI) - 1};
static str method_col = {A_METHOD, sizeof(A_METHOD) - 1};
static str cseq_col = {A_CSEQ, sizeof(A_CSEQ) - 1};
static str ouri_col = {A_OURI, sizeof(A_OURI) - 1};
static str fromtag_col = {A_FROMTAG, sizeof(A_FROMTAG) - 1};
static str to_col = {A_TO, sizeof(A_TO) - 1};
static str diguser_col = {A_DIGUSER, sizeof(A_DIGUSER) - 1};
static str reqtimestamp_col = {A_REQTIMESTAMP, sizeof(A_REQTIMESTAMP) - 1};
static str todid_col = {A_TODID, sizeof(A_TODID) - 1};
static str fromuri_col = {A_FROMURI, sizeof(A_FROMURI) - 1};
static str fromuid_col = {A_FROMUID, sizeof(A_FROMUID) - 1};
static str fromdid_col = {A_FROMDID, sizeof(A_FROMDID) - 1};
static str digrealm_col = {A_DIGREALM, sizeof(A_DIGREALM) - 1};
static str status_col = {A_STATUS, sizeof(A_STATUS) - 1};
static str touri_col = {A_TOURI, sizeof(A_TOURI) - 1};
static str touid_col = {A_TOUID, sizeof(A_TOUID) - 1};
static str restimestamp_col = {A_RESTIMESTAMP, sizeof(A_RESTIMESTAMP) - 1};
static str src_ip_col = {A_SRCIP, sizeof(A_SRCIP) - 1};
static str src_port_col = {A_SRCPORT, sizeof(A_SRCPORT) - 1};

static db_func_t acc_dbf;
static db_con_t* db_handle = 0;

/* Attribute-value pairs */
static regex_t attrs_re;
static char* attrs = "$^"; /* non-sense which never matches; */

static db_key_t keys[sizeof(ALL_LOG_FMT) - 1];
static db_val_t vals[sizeof(ALL_LOG_FMT) - 1];
static int db_n;

static int acc_db_request(struct sip_msg *rq, char *comment, char *foo);

static cmd_export_t cmds[] = {
	{"acc_db_request", acc_db_request, 1, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


static param_export_t params[] = {
	{"early_media",		INT_PARAM, &early_media },
	{"failed_transactions",	INT_PARAM, &failed_transactions },
	{"report_ack",		INT_PARAM, &report_ack },
	{"report_cancels",	INT_PARAM, &report_cancels },
	{"log_flag",		INT_PARAM, &log_flag },
	{"log_missed_flag",	INT_PARAM, &log_missed_flag },
	{"log_fmt",		STR_PARAM, &log_fmt },
        {"attrs",               STR_PARAM, &attrs },
	{"db_url",              STR_PARAM, &db_url.s },
	{"acc_table",           STR_PARAM, &acc_table.s },
	{"mc_table",            STR_PARAM, &mc_table.s },
	{"attrs_column",        STR_PARAM, &attrs_col.s },
	{"callid_column",       STR_PARAM, &callid_col.s },
	{"totag_column",        STR_PARAM, &totag_col.s },
	{"from_column",         STR_PARAM, &from_col.s },
	{"flags_column",        STR_PARAM, &flags_col.s },
	{"iuri_column",         STR_PARAM, &iuri_col.s },
	{"method_column",       STR_PARAM, &method_col.s },
	{"cseq_column",         STR_PARAM, &cseq_col.s },
	{"ouri_column",         STR_PARAM, &ouri_col.s },
	{"fromtag_column",      STR_PARAM, &fromtag_col.s },
	{"to_column",           STR_PARAM, &to_col.s },
	{"diguser_column",      STR_PARAM, &diguser_col.s },
	{"reqtimestamp_column", STR_PARAM, &reqtimestamp_col.s },
	{"todid_column",        STR_PARAM, &todid_col.s },
	{"fromuri_column",      STR_PARAM, &fromuri_col.s },
	{"fromuid_column",      STR_PARAM, &fromuid_col.s },
	{"fromdid_column",      STR_PARAM, &fromdid_col.s },
	{"digrealm_column",     STR_PARAM, &digrealm_col.s },
	{"status_column",       STR_PARAM, &status_col.s },
	{"touri_column",        STR_PARAM, &touri_col.s },
	{"touid_column",        STR_PARAM, &touid_col.s },
	{"restimestamp_column", STR_PARAM, &restimestamp_col.s },
	{"src_ip_column",       STR_PARAM, &src_ip_col.s },
	{"src_port_column",     STR_PARAM, &src_port_col.s },
	{0, 0, 0}
};


struct module_exports exports= {
	"acc_db",
	cmds,        /* exported functions */
	params,      /* exported params */
	mod_init,    /* initialization module */
	0,	     /* response function */
	mod_destroy, /* destroy function */
	0,	     /* oncancel function */
	child_init   /* per-child init function */
};


static inline int skip_cancel(struct sip_msg *msg)
{
        return (msg->REQ_METHOD == METHOD_CANCEL) && report_cancels == 0;
}

static int verify_fmt(char *fmt) {

	if (!fmt) {
		LOG(L_ERR, "ERROR:acc:verify_fmt: formatting string zero\n");
		return -1;
	}

	if (!(*fmt)) {
		LOG(L_ERR, "ERROR:acc:verify_fmt: formatting string empty\n");
		return -1;
	}

	if (strlen(fmt) > ALL_LOG_FMT_LEN) {
		LOG(L_ERR, "ERROR:acc:verify_fmt: formatting string too long\n");
		return -1;
	}

	while(*fmt) {
		if (!strchr(ALL_LOG_FMT, *fmt)) {
			LOG(L_ERR, "ERROR:acc:verify_fmt: char in log_fmt invalid: %c\n", *fmt);
			return -1;
		}
		fmt++;
	}
	return 1;
}


/*
 * Return true if accounting is enabled and the
 * transaction is marked for accounting
 */
static inline int is_acc_on(struct sip_msg *rq)
{   
	return log_flag && isflagset(rq, log_flag) == 1;
}


/*
 * Return true if missed_call accounting is enabled
 * and the transaction has the flag set
 */
static inline int is_mc_on(struct sip_msg *rq)
{
	return log_missed_flag && isflagset(rq, log_missed_flag) == 1;
}


static inline void preparse_req(struct sip_msg *rq)
{
	     /* try to parse from for From-tag for accounted transactions;
	      * don't be worried about parsing outcome -- if it failed,
	      * we will report N/A. There is no need to parse digest credentials
	      * here even if we account them, because the authentication function
	      * will do it before us and if not then we will account n/a.
	      */
	parse_headers(rq, HDR_CALLID_F | HDR_FROM_F | HDR_TO_F | HDR_CSEQ_F, 0 );
	parse_from_header(rq);
}


/* is this reply of interest for accounting ? */
static inline int should_acc_reply(struct cell* t, int code)
{
	struct sip_msg *r;

	r = t->uas.request;

	     /* validation */
	if (r == 0) {
		LOG(L_ERR, "ERROR:acc:should_acc_reply: 0 request\n");
		return 0;
	}

	     /* negative transactions reported otherwise only if explicitly
	      * demanded */
	if (!failed_transactions && code >= 300) return 0;
	if (!is_acc_on(r)) return 0;
	if (skip_cancel(r)) return 0;
	if (code < 200 && ! (early_media && code == 183)) return 0;
	return 1; /* seed is through, we will account this reply */
}


/* Extract username attribute from authorized credentials */
static inline str* cred_user(struct sip_msg* rq)
{
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred = (auth_body_t*)(h->parsed);
	if (!cred || !cred->digest.username.user.len)
		return 0;
	return &cred->digest.username.user;
}


/* Extract realm attribute from authorized credentials */
static inline str* cred_realm(struct sip_msg* rq)
{
	str* realm;
	struct hdr_field* h;
	auth_body_t* cred;

	get_authorized_cred(rq->proxy_auth, &h);
	if (!h) get_authorized_cred(rq->authorization, &h);
	if (!h) return 0;
	cred = (auth_body_t*)(h->parsed);
	if (!cred) return 0;
	realm = GET_REALM(&cred->digest);
	if (!realm->len || !realm->s) {
		return 0;
	}
	return realm;
}


/* Return To header field from the request in case of faked reply or
 * missing To header field in the reply
 */
static inline struct hdr_field* valid_to(struct cell* t, struct sip_msg* reply)
{
	if (reply == FAKED_REPLY || !reply || !reply->to) {
		return t->uas.request->to;
	} else {
		return reply->to;
	}
}


static int init_data(char* fmt)
{
	db_n = 0;
	while(*fmt) {
		switch(*fmt) {
		case 'a': /* attr */
			keys[db_n] = attrs_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;
			
		case 'c': /* callid */
			keys[db_n] = callid_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'd': /* To tag */
			keys[db_n] = totag_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'f': /* From */
			keys[db_n] = from_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;
			
		case 'g': /* flags */
			keys[db_n] = flags_col.s;
			vals[db_n].type = DB_INT;
			vals[db_n].nul = 0;
			break;
			
		case 'i': /* Inbound ruri */
			keys[db_n] = iuri_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'm': /* method */
			keys[db_n] = method_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'n': /* sip_cseq */
			keys[db_n] = cseq_col.s;
			vals[db_n].type = DB_INT;
			vals[db_n].nul = 0;
			break;

		case 'o': /* outbound_ruri */
			keys[db_n] = ouri_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'p': /* src_ip */
			keys[db_n] = src_ip_col.s;
			vals[db_n].type = DB_INT;
			vals[db_n].nul = 0;
			break;

		case 'r': /* from_tag */
			keys[db_n] = fromtag_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 't': /* sip_to */
			keys[db_n] = to_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'u': /* digest_username */
			keys[db_n] = diguser_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'x': /* request_timestamp */
			keys[db_n] = reqtimestamp_col.s;
			vals[db_n].type = DB_DATETIME;
			vals[db_n].nul = 0;
			break;
			
		case 'D': /* to_did */
			keys[db_n] = todid_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'F': /* from_uri */
			keys[db_n] = fromuri_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'I': /* from_uid */
			keys[db_n] = fromuid_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'M': /* from_did */
			keys[db_n] = fromdid_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'P': /* src_port */
			keys[db_n] = src_port_col.s;
			vals[db_n].type = DB_INT;
			vals[db_n].nul = 0;
			break;

		case 'R': /* digest_realm */
			keys[db_n] = digrealm_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'S': /* sip_status */
			keys[db_n] = status_col.s;
			vals[db_n].type = DB_INT;
			vals[db_n].nul = 0;
			break;

		case 'T': /* to_uri */
			keys[db_n] = touri_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'U': /* to_uid */
			keys[db_n] = touid_col.s;
			vals[db_n].type = DB_STR;
			vals[db_n].nul = 0;
			break;

		case 'X': /* response_timestamp */
			keys[db_n] = restimestamp_col.s;
			vals[db_n].type = DB_DATETIME;
			vals[db_n].nul = 0;
			break;
		}

		fmt++;
		db_n++;
	}
	return 0;
}



/* create an array of str's for accounting using a formatting string;
 * this is the heart of the accounting module -- it prints whatever
 * requested in a way, that can be used for syslog, radius,
 * sql, whatsoever
 * tm sip_msg_clones does not clone (shmmem-zed) parsed fields, other then Via1,2. Such fields clone now or use from rq_rp
 */
static int fmt2strar(char *fmt,             /* what would you like to account ? */
		     struct sip_msg *rq,    /* accounted message */
		     struct hdr_field *to,
		     unsigned int code,
		     time_t req_time)       /* Timestamp of the request */
{
	int cnt;
	struct to_body* from, *pto;
	str *cr;
	struct cseq_body *cseq;

	cnt = 0;

	     /* we don't care about parsing here; either the function
	      * was called from script, in which case the wrapping function
	      * is supposed to parse, or from reply processing in which case
	      * TM should have preparsed from REQUEST_IN callback; what's not
	      * here is replaced with NA
	      */
	while(*fmt) {
		if (cnt == ALL_LOG_FMT_LEN) {
			LOG(L_ERR, "ERROR:acc:fmt2strar: Formatting string is too long\n");
			return 0;
		}

		switch(*fmt) {
		case 'a': /* attr */
			vals[cnt].nul = 1;
			break;

		case 'c': /* sip_callid */
			if (rq->callid && rq->callid->body.len) {
				vals[cnt].val.str_val = rq->callid->body;
			} else {
				vals[cnt].nul = 1;
			}
			break;
				  
		case 'd': /* to_tag */
			if (to && (pto = (struct to_body*)(to->parsed)) && pto->tag_value.len) {
				vals[cnt].val.str_val = pto->tag_value;
			} else {
				vals[cnt].nul = 1;
			}
			break;
			
		case 'f': /* sip_from */
			if (rq->from && rq->from->body.len) {
				vals[cnt].val.str_val = rq->from->body;
			} else {
				vals[cnt].nul = 1;
			}
			break;

		case 'g': /* flags */
			vals[cnt].val.int_val = rq->flags;
			break;

		case 'i': /* inbound_ruri */
			vals[cnt].val.str_val = rq->first_line.u.request.uri;
			break;

		case 'm': /* sip_method */
			vals[cnt].val.str_val = rq->first_line.u.request.method;
			break;

		case 'n': /* sip_cseq */
			if (rq->cseq && (cseq = get_cseq(rq)) && cseq->number.len) {
				str2int(&cseq->number, (unsigned int*)&vals[cnt].val.int_val);
			} else {
				vals[cnt].nul = 1;
			}
			break;

		case 'o': /* outbound_ruri */
			if (rq->new_uri.len) vals[cnt].val.str_val = rq->new_uri;
			else vals[cnt].val.str_val = rq->first_line.u.request.uri;
			break;

		case 'p':
			vals[cnt].val.int_val = rq->rcv.src_ip.u.addr32[0];
			break;
			break;
			
		case 'r': /* from_tag */
			if (rq->from && (from = get_from(rq)) && from->tag_value.len) {
				vals[cnt].val.str_val = from->tag_value;
			} else {
				vals[cnt].nul = 1;
			}
			break;

		case 't': /* sip_to */
			if (to && to->body.len) vals[cnt].val.str_val = to->body;
			else vals[cnt].nul = 1;
			break;

		case 'u': /* digest_username */
			cr = cred_user(rq);
			if (cr) vals[cnt].val.str_val = *cr;
			else vals[cnt].nul = 1;
			break;

		case 'x': /* request_timestamp */
			vals[cnt].val.time_val = req_time;
			break;

		case 'D': /* to_did */
			vals[cnt].nul = 1;
			break;

		case 'F': /* from_uri */
			if (rq->from && (from = get_from(rq)) && from->uri.len) {
				vals[cnt].val.str_val = from->uri;
			} else vals[cnt].nul = 1;
			break;

		case 'I': /* from_uid */
			vals[cnt].nul = 1;
			break;

		case 'M': /* from_did */
			vals[cnt].nul = 1;
			break;

		case 'P': /* source_port */
			vals[cnt].val.int_val = rq->rcv.src_port;
			break;

		case 'R': /* digest_realm */
			cr = cred_realm(rq);
			if (cr) vals[cnt].val.str_val = *cr;
			else vals[cnt].nul = 1;
			break;

		case 'S': /* sip_status */
			if (code > 0) vals[cnt].val.int_val = code;
			else vals[cnt].nul = 1;
			break;

		case 'T': /* to_uri */
			if (rq->to && (pto = get_to(rq)) && pto->uri.len) vals[cnt].val.str_val = pto->uri;
			else vals[cnt].nul = 1;
			break;

		case 'U': /* to_uid */
			vals[cnt].nul = 1;
			break;

		case 'X': /* response_timestamp */
			vals[cnt].val.time_val = time(0);
			break;

		default:
			LOG(L_CRIT, "BUG:acc:fmt2strar: unknown char: %c\n", *fmt);
			return 0;
		} /* switch (*fmt) */

		fmt++;
		cnt++;
	} /* while (*fmt) */

	return cnt;
}


int log_request(struct sip_msg *rq, struct hdr_field *to, char *table, unsigned int code, time_t req_timestamp)
{
	int cnt;
	if (skip_cancel(rq)) return 1;

        cnt = fmt2strar(log_fmt, rq, to, code, req_timestamp);
	if (cnt == 0) {
		LOG(L_ERR, "ERROR:acc:log_request: fmt2strar failed\n");
		return -1;
	}

	if (!db_url.len) {
		LOG(L_ERR, "ERROR:acc:log_request: can't log -- no db_url set\n");
		return -1;
	}

	if (acc_dbf.use_table(db_handle, table) < 0) {
		LOG(L_ERR, "ERROR:acc:log_request:Error in use_table\n");
		return -1;
	}
	
	if (acc_dbf.insert(db_handle, keys, vals, cnt) < 0) {
		LOG(L_ERR, "ERROR:acc:log_request:acc_request: Error while inserting to database\n");
		return -1;
	}
	
	return 1;
}


static void log_reply(struct cell* t , struct sip_msg* reply, unsigned int code, time_t req_time)
{
	log_request(t->uas.request, valid_to(t,reply), acc_table.s, code, req_time);
}


static void log_ack(struct cell* t , struct sip_msg *ack, time_t req_time)
{
	struct sip_msg *rq;
	struct hdr_field *to;

	rq = t->uas.request;
	if (ack->to) to = ack->to; 
	else to = rq->to;
	log_request(ack, to, acc_table.s, t->uas.status, req_time);
}


static void log_missed(struct cell* t, struct sip_msg* reply, unsigned int code, time_t req_time)
{
        log_request(t->uas.request, valid_to(t, reply), mc_table.s, code, req_time);
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_db_request(struct sip_msg *rq, char* comment, char* s2)
{
	preparse_req(rq);
	return log_request(rq, rq->to, acc_table.s, 0, time(0));
}


static void ack_handler(struct cell* t, int type, struct tmcb_params* ps)
{
	if (is_acc_on(t->uas.request)) {
		preparse_req(ps->req);
		log_ack(t, ps->req, (time_t)*(ps->param));
	}
}


/* initiate a report if we previously enabled MC accounting for this t */
static void failure_handler(struct cell *t, int type, struct tmcb_params* ps)
{
	/* validation */
	if (t->uas.request == 0) {
		DBG("DBG:acc:failure_handler: No uas.request, skipping local transaction\n");
		return;
	}

	if (is_invite(t) && ps->code >= 300) {
		if (is_mc_on(t->uas.request)) {
			log_missed(t, ps->rpl, ps->code, (time_t)*(ps->param));
			resetflag(t->uas.request, log_missed_flag);
		}
	}
}


/* initiate a report if we previously enabled accounting for this t */
static void replyout_handler(struct cell* t, int type, struct tmcb_params* ps)
{
	if (t->uas.request == 0) {
		DBG("DBG:acc:replyout_handler: No uas.request, local transaction, skipping\n");
		return;
	}
	
	     /* acc_onreply is bound to TMCB_REPLY which may be called
	      * from _reply, like when FR hits; we should not miss this
	      * event for missed calls either
	      */
	failure_handler(t, type, ps);
	if (!should_acc_reply(t, ps->code)) return;
	if (is_acc_on(t->uas.request)) log_reply(t, ps->rpl, ps->code, (time_t)*(ps->param));
}


/* parse incoming replies before cloning */
static void replyin_handler(struct cell *t, int type, struct tmcb_params* ps)
{
	     /* validation */
	if (t->uas.request == 0) {
		LOG(L_ERR, "ERROR:acc:replyin_handler:replyin_handler: 0 request\n");
		return;
	}
	
	     /* don't parse replies in which we are not interested */
	     /* missed calls enabled ? */
	if (((is_invite(t) && ps->code >= 300 && is_mc_on(t->uas.request)) 
	     || should_acc_reply(t, ps->code))
	    && (ps->rpl && ps->rpl != FAKED_REPLY)) {
		parse_headers(ps->rpl, HDR_TO_F, 0);
	}
}


/* prepare message and transaction context for later accounting */
static void on_req(struct cell* t, int type, struct tmcb_params *ps)
{
	time_t req_time;
	     /* Pass the timestamp of the request as a parameter to callbacks */
	req_time = time(0);

	if (is_acc_on(ps->req) || is_mc_on(ps->req)) {
		if (tmb.register_tmcb(0, t, TMCB_RESPONSE_OUT, replyout_handler, (void*)req_time) <= 0) {
			LOG(L_ERR, "ERROR:acc:on_req: Error while registering TMCB_RESPONSE_OUT callback\n");
			return;
		}

		if (report_ack) {
			if (tmb.register_tmcb(0, t, TMCB_E2EACK_IN, ack_handler, (void*)req_time) <= 0) {
				LOG(L_ERR, "ERROR:acc:on_req: Error while registering TMCB_E2EACK_IN callback\n");
				return;
			}
		}

		if (tmb.register_tmcb(0, t, TMCB_ON_FAILURE_RO, failure_handler, (void*)req_time) <= 0) {
			LOG(L_ERR, "ERROR:acc:on_req: Error while registering TMCB_ON_FAILURE_RO callback\n");
			return;
		}

		if (tmb.register_tmcb(0, t, TMCB_RESPONSE_IN, replyin_handler, (void*)req_time) <= 0) {
			LOG(L_ERR, "ERROR:acc:on_req: Error while registering TMCB_RESPONSE_IN callback\n");
			return;
		}
		
		     /* do some parsing in advance */
		preparse_req(ps->req);
		     /* also, if that is INVITE, disallow silent t-drop */
		if (ps->req->REQ_METHOD == METHOD_INVITE) {
			DBG("DEBUG: noisy_timer set for accounting\n");
			t->flags |= T_NOISY_CTIMER_FLAG;
		}
	}
}


static int child_init(int rank)
{
	if (db_url.s) {
		db_handle = acc_dbf.init(db_url.s);
		if (db_handle == 0) {
			LOG(L_ERR, "ERROR:acc:child_init: unable to connect to the database\n");
			return -1;
		}
		return 0;
	} else {
		LOG(L_CRIT, "BUG:acc:child_init: null db url\n");
		return -1;
	}
}


static void mod_destroy(void)
{
	if (db_handle && acc_dbf.close) acc_dbf.close(db_handle);
}


static int mod_init(void)
{
	load_tm_f load_tm;

	     /* import the TM auto-loading function */
	if ( !(load_tm = (load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LOG(L_ERR, "ERROR:acc:mod_init: can't import load_tm\n");
		return -1;
	}
	     /* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1) return -1;
	if (verify_fmt(log_fmt)==-1) return -1;
	
	     /* register callbacks*/
	     /* listen for all incoming requests  */
	if (tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, on_req, 0 ) <= 0) {
		LOG(L_ERR,"ERROR:acc:mod_init: cannot register TMCB_REQUEST_IN "
		    "callback\n");
		return -1;
	}
	
	if (regcomp(&attrs_re, attrs, REG_EXTENDED | REG_ICASE)) {
		LOG(L_ERR, "ERROR: acc: Cannot compile AVP regular expression\n");
		return -1;
	}

	if (bind_dbmod(db_url.s, &acc_dbf) < 0) {
		LOG(L_ERR, "ERROR:acc:mod_init: bind_db failed\n");
		return -1;
	}
	
	     /* Check database capabilities */
	if (!DB_CAPABILITY(acc_dbf, DB_CAP_INSERT)) {
		LOG(L_ERR, "ERROR:acc:mod_init: Database module does not implement insert function\n");
		return -1;
	}

	init_data(log_fmt);

	db_url.len = strlen(db_url.s);
	acc_table.len = strlen(acc_table.s);
	mc_table.len = strlen(mc_table.s);
	attrs_col.len = strlen(attrs_col.s);
	callid_col.len = strlen(callid_col.s);
	totag_col.len = strlen(totag_col.s);
	from_col.len = strlen(from_col.s);
	flags_col.len = strlen(flags_col.s);
	iuri_col.len = strlen(iuri_col.s);
	method_col.len = strlen(method_col.s);
	cseq_col.len = strlen(cseq_col.s);
	ouri_col.len = strlen(ouri_col.s);
	fromtag_col.len = strlen(fromtag_col.s);
	to_col.len = strlen(to_col.s);
	diguser_col.len = strlen(diguser_col.s);
	reqtimestamp_col.len = strlen(reqtimestamp_col.s);
	todid_col.len = strlen(todid_col.s);
	fromuri_col.len = strlen(fromuri_col.s);
	fromuid_col.len = strlen(fromuid_col.s);
	fromdid_col.len = strlen(fromdid_col.s);
	digrealm_col.len = strlen(digrealm_col.s);
	status_col.len = strlen(status_col.s);
	touri_col.len = strlen(touri_col.s);
	touid_col.len = strlen(touid_col.s);
	restimestamp_col.len = strlen(restimestamp_col.s);
	src_ip_col.len = strlen(src_ip_col.s);
	src_port_col.len = strlen(src_port_col.s);

	return 0;
}
