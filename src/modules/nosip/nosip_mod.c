/*
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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
#include "../../core/receive.h"
#include "../../core/events.h"
#include "../../core/action.h"
#include "../../core/script_cb.h"
#include "../../core/route.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"

MODULE_VERSION

static int nosip_rcv_msg(sr_event_param_t *evp);
static int mod_init(void);

static int pv_get_nosip(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

static int nosip_route_no=-1;
static char* nosip_msg_match = NULL;
static regex_t nosip_msg_match_regexp;
static char* nosip_msg_skip = NULL;
static regex_t nosip_msg_skip_regexp;


static cmd_export_t cmds[] = {
	{0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{{"nosip", (sizeof("nosip")-1)}, /* */
		PVT_OTHER, pv_get_nosip, 0,
		0, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
	{"msg_match",       STR_PARAM, &nosip_msg_match},
	{"msg_skip",        STR_PARAM, &nosip_msg_skip},
	{0, 0, 0}
};

/** module exports */
struct module_exports exports = {
	"nosip",         /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	0,               /* per-child init function */
	0                /* module destroy function */
};

/**
 *
 */
static int mod_init(void)
{
	int route_no;

	route_no=route_get(&event_rt, "nosip:msg");
	if (route_no==-1)
	{
		LM_ERR("failed to find event_route[nosip:msg]\n");
		return -1;
	}
	if (event_rt.rlist[route_no]==0)
	{
		LM_ERR("event_route[nosip:msg] is empty\n");
		return -1;
	}
	nosip_route_no=route_no;

	/* register non-sip hooks */
	sr_event_register_cb(SREV_RCV_NOSIP, nosip_rcv_msg);

	if(nosip_msg_match!=NULL)
	{
		memset(&nosip_msg_match_regexp, 0, sizeof(regex_t));
		if (regcomp(&nosip_msg_match_regexp, nosip_msg_match, REG_EXTENDED)!=0) {
			LM_ERR("bad match re %s\n", nosip_msg_match);
			return E_BAD_RE;
		}
	}
	if(nosip_msg_skip!=NULL)
	{
		memset(&nosip_msg_skip_regexp, 0, sizeof(regex_t));
		if (regcomp(&nosip_msg_skip_regexp, nosip_msg_skip, REG_EXTENDED)!=0) {
			LM_ERR("bad skip re %s\n", nosip_msg_skip);
			return E_BAD_RE;
		}
	}
	return 0;
}

/**
 *
 */
static int pv_get_nosip(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res)
{
	str sb;
	if(msg==NULL || res==NULL)
		return -1;
	sb.s = msg->buf;
	sb.len = msg->len;
	return pv_get_strval(msg, param, res, &sb);
}


/**
 *
 */
static int nosip_rcv_msg(sr_event_param_t *evp)
{
	sip_msg_t* msg;
	regmatch_t pmatch;
	char c;
	struct run_act_ctx ra_ctx;

	msg = (sip_msg_t*)evp->data;

	if(nosip_msg_skip!=NULL || nosip_msg_match!=NULL)
	{
		c = msg->buf[msg->len];
		msg->buf[msg->len] = '\0';
		if (nosip_msg_skip!=NULL &&
			regexec(&nosip_msg_skip_regexp, msg->buf,
					1, &pmatch, 0)==0)
		{
			LM_DBG("matched skip re\n");
			msg->buf[msg->len] = c;
			return -1;
		}
		if (nosip_msg_match!=NULL &&
			regexec(&nosip_msg_match_regexp, msg->first_line.u.request.uri.s,
					1, &pmatch, 0)!=0)
		{
			LM_DBG("message not matched\n");
			msg->buf[msg->len] = c;
			return -1;
		}
		msg->buf[msg->len] = c;
	}

	init_run_actions_ctx(&ra_ctx);
	run_actions(&ra_ctx, event_rt.rlist[nosip_route_no], msg);
	return 0;
}
