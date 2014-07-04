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
#include "../../id.h"
#include "attrs.h"
#include "../../modules/tm/tm_load.h"

/*
 * TODO:
 * - Quote attribute values properly
 * - Save request timestamp
 * - Save response timestamp
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
 * s: server_id
 * t: sip_to
 * u: digest_username
 * x: request_timestamp
 * D: to_did
 * F: from_uri
 * I: from_uid
 * M: from_did
 * R: digest_realm
 * P: source_port
 * S: sip_status
 * T: to_uri
 * U: to_uid
 * X: response_timestamp
 */

#define ALL_LOG_FMT "acdfgimnoprstuxDFIMPRSTUX"
#define ALL_LOG_FMT_LEN (sizeof(ALL_LOG_FMT) - 1)

#define A_SEPARATOR ", " /* must be shorter than ACC! */
#define A_SEPARATOR_LEN (sizeof(A_SEPARATOR) - 1)
#define A_EQ "="
#define A_EQ_LEN (sizeof(A_EQ) - 1)
#define A_EOL "\n\0"
#define A_EOL_LEN (sizeof(A_EOL) - 1)

#define ACC "ACC: "  /* Prefix of accounting messages in syslog */
#define ACC_LEN (sizeof(ACC) - 1)

#define ACC_REQUEST "request accounted: "
#define ACC_MISSED "call missed: "
#define ACC_ANSWERED "transaction answered: "
#define ACC_ACKED "request acknowledged: "

#define NA "n/a"

#define ATR(atr) atr_arr[cnt].s = A_##atr; atr_arr[cnt].len = sizeof(A_##atr) - 1;

#define A_ATTRS        "attrs"
#define A_CALLID       "callid"
#define A_TOTAG        "to_tag"
#define A_FROM         "from"
#define A_FLAGS        "flags"
#define A_IURI         "in_ruri"
#define A_METHOD       "sip_method"
#define A_CSEQ         "cseq"
#define A_OURI         "out_ruri"
#define A_FROMTAG      "from_tag"
#define A_TO           "to"
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
#define A_SERVERID     "server_id"

MODULE_VERSION

struct tm_binds tmb;

static int mod_init( void );
static int fix_log_flag( modparam_t type, void* val);
static int fix_log_missed_flag( modparam_t type, void* val);

static int log_level = L_NOTICE; /* noisiness level logging facilities are used */

static int early_media = 0;         /* Enable/disable early media (183) accounting */
static int failed_transactions = 0; /* Enable/disable accounting of failed (>= 300) transactions */
static int report_cancels = 0;      /* Enable/disable CANCEL reporting */
static int report_ack = 0;          /* Enable/disable end-to-end ACK reports */
static int log_flag = 0;            /* Flag that marks transactions to be accounted */
static int log_missed_flag = 0;     /* Transaction having this flag set will be accounted in missed calls when fails */
static char* log_fmt = ALL_LOG_FMT; /* Formating string that controls what information will be collected and accounted */

/* Attribute-value pairs */
static char* attrs = "";
avp_ident_t* avps;
int avps_n;

static int acc_log_request0(struct sip_msg *rq, char *p1, char *p2);
static int acc_log_missed0(struct sip_msg *rq, char *p1, char *p2);
static int acc_log_request1(struct sip_msg *rq, char *p1, char *p2);
static int acc_log_missed1(struct sip_msg *rq, char *p1, char *p2);

static str na = STR_STATIC_INIT(NA);


static cmd_export_t cmds[] = {
	{"acc_syslog_log",    acc_log_request0, 0, 0,               REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_syslog_missed", acc_log_missed0,  0, 0,               REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_syslog_log",    acc_log_request1, 1, fixup_var_str_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_syslog_missed", acc_log_missed1,  1, fixup_var_str_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


static param_export_t params[] = {
	{"early_media",		PARAM_INT, &early_media         },
	{"failed_transactions",	PARAM_INT, &failed_transactions },
	{"report_ack",		PARAM_INT, &report_ack          },
	{"report_cancels",	PARAM_INT, &report_cancels 	},
	{"log_flag",		PARAM_INT, &log_flag         	},
	{"log_flag",		PARAM_STRING|PARAM_USE_FUNC, fix_log_flag},
	{"log_missed_flag",	PARAM_INT, &log_missed_flag	},
	{"log_missed_flag",	PARAM_STRING|PARAM_USE_FUNC, fix_log_missed_flag},
	{"log_level",		PARAM_INT, &log_level           },
	{"log_fmt",		PARAM_STRING, &log_fmt          },
	{"attrs",               PARAM_STRING, &attrs            },
	{0, 0, 0}
};


struct module_exports exports= {
	"acc_syslog",
	cmds,     /* exported functions */
	0,        /* RPC methods */
	params,   /* exported params */
	mod_init, /* initialization module */
	0,	  /* response function */
	0,        /* destroy function */
	0,	  /* oncancel function */
	0         /* per-child init function */
};



/* fixes log_flag param (resolves possible named flags) */
static int fix_log_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "acc_syslog", "log_flag", &log_flag);
}



/* fixes log_missed_flag param (resolves possible named flags) */
static int fix_log_missed_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "acc_syslog", "log_missed_flag", &log_missed_flag);
}


#define TM_BUF_LEN sizeof("10000-12-31 23:59:59")

static inline int convert_time(str* buf, time_t time)
{
	struct tm* tm;

	if (!buf->s || buf->len < TM_BUF_LEN) {
		LOG(L_ERR, "ERROR:acc:convert_time: Buffer too short\n");
		return -1;
	}

	tm = gmtime(&time);
	buf->len = strftime(buf->s, buf->len, "%Y-%m-%d %H:%M:%S", tm);
	return 0;
}


static inline int skip_cancel(struct sip_msg *msg)
{
        return (msg->REQ_METHOD == METHOD_CANCEL) && report_cancels == 0;
}


/*
 * Append a constant string, uses sizeof to figure the length
 * of the string
 */
#define append(buf, ptr)                         \
    do {                                         \
        memcpy((buf).s, (ptr), sizeof(ptr) - 1); \
        (buf).s += sizeof(ptr) - 1;              \
        (buf).len -= sizeof(ptr) - 1;            \
    } while(0);


#define str_append_str(buf, str)			  \
    do {                                      \
        memcpy((buf).s, (str).s, (str).len);  \
        (buf).s += (str).len;                 \
        (buf).len -= (str).len;               \
    } while(0);


int verify_fmt(char *fmt) {

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


/* create an array of str's for accounting using a formatting string;
 * this is the heart of the accounting module -- it prints whatever
 * requested in a way, that can be used for syslog, radius,
 * sql, whatsoever
 * tm sip_msg_clones does not clone (shmmem-zed) parsed fields, other then Via1,2. Such fields clone now or use from rq_rp
 */
static int fmt2strar(char *fmt,             /* what would you like to account ? */
		     struct sip_msg *rq,    /* accounted message */
		     str* ouri,             /* Outbound Request-URI */
		     struct hdr_field *to,  /* To header field (used to extract tag) */
		     str *phrase,
		     int *total_len,        /* total length of accounted values */
		     int *attr_len,         /* total length of accounted attribute names */
		     str **val_arr,         /* that's the output -- must have MAX_ACC_COLUMNS */
		     str *atr_arr,
		     time_t req_time)       /* Timestamp of the request */
{
	static char flags_buf[INT2STR_MAX_LEN], tm_buf[TM_BUF_LEN],
		rqtm_buf[TM_BUF_LEN], srcip_buf[IP_ADDR_MAX_STR_SIZE],
		srcport_buf[INT2STR_MAX_LEN], serverid_buf[INT2STR_MAX_LEN];
	int cnt, tl, al;
	struct to_body* from, *pto;
	static str mycode, flags, tm_s, rqtm_s, src_ip, src_port, from_uid, to_uid, server_id_str;
	str *cr, *at;
	struct cseq_body *cseq;
	char* p;

	cnt = tl = al = 0;

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
			at = print_attrs(avps, avps_n, 1);
			if (!at) {
				val_arr[cnt] = &na;
			} else {
				val_arr[cnt] = at;
			}
			ATR(ATTRS);
			break;

		case 'c': /* sip_callid */
			val_arr[cnt] = (rq->callid && rq->callid->body.len) ? &rq->callid->body : &na;
			ATR(CALLID);
			break;

		case 'd': /* to_tag */
			val_arr[cnt] = (to && (pto = (struct to_body*)(to->parsed)) && pto->tag_value.len) ? & pto->tag_value : &na;
			ATR(TOTAG);
			break;

		case 'f': /* sip_from */
			val_arr[cnt] = (rq->from && rq->from->body.len) ? &rq->from->body : &na;
			ATR(FROM);
			break;

		case 'g': /* flags */
			p = int2str(rq->flags, &flags.len);
			memcpy(flags_buf, p, flags.len);
			flags.s = flags_buf;
			val_arr[cnt] = &flags;
			ATR(FLAGS);
			break;

		case 'i': /* inbound_ruri */
			val_arr[cnt] = &rq->first_line.u.request.uri;
			ATR(IURI);
			break;

		case 'm': /* sip_method */
			val_arr[cnt] = &rq->first_line.u.request.method;
			ATR(METHOD);
			break;

		case 'n': /* sip_cseq */
			if (rq->cseq && (cseq = get_cseq(rq)) && cseq->number.len) val_arr[cnt] = &cseq->number;
			else val_arr[cnt]=&na;
			ATR(CSEQ);
			break;

		case 'o': /* outbound_ruri */
		        val_arr[cnt] = ouri;
			ATR(OURI);
			break;

		case 'p': /* source_ip */
			     /* We need to make a copy of the string here because ip_addr2a uses static
			      * buffer and subseqent calls to the function would destroy the result
			      */
			src_ip.s = srcip_buf;
			p = ip_addr2a(&rq->rcv.src_ip);
			src_ip.len = strlen(p);
			memcpy(src_ip.s, p, src_ip.len);
			val_arr[cnt] = &src_ip;
			ATR(SRCIP);
			break;

		case 'r': /* from_tag */
			if (rq->from && (from = get_from(rq)) && from->tag_value.len) {
				val_arr[cnt] = &from->tag_value;
			} else {
				val_arr[cnt] = &na;
			}
			ATR(FROMTAG);
			break;

		case 's': /* server_id */
			p = int2str(server_id, &server_id_str.len);
			memcpy(serverid_buf, p, server_id_str.len);
			server_id_str.s = serverid_buf;
			val_arr[cnt] = &server_id_str;
			ATR(SERVERID);
			break;

		case 't': /* sip_to */
			val_arr[cnt] = (to && to->body.len) ? &to->body : &na;
			ATR(TO);
			break;

		case 'u': /* digest_username */
			cr = cred_user(rq);
			if (cr) val_arr[cnt] = cr;
			else val_arr[cnt] = &na;
			ATR(DIGUSER);
			break;

		case 'x': /* request_timestamp */
			rqtm_s.s = rqtm_buf;
			rqtm_s.len = TM_BUF_LEN;
			convert_time(&rqtm_s, req_time);
			val_arr[cnt] = &rqtm_s;
			ATR(REQTIMESTAMP);
			break;

		case 'D': /* to_did */
			val_arr[cnt] = &na;
			ATR(TODID);
			break;

		case 'F': /* from_uri */
			if (rq->from && (from = get_from(rq)) && from->uri.len) {
				val_arr[cnt] = &from->uri;
			} else val_arr[cnt] = &na;
			ATR(FROMURI);
			break;

		case 'I': /* from_uid */
			if (get_from_uid(&from_uid, rq) < 0) {
				val_arr[cnt] = &na;
			} else {
				val_arr[cnt] = &from_uid;
			}
			ATR(FROMUID);
			break;

		case 'M': /* from_did */
			val_arr[cnt] = &na;
			ATR(FROMDID);
			break;

		case 'P': /* source_port */
			p = int2str(rq->rcv.src_port, &src_port.len);
			memcpy(srcport_buf, p, src_port.len);
			src_port.s = srcport_buf;
			val_arr[cnt] = &src_port;
			ATR(SRCPORT);
			break;

		case 'R': /* digest_realm */
			cr = cred_realm(rq);
			if (cr) val_arr[cnt] = cr;
			else val_arr[cnt] = &na;
			ATR(DIGREALM);
			break;

		case 'S': /* sip_status */
			if (phrase->len >= 3) {
				mycode.s = phrase->s;
				mycode.len = 3;
				val_arr[cnt] = &mycode;
			} else val_arr[cnt] = &na;
			ATR(STATUS);
			break;

		case 'T': /* to_uri */
			if (rq->to && (pto = get_to(rq)) && pto->uri.len) val_arr[cnt] = &pto->uri;
			else val_arr[cnt] = &na;
			ATR(TOURI);
			break;

		case 'U': /* to_uid */
			if (get_to_uid(&to_uid, rq) < 0) {
				val_arr[cnt] = &na;
			} else {
				val_arr[cnt] = &to_uid;
			}
			ATR(TOUID);
			break;

		case 'X': /* response_timestamp */
			tm_s.s = tm_buf;
			tm_s.len = TM_BUF_LEN;
			convert_time(&tm_s, time(0));
			val_arr[cnt] = &tm_s;
			ATR(RESTIMESTAMP);
			break;

		default:
			LOG(L_CRIT, "BUG:acc:fmt2strar: unknown char: %c\n", *fmt);
			return 0;
		} /* switch (*fmt) */

		tl += val_arr[cnt]->len;
		al += atr_arr[cnt].len;
		fmt++;
		cnt++;
	} /* while (*fmt) */

	*total_len = tl;
	*attr_len = al;
	return cnt;
}



static int log_request(struct sip_msg* rq, str* ouri, struct hdr_field* to, str* txt, str* phrase, time_t req_time)
{
	static str* val_arr[ALL_LOG_FMT_LEN];
	static str atr_arr[ALL_LOG_FMT_LEN];
	int len, attr_cnt, attr_len, i;
	char *log_msg;
	str buf;

	if (skip_cancel(rq)) return 1;

	attr_cnt = fmt2strar(log_fmt, rq, ouri, to, phrase, &len, &attr_len, val_arr, atr_arr, req_time);
	if (!attr_cnt) {
		LOG(L_ERR, "ERROR:acc:log_request: fmt2strar failed\n");
		return -1;
	}

	len += attr_len + ACC_LEN + txt->len + A_EOL_LEN + attr_cnt * (A_SEPARATOR_LEN + A_EQ_LEN) - A_SEPARATOR_LEN;
	log_msg = pkg_malloc(len);
	if (!log_msg) {
		LOG(L_ERR, "ERROR:acc:log_request: No memory left for %d bytes\n", len);
		return -1;
	}

	     /* skip leading text and begin with first item's
	      * separator ", " which will be overwritten by the
	      * leading text later
	      * */
	buf.s = log_msg + ACC_LEN + txt->len - A_SEPARATOR_LEN;
	buf.len = len - ACC_LEN - txt->len + A_SEPARATOR_LEN;

	for (i = 0; i < attr_cnt; i++) {
		append(buf, A_SEPARATOR);
		str_append_str(buf, atr_arr[i]);
		append(buf, A_EQ);
		str_append_str(buf, *(val_arr[i]))
	}

	     /* terminating text */
	append(buf, A_EOL);

	     /* leading text */
	buf.s = log_msg;
	buf.len = len;
	append(buf, ACC);
	str_append_str(buf, *txt);

	LOG(log_level, "%s", log_msg);
	pkg_free(log_msg);
	return 1;
}


static void log_reply(struct cell* t , struct sip_msg* reply, unsigned int code, time_t req_time)
{
	str code_str, *ouri;
	static str lead = STR_STATIC_INIT(ACC_ANSWERED);
	static char code_buf[INT2STR_MAX_LEN];
	char* p;

	p = int2str(code, &code_str.len);
	memcpy(code_buf, p, code_str.len);
	code_str.s = code_buf;

	if (t->relayed_reply_branch >= 0) {
	    ouri = &t->uac[t->relayed_reply_branch].uri;
	} else {
	    ouri = GET_NEXT_HOP(t->uas.request);
	}

	log_request(t->uas.request, ouri, valid_to(t,reply), &lead, &code_str, req_time);
}


static void log_ack(struct cell* t , struct sip_msg *ack, time_t req_time)
{
	struct sip_msg *rq;
	struct hdr_field *to;
	static str lead = STR_STATIC_INIT(ACC_ACKED);
	static char code_buf[INT2STR_MAX_LEN];
	str code_str;
	char* p;

	rq = t->uas.request;
	if (ack->to) to = ack->to;
	else to = rq->to;
	p = int2str(t->uas.status, &code_str.len);
	memcpy(code_buf, p, code_str.len);
	code_str.s = code_buf;
	log_request(ack, GET_RURI(ack), to, &lead, &code_str, req_time);
}


static void log_missed(struct cell* t, struct sip_msg* reply, unsigned int code, time_t req_time)
{
        str acc_text, *ouri;
        static str leading_text = STR_STATIC_INIT(ACC_MISSED);

        get_reply_status(&acc_text, reply, code);
        if (acc_text.s == 0) {
                LOG(L_ERR, "ERROR:acc:log_missed: get_reply_status failed\n" );
                return;
        }

	if (t->relayed_reply_branch >= 0) {
	    ouri = &t->uac[t->relayed_reply_branch].uri;
	} else {
	    ouri = GET_NEXT_HOP(t->uas.request);
	}

        log_request(t->uas.request, ouri, valid_to(t, reply), &leading_text, &acc_text, req_time);
        pkg_free(acc_text.s);
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_log_request1(struct sip_msg *rq, char* p1, char* p2)
{
	str phrase;
	str txt = STR_STATIC_INIT(ACC_REQUEST);

	if (get_str_fparam(&phrase, rq, (fparam_t*)p1) < 0) {
	    phrase.s = 0;
	    phrase.len = 0;
	}
	preparse_req(rq);
	return log_request(rq, GET_RURI(rq), rq->to, &txt, &phrase, time(0));
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_log_missed1(struct sip_msg *rq, char* p1, char* p2)
{
	str phrase;
	str txt = STR_STATIC_INIT(ACC_MISSED);

	if (get_str_fparam(&phrase, rq, (fparam_t*)p1) < 0) {
	    phrase.s = 0;
	    phrase.len = 0;
	}
	preparse_req(rq);
	return log_request(rq, GET_RURI(rq), rq->to, &txt, &phrase, time(0));
}



/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_log_request0(struct sip_msg *rq, char* p1, char* p2)
{
	static str phrase = STR_NULL;
	str txt = STR_STATIC_INIT(ACC_REQUEST);
	return log_request(rq, GET_RURI(rq), rq->to, &txt, &phrase, time(0));
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_log_missed0(struct sip_msg *rq, char* p1, char* p2)
{
	static str phrase = STR_NULL;
	str txt = STR_STATIC_INIT(ACC_MISSED);
	preparse_req(rq);
	return log_request(rq, GET_RURI(rq), rq->to, &txt, &phrase, time(0));
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
void on_req(struct cell* t, int type, struct tmcb_params *ps)
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


static int mod_init(void)
{
	load_tm_f load_tm;

	     /* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
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

	if (parse_attrs(&avps, &avps_n, attrs) < 0) {
		ERR("Error while parsing 'attrs' module parameter\n");
		return -1;
	}

	return 0;
}
