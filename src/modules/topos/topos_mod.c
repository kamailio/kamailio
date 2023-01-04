/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 */

/*!
 * \file
 * \brief Kamailio topos :: Module interface
 * \ingroup topos
 * Module: \ref topos
 */

/*! \defgroup topos Kamailio :: Topology stripping
 *
 * This module removes the SIP routing headers that show topology details.
 * The script interpreter gets the SIP messages with full content, so all
 * existing functionality is preserved.
 * @{
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/events.h"
#include "../../core/dprint.h"
#include "../../core/tcp_options.h"
#include "../../core/ut.h"
#include "../../core/forward.h"
#include "../../core/config.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_methods.h"
#include "../../core/timer_proc.h"
#include "../../core/fmsg.h"
#include "../../core/onsend.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "../../lib/srdb1/db.h"
#include "../../core/utils/sruid.h"

#include "../../modules/sanity/api.h"

#include "tps_storage.h"
#include "tps_msg.h"
#include "api.h"
#include "tps_mask.h"

MODULE_VERSION


/* Database connection handle */
db1_con_t* _tps_db_handle = NULL;
/* DB functions */
db_func_t _tpsdbf;
/* sruid to get internal uid */
sruid_t _tps_sruid;

/** module parameters */
static str _tps_db_url = str_init(DEFAULT_DB_URL);
int _tps_param_mask_callid = 0;
int _tps_sanity_checks = 0;
int _tps_rr_update = 0;
int _tps_header_mode = 0;
str _tps_storage = str_init("db");
str _tps_callid_prefix = str_init("!!:");
str _tps_key = str_init("aL9.n~8hHSfE");

extern int _tps_branch_expire;
extern int _tps_dialog_expire;
extern unsigned int _tps_methods_nocontact;
str _tps_methods_nocontact_list = str_init("");
extern unsigned int _tps_methods_noinitial;
str _tps_methods_noinitial_list = str_init("");

int _tps_clean_interval = 60;

#define TPS_EVENTRT_OUTGOING 1
#define TPS_EVENTRT_SENDING  2
#define TPS_EVENTRT_INCOMING  4
#define TPS_EVENTRT_RECEIVING 8
static int _tps_eventrt_mode = TPS_EVENTRT_OUTGOING | TPS_EVENTRT_SENDING
				| TPS_EVENTRT_INCOMING | TPS_EVENTRT_RECEIVING;
static int _tps_eventrt_outgoing = -1;
static str _tps_eventrt_callback = STR_NULL;
static str _tps_eventrt_outgoing_name = str_init("topos:msg-outgoing");
static int _tps_eventrt_sending = -1;
static str _tps_eventrt_sending_name = str_init("topos:msg-sending");
static int _tps_eventrt_incoming = -1;
static str _tps_eventrt_incoming_name = str_init("topos:msg-incoming");
static int _tps_eventrt_receiving = -1;
static str _tps_eventrt_receiving_name = str_init("topos:msg-receiving");

str _tps_contact_host = str_init("");
int _tps_contact_mode = 0;
str _tps_cparam_name = str_init("tps");

str _tps_xavu_cfg = STR_NULL;
str _tps_xavu_field_acontact = STR_NULL;
str _tps_xavu_field_bcontact = STR_NULL;
str _tps_xavu_field_contact_host = STR_NULL;

str _tps_context_param = STR_NULL;
str _tps_context_value = STR_NULL;

sanity_api_t scb;

int tps_msg_received(sr_event_param_t *evp);
int tps_msg_sent(sr_event_param_t *evp);

static int tps_execute_event_route(sip_msg_t *msg, sr_event_param_t *evp,
		int evtype, int evidx, str *evname);

/** module functions */
/* Module init function prototype */
static int mod_init(void);
/* Module child-init function prototype */
static int child_init(int rank);
/* Module destroy function prototype */
static void destroy(void);

static int w_tps_set_context(sip_msg_t* msg, char* pctx, char* p2);

int bind_topos(topos_api_t *api);

static cmd_export_t cmds[]={
	{"tps_set_context", (cmd_function)w_tps_set_context,
		1, fixup_spve_null, fixup_free_spve_null,
		ANY_ROUTE},

	{"bind_topos",  (cmd_function)bind_topos,  0,
		0, 0, 0},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"storage",		PARAM_STR, &_tps_storage},
	{"db_url",		PARAM_STR, &_tps_db_url},
	{"mask_callid",		PARAM_INT, &_tps_param_mask_callid},
	{"sanity_checks",	PARAM_INT, &_tps_sanity_checks},
	{"header_mode",	PARAM_INT, &_tps_header_mode},
	{"branch_expire",	PARAM_INT, &_tps_branch_expire},
	{"dialog_expire",	PARAM_INT, &_tps_dialog_expire},
	{"clean_interval",	PARAM_INT, &_tps_clean_interval},
	{"event_callback",	PARAM_STR, &_tps_eventrt_callback},
	{"event_mode",		PARAM_INT, &_tps_eventrt_mode},
	{"contact_host",	PARAM_STR, &_tps_contact_host},
	{"contact_mode",	PARAM_INT, &_tps_contact_mode},
	{"cparam_name",		PARAM_STR, &_tps_cparam_name},
	{"xavu_cfg",		PARAM_STR, &_tps_xavu_cfg},
	{"xavu_field_a_contact",	PARAM_STR, &_tps_xavu_field_acontact},
	{"xavu_field_b_contact",	PARAM_STR, &_tps_xavu_field_bcontact},
	{"xavu_field_contact_host", PARAM_STR, &_tps_xavu_field_contact_host},
	{"rr_update",		PARAM_INT, &_tps_rr_update},
	{"context",			PARAM_STR, &_tps_context_param},
	{"methods_nocontact",		PARAM_STR, &_tps_methods_nocontact_list},
	{"methods_noinitial",		PARAM_STR, &_tps_methods_noinitial_list},
	{"callid_prefix",	PARAM_STR, &_tps_callid_prefix},
	{"mask_key",		PARAM_STR, &_tps_key},

	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"topos",    /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported  functions */
	params,     /* exported parameters */
	0,          /* exported rpc functions */
	0,          /* exported pseudo-variables */
	0,          /* response handling function */
	mod_init,   /* module initialization function */
	child_init, /* child initialization function */
	destroy     /* destroy function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	_tps_eventrt_outgoing = route_lookup(&event_rt, _tps_eventrt_outgoing_name.s);
	if(_tps_eventrt_outgoing<0
			|| event_rt.rlist[_tps_eventrt_outgoing]==NULL) {
		_tps_eventrt_outgoing = -1;
	}
	_tps_eventrt_sending = route_lookup(&event_rt, _tps_eventrt_sending_name.s);
	if(_tps_eventrt_sending<0
			|| event_rt.rlist[_tps_eventrt_sending]==NULL) {
		_tps_eventrt_sending = -1;
	}
	_tps_eventrt_incoming = route_lookup(&event_rt, _tps_eventrt_incoming_name.s);
	if(_tps_eventrt_incoming<0
			|| event_rt.rlist[_tps_eventrt_incoming]==NULL) {
		_tps_eventrt_incoming = -1;
	}
	_tps_eventrt_receiving = route_lookup(&event_rt, _tps_eventrt_receiving_name.s);
	if(_tps_eventrt_receiving<0
			|| event_rt.rlist[_tps_eventrt_receiving]==NULL) {
		_tps_eventrt_receiving = -1;
	}

	if(faked_msg_init()<0) {
		LM_ERR("failed to init fmsg\n");
		return -1;
	}

	if(_tps_methods_nocontact_list.len>0) {
		if(parse_methods(&_tps_methods_nocontact_list, &_tps_methods_nocontact)<0) {
			LM_ERR("failed to parse methods_nocontact parameter\n");
			return -1;
		}
	}
	if(_tps_methods_noinitial_list.len>0) {
		if(parse_methods(&_tps_methods_noinitial_list, &_tps_methods_noinitial)<0) {
			LM_ERR("failed to parse methods_noinitial parameter\n");
			return -1;
		}
	}
	if(_tps_storage.len==2 && strncmp(_tps_storage.s, "db", 2)==0) {
		/* Find a database module */
		if (db_bind_mod(&_tps_db_url, &_tpsdbf)) {
			LM_ERR("unable to bind database module\n");
			return -1;
		}
		if (!DB_CAPABILITY(_tpsdbf, DB_CAP_ALL)) {
			LM_CRIT("database modules does not "
				"provide all functions needed\n");
			return -1;
		}
	} else {
		if(_tps_storage.len!=7 && strncmp(_tps_storage.s, "redis", 5)!=0) {
			LM_ERR("unknown storage type: %.*s\n",
					_tps_storage.len, _tps_storage.s);
			return -1;
		}
	}

	if(_tps_sanity_checks!=0) {
		if(sanity_load_api(&scb)<0) {
			LM_ERR("cannot bind to sanity module\n");
			goto error;
		}
	}
	if(tps_storage_lock_set_init()<0) {
		LM_ERR("failed to initialize locks set\n");
		return -1;
	}

	if(sruid_init(&_tps_sruid, '-', "tpsh", SRUID_INC)<0)
		return -1;

	if (_tps_contact_mode == 2 && (_tps_xavu_cfg.len <= 0
				|| _tps_xavu_field_acontact.len <= 0
				|| _tps_xavu_field_bcontact.len <= 0)) {
		LM_ERR("contact_mode parameter is 2,"
				" but a_contact or b_contact xavu fields not defined\n");
		return -1;
	}
	tps_mask_init();
	sr_event_register_cb(SREV_NET_DATA_IN,  tps_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, tps_msg_sent);

#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
#endif

	if(sr_wtimer_add(tps_storage_clean, NULL, _tps_clean_interval)<0)
		return -1;

	return 0;
error:
	return -1;
}

/**
 *
 */
static int child_init(int rank)
{
	if(sruid_init(&_tps_sruid, '-', "tpsh", SRUID_INC)<0)
		return -1;

	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if(_tps_storage.len==2 && strncmp(_tps_storage.s, "db", 2)==0) {
		_tps_db_handle = _tpsdbf.init(&_tps_db_url);
		if (!_tps_db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}
	}
	return 0;

}

/**
 *
 */
static void destroy(void)
{
	if(_tps_storage.len==2 && strncmp(_tps_storage.s, "db", 2)==0) {
		if (_tps_db_handle) {
			_tpsdbf.close(_tps_db_handle);
			_tps_db_handle = 0;
		}
	}
	tps_storage_lock_set_destroy();
}

/**
 *
 */
static int ki_tps_set_context(sip_msg_t* msg, str* ctx)
{
	if(ctx==NULL || ctx->len<=0) {
		if(_tps_context_value.s) {
			pkg_free(_tps_context_value.s);
		}
		_tps_context_value.s = NULL;
		_tps_context_value.len = 0;
		return 1;
	}

	if(_tps_context_value.len>=ctx->len) {
		memcpy(_tps_context_value.s, ctx->s, ctx->len);
		_tps_context_value.len = ctx->len;
		return 1;
	}

	if(_tps_context_value.s) {
		pkg_free(_tps_context_value.s);
	}
	_tps_context_value.len = 0;

	_tps_context_value.s = (char*)pkg_mallocxz(ctx->len + 1);
	if(_tps_context_value.s==NULL) {
		PKG_MEM_ERROR;
		return -1;
	}

	memcpy(_tps_context_value.s, ctx->s, ctx->len);
	_tps_context_value.len = ctx->len;

	return 1;
}

/**
 *
 */
static int w_tps_set_context(sip_msg_t* msg, char* pctx, char* p2)
{
	str sctx = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pctx, &sctx)<0) {
		LM_ERR("failed to get context parameter\n");
		return -1;
	}

	return ki_tps_set_context(msg, &sctx);
}

/**
 *
 */
int tps_prepare_msg(sip_msg_t *msg)
{
	if (parse_msg(msg->buf, msg->len, msg)!=0) {
		LM_DBG("outbuf buffer parsing failed!");
		return 1;
	}

	if(msg->first_line.type==SIP_REQUEST) {
		if(!IS_SIP(msg)) {
			LM_DBG("non sip request message\n");
			return 1;
		}
	} else if(msg->first_line.type==SIP_REPLY) {
		if(!IS_SIP_REPLY(msg)) {
			LM_DBG("non sip reply message\n");
			return 1;
		}
	} else {
		LM_DBG("unknown sip message type %d\n", msg->first_line.type);
		return 1;
	}

	if(parse_headers(msg, HDR_VIA2_F, 0)<0) {
		LM_DBG("no via2 has been parsed\n");
	}

	if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL) {
		LM_ERR("cannot parse cseq header\n");
		return -1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0)==-1) {
		LM_DBG("parsing headers failed [[%.*s]]\n",
				msg->len, msg->buf);
		return 2;
	}

	if(parse_from_header(msg)<0) {
		LM_ERR("cannot parse FROM header\n");
		return 3;
	}

	if(parse_to_header(msg)<0 || msg->to==NULL) {
		LM_ERR("cannot parse TO header\n");
		return 3;
	}

	if(get_to(msg)==NULL) {
		LM_ERR("cannot get TO header\n");
		return 3;
	}

	if(msg->via1==NULL || msg->callid==NULL) {
		LM_ERR("mandatory headers missing - via1: %p callid: %p\n",
				msg->via1, msg->callid);
		return 4;
	}

	return 0;
}

/**
 *
 */
int tps_msg_received(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	char *nbuf = NULL;
	int dialog;
	int ret;

	ki_tps_set_context(NULL, NULL);

	obuf = (str*)evp->data;

	if(tps_execute_event_route(NULL, evp, TPS_EVENTRT_INCOMING,
				_tps_eventrt_incoming, &_tps_eventrt_incoming_name)==1) {
		return 0;
	}

	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	ret = 0;
	if(tps_prepare_msg(&msg)!=0) {
		goto done;
	}

	if(tps_skip_msg(&msg)) {
		goto done;
	}

	if(tps_execute_event_route(&msg, evp, TPS_EVENTRT_RECEIVING,
				_tps_eventrt_receiving, &_tps_eventrt_receiving_name)==1) {
		goto done;
	}

	if(msg.first_line.type==SIP_REQUEST) {
		if(_tps_sanity_checks!=0) {
			if(scb.check_defaults(&msg)<1) {
				LM_ERR("sanity checks failed\n");
				goto done;
			}
		}
		dialog = (get_to(&msg)->tag_value.len>0)?1:0;
		if(dialog) {
			/* dialog request */
			tps_request_received(&msg, dialog);
		}
	} else {
		/* reply */
		tps_response_received(&msg);
	}

	nbuf = tps_msg_update(&msg, (unsigned int*)&obuf->len);

	if(nbuf==NULL) {
		LM_ERR("not enough pkg memory for new message\n");
		ret = -1;
		goto done;
	}
	if(obuf->len>=BUF_SIZE) {
		LM_ERR("new buffer overflow (%d)\n", obuf->len);
		ret = -1;
		goto done;
	}
	memcpy(obuf->s, nbuf, obuf->len);
	obuf->s[obuf->len] = '\0';

done:
	if(nbuf!=NULL)
		pkg_free(nbuf);
	free_sip_msg(&msg);
	return ret;
}

/**
 *
 */
int tps_msg_sent(sr_event_param_t *evp)
{
	sip_msg_t msg;
	str *obuf;
	int dialog;
	int local;
	str nbuf = STR_NULL;

	obuf = (str*)evp->data;

	if(tps_execute_event_route(NULL, evp, TPS_EVENTRT_OUTGOING,
				_tps_eventrt_outgoing, &_tps_eventrt_outgoing_name)==1) {
		return 0;
	}

	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(tps_prepare_msg(&msg)!=0) {
		goto done;
	}

	if(tps_skip_msg(&msg)) {
		goto done;
	}

	if(tps_execute_event_route(&msg, evp, TPS_EVENTRT_SENDING,
				_tps_eventrt_sending, &_tps_eventrt_sending_name)==1) {
		goto done;
	}

	if(msg.first_line.type==SIP_REQUEST) {


		dialog = (get_to(&msg)->tag_value.len>0)?1:0;

		local = 0;
		if(msg.via2==0) {
			local = 1;
		}

		if(local==1 && dialog==0) {
			if((get_cseq(&msg)->method_id)
						& (METHOD_OPTIONS|METHOD_NOTIFY|METHOD_KDMQ)) {
				/* skip local out-of-dialog requests (e.g., keepalive, dmq) */
				goto done;
			}
		}

		tps_request_sent(&msg, dialog, local);
	} else {
		/* reply */
		if(msg.first_line.u.reply.statuscode==100) {
			/* nothing to do - it should be locally generated */
			goto done;
		}
		tps_response_sent(&msg);
	}

	nbuf.s = tps_msg_update(&msg, (unsigned int*)&nbuf.len);
	if(nbuf.s!=NULL) {
		LM_DBG("new outbound buffer generated\n");
		pkg_free(obuf->s);
		obuf->s = nbuf.s;
		obuf->len = nbuf.len;
	} else {
		LM_ERR("failed to generate new outbound buffer\n");
	}

done:
	free_sip_msg(&msg);
	return 0;
}

/**
 *
 */
int tps_get_dialog_expire(void)
{
	return _tps_dialog_expire;
}

/**
 *
 */
int tps_get_branch_expire(void)
{
	return _tps_branch_expire;
}

/**
 *
 */
static int tps_execute_event_route(sip_msg_t *msg, sr_event_param_t *evp,
		int evtype, int evidx, str *evname)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb;
	sr_kemi_eng_t *keng = NULL;
	onsend_info_t onsnd_info = {0};
	onsend_info_t *p_onsend_bak;

	if(!(_tps_eventrt_mode & evtype)) {
		return 0;
	}

	p_onsend_bak = p_onsend;

	if(evidx<0) {
		if(_tps_eventrt_callback.s!=NULL || _tps_eventrt_callback.len>0) {
			keng = sr_kemi_eng_get();
			if(keng==NULL) {
				LM_DBG("event callback (%s) set, but no cfg engine\n",
						_tps_eventrt_callback.s);
				goto done;
			}
		}
	}

	if(evidx<0 && keng==NULL) {
		return 0;
	}

	LM_DBG("executing event_route[topos:%.*s] (%d)\n", evname->len, evname->s,
			evidx);
	fmsg = faked_msg_next();

	if(evp->dst) {
		onsnd_info.to = &evp->dst->to;
		onsnd_info.send_sock = evp->dst->send_sock;
	}
	if(msg!=NULL) {
		onsnd_info.buf = msg->buf;
		onsnd_info.len = msg->len;
		onsnd_info.msg = msg;
	} else {
		onsnd_info.buf = fmsg->buf;
		onsnd_info.len = fmsg->len;
		onsnd_info.msg = fmsg;
	}
	p_onsend = &onsnd_info;

	rtb = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	if(evidx>=0) {
		run_top_route(event_rt.rlist[evidx], (msg)?msg:fmsg, &ctx);
	} else {
		if(keng!=NULL) {
			if(sr_kemi_ctx_route(keng, &ctx, (msg)?msg:fmsg, EVENT_ROUTE,
						&_tps_eventrt_callback, evname)<0) {
				LM_ERR("error running event route kemi callback\n");
				p_onsend=p_onsend_bak;
				return -1;
			}
		}
	}
	set_route_type(rtb);
	if(ctx.run_flags&DROP_R_F) {
		LM_DBG("exit due to 'drop' in event route\n");
		p_onsend=p_onsend_bak;
		return 1;
	}

done:
	p_onsend=p_onsend_bak;
	return 0;
}

/**
 *
 */
int bind_topos(topos_api_t *api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	memset(api, 0, sizeof(topos_api_t));
	api->set_storage_api = tps_set_storage_api;
	api->get_dialog_expire = tps_get_dialog_expire;
	api->get_branch_expire = tps_get_branch_expire;

	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_topos_exports[] = {
	{ str_init("topos"), str_init("tps_set_context"),
		SR_KEMIP_INT, ki_tps_set_context,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_topos_exports);

	return 0;
}

/** @} */
