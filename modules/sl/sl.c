/*
 * $Id$
 *
 * sl module - stateless reply
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free
 *  2005-03-01  force for stateless replies the incoming interface of
 *              the request (bogdan)
 */

/**
 * @file
 * @brief SL :: module definitions
 *
 * @ingroup sl
 * Module: @ref sl
 */

/*!
 * @defgroup sl SL :: The SER SL Module
 *
 * The SL module allows SER to act as a stateless UA server and
 * generate replies to SIP requests without keeping state. That is beneficial
 * in many scenarios, in which you wish not to burden server's memory and scale
 * well.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../mem/mem.h"

#include "../../modules/tm/tm_load.h"

#include "sl_stats.h"
#include "sl_funcs.h"
#include "sl.h"

MODULE_VERSION

static int default_code = 500;
static str default_reason = STR_STATIC_INIT("Internal Server Error");

int _sl_filtered_ack_route = -1; /* default disabled */

static int sl_bind_tm = 1;
static struct tm_binds tmb;

static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2);
static int w_send_reply(struct sip_msg* msg, char* str1, char* str2);
static int w_sl_reply_error(struct sip_msg* msg, char* str, char* str2);
static int bind_sl(sl_api_t* api);
static int mod_init(void);
static int child_init(int rank);
static void mod_destroy();
static int fixup_sl_reply(void** param, int param_no);

static cmd_export_t cmds[]={
	{"sl_send_reply",  w_sl_send_reply,             2, fixup_sl_reply,
		REQUEST_ROUTE},
	{"sl_reply",       w_sl_send_reply,             2, fixup_sl_reply,
		REQUEST_ROUTE},
	{"send_reply",     w_send_reply,                2, fixup_sl_reply,
		REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"sl_reply_error", w_sl_reply_error,            0, 0,
		REQUEST_ROUTE},
	{"bind_sl",        (cmd_function)bind_sl,       0, 0,              0},
	{0,0,0,0,0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_code",   PARAM_INT, &default_code},
	{"default_reason", PARAM_STR, &default_reason},
	{"bind_tm",        PARAM_INT, &sl_bind_tm},

    {0, 0, 0}
};
    

#ifdef STATIC_SL
struct module_exports sl_exports = {
#else
struct module_exports exports= {
#endif
    "sl",
    cmds,
    sl_rpc,     /* RPC methods */
    params,     /* param exports */
    mod_init,   /* module initialization function */
    (response_function) 0,
    mod_destroy,
    0,
    child_init  /* per-child init function */
};


static int mod_init(void)
{
	if (init_sl_stats() < 0) {
		ERR("init_sl_stats failed\n");
		return -1;
	}
	if (sl_register_kstats()<0) {
		ERR("init k stats failed\n");
		return -1;
	}

	/* if SL loaded, filter ACKs on beginning */
	if (register_script_cb( sl_filter_ACK, PRE_SCRIPT_CB|REQUEST_CB, 0 )<0) {
		ERR("Failed to install SCRIPT callback\n");
		return -1;
	}
	if(sl_startup()<0)
	{
		ERR("Failed to do startup tasks\n");
		return -1;
	}

	if(sl_bind_tm!=0)
	{
		if(load_tm_api(&tmb)==-1)
		{
			LM_INFO("could not bind tm module - only stateless mode"
					" available\n");
			sl_bind_tm=0;
		}
	}

	_sl_filtered_ack_route=route_lookup(&event_rt, "sl:filtered-ack");
	if (_sl_filtered_ack_route>=0 && event_rt.rlist[_sl_filtered_ack_route]==0)
		_sl_filtered_ack_route=-1; /* disable */

	return 0;
}

static int child_init(int rank)
{
	if (rank == PROC_INIT) {
		if (init_sl_stats_child() < 0) {
			ERR("init_sl_stats_child failed\n");
			return -1;
		}
	}
	return 0;
}


static void mod_destroy()
{
	sl_stats_destroy();
	sl_shutdown();
}


/**
 * @brief Small wrapper around sl_send_reply
 *
 * Warapper around sl_send_rply() which accepts parameters that include
 * config variables
 *
 */
static int w_sl_send_reply(struct sip_msg* msg, char* p1, char* p2)
{
    int code, ret;
    str reason;
    char* r;

    if (get_int_fparam(&code, msg, (fparam_t*)p1) < 0) {
		code = default_code;
    }
    
    if (get_str_fparam(&reason, msg, (fparam_t*)p2) < 0) {
		reason = default_reason;;
    }

	if(reason.s[reason.len-1]=='\0') {
		r = reason.s;
	} else {
		r = as_asciiz(&reason);
		if (r == NULL) r = default_reason.s;
	}
	ret = sl_send_reply(msg, code, r);
    if (r!=reason.s) pkg_free(r);
    return ret;
}


/**
 * @brief Small wrapper around sl_reply_error
 */
static int w_sl_reply_error( struct sip_msg* msg, char* str, char* str2)
{
	return sl_reply_error( msg );
}

/**
 * @brief send stateful reply if transaction was created
 *
 * Check if transation was created for respective SIP request and reply
 * in stateful mode, otherwise send stateless reply
 *
 * @param msg - SIP message structure
 * @param code - reply status code
 * @param reason - reply reason phrase
 * @return 1 for success and -1 for failure
 */
int send_reply(struct sip_msg *msg, int code, str *reason)
{
    char *r = NULL;
	struct cell *t;
	int ret = 1;

	if(reason->s[reason->len-1]=='\0') {
		r = reason->s;
	} else {
		r = as_asciiz(reason);
		if (r == NULL)
		{
			LM_ERR("no pkg for reason phrase\n");
			return -1;
		}
	}

	if(sl_bind_tm!=0)
	{
		t = tmb.t_gett();
		if(t!= NULL && t!=T_UNDEFINED)
		{
			if(tmb.t_reply(msg, code, r)< 0)
			{
				LM_ERR("failed to reply stateful (tm)\n");
				goto error;
			}
			LM_DBG("reply in stateful mode (tm)\n");
			goto done;
		}
	}

	LM_DBG("reply in stateless mode (sl)\n");
	ret = sl_send_reply(msg, code, r);

done:
	if(r!=reason->s) pkg_free(r);
	return ret;

error:
	if(r!=reason->s) pkg_free(r);
	return -1;
}

/**
 * @brief Small wrapper around send_reply
 */
static int w_send_reply(struct sip_msg* msg, char* p1, char* p2)
{
    int code;
    str reason;

    if (get_int_fparam(&code, msg, (fparam_t*)p1) < 0) {
		code = default_code;
    }

    if (get_str_fparam(&reason, msg, (fparam_t*)p2) < 0) {
		reason = default_reason;
    }

	return send_reply(msg, code, &reason);
}

/**
 * @brief store To-tag value in totag parameter
 */
int get_reply_totag(struct sip_msg *msg, str *totag)
{
	struct cell * t;
	if(msg==NULL || totag==NULL)
		return -1;
	if(sl_bind_tm!=0)
	{
		t = tmb.t_gett();
		if(t!= NULL && t!=T_UNDEFINED)
		{
			if(tmb.t_get_reply_totag(msg, totag)< 0)
			{
				LM_ERR("failed to get totag (tm)\n");
				return -1;
			}
			LM_DBG("totag stateful mode (tm)\n");
			return 1;
		}
	}

	LM_DBG("totag stateless mode (sl)\n");
	return sl_get_reply_totag(msg, totag);
}


/**
 * @brief fixup for SL reply config file functions
 */
static int fixup_sl_reply(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_var_int_12(param, 1);
	} else if (param_no == 2) {
		return fixup_var_pve_str_12(param, 2);
	}
	return 0;
}


/**
 * @brief bind functions to SL API structure
 */
static int bind_sl(sl_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->zreply = sl_send_reply;
	api->sreply = sl_send_reply_str;
	api->dreply = sl_send_reply_dlg;
	api->freply = send_reply;
	api->get_reply_totag = get_reply_totag;
	api->register_cb = sl_register_callback;

	return 0;
}
