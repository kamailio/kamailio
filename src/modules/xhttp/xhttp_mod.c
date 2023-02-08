/*
 * Copyright (C) 2010-2017 Daniel-Constantin Mierla (asipto.com)
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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../core/sr_module.h"
#include "../../core/error.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../../core/dset.h"
#include "../../core/dprint.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/receive.h"
#include "../../core/msg_translator.h"
#include "../../modules/sl/sl.h"
#include "../../core/nonsip_hooks.h"
#include "../../core/action.h"
#include "../../core/script_cb.h"
#include "../../core/route.h"
#include "../../core/sip_msg_clone.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/tcp_options.h"
#include "../../core/kemi.h"

#include "api.h"
#include "xhttp_trans.h"

MODULE_VERSION

static int xhttp_handler(sip_msg_t *msg);
static int w_xhttp_send_reply(
		sip_msg_t *msg, char *pcode, char *preason, char *pctype, char *pbody);
static int mod_init(void);

static int fixup_xhttp_reply(void **param, int param_no);

static int pv_get_huri(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

static int xhttp_route_no = DEFAULT_RT;
static char *xhttp_url_match = NULL;
static regex_t xhttp_url_match_regexp;
static char *xhttp_url_skip = NULL;
static regex_t xhttp_url_skip_regexp;

/** SL API structure */
sl_api_t slb;

static str xhttp_event_callback = STR_NULL;

/* clang-format off */
static pv_export_t mod_pvs[] = {
	{{"hu", (sizeof("hu")-1)}, /* */
		PVT_OTHER, pv_get_huri, 0,
		0, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static tr_export_t mod_trans[] = {
	{ {"url", sizeof("url")-1},
		xhttp_tr_parse_url },

	{ { 0, 0 }, 0 }
};

static param_export_t params[] = {
	{"url_match",       PARAM_STRING, &xhttp_url_match},
	{"url_skip",        PARAM_STRING, &xhttp_url_skip},
	{"event_callback",  PARAM_STR,    &xhttp_event_callback},
	{0, 0, 0}
};

static cmd_export_t cmds[] = {
	{"xhttp_reply",    (cmd_function)w_xhttp_send_reply,
		4, fixup_xhttp_reply,  0, REQUEST_ROUTE},
	{"bind_xhttp",     (cmd_function)bind_xhttp,
		0, 0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0}
};

/** module exports */
struct module_exports exports= {
	"xhttp",         /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* exported rpc functions */
	mod_pvs,         /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module init function */
	0,               /* per-child init function */
	0                /* module destroy function */
};
/* clang-format on */

/**
 *
 */
static int mod_init(void)
{
	struct nonsip_hook nsh;
	int route_no;
	sr_kemi_eng_t *keng = NULL;

	if(xhttp_event_callback.s != NULL && xhttp_event_callback.len > 0) {
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_ERR("failed to find kemi engine\n");
			return -1;
		}
		xhttp_route_no = -1;
	} else {
		route_no = route_lookup(&event_rt, "xhttp:request");
		if(route_no == -1) {
			LM_ERR("failed to find event_route[xhttp:request]\n");
			return -1;
		}
		if(event_rt.rlist[route_no] == 0) {
			LM_WARN("event_route[xhttp:request] is empty\n");
		}
		xhttp_route_no = route_no;
	}

	if(cfg_get(tcp, tcp_cfg, accept_no_cl)==0) {
		LM_WARN("tcp_accept_no_cl not set - usually required"
				" to handle HTTP requests with no Content-Length\n");
	}

	/* bind the SL API */
	if(sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* register non-sip hooks */
	memset(&nsh, 0, sizeof(nsh));
	nsh.name = "xhttp";
	nsh.destroy = 0;
	nsh.on_nonsip_req = xhttp_handler;
	if(register_nonsip_msg_hook(&nsh) < 0) {
		LM_ERR("Failed to register non sip msg hooks\n");
		return -1;
	}

	if(xhttp_url_match != NULL) {
		memset(&xhttp_url_match_regexp, 0, sizeof(regex_t));
		if(regcomp(&xhttp_url_match_regexp, xhttp_url_match, REG_EXTENDED)
				!= 0) {
			LM_ERR("bad match re %s\n", xhttp_url_match);
			return E_BAD_RE;
		}
	}
	if(xhttp_url_skip != NULL) {
		memset(&xhttp_url_skip_regexp, 0, sizeof(regex_t));
		if(regcomp(&xhttp_url_skip_regexp, xhttp_url_skip, REG_EXTENDED) != 0) {
			LM_ERR("bad skip re %s\n", xhttp_url_skip);
			return E_BAD_RE;
		}
	}
	return 0;
}


/**
 *
 */
static int pv_get_huri(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	if(msg == NULL || res == NULL)
		return -1;

	return pv_get_strval(msg, param, res, &msg->first_line.u.request.uri);
}


/**
 *
 */
static char *xhttp_to_sip(sip_msg_t *msg, int *new_msg_len)
{
	unsigned int len, via_len;
	char *via, *new_msg, *p;
	str ip, port;
	struct hostport hp;
	struct dest_info dst;

	ip.s = ip_addr2strz(&msg->rcv.src_ip);
	ip.len = strlen(ip.s);
	port.s = int2str(msg->rcv.src_port, &port.len);
	hp.host = &ip;
	hp.port = &port;
	init_dst_from_rcv(&dst, &msg->rcv);
	via = via_builder(&via_len, NULL, &dst, 0, 0, &hp);
	if(via == 0) {
		LM_DBG("failed to build via\n");
		return 0;
	}
	len = via_len + msg->len;
	p = new_msg = pkg_malloc(len + 1);
	if(new_msg == 0) {
		PKG_MEM_ERROR_FMT(" (%d bytes)\n", len);
		pkg_free(via);
		return 0;
	}

	/* new message:
	 * <orig first line>
	 * Via: <faked via>
	 * <orig http message w/o the first line>
	 */
	memcpy(p, msg->first_line.u.request.method.s, msg->first_line.len);
	p += msg->first_line.len;
	memcpy(p, via, via_len);
	p += via_len;
	memcpy(p, SIP_MSG_START(msg) + msg->first_line.len,
			msg->len - msg->first_line.len);
	new_msg[len] = 0;
	pkg_free(via);
	*new_msg_len = len;
	return new_msg;
}


/**
 *
 */
static int xhttp_process_request(
		sip_msg_t *orig_msg, char *new_buf, unsigned int new_len)
{
	int ret;
	int backup_rt;
	sip_msg_t tmp_msg, *msg;
	struct run_act_ctx ra_ctx;
	sr_kemi_eng_t *keng = NULL;
	str evrtname = str_init("xhttp:request");

	ret = 0;
	backup_rt = get_route_type();
	if(new_buf && new_len) {
		memset(&tmp_msg, 0, sizeof(sip_msg_t));
		tmp_msg.buf = new_buf;
		tmp_msg.len = new_len;
		tmp_msg.rcv = orig_msg->rcv;
		tmp_msg.id = orig_msg->id;
		tmp_msg.set_global_address = orig_msg->set_global_address;
		tmp_msg.set_global_port = orig_msg->set_global_port;
		if(parse_msg(new_buf, new_len, &tmp_msg) != 0) {
			LM_ERR("parse_msg failed\n");
			goto error;
		}
		msg = &tmp_msg;
	} else {
		msg = orig_msg;
	}

	if((msg->first_line.type != SIP_REQUEST) || (msg->via1 == 0)
			|| (msg->via1->error != PARSE_OK)) {
		LM_CRIT("strange message: %.*s\n", msg->len, msg->buf);
		goto error;
	}

	set_route_type(EVENT_ROUTE);
	if(exec_pre_script_cb(msg, REQUEST_CB_TYPE) == 0) {
		goto done;
	}

	init_run_actions_ctx(&ra_ctx);
	if(xhttp_route_no >= 0) {
		if(run_actions(&ra_ctx, event_rt.rlist[xhttp_route_no], msg) < 0) {
			ret = -1;
			LM_DBG("error while trying script\n");
			goto done;
		}
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL) {
			if(sr_kemi_route(
					   keng, msg, EVENT_ROUTE, &xhttp_event_callback, &evrtname)
					< 0) {
				LM_ERR("error running event route kemi callback\n");
			}
		} else {
			LM_ERR("no event route or kemi callback found for execution\n");
		}
	}

done:
	exec_post_script_cb(msg, REQUEST_CB_TYPE);
	if(msg != orig_msg) {
		free_sip_msg(msg);
	}
	set_route_type(backup_rt);
	return ret;

error:
	return -1;
}


/**
 *
 */
static int xhttp_handler(sip_msg_t *msg)
{
	int ret;
	char *fake_msg;
	int fake_msg_len;
	regmatch_t pmatch;
	char c;

	ret = NONSIP_MSG_DROP;

	if(!IS_HTTP(msg)) {
		/* only http msg type */
		return NONSIP_MSG_PASS;
	}

	if(xhttp_url_skip != NULL || xhttp_url_match != NULL) {
		c = msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len];
		msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len] =
				'\0';
		if(xhttp_url_skip != NULL
				&& regexec(&xhttp_url_skip_regexp,
						   msg->first_line.u.request.uri.s, 1, &pmatch, 0)
						   == 0) {
			LM_DBG("URL matched skip re\n");
			msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len] =
					c;
			return NONSIP_MSG_PASS;
		}
		if(xhttp_url_match != NULL
				&& regexec(&xhttp_url_match_regexp,
						   msg->first_line.u.request.uri.s, 1, &pmatch, 0)
						   != 0) {
			LM_DBG("URL not matched\n");
			msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len] =
					c;
			return NONSIP_MSG_PASS;
		}
		msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len] = c;
	}

	if(msg->via1 == 0) {
		fake_msg = xhttp_to_sip(msg, &fake_msg_len);
		if(fake_msg == 0) {
			LM_ERR("out of memory\n");
			ret = NONSIP_MSG_ERROR;
		} else {
			DBG("new fake msg created (%d bytes):\n<%.*s>\n", fake_msg_len,
					fake_msg_len, fake_msg);
			if(xhttp_process_request(msg, fake_msg, fake_msg_len) < 0) {
				ret = NONSIP_MSG_ERROR;
			}
			pkg_free(fake_msg);
		}
		return ret;
	} else {
		LM_DBG("http msg unchanged (%d bytes):\n<%.*s>\n", msg->len, msg->len,
				msg->buf);
		if(xhttp_process_request(msg, 0, 0) < 0)
			ret = NONSIP_MSG_ERROR;
		return ret;
	}
}


/**
 *
 */
static int xhttp_send_reply(
		sip_msg_t *msg, int code, str *reason, str *ctype, str *body)
{
	str tbuf;

	if(ctype != NULL && ctype->len > 0) {
		/* add content-type */
		tbuf.len = sizeof("Content-Type: ") - 1 + ctype->len + CRLF_LEN;
		tbuf.s = pkg_malloc(sizeof(char) * (tbuf.len));

		if(tbuf.s == 0) {
			PKG_MEM_ERROR;
			return -1;
		}
		memcpy(tbuf.s, "Content-Type: ", sizeof("Content-Type: ") - 1);
		memcpy(tbuf.s + sizeof("Content-Type: ") - 1, ctype->s, ctype->len);
		memcpy(tbuf.s + sizeof("Content-Type: ") - 1 + ctype->len, CRLF,
				CRLF_LEN);
		if(add_lump_rpl(msg, tbuf.s, tbuf.len, LUMP_RPL_HDR) == 0) {
			LM_ERR("failed to insert content-type lump\n");
			pkg_free(tbuf.s);
			return -1;
		}
		pkg_free(tbuf.s);
		LM_DBG("response with content-type: %.*s\n", ctype->len, ctype->s);
	}

	if(body != NULL && body->len > 0) {
		if(add_lump_rpl(msg, body->s, body->len, LUMP_RPL_BODY) == 0) {
			LM_ERR("Error while adding reply lump\n");
			return -1;
		}
		LM_DBG("response with body: %.*s\n", body->len, body->s);
	}
	LM_DBG("sending out response: %d %.*s\n", code, reason->len, reason->s);
	if(slb.sreply(msg, code, reason) < 0) {
		LM_ERR("Error while sending reply\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static int w_xhttp_send_reply(
		sip_msg_t *msg, char *pcode, char *preason, char *pctype, char *pbody)
{
	str body = {0, 0};
	str reason = {"OK", 2};
	str ctype = {"text/plain", 10};
	int code = 200;

	if(pcode == 0 || preason == 0 || pctype == 0 || pbody == 0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)pcode, &code) != 0) {
		LM_ERR("no reply code value\n");
		return -1;
	}
	if(code < 100 || code > 700) {
		LM_ERR("invalid code parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)preason, &reason) != 0) {
		LM_ERR("unable to get reason\n");
		return -1;
	}
	if(reason.s == NULL || reason.len == 0) {
		LM_ERR("invalid reason parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pctype, &ctype) != 0) {
		LM_ERR("unable to get content type\n");
		return -1;
	}
	if(ctype.s == NULL) {
		LM_ERR("invalid content-type parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pbody, &body) != 0) {
		LM_ERR("unable to get body\n");
		return -1;
	}
	if(body.s == NULL) {
		LM_ERR("invalid body parameter\n");
		return -1;
	}

	if(xhttp_send_reply(msg, code, &reason, &ctype, &body) < 0)
		return -1;
	return 1;
}


/**
 *
 */
static int fixup_xhttp_reply(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_igp_null(param, 1);
	} else if(param_no == 2) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 3) {
		return fixup_spve_null(param, 1);
	} else if(param_no == 4) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}

/**
 *
 */
int bind_xhttp(xhttp_api_t *api)
{
	if(!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->reply = xhttp_send_reply;
	return 0;
}

/**
 *
 */
static sr_kemi_xval_t _sr_kemi_xhttp_xval = {0};

/**
 *
 */
static sr_kemi_xval_t* ki_xhttp_get_hu(sip_msg_t *msg)
{
	memset(&_sr_kemi_xhttp_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_xhttp_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_xhttp_xval;
	}

	_sr_kemi_xhttp_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_xhttp_xval.v.s = msg->first_line.u.request.uri;
	return &_sr_kemi_xhttp_xval;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_xhttp_exports[] = {
	{ str_init("xhttp"), str_init("xhttp_reply"),
		SR_KEMIP_INT, xhttp_send_reply,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp"), str_init("get_hu"),
		SR_KEMIP_XVAL, ki_xhttp_get_hu,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_xhttp_exports);
	return register_trans_mod(path, mod_trans);
}

/** @} */
