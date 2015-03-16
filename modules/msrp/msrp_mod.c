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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../action.h"
#include "../../mod_fix.h"
#include "../../events.h"
#include "../../tcp_conn.h"
#include "../../pvar.h"
#include "../../timer_proc.h" /* register_sync_timer */

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

static int msrp_frame_received(void *data);
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
	{0, 0, 0}
};

struct module_exports exports = {
	"msrp",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	mod_pvs,        /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
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
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	return register_trans_mod(path, mod_trans);
}

/**
 *
 */
static int w_msrp_relay(sip_msg_t* msg, char* str1, char* str2)
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
static int w_msrp_reply(struct sip_msg* msg, char* code, char* text,
		char *hdrs)
{
	str rcode;
	str rtext;
	str rhdrs;
	msrp_frame_t *mf;
	int ret;

	if(fixup_get_svalue(msg, (gparam_t*)code, &rcode)!=0)
	{
		LM_ERR("no reply status code\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)text, &rtext)!=0)
	{
		LM_ERR("no reply status phrase\n");
		return -1;
	}

	if(hdrs!=NULL && fixup_get_svalue(msg, (gparam_t*)hdrs, &rhdrs)!=0)
	{
		LM_ERR("invalid extra headers\n");
		return -1;
	}

	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_reply(mf, &rcode, &rtext, (hdrs!=NULL)?&rhdrs:NULL);
	if(ret==0) ret = 1;
	return ret;
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
static int w_msrp_is_request(sip_msg_t* msg, char* str1, char* str2)
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
static int w_msrp_is_reply(sip_msg_t* msg, char* str1, char* str2)
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
static int w_msrp_set_dst(sip_msg_t* msg, char* taddr, char* fsock)
{
	str rtaddr  = {0};
	str rfsock = {0};
	msrp_frame_t *mf;
	int ret;

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


	mf = msrp_get_current_frame();
	if(mf==NULL)
		return -1;

	ret = msrp_env_set_dstinfo(mf, &rtaddr, &rfsock, 0);
	if(ret==0) ret = 1;
	return ret;
}

/**
 *
 */
static int w_msrp_relay_flags(sip_msg_t* msg, char *tflags, char* str2)
{
	int rtflags = 0;
	msrp_frame_t *mf;
	int ret;
	if(fixup_get_ivalue(msg, (gparam_t*)tflags, &rtflags)!=0)
	{
		LM_ERR("invalid send flags parameter\n");
		return -1;
	}

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
static int w_msrp_reply_flags(sip_msg_t* msg, char *tflags, char* str2)
{
	int rtflags = 0;
	msrp_frame_t *mf;
	int ret;
	if(fixup_get_ivalue(msg, (gparam_t*)tflags, &rtflags)!=0)
	{
		LM_ERR("invalid send flags parameter\n");
		return -1;
	}

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
static int w_msrp_cmap_save(sip_msg_t* msg, char* str1, char* str2)
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
static int w_msrp_cmap_lookup(sip_msg_t* msg, char* str1, char* str2)
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
static int msrp_frame_received(void *data)
{
	tcp_event_info_t *tev;
	static msrp_frame_t mf;
	sip_msg_t *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;


	tev = (tcp_event_info_t*)data;

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
	rt = route_get(&event_rt, "msrp:frame-in");
	if(rt>=0 && event_rt.rlist[rt]!=NULL) {
		LM_DBG("executing event_route[msrp:frame-in] (%d)\n", rt);
		fmsg = msrp_fake_sipmsg(&mf);
		if(fmsg!=NULL)
			fmsg->rcv = *tev->rcv;
		rtb = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		run_top_route(event_rt.rlist[rt], fmsg, &ctx);
		if(ctx.run_flags&DROP_R_F)
		{
			LM_DBG("exit due to 'drop' in event route\n");
		}
		set_route_type(rtb);
		if(fmsg!=NULL)
			free_sip_msg(fmsg);
	}
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
