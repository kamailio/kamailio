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
#include <radiusclient-ng.h>
#include "../../rad_dict.h"

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
#include "../../modules/tm/tm_load.h"

#include "../../parser/parse_rr.h"
#include "../../trim.h"
#include "../acc_syslog/attrs.h"

/*
 * FIXME:
 * - Quote attribute values properly
 * - Explicitly called accounting function will generate Acct-Status-Type
 *   set to failed (rad_status does not work properly in this case), it
 *   should generate Interim-Update
 * - INVITEs with to tag should generate Interim-Update
 * - Configurable option which allows to switch From/To based on the
 *   value of ftag route parameter
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

MODULE_VERSION

struct tm_binds tmb;

static int mod_init( void );
static int fix_log_flag( modparam_t type, void* val);
static int fix_log_missed_flag( modparam_t type, void* val);

static int early_media = 0;         /* Enable/disable early media (183) accounting */
static int failed_transactions = 0; /* Enable/disable accounting of failed (>= 300) transactions */
static int report_cancels = 0;      /* Enable/disable CANCEL reporting */
static int report_ack = 0;          /* Enable/disable end-to-end ACK reports */
static int log_flag = 0;            /* Flag that marks transactions to be accounted */
static int log_missed_flag = 0;     /* Transaction having this flag set will be accounted in missed calls when fails */
static char* log_fmt = ALL_LOG_FMT; /* Formating string that controls what information will be collected and accounted */

/* Attribute-value pairs */
static char* attrs_param = "";
avp_ident_t* avps;
int avps_n;

static char *radius_config = "/usr/local/etc/radiusclient-ng/radiusclient.conf";
static int service_type = -1;
static int swap_dir = 0;

static void *rh;
static struct attr attrs[A_MAX];
static struct val vals[V_MAX];

static int acc_rad_request0(struct sip_msg *rq, char *p1, char *p2);
static int acc_rad_missed0(struct sip_msg *rq, char *p1, char *p2);
static int acc_rad_request1(struct sip_msg *rq, char *p1, char *p2);
static int acc_rad_missed1(struct sip_msg *rq, char *p1, char *p2);

static cmd_export_t cmds[] = {
	{"acc_rad_log",    acc_rad_request0, 0, 0,               REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_rad_missed", acc_rad_missed0,  0, 0,               REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_rad_log",    acc_rad_request1, 1, fixup_var_int_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{"acc_rad_missed", acc_rad_missed1,  1, fixup_var_int_1, REQUEST_ROUTE | FAILURE_ROUTE},
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
	{"log_fmt",		PARAM_STRING, &log_fmt          },
	{"attrs",               PARAM_STRING, &attrs_param      },
	{"radius_config",	PARAM_STRING, &radius_config	},
	{"service_type", 	PARAM_INT, &service_type        },
	{"swap_direction",      PARAM_INT, &swap_dir            },
	{0, 0, 0}
};


struct module_exports exports= {
	"acc_radius",
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
	return fix_flag(type, val, "acc_radius", "log_flag", &log_flag);
}



/* fixes log_missed_flag param (resolves possible named flags) */
static int fix_log_missed_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "acc_radius", "log_missed_flag", &log_missed_flag);
}



static inline int skip_cancel(struct sip_msg *msg)
{
        return (msg->REQ_METHOD == METHOD_CANCEL) && report_cancels == 0;
}

static int check_ftag(struct sip_msg* msg, str* uri)
{
	param_hooks_t hooks;
	param_t* params;
	char* semi;
	struct to_body* from;
	str t;

	t = *uri;
	params = 0;
	semi = q_memchr(t.s, ';', t.len);
	if (!semi) {
		DBG("No ftag parameter found\n");
		return -1;
	}

	t.len -= semi - uri->s + 1;
	t.s = semi + 1;
	trim_leading(&t);

	if (parse_params(&t, CLASS_URI, &hooks, &params) < 0) {
		ERR("Error while parsing parameters\n");
		return -1;
	}

	if (!hooks.uri.ftag) {
		DBG("No ftag parameter found\n");
		goto err;
	}

	from = get_from(msg);

	if (!from || !from->tag_value.len || !from->tag_value.s) {
		DBG("No from tag parameter found\n");
		goto err;
	}

	if (from->tag_value.len == hooks.uri.ftag->body.len &&
	    !strncmp(from->tag_value.s, hooks.uri.ftag->body.s, hooks.uri.ftag->body.len)) {
		DBG("Route ftag and From tag are same\n");
		free_params(params);
		return 0;
	} else {
		DBG("Route ftag and From tag are NOT same\n");
		free_params(params);
		return 1;
	}

 err:
	if (params) free_params(params);
	return -1;
}

static int get_direction(struct sip_msg* msg)
{
	int ret;
	if (parse_orig_ruri(msg) < 0) {
		return -1;
	}

	if (!msg->parsed_orig_ruri_ok) {
		ERR("Error while parsing original Request-URI\n");
		return -1;
	}

	ret = check_self(&msg->parsed_orig_ruri.host,
			 msg->parsed_orig_ruri.port_no ? msg->parsed_orig_ruri.port_no : SIP_PORT, 0);/* match all protos*/
	if (ret < 0) return -1;
	if (ret > 0) {
		     /* Route is in ruri */
		return check_ftag(msg, &msg->first_line.u.request.uri);
	} else {
		if (msg->route) {
			if (parse_rr(msg->route) < 0) {
				ERR("Error while parsing Route HF\n");
				return -1;
			}
		        ret = check_ftag(msg, &((rr_t*)msg->route->parsed)->nameaddr.uri);
			if (msg->route->parsed) free_rr((rr_t**)(void*)&msg->route->parsed);
			return ret;
		} else {
			DBG("No Route headers found\n");
			return -1;
		}
	}
}


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
	parse_headers(rq, HDR_CALLID_F | HDR_FROM_F | HDR_TO_F | HDR_CSEQ_F | HDR_ROUTE_F, 0 );
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
static int fmt2rad(char *fmt,
		   struct sip_msg *rq,
		   str* ouri,
		   struct hdr_field *to,
		   unsigned int code,
		   VALUE_PAIR** send,
		   time_t req_time)       /* Timestamp of the request */
{
	static unsigned int cseq_num, src_port, src_ip;
	static time_t rq_time, rs_time;
	int cnt;
	struct to_body* from, *pto;
	str val, *cr, *at;
	struct cseq_body *cseq;
	struct attr* attr;
	int dir;

	cnt = 0;
	dir = -2;

	     /* we don't care about parsing here; either the function
	      * was called from script, in which case the wrapping function
	      * is supposed to parse, or from reply processing in which case
	      * TM should have preparsed from REQUEST_IN callback.
	      */
	while(*fmt) {
		if (cnt == ALL_LOG_FMT_LEN) {
			LOG(L_ERR, "ERROR:acc:fmt2rad: Formatting string is too long\n");
			return 0;
		}

		attr = 0;
		switch(*fmt) {
		case 'a': /* attr */
			at = print_attrs(avps, avps_n, 0);
			if (at) {
				attr = &attrs[A_SER_ATTR];
				val = *at;
			}
			break;

		case 'c': /* sip_callid */
			if (rq->callid && rq->callid->body.len) {
				attr = &attrs[A_ACCT_SESSION_ID];
				val = rq->callid->body;
			}
			break;

		case 'd': /* to_tag */
			if (swap_dir && dir == -2) dir = get_direction(rq);
			if (dir <= 0) {
				if (to && (pto = (struct to_body*)(to->parsed)) && pto->tag_value.len) {
					attr = &attrs[A_SIP_TO_TAG];
					val = pto->tag_value;
				}
			} else {
				if (rq->from && (from = get_from(rq)) && from->tag_value.len) {
					attr = &attrs[A_SIP_TO_TAG];
					val = from->tag_value;
				}
			}
			break;

		case 'f': /* sip_from */
			if (rq->from && rq->from->body.len) {
				attr = &attrs[A_SER_FROM];
				val = rq->from->body;
			}
			break;

		case 'g': /* flags */
			attr = &attrs[A_SER_FLAGS];
			val.s = (char*)&rq->flags;
			val.len = sizeof(unsigned int);
			break;

		case 'i': /* inbound_ruri */
			attr = &attrs[A_SER_ORIGINAL_REQUEST_ID];
			val = rq->first_line.u.request.uri;
			break;

		case 'm': /* sip_method */
			attr = &attrs[A_SIP_METHOD];
			val = rq->first_line.u.request.method;
			break;

		case 'n': /* sip_cseq */
			if (rq->cseq && (cseq = get_cseq(rq)) && cseq->number.len) {
				attr = &attrs[A_SIP_CSEQ];
				str2int(&cseq->number, &cseq_num);
				val.s = (char*)&cseq_num;
				val.len = sizeof(unsigned int);
			}
			break;

		case 'o': /* outbound_ruri */
			attr = &attrs[A_SIP_TRANSLATED_REQUEST_ID];
			val = *ouri;
			break;

		case 'p': /* Source IP address */
			attr = &attrs[A_SIP_SOURCE_IP_ADDRESS];
			src_ip = ntohl(rq->rcv.src_ip.u.addr32[0]);
			val.s = (char*)&src_ip;
			val.len = sizeof(src_ip);
			break;

		case 'r': /* from_tag */
			if (swap_dir && dir == -2) dir = get_direction(rq);
			if (dir <= 0) {
				if (rq->from && (from = get_from(rq)) && from->tag_value.len) {
					attr = &attrs[A_SIP_FROM_TAG];
					val = from->tag_value;
				}
			} else {
				if (to && (pto = (struct to_body*)(to->parsed)) && pto->tag_value.len) {
					attr = &attrs[A_SIP_FROM_TAG];
					val = pto->tag_value;
				}
			}
			break;

		case 's': /* server_id */
			attr = &attrs[A_SER_SERVER_ID];
			val.s = (char*)&server_id;
			val.len = sizeof(int);
			break;

		case 't': /* sip_to */
			if (to && to->body.len) {
				attr = &attrs[A_SER_TO];
				val = to->body;
			}
			break;

		case 'u': /* digest_username */
			cr = cred_user(rq);
			if (cr) {
				attr = &attrs[A_SER_DIGEST_USERNAME];
				val = *cr;
			}
			break;

		case 'x': /* request_timestamp */
			attr = &attrs[A_SER_REQUEST_TIMESTAMP];
			rq_time = req_time;
			val.s = (char*)&rq_time;
			val.len = sizeof(time_t);
			break;

		case 'D': /* to_did */
			break;

		case 'F': /* from_uri */
			if (swap_dir && dir == -2) dir = get_direction(rq);
			if (dir <= 0) {
				if (rq->from && (from = get_from(rq)) && from->uri.len) {
					attr = &attrs[A_CALLING_STATION_ID];
					val = from->uri;
				}
			} else {
				if (rq->to && (pto = get_to(rq)) && pto->uri.len) {
					attr = &attrs[A_CALLING_STATION_ID];
					val = pto->uri;
				}
			}
			break;

		case 'I': /* from_uid */
			if (get_from_uid(&val, rq) < 0) {
				attr = &attrs[A_SER_FROM_UID];
			}
			break;

		case 'M': /* from_did */
			break;

		case 'P': /* Source port */
			attr = &attrs[A_SIP_SOURCE_PORT];
			src_port = rq->rcv.src_port;
			val.s = (char*)&src_port;
			val.len = sizeof(unsigned int);
			break;

		case 'R': /* digest_realm */
			cr = cred_realm(rq);
			if (cr) {
				attr = &attrs[A_SER_DIGEST_REALM];
				val = *cr;
			}
			break;

		case 'S': /* sip_status */
			attr = &attrs[A_SIP_RESPONSE_CODE];
			val.s = (char*)&code;
			val.len = sizeof(unsigned int);
			break;

		case 'T': /* to_uri */
			if (swap_dir && dir == -2) dir = get_direction(rq);
			if (dir <= 0) {
				if (rq->to && (pto = get_to(rq)) && pto->uri.len) {
					attr = &attrs[A_CALLED_STATION_ID];
					val = pto->uri;
				}
			} else {
				if (rq->from && (from = get_from(rq)) && from->uri.len) {
					attr = &attrs[A_CALLED_STATION_ID];
					val = from->uri;
				}
			}
			break;

		case 'U': /* to_uid */
			if (get_from_uid(&val, rq) < 0) {
				attr = &attrs[A_SER_TO_UID];
			}
			break;

		case 'X': /* response_timestamp */
			attr = &attrs[A_SER_RESPONSE_TIMESTAMP];
			rs_time = time(0);
			val.s = (char*)&rs_time;
			val.len = sizeof(time_t);
			break;

		default:
			LOG(L_CRIT, "BUG:acc:fmt2rad: unknown char: %c\n", *fmt);
			return -1;
		} /* switch (*fmt) */

		if (attr) {
			if (!rc_avpair_add(rh, send, ATTRID(attr->v), val.s, val.len, VENDOR(attr->v))) {
				LOG(L_ERR, "ERROR:acc:fmt2rad: Failed to add attribute %s\n",
				    attr->n);
				return -1;
			}
		}

		fmt++;
		cnt++;
	} /* while (*fmt) */

	return 0;
}


/*
 * Return the value of Acc-Status-Type attribute based on SIP method
 * and response code
 */
static inline UINT4 rad_status(struct sip_msg *rq, unsigned int code)
{
	     /* Faked reply */
	if (code == 0) {
		return vals[V_FAILED].v;
	}

	     /* Successful call start */
	if ((rq->REQ_METHOD == METHOD_INVITE || rq->REQ_METHOD == METHOD_ACK)
	    && code >= 200 && code < 300) {
		return vals[V_START].v;
	}

	     /* Successful call termination */
	if ((rq->REQ_METHOD == METHOD_BYE || rq->REQ_METHOD == METHOD_CANCEL)) {
		return vals[V_STOP].v;
	}

	     /* Successful transaction */
	if (code >= 200 && code < 300) {
		return vals[V_INTERIM_UPDATE].v;
	}

	     /* Otherwise it failed */
	return vals[V_FAILED].v;
}


/*
 * Add User-Name attribute
 */
static inline int add_user_name(struct sip_msg* rq, void* rh, VALUE_PAIR** send)
{
	struct sip_uri puri;
	str* user, *realm;
	str user_name;
	struct to_body* from;

	user = cred_user(rq);  /* try to take it from credentials */
	realm = cred_realm(rq);

	if (!user || !realm) {
		if (rq->from && (from = get_from(rq)) && from->uri.len) {
			if (parse_uri(from->uri.s, from->uri.len, &puri) < 0 ) {
				LOG(L_ERR, "ERROR:acc:add_user_name: Bad From URI\n");
				return -1;
			}

			user = &puri.user;
			realm = &puri.host;
		} else {
			DBG("acc:add_user_name: Neither digest nor From found, mandatory attribute User-Name not added\n");
			return -1;
		}
	}

	user_name.len = user->len + 1 + realm->len;
	user_name.s = pkg_malloc(user_name.len);
	if (!user_name.s) {
		LOG(L_ERR, "ERROR:acc:add_user_name: no memory\n");
		return -1;
	}
	memcpy(user_name.s, user->s, user->len);
	user_name.s[user->len] = '@';
	memcpy(user_name.s + user->len + 1, realm->s, realm->len);

	if (!rc_avpair_add(rh, send, ATTRID(attrs[A_USER_NAME].v),
			   user_name.s, user_name.len, VENDOR(attrs[A_USER_NAME].v))) {
		LOG(L_ERR, "ERROR:acc:add_user_name: Failed to add User-Name attribute\n");
		pkg_free(user_name.s);
		return -1;
	}
	pkg_free(user_name.s);
	
	return 0;
}


/* skip leading text and begin with first item's
 * separator ", " which will be overwritten by the
 * leading text later
 *
 */
static int log_request(struct sip_msg* rq, str* ouri, struct hdr_field* to, unsigned int code, time_t req_time)
{
	VALUE_PAIR *send;
	UINT4 av_type;

	send = NULL;
	if (skip_cancel(rq)) return 1;

	if (fmt2rad(log_fmt, rq, ouri, to, code, &send, req_time) < 0) goto error;

	     /* Add Acct-Status-Type attribute */
	av_type = rad_status(rq, code);
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_ACCT_STATUS_TYPE].v), &av_type, -1, 
			   VENDOR(attrs[A_ACCT_STATUS_TYPE].v))) {
		ERR("Add Status-Type\n");
		goto error;
	}

	     /* Add Service-Type attribute */
	av_type = (service_type != -1) ? service_type : vals[V_SIP_SESSION].v;
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SERVICE_TYPE].v), &av_type, -1, 
			   VENDOR(attrs[A_SERVICE_TYPE].v))) {
		ERR("add STATUS_TYPE\n");
		goto error;
	}

	     /* Add User-Name attribute */
	if (add_user_name(rq, rh, &send) < 0) goto error;

	     /* Send the request out */
	if (rc_acct(rh, SIP_PORT, send) != OK_RC) {
		ERR("RADIUS accounting request failed\n");
		goto error;
	}

	rc_avpair_free(send);
	return 1;

error:
	rc_avpair_free(send);
	return -1;
}


static void log_reply(struct cell* t , struct sip_msg* reply, unsigned int code, time_t req_time)
{
        str* ouri;

	if (t->relayed_reply_branch >= 0) {
	    ouri = &t->uac[t->relayed_reply_branch].uri;
	} else {
	    ouri = GET_NEXT_HOP(t->uas.request);
	}

	log_request(t->uas.request, ouri, valid_to(t, reply), code, req_time);
}


static void log_ack(struct cell* t , struct sip_msg *ack, time_t req_time)
{
	struct sip_msg *rq;
	struct hdr_field *to;

	rq = t->uas.request;
	if (ack->to) to = ack->to;
	else to = rq->to;
	log_request(ack, GET_RURI(ack), to, t->uas.status, req_time);
}


static void log_missed(struct cell* t, struct sip_msg* reply, unsigned int code, time_t req_time)
{
        str* ouri;

	if (t->relayed_reply_branch >= 0) {
	    ouri = &t->uac[t->relayed_reply_branch].uri;
	} else {
	    ouri = GET_NEXT_HOP(t->uas.request);
	}

        log_request(t->uas.request, ouri , valid_to(t, reply), code, req_time);
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_rad_request0(struct sip_msg *rq, char* p1, char* p2)
{
	preparse_req(rq);
	return log_request(rq, GET_RURI(rq), rq->to, 0, time(0));
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_rad_missed0(struct sip_msg *rq, char* p1, char* p2)
{
	preparse_req(rq);
	return log_request(rq, GET_RURI(rq), rq->to, 0, time(0));
}

/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_rad_request1(struct sip_msg *rq, char* p1, char* p2)
{
    int code;
    preparse_req(rq);
    if (get_int_fparam(&code, rq, (fparam_t*)p1) < 0) {
	code = 0;
    }
    return log_request(rq, GET_RURI(rq), rq->to, code, time(0));
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int acc_rad_missed1(struct sip_msg *rq, char* p1, char* p2)
{
    int code;
    preparse_req(rq);
    if (get_int_fparam(&code, rq, (fparam_t*)p1) < 0) {
	code = 0;
    }
    return log_request(rq, GET_RURI(rq), rq->to, code, time(0));
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
	DICT_VENDOR *vend;
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

	memset(attrs, 0, sizeof(attrs));
	memset(vals, 0, sizeof(vals));

	attrs[A_USER_NAME].n		     = "User-Name";
	attrs[A_SERVICE_TYPE].n		     = "Service-Type";
	attrs[A_CALLED_STATION_ID].n	     = "Called-Station-Id";
	attrs[A_CALLING_STATION_ID].n	     = "Calling-Station-Id";
	attrs[A_ACCT_STATUS_TYPE].n	     = "Acct-Status-Type";
	attrs[A_ACCT_SESSION_ID].n	     = "Acct-Session-Id";

	attrs[A_SIP_METHOD].n		     = "Sip-Method";
	attrs[A_SIP_RESPONSE_CODE].n	     = "Sip-Response-Code";
	attrs[A_SIP_CSEQ].n		     = "Sip-CSeq";
	attrs[A_SIP_TO_TAG].n		     = "Sip-To-Tag";
	attrs[A_SIP_FROM_TAG].n		     = "Sip-From-Tag";
	attrs[A_SIP_TRANSLATED_REQUEST_ID].n = "Sip-Translated-Request-Id";
	attrs[A_SIP_SOURCE_IP_ADDRESS].n     = "Sip-Source-IP-Address";
	attrs[A_SIP_SOURCE_PORT].n           = "Sip-Source-Port";

	attrs[A_SER_ATTR].n                  = "SER-Attr";
	attrs[A_SER_FROM].n                  = "SER-From";
	attrs[A_SER_FLAGS].n                 = "SER-Flags";
	attrs[A_SER_ORIGINAL_REQUEST_ID].n   = "SER-Original-Request-Id";
	attrs[A_SER_TO].n                    = "SER-To";
	attrs[A_SER_DIGEST_USERNAME].n       = "SER-Digest-Username";
	attrs[A_SER_DIGEST_REALM].n          = "SER-Digest-Realm";
	attrs[A_SER_REQUEST_TIMESTAMP].n     = "SER-Request-Timestamp";
	attrs[A_SER_TO_DID].n                = "SER-To-DID";
	attrs[A_SER_FROM_UID].n              = "SER-From-UID";
	attrs[A_SER_FROM_DID].n              = "SER-From-DID";
	attrs[A_SER_TO_UID].n                = "SER-To-UID";
	attrs[A_SER_RESPONSE_TIMESTAMP].n    = "SER-Response-Timestamp";
	attrs[A_SER_SERVER_ID].n             = "SER-Server-ID";

	vals[V_START].n			     = "Start";
	vals[V_STOP].n			     = "Stop";
	vals[V_INTERIM_UPDATE].n             = "Interim-Update";
	vals[V_FAILED].n		     = "Failed";
	vals[V_SIP_SESSION].n		     = "Sip-Session";

	     /* open log */
	rc_openlog("ser");
	     /* read config */
	if ((rh = rc_read_config(radius_config)) == NULL) {
		LOG(L_ERR, "ERROR:acc:mod_init: Error opening radius config file: %s\n",
		    radius_config);
		return -1;
	}
	     /* read dictionary */
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
		LOG(L_ERR, "ERROR:acc:mod_init: Error reading radius dictionary\n");
		return -1;
	}

	vend = rc_dict_findvend(rh, "iptelorg");
	if (vend == NULL) {
		ERR("RADIUS dictionary is missing required vendor 'iptelorg'\n");
		return -1;
	}

	INIT_AV(rh, attrs, vals, "acc", -1, -1);

	if (service_type != -1) {
		vals[V_SIP_SESSION].v = service_type;
	}

	if (parse_attrs(&avps, &avps_n, attrs_param) < 0) {
		ERR("Error while parsing 'attrs' module parameter\n");
		return -1;
	}

	return 0;
}
