/*
 * $Id$
 *
 * sl module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free
 *  2005-03-01  force for stateless replies the incoming interface of
 *              the request (bogdan)
 *  2006-03-29  callbacks for sending replies added (bogdan)
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
#include "sl_funcs.h"
#include "sl_cb.h"

MODULE_VERSION


static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2);
static int w_sl_reply_error(struct sip_msg* msg, char* str, char* str2);
static int fixup_sl_send_reply(void** param, int param_no);
static int mod_init(void);
static void mod_destroy();
extern int totag_avpid;
/* module parameter */
int sl_enable_stats = 1;

/* statistic variables */
stat_var *tx_1xx_rpls;
stat_var *tx_2xx_rpls;
stat_var *tx_3xx_rpls;
stat_var *tx_4xx_rpls;
stat_var *tx_5xx_rpls;
stat_var *tx_6xx_rpls;
stat_var *sent_rpls;
stat_var *sent_err_rpls;
stat_var *rcv_acks;


static cmd_export_t cmds[]={
	{"sl_send_reply",   w_sl_send_reply,            2,  fixup_sl_send_reply,
			REQUEST_ROUTE},
	{"sl_reply_error",  w_sl_reply_error,           0,  0,
			REQUEST_ROUTE},
	{"register_slcb",  (cmd_function)register_slcb, 0,  0,
			0},
	{0,0,0,0,0}
};

static param_export_t mod_params[]={
	{ "enable_stats",  INT_PARAM, &sl_enable_stats },
	{ "totag_avpid",   INT_PARAM, &totag_avpid     },
	{ 0,0,0 }
};


stat_export_t mod_stats[] = {
	{"1xx_replies" ,       0,  &tx_1xx_rpls    },
	{"2xx_replies" ,       0,  &tx_2xx_rpls    },
	{"3xx_replies" ,       0,  &tx_3xx_rpls    },
	{"4xx_replies" ,       0,  &tx_4xx_rpls    },
	{"5xx_replies" ,       0,  &tx_5xx_rpls    },
	{"6xx_replies" ,       0,  &tx_6xx_rpls    },
	{"sent_replies" ,      0,  &sent_rpls      },
	{"sent_err_replies" ,  0,  &sent_err_rpls  },
	{"received_ACKs" ,     0,  &rcv_acks       },
	{0,0,0}
};




#ifdef STATIC_SL
struct module_exports sl_exports = {
#else
struct module_exports exports= {
#endif
	"sl",         /* module's name */
	cmds,         /* exported functions */
	mod_params,   /* param exports */
	mod_stats,    /* exported statistics */
	0,            /* exported MI functions */
	mod_init,     /* module initialization function */
	0,            /* reply processing function */
	mod_destroy,
	0             /* per-child init function */
};




static int mod_init(void)
{
	LOG(L_INFO,"StateLess module - initializing\n");

	/* if statistics are disabled, prevent their registration to core */
	if (sl_enable_stats==0)
#ifdef STATIC_SL
		sl_exports.stats = 0;
#else
		exports.stats = 0;
#endif

	/* filter all ACKs before script */
	if (register_script_cb(sl_filter_ACK, PRE_SCRIPT_CB|REQ_TYPE_CB, 0 )!=0) {
		LOG(L_ERR,"ERROR:sl:mod_init: register_script_cb failed\n");
		return -1;
	}

	/* init internal SL stuff */
	if (sl_startup()!=0) {
		LOG(L_ERR,"ERROR:sl:mod_init: sl_startup failed\n");
		return -1;
	}

	return 0;
}




static void mod_destroy()
{
	sl_shutdown();
	destroy_slcb_lists();

}




static int fixup_sl_send_reply(void** param, int param_no)
{
	unsigned long code;
	int err;

	if (param_no==1){
		code=str2s(*param, strlen(*param), &err);
		if (err==0){
			pkg_free(*param);
			*param=(void*)code;
			return 0;
		}else{
			LOG(L_ERR, "SL module:fixup_sl_send_reply: bad  number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	return 0;
}






static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2)
{
	return sl_send_reply( msg, (unsigned int)(unsigned long)str, str2);
}


static int w_sl_reply_error( struct sip_msg* msg, char* str, char* str2)
{
	return sl_reply_error( msg );
}


