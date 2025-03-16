/**
 * Copyright (C) 2023 Daniel-Constantin Mierla (asipto.com)
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

#include <microhttpd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"
#include "../../core/cfg/cfg_struct.h"

MODULE_VERSION

static int _microhttpd_listen_port = 8280;
static int _microhttpd_server_pid = -1;
static str _microhttpd_listen_addr = str_init("");

static int microhttpd_route_no = -1;
static str microhttpd_event_callback = STR_NULL;

static int microhttpd_server_run(void);

static int w_mhttpd_send_reply(
		sip_msg_t *msg, char *pcode, char *preason, char *pctype, char *pbody);

static int fixup_mhttpd_send_reply(void **param, int param_no);
static int fixup_free_mhttpd_send_reply(void **param, int param_no);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

int pv_get_mhttpd(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
int pv_parse_mhttpd_name(pv_spec_p sp, str *in);

/* clang-format off */
static pv_export_t mod_pvs[] = {
	{{"mhttpd", (sizeof("mhttpd") - 1)}, PVT_OTHER, pv_get_mhttpd, 0,
			pv_parse_mhttpd_name, 0, 0, 0},

	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static cmd_export_t cmds[] = {
	{"mhttpd_reply",    (cmd_function)w_mhttpd_send_reply, 4,
		fixup_mhttpd_send_reply, fixup_free_mhttpd_send_reply,
		REQUEST_ROUTE|EVENT_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"listen_port",     PARAM_INT,    &_microhttpd_listen_port},
	{"listen_addr",     PARAM_STR,    &_microhttpd_listen_addr},
	{"event_callback",  PARAM_STR,    &microhttpd_event_callback},
	{0, 0, 0}
};

struct module_exports exports = {
	"microhttpd",	 /* module name */
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

	if(microhttpd_event_callback.s != NULL
			&& microhttpd_event_callback.len > 0) {
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_ERR("failed to find kemi engine\n");
			return -1;
		}
		microhttpd_route_no = -1;
	} else {
		route_no = route_lookup(&event_rt, "microhttpd:request");
		if(route_no == -1) {
			LM_ERR("failed to find event_route[microhttpd:request]\n");
			return -1;
		}
		if(event_rt.rlist[route_no] == 0) {
			LM_WARN("event_route[microhttpd:request] is empty\n");
		}
		microhttpd_route_no = route_no;
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

	pid = fork_process(PROC_NOCHLDINIT, "MicroHTTPd Server Process", 1);
	if(pid < 0)
		return -1; /* error */
	if(pid == 0) {
		/* child */
		_microhttpd_server_pid = getpid();

		/* do child init to allow execution of rpc like functions */
		if(init_child(PROC_RPC) < 0) {
			LM_DBG("failed to do RPC child init for dispatcher\n");
			return -1;
		}
		/* initialize the config framework */
		if(cfg_child_init())
			return -1;
		if(microhttpd_server_run() < 0) {
			LM_ERR("failed to initialize microhttpd server process\n");
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

typedef struct ksr_mhttpd_ctx
{
	struct MHD_Connection *connection;
	str method;
	str url;
	str httpversion;
	str data;
	const union MHD_ConnectionInfo *cinfo;
	char srcipbuf[IP_ADDR_MAX_STR_SIZE];
	str srcip;
} ksr_mhttpd_ctx_t;

static ksr_mhttpd_ctx_t _ksr_mhttpd_ctx = {0};

/**
 * parse the name of the $mhttpd(name)
 */
int pv_parse_mhttpd_name(pv_spec_p sp, str *in)
{
	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;
	switch(in->len) {
		case 3:
			if(strncasecmp(in->s, "url", 3) == 0) {
				sp->pvp.pvn.u.isname.name.n = 0;
			} else {
				goto error;
			}
			break;
		case 4:
			if(strncasecmp(in->s, "data", 4) == 0) {
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
 * return the value of $mhttpd(name)
 */
int pv_get_mhttpd(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	struct sockaddr *srcaddr = NULL;
	const char *hdrval = NULL;

	if(param == NULL) {
		return -1;
	}
	if(_ksr_mhttpd_ctx.connection == NULL) {
		return pv_get_null(msg, param, res);
	}
	if(param->pvn.u.isname.type == PVT_HDR) {
		hdrval = MHD_lookup_connection_value(_ksr_mhttpd_ctx.connection,
				MHD_HEADER_KIND, param->pvn.u.isname.name.s.s + 2);
		if(hdrval == NULL) {
			return pv_get_null(msg, param, res);
		}
		return pv_get_strzval(msg, param, res, (char *)hdrval);
	}

	switch(param->pvn.u.isname.name.n) {
		case 0: /* url */
			return pv_get_strval(msg, param, res, &_ksr_mhttpd_ctx.url);
		case 1: /* data */
			return pv_get_strval(msg, param, res, &_ksr_mhttpd_ctx.data);
		case 2: /* size */
			return pv_get_sintval(msg, param, res, _ksr_mhttpd_ctx.data.len);
		case 3: /* method */
			return pv_get_strval(msg, param, res, &_ksr_mhttpd_ctx.method);
		case 4: /* version */
			return pv_get_strval(msg, param, res, &_ksr_mhttpd_ctx.httpversion);
		case 5: /* srcip */
			if(_ksr_mhttpd_ctx.srcip.len > 0) {
				return pv_get_strval(msg, param, res, &_ksr_mhttpd_ctx.srcip);
			}
			srcaddr =
					(_ksr_mhttpd_ctx.cinfo ? _ksr_mhttpd_ctx.cinfo->client_addr
										   : NULL);
			if(srcaddr == NULL) {
				return pv_get_null(msg, param, res);
			}
			switch(srcaddr->sa_family) {
				case AF_INET:
					if(!inet_ntop(AF_INET,
							   &(((struct sockaddr_in *)srcaddr)->sin_addr),
							   _ksr_mhttpd_ctx.srcipbuf,
							   IP_ADDR_MAX_STR_SIZE)) {
						return pv_get_null(msg, param, res);
					}
					break;
				case AF_INET6:
					if(!inet_ntop(AF_INET6,
							   &(((struct sockaddr_in6 *)srcaddr)->sin6_addr),
							   _ksr_mhttpd_ctx.srcipbuf,
							   IP_ADDR_MAX_STR_SIZE)) {
						return pv_get_null(msg, param, res);
					}
					break;
				default:
					return pv_get_null(msg, param, res);
			}
			_ksr_mhttpd_ctx.srcip.s = _ksr_mhttpd_ctx.srcipbuf;
			_ksr_mhttpd_ctx.srcip.len = strlen(_ksr_mhttpd_ctx.srcipbuf);
			return pv_get_strval(msg, param, res, &_ksr_mhttpd_ctx.srcip);
		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int ksr_mhttpd_send_reply(
		sip_msg_t *msg, int rcode, str *sreason, str *sctype, str *sbody)
{
	struct MHD_Response *response;
	int ret;

	if(_ksr_mhttpd_ctx.connection == NULL) {
		LM_ERR("no connection available\n");
		return -1;
	}

	if(rcode < 100 || rcode >= 700) {
		LM_ERR("invalid code parameter\n");
		return -1;
	}
	if(sreason->s == NULL || sreason->len == 0) {
		LM_ERR("invalid reason parameter\n");
		return -1;
	}
	if(sctype->s == NULL) {
		LM_ERR("invalid content-type parameter\n");
		return -1;
	}
	if(sbody->s == NULL) {
		LM_ERR("invalid body parameter\n");
		return -1;
	}

	response = MHD_create_response_from_buffer(
			sbody->len, sbody->s, MHD_RESPMEM_MUST_COPY);
	if(response == NULL) {
		LM_ERR("failed to create the response\n");
		return -1;
	}
	if(sctype->len > 0) {
		MHD_add_response_header(response, "Content-Type", sctype->s);
	}
	ret = MHD_queue_response(
			_ksr_mhttpd_ctx.connection, (unsigned int)rcode, response);
	MHD_destroy_response(response);

	LM_DBG("queue response return: %d (%s)\n", ret,
			(ret == MHD_YES) ? "YES" : "XYZ");

	return (ret == MHD_YES) ? 1 : -1;
}

/**
 *
 */
static int w_mhttpd_send_reply(
		sip_msg_t *msg, char *pcode, char *preason, char *pctype, char *pbody)
{
	str body = str_init("");
	str reason = str_init("OK");
	str ctype = str_init("text/plain");
	int code = 200;

	if(_ksr_mhttpd_ctx.connection == NULL) {
		LM_ERR("no connection available\n");
		return -1;
	}

	if(pcode == 0 || preason == 0 || pctype == 0 || pbody == 0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)pcode, &code) != 0) {
		LM_ERR("no reply code value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)preason, &reason) != 0) {
		LM_ERR("unable to get reason\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pctype, &ctype) != 0) {
		LM_ERR("unable to get content type\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)pbody, &body) != 0) {
		LM_ERR("unable to get body\n");
		return -1;
	}

	return ksr_mhttpd_send_reply(msg, code, &reason, &ctype, &body);
}

static int fixup_mhttpd_send_reply(void **param, int param_no)
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

static int fixup_free_mhttpd_send_reply(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_igp_null(param, 1);
	} else if(param_no == 2) {
		return fixup_free_spve_null(param, 1);
	} else if(param_no == 3) {
		return fixup_free_spve_null(param, 1);
	} else if(param_no == 4) {
		return fixup_free_spve_null(param, 1);
	}
	return 0;
}

static enum MHD_Result ksr_microhttpd_request(void *cls,
		struct MHD_Connection *connection, const char *url, const char *method,
		const char *version, const char *upload_data, size_t *upload_data_size,
		void **ptr)
{
	static int _first_callback;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("microhttpd:request");
	sip_msg_t *fmsg = NULL;
	run_act_ctx_t ctx;
	int rtb;

	if(&_first_callback != *ptr) {
		/* the first time only the headers are valid,
		   do not respond in the first round... */
		*ptr = &_first_callback;
		return MHD_YES;
	}
	*ptr = NULL; /* clear context pointer */

	_ksr_mhttpd_ctx.connection = connection;
	_ksr_mhttpd_ctx.method.s = (char *)method;
	_ksr_mhttpd_ctx.method.len = strlen(_ksr_mhttpd_ctx.method.s);
	_ksr_mhttpd_ctx.url.s = (char *)url;
	_ksr_mhttpd_ctx.url.len = strlen(_ksr_mhttpd_ctx.url.s);
	_ksr_mhttpd_ctx.httpversion.s = (char *)version;
	_ksr_mhttpd_ctx.httpversion.len = strlen(_ksr_mhttpd_ctx.httpversion.s);
	if(*upload_data_size > 0) {
		_ksr_mhttpd_ctx.data.s = (char *)upload_data;
		_ksr_mhttpd_ctx.data.len = (int)(*upload_data_size);
	} else {
		_ksr_mhttpd_ctx.data.s = NULL;
		_ksr_mhttpd_ctx.data.len = 0;
	}
	_ksr_mhttpd_ctx.cinfo = MHD_get_connection_info(
			connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	_ksr_mhttpd_ctx.srcip.s = NULL;
	_ksr_mhttpd_ctx.srcip.len = 0;

	LM_DBG("executing event_route[%s] (%d)\n", evname.s, microhttpd_route_no);
	if(faked_msg_init() < 0) {
		return MHD_NO;
	}
	fmsg = faked_msg_next();
	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	if(microhttpd_route_no >= 0) {
		run_top_route(event_rt.rlist[microhttpd_route_no], fmsg, &ctx);
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL) {
			if(sr_kemi_ctx_route(keng, &ctx, fmsg, EVENT_ROUTE,
					   &microhttpd_event_callback, &evname)
					< 0) {
				LM_ERR("error running event route kemi callback\n");
				return MHD_NO;
			}
		}
	}
	set_route_type(rtb);
	if(ctx.run_flags & DROP_R_F) {
		LM_ERR("exit due to 'drop' in event route\n");
		return MHD_NO;
	}

	return MHD_YES;
}

#define KSR_MICROHTTPD_PAGE               \
	"<html><head><title>Kamailio</title>" \
	"</head><body>Thanks for flying Kamailio!</body></html>"
/**
 *
 */
static int microhttpd_server_run(void)
{

	struct MHD_Daemon *d;
	struct sockaddr_in address;

	if(_microhttpd_listen_addr.len > 0) {
		address.sin_family = AF_INET;
		address.sin_port = htons(_microhttpd_listen_port);
		if(inet_pton(AF_INET, _microhttpd_listen_addr.s, &address.sin_addr)
				<= 0) {
			LM_ERR("failed to convert listen address\n");
			return -1;
		}
		LM_DBG("preparing to listen on %s :%d\n", _microhttpd_listen_addr.s,
				_microhttpd_listen_port);
		d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, _microhttpd_listen_port,
				NULL, NULL, &ksr_microhttpd_request, KSR_MICROHTTPD_PAGE,
				MHD_OPTION_SOCK_ADDR, &address, MHD_OPTION_END);
	} else {
		LM_DBG("preparing to listen on port: %d\n", _microhttpd_listen_port);
		d = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, _microhttpd_listen_port,
				NULL, NULL, &ksr_microhttpd_request, KSR_MICROHTTPD_PAGE,
				MHD_OPTION_END);
	}

	if(d == NULL) {
		return -1;
	}
	while(1) {
		sleep(10);
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_mhttpd_exports[] = {
	{ str_init("microhttpd"), str_init("mhttpd_reply"),
		SR_KEMIP_INT, ksr_mhttpd_send_reply,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */


/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_mhttpd_exports);
	return 0;
}
