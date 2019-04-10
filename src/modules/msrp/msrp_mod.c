/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/dset.h"
#include "../../core/action.h"
#include "../../core/mod_fix.h"
#include "../../core/events.h"
#include "../../core/tcp_conn.h"
#include "../../core/pvar.h"
#include "../../core/timer_proc.h" /* register_sync_timer */
#include "../../core/kemi.h"

#include "msrp_parser.h"
#include "msrp_netio.h"
#include "msrp_vars.h"
#include "msrp_env.h"
#include "msrp_cmap.h"

MODULE_VERSION

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_msrp_relay(sip_msg_t* msg, char* str1, char* str2);
static int w_msrp_reply2(sip_msg_t* msg, char* code, char* text);
static int w_msrp_reply3(sip_msg_t* msg, char* code, char* text, char* hdrs);
static int w_msrp_is_request(sip_msg_t* msg, char* str1, char* str2);
static int w_msrp_is_reply(sip_msg_t* msg, char* str1, char* str2);
static int w_msrp_set_dst(sip_msg_t* msg, char* taddr, char* fsock);
static int w_msrp_relay_flags(sip_msg_t* msg, char *tflags, char* str2);
static int w_msrp_reply_flags(sip_msg_t* msg, char *tflags, char* str2);
static int w_msrp_cmap_save(sip_msg_t* msg, char* str1, char* str2);
static int w_msrp_cmap_lookup(sip_msg_t* msg, char* str1, char* str2);

static void msrp_local_timer(unsigned int ticks, void* param); /*!< Local timer handler */

int msrp_param_sipmsg = 1;
int msrp_cmap_size = 0;
int msrp_auth_min_expires = 60;
int msrp_auth_max_expires = 3600;
int msrp_timer_interval = 60;
str msrp_use_path_addr = { 0 };
int msrp_tls_module_loaded = 0;
str msrp_event_callback = STR_NULL;

static int msrp_frame_received(sr_event_param_t *evp);
sip_msg_t *msrp_fake_sipmsg(msrp_frame_t *mf);

static tr_export_t mod_trans[] = {
	{ {"msrpuri", sizeof("msrpuri")-1}, /* string class */
		tr_parse_msrpuri },

	{ { 0, 0 }, 0 }
};

static pv_export_t mod_pvs[] = {
	{ {"msrp", (sizeof("msrp")-1)}, PVT_OTHER, pv_get_msrp,
		pv_set_msrp, pv_parse_msrp_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"msrp_relay", (cmd_function)w_msrp_relay, 0, 0,
		0, ANY_ROUTE},
	{"msrp_reply", (cmd_function)w_msrp_reply2, 2, fixup_spve_spve,
		0, ANY_ROUTE},
	{"msrp_reply", (cmd_function)w_msrp_reply3, 3, fixup_spve_all,
		0, ANY_ROUTE},
	{"msrp_is_request",  (cmd_function)w_msrp_is_request, 0, 0,
		0, ANY_ROUTE},
	{"msrp_is_reply",    (cmd_function)w_msrp_is_reply, 0, 0,
		0, ANY_ROUTE},
	{"msrp_set_dst",     (cmd_function)w_msrp_set_dst, 2, fixup_spve_all,
		0, ANY_ROUTE},
	{"msrp_relay_flags", (cmd_function)w_msrp_relay_flags, 1, fixup_igp_null,
		0, ANY_ROUTE},
	{"msrp_reply_flags", (cmd_function)w_msrp_reply_flags, 1, fixup_igp_null,
		0, ANY_ROUTE},
	{"msrp_cmap_save",   (cmd_function)w_msrp_cmap_save, 0, 0,
		0, ANY_ROUTE},
	{"msrp_cmap_lookup", (cmd_function)w_msrp_cmap_lookup, 0, 0,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"sipmsg",            PARAM_INT,   &msrp_param_sipmsg},
	{"cmap_size",         PARAM_INT,   &msrp_cmap_size},
	{"auth_min_expires",  PARAM_INT,   &msrp_auth_min_expires},
	{"auth_max_expires",  PARAM_INT,   &msrp_auth_max_expires},
	{"timer_interval",    PARAM_INT,   &msrp_timer_interval},
	{"use_path_addr",     PARAM_STR,   &msrp_use_path_addr},
	{"event_callback",    PARAM_STR,   &msrp_event_callback},
	{0, 0, 0}
};

struct module_exports exports = {
	"msrp",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	if(msrp_sruid_init()<0) {
		LM_ERR("cannot init msrp uid\n");
		return -1;
	}

	if(msrp_cmap_init_rpc()<0)
	{
		LM_ERR("failed to register cmap RPC commands\n");
		return -1;
	}

	if(msrp_cmap_size>0) {
		if(msrp_cmap_size>16)
			msrp_cmap_size = 16;
		if(msrp_cmap_init(1<<msrp_cmap_size)<0) {
			LM_ERR("Cannot init internal cmap\n");
			return -1;
		}
		if(msrp_timer_interval<=0)
			msrp_timer_interval = 60;
		register_sync_timers(1);
	}

	sr_event_register_cb(SREV_TCP_MSRP_FRAME, msrp_frame_received);

	if(!module_loaded("tls")) {
		LM_WARN("\"tls\" module is not loaded. TLS is mandatory for"
			" MSRP Relays. To comply with RFC 4976 you must use"
			"  TLS.\n");
	} else {
		msrp_tls_module_loaded = 1;
	}

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	if(msrp_sruid_init()<0) {
		LM_ERR("cannot init msrp uid\n");
		return -1;
	}

	if (rank!=PROC_MAIN)
		return 0;
	if(msrp_cmap_size>0) {
		if(fork_sync_timer(PROC_TIMER, "MSRP Timer", 1 /*socks flag*/,
				msrp_local_timer, NULL, msrp_timer_interval /*sec*/)<0) {
			LM_ERR("failed to start timer routine as process\n");
			return -1; /* error */
		}
	}

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

/**
 *
 */
static int ki_msrp_relay(sip_msg_t* msg)
{
	msrp_frame_t *mf;
	int ret;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_relay(mf);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_relay(sip_msg_t* msg, char* str1, char* str2)
{
	return ki_msrp_relay(msg);
}

/**
 *
 */
static int ki_msrp_reply(sip_msg_t* msg, str* rcode, str* rtext,
		str *rhdrs)
{
	msrp_frame_t *mf;
	int ret;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_reply(mf, rcode, rtext,
			(rhdrs!=NULL && rhdrs->len>0)?rhdrs:NULL);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_reply(sip_msg_t *msg, char *code, char *text, char *hdrs)
{
	str rcode = STR_NULL;
	str rtext = STR_NULL;
	str rhdrs = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)code, &rcode) != 0) {
		LM_ERR("no reply status code\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)text, &rtext) != 0) {
		LM_ERR("no reply status phrase\n");
		return -1;
	}

	if(hdrs != NULL && fixup_get_svalue(msg, (gparam_t *)hdrs, &rhdrs) != 0) {
		LM_ERR("invalid extra headers\n");
		return -1;
	}
	return ki_msrp_reply(msg, &rcode, &rtext, (hdrs!=NULL)?&rhdrs:NULL);
}

/**
 *
 */
static int w_msrp_reply2(sip_msg_t* msg, char* code, char* text)
{
	return w_msrp_reply(msg, code, text, NULL);
}

/**
 *
 */
static int w_msrp_reply3(sip_msg_t* msg, char* code, char* text,
		char *hdrs)
{
	return w_msrp_reply(msg, code, text, hdrs);
}

/**
 *
 */
static int ki_msrp_is_request(sip_msg_t* msg)
{
	msrp_frame_t *mf;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;
	if(mf->fline.msgtypeid==MSRP_REQUEST)
		return 1;
	return -1;
}

/**
 *
 */
static int w_msrp_is_request(sip_msg_t* msg, char* str1, char* str2)
{
	return ki_msrp_is_request(msg);
}

/**
 *
 */
static int ki_msrp_is_reply(sip_msg_t* msg)
{
	msrp_frame_t *mf;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;
	if(mf->fline.msgtypeid==MSRP_REPLY)
		return 1;
	return -1;
}

/**
 *
 */
static int w_msrp_is_reply(sip_msg_t* msg, char* str1, char* str2)
{
	return ki_msrp_is_reply(msg);
}

/**
 *
 */
static int ki_msrp_set_dst(sip_msg_t* msg, str* rtaddr, str* rfsock)
{
	msrp_frame_t *mf;
	int ret;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_env_set_dstinfo(mf, rtaddr, rfsock, 0);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_set_dst(sip_msg_t* msg, char* taddr, char* fsock)
{
	str rtaddr  = {0};
	str rfsock = {0};

	if(fixup_get_svalue(msg, (gparam_t*)taddr, &rtaddr)!=0)
	{
		LM_ERR("invalid target address parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)fsock, &rfsock)!=0)
	{
		LM_ERR("invalid local socket parameter\n");
		return -1;
	}
	return ki_msrp_set_dst(msg, &rtaddr, &rfsock);
}

/**
 *
 */
static int ki_msrp_relay_flags(sip_msg_t* msg, int rtflags)
{
	msrp_frame_t *mf;
	int ret;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_env_set_sndflags(mf, rtflags);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_relay_flags(sip_msg_t* msg, char *tflags, char* str2)
{
	int rtflags = 0;

	if(fixup_get_ivalue(msg, (gparam_t *)tflags, &rtflags) != 0) {
		LM_ERR("invalid send flags parameter\n");
		return -1;
	}

	return ki_msrp_relay_flags(msg, rtflags);
}

/**
 *
 */
static int ki_msrp_reply_flags(sip_msg_t *msg, int rtflags)
{
	msrp_frame_t *mf;
	int ret;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_env_set_rplflags(mf, rtflags);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_reply_flags(sip_msg_t *msg, char *tflags, char *str2)
{
	int rtflags = 0;

	if(fixup_get_ivalue(msg, (gparam_t *)tflags, &rtflags) != 0) {
		LM_ERR("invalid send flags parameter\n");
		return -1;
	}
	return ki_msrp_reply_flags(msg, rtflags);
}

/**
 *
 */
static int ki_msrp_cmap_save(sip_msg_t* msg)
{
	msrp_frame_t *mf;
	int ret;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_cmap_save(mf);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_cmap_save(sip_msg_t* msg, char* str1, char* str2)
{
	return ki_msrp_cmap_save(msg);
}

/**
 *
 */
static int ki_msrp_cmap_lookup(sip_msg_t* msg)
{
	msrp_frame_t *mf;
	int ret;

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_cmap_lookup(mf);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_cmap_lookup(sip_msg_t* msg, char* str1, char* str2)
{
	return ki_msrp_cmap_lookup(msg);
}

/**
 *
 */
static int msrp_frame_received(sr_event_param_t *evp)
{
	tcp_event_info_t *tev;
	static msrp_frame_t mf;
	sip_msg_t *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("msrp:frame-in");

	tev = (tcp_event_info_t*)evp->data;

	if(tev==NULL || tev->buf==NULL || tev->len<=0)
	{
		LM_DBG("invalid parameters\n");
		return -1;
	}

	memset(&mf, 0, sizeof(msrp_frame_t));
	mf.buf.s = tev->buf;
	mf.buf.len = tev->len;
	mf.tcpinfo = tev;
	if(msrp_parse_frame(&mf)<0)
	{
		LM_ERR("error parsing msrp frame\n");
		return -1;
	}
	msrp_reset_env();
	msrp_set_current_frame(&mf);
	fmsg = msrp_fake_sipmsg(&mf);
	if(fmsg != NULL)
		fmsg->rcv = *tev->rcv;
	rtb = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if(msrp_event_callback.s == NULL || msrp_event_callback.len <= 0) {
		/* native cfg script execution */
		rt = route_get(&event_rt, evname.s);
		LM_DBG("executing event_route[msrp:frame-in] (%d)\n", rt);
		if(rt >= 0 && event_rt.rlist[rt] != NULL) {
			run_top_route(event_rt.rlist[rt], fmsg, &ctx);
		} else {
			LM_ERR("empty event route block for msrp handling\n");
		}
	} else {
		/* kemi script execution */
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			LM_ERR("event callback (%s) set, but no cfg engine\n",
					msrp_event_callback.s);
		} else {
			if(sr_kemi_ctx_route(keng, &ctx, fmsg, EVENT_ROUTE,
						&msrp_event_callback, &evname)<0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	if(ctx.run_flags & DROP_R_F) {
		LM_DBG("exit due to 'drop' in event route\n");
	}
	set_route_type(rtb);
	if(fmsg != NULL)
		free_sip_msg(fmsg);
	msrp_reset_env();
	msrp_destroy_frame(&mf);
	return 0;
}

/**
 *
 */
static void msrp_local_timer(unsigned int ticks, void* param)
{
	msrp_cmap_clean();
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_msrp_exports[] = {
	{ str_init("msrp"), str_init("relay"),
		SR_KEMIP_INT, ki_msrp_relay,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("reply"),
		SR_KEMIP_INT, ki_msrp_reply,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("is_request"),
		SR_KEMIP_INT, ki_msrp_is_request,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("is_reply"),
		SR_KEMIP_INT, ki_msrp_is_reply,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("set_dst"),
		SR_KEMIP_INT, ki_msrp_set_dst,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("relay_flags"),
		SR_KEMIP_INT, ki_msrp_relay_flags,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("reply_flags"),
		SR_KEMIP_INT, ki_msrp_reply_flags,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("cmap_save"),
		SR_KEMIP_INT, ki_msrp_cmap_save,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("msrp"), str_init("cmap_lookup"),
		SR_KEMIP_INT, ki_msrp_cmap_lookup,
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
	sr_kemi_modules_add(sr_kemi_msrp_exports);
	return register_trans_mod(path, mod_trans);
}
