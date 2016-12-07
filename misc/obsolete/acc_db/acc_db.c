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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../modules/tm/t_hooks.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/tm/h_table.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/digest/digest.h"
#include "../../usr_avp.h"
#include "../../modules/tm/tm_load.h"
#include "../../usr_avp.h"
#include "../../lib/srdb2/db.h"
#include "../../trim.h"
#include "../../id.h"
#include "../acc_syslog/attrs.h"

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

#define ALL_LOG_FMT "acdfgimnoprstuxDFIMPRSTUX"
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
#define A_SERVER_ID    "server_id"

MODULE_VERSION

struct tm_binds tmb;

static int mod_init(void);
static void mod_destroy(void);
static int child_init(int rank);
static int fix_log_flag( modparam_t type, void* val);
static int fix_log_missed_flag( modparam_t type, void* val);

static int early_media = 0;         /* Enable/disable early media (183) accounting */
static int failed_transactions = 0; /* Enable/disable accounting of failed (>= 300) transactions */
static int report_cancels = 0;      /* Enable/disable CANCEL reporting */
static int report_ack = 0;          /* Enable/disable end-to-end ACK reports */
static int log_flag = 0;            /* Flag that marks transactions to be accounted */
static int log_missed_flag = 0;     /* Transaction having this flag set will be accounted in missed calls when fails */
static char* log_fmt = ALL_LOG_FMT; /* Formating string that controls what information will be collected and accounted */

#define DEFAULT_ACC_TABLE "acc"
#define DEFAULT_MC_TABLE "missed_calls"

static str db_url = STR_STATIC_INIT(DEFAULT_DB_URL);
static str acc_table = STR_STATIC_INIT(DEFAULT_ACC_TABLE);
static str mc_table = STR_STATIC_INIT(DEFAULT_MC_TABLE);

static str attrs_col = STR_STATIC_INIT(A_ATTRS);
static str callid_col = STR_STATIC_INIT(A_CALLID);
static str totag_col = STR_STATIC_INIT(A_TOTAG);
static str from_col = STR_STATIC_INIT(A_FROM);
static str flags_col = STR_STATIC_INIT(A_FLAGS);
static str iuri_col = STR_STATIC_INIT(A_IURI);
static str method_col = STR_STATIC_INIT(A_METHOD);
static str cseq_col = STR_STATIC_INIT(A_CSEQ);
static str ouri_col = STR_STATIC_INIT(A_OURI);
static str fromtag_col = STR_STATIC_INIT(A_FROMTAG);
static str to_col = STR_STATIC_INIT(A_TO);
static str diguser_col = STR_STATIC_INIT(A_DIGUSER);
static str reqtimestamp_col = STR_STATIC_INIT(A_REQTIMESTAMP);
static str todid_col = STR_STATIC_INIT(A_TODID);
static str fromuri_col = STR_STATIC_INIT(A_FROMURI);
static str fromuid_col = STR_STATIC_INIT(A_FROMUID);
static str fromdid_col = STR_STATIC_INIT(A_FROMDID);
static str digrealm_col = STR_STATIC_INIT(A_DIGREALM);
static str status_col = STR_STATIC_INIT(A_STATUS);
static str touri_col = STR_STATIC_INIT(A_TOURI);
static str touid_col = STR_STATIC_INIT(A_TOUID);
static str restimestamp_col = STR_STATIC_INIT(A_RESTIMESTAMP);
static str src_ip_col = STR_STATIC_INIT(A_SRCIP);
static str src_port_col = STR_STATIC_INIT(A_SRCPORT);
static str server_id_col = STR_STATIC_INIT(A_SERVER_ID);

static db_ctx_t* acc_db = NULL;
static db_fld_t fld[sizeof(ALL_LOG_FMT) - 1];
static db_cmd_t* write_mc = NULL, *write_acc = NULL;


/* Attribute-value pairs */
static char* attrs = "";
avp_ident_t* avps;
int avps_n;

static int acc_db_request0(struct sip_msg *rq, char *p1, char *p2);
static int acc_db_missed0(struct sip_msg *rq, char *p1, char *p2);
static int acc_db_request1(struct sip_msg *rq, char *p1, char *p2);
static int acc_db_missed1(struct sip_msg *rq, char *p1, char *p2);

static cmd_export_t cmds[] = {
	{"acc_db_log",    acc_db_request0, 0, 0,               REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_db_missed", acc_db_missed0,  0, 0,               REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_db_log",    acc_db_request1, 1, fixup_var_int_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_db_missed", acc_db_missed1,  1, fixup_var_int_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


static param_export_t params[] = {
	{"early_media",		PARAM_INT, &early_media },
	{"failed_transactions",	PARAM_INT, &failed_transactions },
	{"report_ack",		PARAM_INT, &report_ack },
	{"report_cancels",	PARAM_INT, &report_cancels },
	{"log_flag",		PARAM_INT, &log_flag },
	{"log_flag",		PARAM_STRING|PARAM_USE_FUNC, fix_log_flag},
	{"log_missed_flag",	PARAM_INT, &log_missed_flag },
	{"log_missed_flag",	PARAM_STRING|PARAM_USE_FUNC, fix_log_missed_flag},
	{"log_fmt",		PARAM_STRING, &log_fmt },
	{"attrs",               PARAM_STRING, &attrs },
	{"db_url",              PARAM_STR, &db_url },
	{"acc_table",           PARAM_STR, &acc_table },
	{"mc_table",            PARAM_STR, &mc_table },
	{"attrs_column",        PARAM_STR, &attrs_col },
	{"callid_column",       PARAM_STR, &callid_col },
	{"totag_column",        PARAM_STR, &totag_col },
	{"from_column",         PARAM_STR, &from_col },
	{"flags_column",        PARAM_STR, &flags_col },
	{"iuri_column",         PARAM_STR, &iuri_col },
	{"method_column",       PARAM_STR, &method_col },
	{"cseq_column",         PARAM_STR, &cseq_col },
	{"ouri_column",         PARAM_STR, &ouri_col },
	{"fromtag_column",      PARAM_STR, &fromtag_col },
	{"to_column",           PARAM_STR, &to_col },
	{"diguser_column",      PARAM_STR, &diguser_col },
	{"reqtimestamp_column", PARAM_STR, &reqtimestamp_col },
	{"todid_column",        PARAM_STR, &todid_col },
	{"fromuri_column",      PARAM_STR, &fromuri_col },
	{"fromuid_column",      PARAM_STR, &fromuid_col },
	{"fromdid_column",      PARAM_STR, &fromdid_col },
	{"digrealm_column",     PARAM_STR, &digrealm_col },
	{"status_column",       PARAM_STR, &status_col },
	{"touri_column",        PARAM_STR, &touri_col },
	{"touid_column",        PARAM_STR, &touid_col },
	{"restimestamp_column", PARAM_STR, &restimestamp_col },
	{"src_ip_column",       PARAM_STR, &src_ip_col },
	{"src_port_column",     PARAM_STR, &src_port_col },
	{"server_id_column",    PARAM_STR, &server_id_col},
	{0, 0, 0}
};


struct module_exports exports= {
	"acc_db",
	cmds,        /* exported functions */
	0,           /* RPC methods */
	params,      /* exported params */
	mod_init,    /* initialization module */
	0,	     /* response function */
	mod_destroy, /* destroy function */
	0,	     /* oncancel function */
	child_init   /* per-child init function */
};



/* fixes log_flag param (resolves possible named flags) */
static int fix_log_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "acc_db", "log_flag", &log_flag);
}



/* fixes log_missed_flag param (resolves possible named flags) */
static int fix_log_missed_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "acc_db", "log_missed_flag", &log_missed_flag);
}



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
	int i = 0;

	memset(fld, '\0', sizeof(fld));
	while(*fmt) {
		switch(*fmt) {
		case 'a': /* attr */
			fld[i].name = attrs_col.s;
			fld[i].type = DB_STR;
			break;

		case 'c': /* callid */
			fld[i].type = DB_STR;
			fld[i].name = callid_col.s;
			break;

		case 'd': /* To tag */
			fld[i].type = DB_STR;
			fld[i].name = totag_col.s;
			break;

		case 'f': /* From */
			fld[i].type = DB_STR;
			fld[i].name = from_col.s;
			break;

		case 'g': /* flags */
			fld[i].type = DB_INT;
			fld[i].name = flags_col.s;
			break;

		case 'i': /* Inbound ruri */
			fld[i].type = DB_STR;
		    fld[i].name = iuri_col.s;
			break;

		case 'm': /* method */
			fld[i].type = DB_STR;
			fld[i].name = method_col.s;
			break;

		case 'n': /* sip_cseq */
			fld[i].type = DB_INT;
			fld[i].name = cseq_col.s;
			break;

		case 'o': /* outbound_ruri */
			fld[i].type = DB_STR;
			fld[i].name = ouri_col.s;
			break;

		case 'p': /* src_ip */
			fld[i].type = DB_INT;
			fld[i].name = src_ip_col.s;
			break;

		case 'r': /* from_tag */
			fld[i].type = DB_STR;
			fld[i].name = fromtag_col.s;
			break;

		case 't': /* sip_to */
			fld[i].type = DB_STR;
			fld[i].name = to_col.s;
			break;

		case 'u': /* digest_username */
			fld[i].type = DB_STR;
			fld[i].name = diguser_col.s;
			break;

		case 'x': /* request_timestamp */
			fld[i].type = DB_DATETIME;
			fld[i].name = reqtimestamp_col.s;
			break;

		case 'D': /* to_did */
			fld[i].type = DB_STR;
			fld[i].name = todid_col.s;
			break;

		case 'F': /* from_uri */
			fld[i].type = DB_STR;
			fld[i].name = fromuri_col.s;
			break;

		case 'I': /* from_uid */
			fld[i].type = DB_STR;
			fld[i].name = fromuid_col.s;
			break;

		case 'M': /* from_did */
		    fld[i].type = DB_STR;
			fld[i].name = fromdid_col.s;
			break;

		case 'P': /* src_port */
			fld[i].type = DB_INT;
			fld[i].name = src_port_col.s;
			break;

		case 'R': /* digest_realm */
			fld[i].type = DB_STR;
			fld[i].name = digrealm_col.s;
			break;

		case 'S': /* sip_status */
			fld[i].type = DB_INT;
			fld[i].name = status_col.s;
			break;

		case 'T': /* to_uri */
			fld[i].type = DB_STR;
			fld[i].name = touri_col.s;
			break;

		case 'U': /* to_uid */
			fld[i].type = DB_STR;
			fld[i].name = touid_col.s;
			break;

		case 'X': /* response_timestamp */
			fld[i].type = DB_DATETIME;
			fld[i].name = restimestamp_col.s;
			break;

		case 's': /* server_id */
			fld[i].type = DB_INT;
			fld[i].name = server_id_col.s;
		}

		fmt++;
		i++;
	}
	fld[i].name = NULL;
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
					 str* ouri,             /* Outbound Request-URI */
					 struct hdr_field *to,
					 unsigned int code,
					 time_t req_time,
					 db_fld_t* params)       /* Timestamp of the request */
{
	int cnt;
	struct to_body* from, *pto;
	str *cr, *at;
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

		params[cnt].flags &= ~DB_NULL;
		switch(*fmt) {
		case 'a': /* attr */
			at = print_attrs(avps, avps_n, 0);
			if (!at) {
				params[cnt].flags |= DB_NULL;
			} else {
				params[cnt].v.lstr = *at;
			}
			break;

		case 'c': /* sip_callid */
			if (rq->callid && rq->callid->body.len) {
				params[cnt].v.lstr = rq->callid->body;
			} else {
				params[cnt].flags |= DB_NULL;
			}
			break;

		case 'd': /* to_tag */
			if (to && (pto = (struct to_body*)(to->parsed)) && pto->tag_value.len) {
				params[cnt].v.lstr = pto->tag_value;
			} else {
				params[cnt].flags |= DB_NULL;
			}
			break;

		case 'f': /* sip_from */
			if (rq->from && rq->from->body.len) {
				params[cnt].v.lstr = rq->from->body;
			} else {
				params[cnt].flags |= DB_NULL;				
			}
			break;

		case 'g': /* flags */
			params[cnt].v.int4 = rq->flags;
			break;

		case 'i': /* inbound_ruri */
			params[cnt].v.lstr = rq->first_line.u.request.uri;
			break;

		case 'm': /* sip_method */
			params[cnt].v.lstr = rq->first_line.u.request.method;
			break;

		case 'n': /* sip_cseq */
			if (rq->cseq && (cseq = get_cseq(rq)) && cseq->number.len) {
				str2int(&cseq->number, (unsigned int*)&params[cnt].v.int4);
			} else {
				params[cnt].flags |= DB_NULL;
			}
			break;

		case 'o': /* outbound_ruri */
			params[cnt].v.lstr = *ouri;
			break;

		case 'p':
			params[cnt].v.int4 = rq->rcv.src_ip.u.addr32[0];
			break;

		case 'r': /* from_tag */
			if (rq->from && (from = get_from(rq)) && from->tag_value.len) {
				params[cnt].v.lstr = from->tag_value;
			} else {
				params[cnt].flags |= DB_NULL;
			}
			break;

		case 't': /* sip_to */
			if (to && to->body.len) params[cnt].v.lstr = to->body;
			else params[cnt].flags |= DB_NULL;
			break;

		case 'u': /* digest_username */
			cr = cred_user(rq);
			if (cr) params[cnt].v.lstr = *cr;
			else params[cnt].flags |= DB_NULL;
			break;

		case 'x': /* request_timestamp */
			params[cnt].v.time = req_time;
			break;

		case 'D': /* to_did */
			params[cnt].flags |= DB_NULL;
			break;

		case 'F': /* from_uri */
			if (rq->from && (from = get_from(rq)) && from->uri.len) {
				params[cnt].v.lstr = from->uri;
			} else params[cnt].flags |= DB_NULL;
			break;

		case 'I': /* from_uid */
			if (get_from_uid(&params[cnt].v.lstr, rq) < 0) {
				params[cnt].flags |= DB_NULL;
			}
			break;

		case 'M': /* from_did */
			params[cnt].flags |= DB_NULL;
			break;

		case 'P': /* source_port */
			params[cnt].v.int4 = rq->rcv.src_port;
			break;

		case 'R': /* digest_realm */
			cr = cred_realm(rq);
			if (cr) params[cnt].v.lstr = *cr;
			else params[cnt].flags |= DB_NULL;
			break;

		case 's': /* server_id */
			params[cnt].v.int4 = server_id;
			break;

		case 'S': /* sip_status */
			if (code > 0) params[cnt].v.int4 = code;
			else params[cnt].flags |= DB_NULL;
			break;

		case 'T': /* to_uri */
			if (rq->to && (pto = get_to(rq)) && pto->uri.len) params[cnt].v.lstr = pto->uri;
			else params[cnt].flags |= DB_NULL;
			break;

		case 'U': /* to_uid */
			if (get_to_uid(&params[cnt].v.lstr, rq) < 0) {
				params[cnt].flags |= DB_NULL;
			}
			break;

		case 'X': /* response_timestamp */
			params[cnt].v.time = time(0);
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


int log_request(struct sip_msg *rq, str* ouri, struct hdr_field *to, db_cmd_t* cmd, unsigned int code, time_t req_timestamp)
{
	int cnt;
	if (skip_cancel(rq)) return 1;

	cnt = fmt2strar(log_fmt, rq, ouri, to, code, req_timestamp, cmd->vals);
	if (cnt == 0) {
		LOG(L_ERR, "ERROR:acc:log_request: fmt2strar failed\n");
		return -1;
	}

	if (!db_url.len) {
		LOG(L_ERR, "ERROR:acc:log_request: can't log -- no db_url set\n");
		return -1;
	}

	if (db_exec(NULL, cmd) < 0) {
		ERR("Error while inserting to database\n");
		return -1;
	}

	return 1;
}


static void log_reply(struct cell* t , struct sip_msg* reply, unsigned int code, time_t req_time)
{
	str* ouri;
	
	if (t->relayed_reply_branch >= 0) {
	    ouri = &t->uac[t->relayed_reply_branch].uri;
	} else {
	    ouri = GET_NEXT_HOP(t->uas.request);
	}
	
	log_request(t->uas.request, ouri, valid_to(t,reply), write_acc, code, req_time);
}


static void log_ack(struct cell* t , struct sip_msg *ack, time_t req_time)
{
	struct sip_msg *rq;
	struct hdr_field *to;

	rq = t->uas.request;
	if (ack->to) to = ack->to;
	else to = rq->to;

	log_request(ack, GET_RURI(ack), to, write_acc, t->uas.status, req_time);
}


static void log_missed(struct cell* t, struct sip_msg* reply, unsigned int code, time_t req_time)
{
        str* ouri;

	if (t->relayed_reply_branch >= 0) {
	    ouri = &t->uac[t->relayed_reply_branch].uri;
	} else {
	    ouri = GET_NEXT_HOP(t->uas.request);
	}
	
	log_request(t->uas.request, ouri, valid_to(t, reply), write_mc, code, req_time);
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_db_request0(struct sip_msg *rq, char* s1, char* s2)
{
	preparse_req(rq);
	return log_request(rq, GET_RURI(rq), rq->to, write_acc, 0, time(0));
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_db_missed0(struct sip_msg *rq, char* s1, char* s2)
{
	preparse_req(rq);
	return log_request(rq, GET_RURI(rq), rq->to, write_mc, 0, time(0));
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_db_request1(struct sip_msg *rq, char* p1, char* p2)
{
    int code;

    if (get_int_fparam(&code, rq, (fparam_t*)p1) < 0) {
	code = 0;
    }
    preparse_req(rq);
    return log_request(rq, GET_RURI(rq), rq->to, write_acc, code, time(0));
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_db_missed1(struct sip_msg *rq, char* p1, char* p2)
{
    int code;

    if (get_int_fparam(&code, rq, (fparam_t*)p1) < 0) {
	code = 0;
    }
    preparse_req(rq);
    return log_request(rq, GET_RURI(rq), rq->to, write_mc, code, time(0));
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
		if (tmb.register_tmcb(0, t, TMCB_RESPONSE_OUT, replyout_handler,
								(void*)req_time, 0) <= 0) {
			LOG(L_ERR, "ERROR:acc:on_req: Error while registering TMCB_RESPONSE_OUT callback\n");
			return;
		}

		if (report_ack) {
			if (tmb.register_tmcb(0, t, TMCB_E2EACK_IN, ack_handler,
									(void*)req_time, 0) <= 0) {
				LOG(L_ERR, "ERROR:acc:on_req: Error while registering TMCB_E2EACK_IN callback\n");
				return;
			}
		}

		if (tmb.register_tmcb(0, t, TMCB_ON_FAILURE_RO, failure_handler,
								(void*)req_time, 0) <= 0) {
			LOG(L_ERR, "ERROR:acc:on_req: Error while registering TMCB_ON_FAILURE_RO callback\n");
			return;
		}

		if (tmb.register_tmcb(0, t, TMCB_RESPONSE_IN, replyin_handler,
								(void*)req_time, 0) <= 0) {
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
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if (db_url.s) {
		acc_db = db_ctx("acc_db");
		if (acc_db == NULL) {
			ERR("Error while initializing database layer\n");
			return -1;
		}

		if (db_add_db(acc_db, db_url.s) < 0) goto error;
		if (db_connect(acc_db) < 0) goto error;

		write_acc = db_cmd(DB_PUT, acc_db, acc_table.s, NULL, NULL, fld);
		if (write_acc == NULL) {
			ERR("Error while compiling database query\n");
			goto error;
		}

		write_mc = db_cmd(DB_PUT, acc_db, mc_table.s, NULL, NULL, fld);
		if (write_mc == NULL) {
			ERR("Error while compiling database query\n");
			goto error;
		}

		return 0;
	} else {
		LOG(L_CRIT, "BUG:acc:child_init: null db url\n");
		return -1;
	}
 error:
	if (write_acc) db_cmd_free(write_acc);
	write_acc = NULL;
	if (write_mc) db_cmd_free(write_mc);
	write_mc = NULL;
	if (acc_db) db_ctx_free(acc_db);
	acc_db = NULL;
	return -1;
}


static void mod_destroy(void)
{
	if (write_mc) db_cmd_free(write_mc);
	if (write_acc) db_cmd_free(write_acc);
	if (acc_db) {
		db_disconnect(acc_db);
		db_ctx_free(acc_db);
	}
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
	if (tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, on_req, 0, 0) <= 0) {
		LOG(L_ERR,"ERROR:acc:mod_init: cannot register TMCB_REQUEST_IN "
		    "callback\n");
		return -1;
	}

	init_data(log_fmt);

	if (parse_attrs(&avps, &avps_n, attrs) < 0) {
		ERR("Error while parsing 'attrs' module parameter\n");
		return -1;
	}

	return 0;
}
