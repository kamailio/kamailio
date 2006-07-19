/*
 * $Id$
 *
 * sl module
 *
 *
 * ************************************************ *
 * * Bogdan's Source Memorial                       *
 * *                                                *
 * * Welcome, pilgrim! This is one of rare places  *
 * * kept untouched in memory of brave heart,       *
 * * Bogdan, one of most active ser contributors,   *
 * * and winner of the longest line of code content.*
 * *                                                *
 * * Please, preserve this codework heritage, as    *
 * * most of other work has been smashed away during*
 * * extensive clean-up floods.                     *
 * *                                                *
 * * Hereby, we solicit you to adopt this historical*
 * * piece of code. For $100, your name will be     *
 * * be printed in this banner and we will use      *
 * * collected funds to create and display an ASCII *
 * * statue of Bogdan.                              *
 * **************************************************
 *
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../mem/mem.h"
#include "sl_stats.h"
#include "sl_funcs.h"
#include "sl.h"

MODULE_VERSION

static int default_code = 500;
static str default_reason = STR_STATIC_INIT("Internal Server Error");

static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2);
static int w_sl_reply_error(struct sip_msg* msg, char* str, char* str2);
static int bind_sl(sl_api_t* api);
static int mod_init(void);
static int child_init(int rank);
static void mod_destroy();
static int fixup_sl_reply(void** param, int param_no);

static cmd_export_t cmds[]={
	{"sl_send_reply",  w_sl_send_reply,             2, fixup_sl_reply, REQUEST_ROUTE},
	{"sl_reply",       w_sl_send_reply,             2, fixup_sl_reply, REQUEST_ROUTE},
	{"sl_reply_error", w_sl_reply_error,            0, 0,              REQUEST_ROUTE},
	{"bind_sl",        (cmd_function)bind_sl,       0, 0,              0},
	{"api_sl_reply",   (cmd_function)sl_send_reply, 2, 0,              0},
	{0,0,0,0,0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"default_code",   PARAM_INT, &default_code},
    {"default_reason", PARAM_STR, &default_reason},
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

	     /* if SL loaded, filter ACKs on beginning */
	if (register_script_cb( sl_filter_ACK, PRE_SCRIPT_CB|REQ_TYPE_CB, 0 )<0) {
		ERR("Failed to install SCRIPT callback\n");
		return -1;
	}
	sl_startup();

	return 0;
}

static int child_init(int rank)
{
	if (rank == 1) {
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

    r = as_asciiz(&reason);
    if (r == NULL) r = default_reason.s;
    ret = sl_send_reply(msg, code, r);
    if (r) pkg_free(r);
    return ret;
}


static int w_sl_reply_error( struct sip_msg* msg, char* str, char* str2)
{
	return sl_reply_error( msg );
}


static int fixup_sl_reply(void** param, int param_no)
{
	int ret;

	if (param_no == 1) {
		ret = fix_param(FPARAM_AVP, param);
		if (ret <= 0) return ret;		
		return fix_param(FPARAM_INT, param);
	} else if (param_no == 2) {
	        return fixup_var_str_12(param, 2);
	}
	return 0;
}


static int bind_sl(sl_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	api->reply = (sl_send_reply_f)find_export("api_sl_reply", 2, 0);
	if (api->reply == 0) {
		ERR("Can't bind sl_reply functionn");
		return -1;
	}

	return 0;
}
