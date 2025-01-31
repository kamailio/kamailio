/**
 * Copyright (C) 2024 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <nghttp2/nghttp2.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"
#include "../../core/cfg/cfg_struct.h"

#include "nghttp2_server.h"

MODULE_VERSION

str _nghttp2_listen_port = str_init("8282");
str _nghttp2_listen_addr = str_init("");
str _nghttp2_tls_public_key = str_init("");
str _nghttp2_tls_private_key = str_init("");
int _nghttp2_server_pid = -1;

static int nghttp2_route_no = -1;
static str nghttp2_event_callback = STR_NULL;

static int w_nghttp2_send_reply(sip_msg_t *msg, char *pcode, char *pbody);
static int w_nghttp2_reply_header(sip_msg_t *msg, char *pname, char *pbody);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

int pv_get_nghttp2(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
int pv_parse_nghttp2_name(pv_spec_p sp, str *in);

ksr_nghttp2_ctx_t _ksr_nghttp2_ctx = {0};

/* clang-format off */
static pv_export_t mod_pvs[] = {
	{{"nghttp2", (sizeof("nghttp2") - 1)}, PVT_OTHER, pv_get_nghttp2, 0,
			pv_parse_nghttp2_name, 0, 0, 0},

	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static cmd_export_t cmds[] = {
	{"nghttp2_reply",    (cmd_function)w_nghttp2_send_reply,
		2, fixup_spve_all, fixup_free_spve_all, REQUEST_ROUTE|EVENT_ROUTE},
	{"nghttp2_reply_header",    (cmd_function)w_nghttp2_reply_header,
		2, fixup_spve_all, fixup_free_spve_all, REQUEST_ROUTE|EVENT_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"listen_port",     PARAM_STR,    &_nghttp2_listen_port},
	{"listen_addr",     PARAM_STR,    &_nghttp2_listen_addr},
	{"tls_public_key",	PARAM_STR,    &_nghttp2_tls_public_key},
	{"tls_private_key",	PARAM_STR,    &_nghttp2_tls_private_key},
	{"event_callback",  PARAM_STR,    &nghttp2_event_callback},
	{0, 0, 0}
};

struct module_exports exports = {
	"nghttp2",		 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* exported functions */
	params,			 /* exported parameters */
	0,				 /* exported rpc functions */
	mod_pvs,		 /* exported pseudo-variables */
	0,				 /* response handling function */
	mod_init,		 /* module init function */
	child_init,		 /* per child init function */
	mod_destroy		 /* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	sr_kemi_eng_t *keng = NULL;
	int route_no = -1;

	if(nghttp2_event_callback.s != NULL && nghttp2_event_callback.len > 0) {
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_ERR("failed to find kemi engine\n");
			return -1;
		}
		nghttp2_route_no = -1;
	} else {
		route_no = route_lookup(&event_rt, "nghttp2:request");
		if(route_no == -1) {
			LM_ERR("failed to find event_route[nghttp2:request]\n");
			return -1;
		}
		if(event_rt.rlist[route_no] == 0) {
			LM_WARN("event_route[nghttp2:request] is empty\n");
		}
		nghttp2_route_no = route_no;
	}

	/* add space for one extra process */
	register_procs(1);

	/* add child to update local config framework structures */
	cfg_register_child(1);

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	int pid;

	if(rank != PROC_MAIN)
		return 0;

	pid = fork_process(PROC_NOCHLDINIT, "NGHTTP2 Server Process", 1);
	if(pid < 0)
		return -1; /* error */
	if(pid == 0) {
		/* child */
		_nghttp2_server_pid = getpid();

		/* do child init to allow execution of rpc like functions */
		if(init_child(PROC_RPC) < 0) {
			LM_DBG("failed to do RPC child init for dispatcher\n");
			return -1;
		}
		/* initialize the config framework */
		if(cfg_child_init())
			return -1;
		if(nghttp2_server_run() < 0) {
			LM_ERR("failed to initialize nghttp2 server process\n");
			return -1;
		}
	}

	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 * parse the name of the $nghttp2(name)
 */
int pv_parse_nghttp2_name(pv_spec_p sp, str *in)
{
	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;
	switch(in->len) {
		case 4:
			if(strncasecmp(in->s, "path", 4) == 0) {
				sp->pvp.pvn.u.isname.name.n = 0;
			} else if(strncasecmp(in->s, "data", 4) == 0) {
				sp->pvp.pvn.u.isname.name.n = 1;
			} else if(strncasecmp(in->s, "size", 4) == 0) {
				sp->pvp.pvn.u.isname.name.n = 2;
			} else {
				goto error;
			}
			break;
		case 5:
			if(strncasecmp(in->s, "srcip", 5) == 0) {
				sp->pvp.pvn.u.isname.name.n = 5;
			} else {
				goto error;
			}
			break;
		case 6:
			if(strncasecmp(in->s, "method", 6) == 0) {
				sp->pvp.pvn.u.isname.name.n = 3;
			} else {
				goto error;
			}
			break;
		case 7:
			if(strncasecmp(in->s, "version", 7) == 0) {
				sp->pvp.pvn.u.isname.name.n = 4;
			} else {
				goto error;
			}
			break;
		case 8:
			if(strncasecmp(in->s, "pathfull", 8) == 0) {
				sp->pvp.pvn.u.isname.name.n = 6;
			} else {
				goto error;
			}
			break;

		default:
			if(in->len > 2 && in->s[1] == ':'
					&& (in->s[0] == 'h' || in->s[0] == 'H')) {
				sp->pvp.pvn.type = PV_NAME_INTSTR;
				sp->pvp.pvn.u.isname.type = PVT_HDR;
				sp->pvp.pvn.u.isname.name.s = *in;
				return 0;
			}
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("invalid variable name [%.*s]\n", in->len, in->s);
	return -1;
}

/**
 * return the value of $nghttp2(name)
 */
int pv_get_nghttp2(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	const char *hdrval = NULL;

	if(param == NULL) {
		return -1;
	}
	if(_ksr_nghttp2_ctx.session == NULL) {
		return pv_get_null(msg, param, res);
	}
	if(param->pvn.u.isname.type == PVT_HDR) {
		if(hdrval == NULL) {
			return pv_get_null(msg, param, res);
		}
		return pv_get_strzval(msg, param, res, (char *)hdrval);
	}

	switch(param->pvn.u.isname.name.n) {
		case 0: /* path */
			return pv_get_strval(msg, param, res, &_ksr_nghttp2_ctx.path);
		case 1: /* data */
			return pv_get_strval(msg, param, res, &_ksr_nghttp2_ctx.data);
		case 2: /* size */
			return pv_get_sintval(msg, param, res, _ksr_nghttp2_ctx.data.len);
		case 3: /* method */
			return pv_get_strval(msg, param, res, &_ksr_nghttp2_ctx.method);
		case 4: /* version */
			return pv_get_strval(
					msg, param, res, &_ksr_nghttp2_ctx.httpversion);
		case 5: /* srcip */
			if(_ksr_nghttp2_ctx.srcip.len > 0) {
				return pv_get_strval(msg, param, res, &_ksr_nghttp2_ctx.srcip);
			}
			return pv_get_null(msg, param, res);
		case 6: /* pathfull */
			return pv_get_strval(msg, param, res, &_ksr_nghttp2_ctx.pathfull);

		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int ksr_nghttp2_send_reply(sip_msg_t *msg, str *rcode, str *sbody)
{
	int rv;
	ssize_t writelen;
	int pipefd[2];
	char *p;

	p = strndup(rcode->s, rcode->len);
	if(p == NULL) {
		SYS_MEM_ERROR;
		return -1;
	}
	_ksr_nghttp2_ctx.rplhdrs_v[0].value = (uint8_t *)p;
	_ksr_nghttp2_ctx.rplhdrs_v[0].valuelen = rcode->len;

	if(_ksr_nghttp2_ctx.rplhdrs_n == 0) {
		_ksr_nghttp2_ctx.rplhdrs_n++;
	}

	if(sbody == NULL || sbody->len <= 0) {
		rv = nghttp2_submit_response(_ksr_nghttp2_ctx.session,
				_ksr_nghttp2_ctx.stream_data->stream_id,
				_ksr_nghttp2_ctx.rplhdrs_v, _ksr_nghttp2_ctx.rplhdrs_n, NULL);
		if(rv != 0) {
			LM_ERR("Fatal error: %s", nghttp2_strerror(rv));
			return -1;
		}
		return 1;
	}

	rv = pipe(pipefd);
	if(rv != 0) {
		LM_ERR("Could not create pipe");
		rv = nghttp2_submit_rst_stream(_ksr_nghttp2_ctx.session,
				NGHTTP2_FLAG_NONE, _ksr_nghttp2_ctx.stream_data->stream_id,
				NGHTTP2_INTERNAL_ERROR);
		if(rv != 0) {
			LM_ERR("Fatal error: %s", nghttp2_strerror(rv));
			return -1;
		}
		return 0;
	}

	writelen = write(pipefd[1], sbody->s, sbody->len);
	close(pipefd[1]);

	if(writelen != sbody->len) {
		close(pipefd[0]);
		return -1;
	}

	_ksr_nghttp2_ctx.stream_data->fd = pipefd[0];

	if(ksr_nghttp2_send_response(_ksr_nghttp2_ctx.session,
			   _ksr_nghttp2_ctx.stream_data->stream_id,
			   _ksr_nghttp2_ctx.rplhdrs_v, _ksr_nghttp2_ctx.rplhdrs_n,
			   pipefd[0])
			!= 0) {
		close(pipefd[0]);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_nghttp2_send_reply(sip_msg_t *msg, char *pcode, char *pbody)
{
	str code = str_init("200");
	str body = str_init("");

	if(pcode == 0 || pbody == 0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)pcode, &code) != 0) {
		LM_ERR("no reply code value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pbody, &body) != 0) {
		LM_ERR("unable to get body\n");
		return -1;
	}

	return ksr_nghttp2_send_reply(msg, &code, &body);
}

#define KSR_NGHTTP2_STATUS_NAME ":status"
#define KSR_NGHTTP2_STATUS_CODE "204"

/**
 *
 */
static int ksr_nghttp2_reply_header_clear(void)
{
	int i;
	char *p;

	if(_ksr_nghttp2_ctx.rplhdrs_v[0].value != NULL) {
		free(_ksr_nghttp2_ctx.rplhdrs_v[0].value);
		_ksr_nghttp2_ctx.rplhdrs_v[0].value = NULL;
	}
	for(i = 1; i < _ksr_nghttp2_ctx.rplhdrs_n; i++) {
		if(_ksr_nghttp2_ctx.rplhdrs_v[i].name != NULL) {
			free(_ksr_nghttp2_ctx.rplhdrs_v[i].name);
			_ksr_nghttp2_ctx.rplhdrs_v[i].name = NULL;
			_ksr_nghttp2_ctx.rplhdrs_v[i].namelen = 0;
		}
		if(_ksr_nghttp2_ctx.rplhdrs_v[i].value != NULL) {
			free(_ksr_nghttp2_ctx.rplhdrs_v[i].value);
			_ksr_nghttp2_ctx.rplhdrs_v[i].value = NULL;
			_ksr_nghttp2_ctx.rplhdrs_v[i].valuelen = 0;
		}
		_ksr_nghttp2_ctx.rplhdrs_v[i].flags = 0;
	}

	_ksr_nghttp2_ctx.rplhdrs_n = 0;

	/* first header position kept for ':status' (its name is not duplicated) */
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].name =
			(uint8_t *)KSR_NGHTTP2_STATUS_NAME;
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].namelen =
			sizeof(KSR_NGHTTP2_STATUS_NAME) - 1;
	p = strdup(KSR_NGHTTP2_STATUS_CODE);
	if(p == NULL) {
		SYS_MEM_ERROR;
		return -1;
	}
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].value = (uint8_t *)p;
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].valuelen =
			sizeof(KSR_NGHTTP2_STATUS_CODE) - 1;
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].flags =
			NGHTTP2_NV_FLAG_NONE;
	_ksr_nghttp2_ctx.rplhdrs_n++;

	return 0;
}

/**
 *
 */
static int ksr_nghttp2_reply_header(sip_msg_t *msg, str *sname, str *sbody)
{
	char *p;
	if(_ksr_nghttp2_ctx.rplhdrs_n >= KSR_NGHTTP2_RPLHDRS_SIZE) {
		LM_ERR("too many headers\n");
		return -1;
	}
	if(_ksr_nghttp2_ctx.rplhdrs_n == 0) {
		/* first header position kept for ':status' */
		_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].name =
				(uint8_t *)KSR_NGHTTP2_STATUS_NAME;
		_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].namelen =
				sizeof(KSR_NGHTTP2_STATUS_NAME) - 1;
		p = strdup(KSR_NGHTTP2_STATUS_CODE);
		if(p == NULL) {
			SYS_MEM_ERROR;
			return -1;
		}
		_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].value =
				(uint8_t *)p;
		_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].valuelen =
				sizeof(KSR_NGHTTP2_STATUS_CODE) - 1;
		_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].flags =
				NGHTTP2_NV_FLAG_NONE;
		_ksr_nghttp2_ctx.rplhdrs_n++;
	}

	p = strndup(sname->s, sname->len);
	if(p == NULL) {
		SYS_MEM_ERROR;
		return -1;
	}
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].name = (uint8_t *)p;
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].namelen = sname->len;

	p = strndup(sbody->s, sbody->len);
	if(p == NULL) {
		free(_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].name);
		_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].name = NULL;
		SYS_MEM_ERROR;
		return -1;
	}
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].value = (uint8_t *)p;
	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].valuelen =
			sbody->len;

	_ksr_nghttp2_ctx.rplhdrs_v[_ksr_nghttp2_ctx.rplhdrs_n].flags =
			NGHTTP2_NV_FLAG_NONE;
	_ksr_nghttp2_ctx.rplhdrs_n++;

	return -1;
}

/**
 *
 */
static int w_nghttp2_reply_header(sip_msg_t *msg, char *pname, char *pbody)
{
	str name = str_init("");
	str body = str_init("");

	if(pname == 0 || pbody == 0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)pname, &name) != 0) {
		LM_ERR("unable to get name\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pbody, &body) != 0) {
		LM_ERR("unable to get body\n");
		return -1;
	}

	return ksr_nghttp2_reply_header(msg, &name, &body);
}

void ksr_event_route(void)
{
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("nghttp2:request");
	sip_msg_t *fmsg = NULL;
	run_act_ctx_t ctx;
	int rtb;

	LM_DBG("executing event_route[%s] (%d)\n", evname.s, nghttp2_route_no);

	ksr_nghttp2_reply_header_clear();

	if(faked_msg_init() < 0) {
		return;
	}
	fmsg = faked_msg_next();
	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	if(nghttp2_route_no >= 0) {
		run_top_route(event_rt.rlist[nghttp2_route_no], fmsg, &ctx);
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL) {
			if(sr_kemi_ctx_route(keng, &ctx, fmsg, EVENT_ROUTE,
					   &nghttp2_event_callback, &evname)
					< 0) {
				LM_ERR("error running event route kemi callback\n");
				return;
			}
		}
	}
	set_route_type(rtb);
	if(ctx.run_flags & DROP_R_F) {
		LM_ERR("exit due to 'drop' in event route\n");
		return;
	}

	return;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_nghttp2_exports[] = {
	{ str_init("nghttp2"), str_init("nghttp2_reply"),
		SR_KEMIP_INT, ksr_nghttp2_send_reply,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("nghttp2"), str_init("nghttp2_reply_header"),
		SR_KEMIP_INT, ksr_nghttp2_reply_header,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_nghttp2_exports);
	return 0;
}
