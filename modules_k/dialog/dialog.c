/*
 * $Id$
 *
 * dialog module - basic support for dialog tracking
 *
 * Copyright (C) 2006 Voice Sistem SRL
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
 *  2006-04-14  initial version (bogdan)
 *  2006-11-28  Added statistic support for the number of early and failed
 *              dialogs. (Jeffrey Magder - SOMA Networks) 
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../items.h"
#include "../../script_cb.h"
#include "../../fifo_server.h"
#include "../../mem/mem.h"
#include "../tm/tm_load.h"
#include "../rr/api.h"
#include "../../mi/mi.h"
#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_handlers.h"
#include "dlg_load.h"
#include "dlg_cb.h"

MODULE_VERSION


static int mod_init(void);
static void mod_destroy();

/* module parameter */
static int dlg_hash_size = 4096;
static char* rr_param = "did";
static int dlg_flag = -1;
static char* timeout_spec = 0;
static int default_timeout = 60 * 60 * 12;  /* 12 hours */
static int use_tight_match = 0;

/* statistic variables */
int dlg_enable_stats = 1;
stat_var *active_dlgs = 0;
stat_var *processed_dlgs = 0;
stat_var *expired_dlgs = 0;
stat_var *failed_dlgs = 0;
stat_var *early_dlgs  = 0;

struct tm_binds d_tmb;
struct rr_binds d_rrb;
xl_spec_t timeout_avp;




static cmd_export_t cmds[]={
	{"load_dlg",  (cmd_function)load_dlg,   0, 0,  0},
	{0,0,0,0,0}
};

static param_export_t mod_params[]={
	{ "enable_stats",          INT_PARAM, &dlg_enable_stats       },
	{ "hash_size",             INT_PARAM, &dlg_hash_size          },
	{ "rr_param",              STR_PARAM, &rr_param               },
	{ "dlg_flag",              INT_PARAM, &dlg_flag               },
	{ "timeout_avp",           STR_PARAM, &timeout_spec           },
	{ "default_timeout",       INT_PARAM, &default_timeout        },
	{ "use_tight_match",       INT_PARAM, &use_tight_match        },
	{ 0,0,0 }
};


static stat_export_t mod_stats[] = {
	{"active_dialogs" ,     STAT_NO_RESET,  &active_dlgs       },
	{"early_dialogs",       STAT_NO_RESET,  &early_dlgs        },
	{"processed_dialogs" ,  0,              &processed_dlgs    },
	{"expired_dialogs" ,    0,              &expired_dlgs      },
	{"failed_dialogs",      0,              &failed_dlgs       },
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ "dlg_list",  mi_print_dlgs,   0,  0},
	{ 0, 0, 0, 0}
};


struct module_exports exports= {
	"dialog",        /* module's name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	mod_params,      /* param exports */
	mod_stats,       /* exported statistics */
	mi_cmds,         /* exported MI functions */
	0,               /* exported pseudo-variables */
	mod_init,        /* module initialization function */
	0,               /* reply processing function */
	mod_destroy,
	0                /* per-child init function */
};



int load_dlg( struct dlg_binds *dlgb )
{
	dlgb->register_dlgcb = register_dlgcb;
	return 1;
}



int it_get_dlg_count(struct sip_msg *msg, xl_value_t *res, xl_param_t *param,
		int flags)
{
	int n;
	int l;
	char *ch;

	if(msg==NULL || res==NULL)
		return -1;

	n = active_dlgs ? get_stat_val(active_dlgs) : 0;
	l = 0;
	ch = int2str( n, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->ri = n;
	res->flags = XL_VAL_STR|XL_VAL_INT|XL_TYPE_INT;

	return 0;
}



static int mod_init(void)
{
	int n;

	LOG(L_INFO,"Dialog module - initializing\n");

	/* param checkings */
	if (dlg_flag==-1) {
		LOG(L_ERR,"ERROR:dialog:mod_init: no dlg flag set!!\n");
		return -1;
	} else if (dlg_flag>=8*sizeof(int)) {
		LOG(L_ERR,"ERROR:dialog:mod_init: invalid dlg flag %d!!\n",dlg_flag);
		return -1;
	}

	if (rr_param==0 || rr_param[0]==0) {
		LOG(L_ERR,"ERROR:dialog:mod_init: empty rr_param!!\n");
		return -1;
	} else if (strlen(rr_param)>MAX_DLG_RR_PARAM_NAME) {
		LOG(L_ERR,"ERROR:dialog:mod_init: rr_param too long (max=%d)!!\n",
			MAX_DLG_RR_PARAM_NAME);
		return -1;
	}

	if (timeout_spec) {
		if ( xl_parse_spec(timeout_spec, &timeout_avp, XL_THROW_ERROR
		|XL_DISABLE_MULTI|XL_DISABLE_COLORS)==0 && (timeout_avp.type!=XL_AVP)){
			LOG(L_ERR, "ERROR:dialog:mod_init: malformed or non AVP timeout "
				"AVP definition in '%s'\n", timeout_spec);
			return -1;
		}
	}

	if (default_timeout<=0) {
		LOG(L_ERR,"ERROR:dialog:mod_init: 0 default_timeout not accepted!!\n");
		return -1;
	}

	/* if statistics are disabled, prevent their registration to core */
	if (dlg_enable_stats==0)
		exports.stats = 0;

	/* load the TM API */
	if (load_tm_api(&d_tmb)!=0) {
		LOG(L_ERR, "ERROR:dialog:mod_init: can't load TM API\n");
		return -1;
	}

	/* load RR API also */
	if (load_rr_api(&d_rrb)!=0) {
		LOG(L_ERR, "ERROR:dialog:mod_init: can't load RR API\n");
		return -1;
	}

	/* register callbacks*/
	/* listen for all incoming requests  */
	if ( d_tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, dlg_onreq, 0 ) <=0 ) {
		LOG(L_ERR,"ERROR:dialog:mod_init: cannot register TMCB_REQUEST_IN "
			"callback\n");
		return -1;
	}

	/* listen for all routed requests  */
	if ( d_rrb.register_rrcb( dlg_onroute, 0 ) <0 ) {
		LOG(L_ERR,"ERROR:dialog:mod_init: cannot register RR callback\n");
		return -1;
	}

	if ( register_timer( dlg_timer_routine, 0, 1)<0 ) {
		LOG(L_ERR,"ERROR:dialog:mod_init: failed to register timer \n");
		return -1;
	}

	/* init handlers */
	init_dlg_handlers( rr_param, dlg_flag,
		timeout_spec?&timeout_avp:0, default_timeout, use_tight_match);

	/* init timer */
	if (init_dlg_timer(dlg_ontimeout)!=0) {
		LOG(L_ERR,"ERROR:dialog:mod_init: cannot init timer list\n");
		return -1;
	}

	/* init callbacks */
	if (init_dlg_callbacks()!=0) {
		LOG(L_ERR,"ERROR:dialog:mod_init: cannot init callbacks\n");
		return -1;
	}

	/* initialized the hash table */
	for( n=0 ; n<(8*sizeof(n)) ; n++) {
		if (dlg_hash_size==(1<<n))
			break;
		if (dlg_hash_size<(1<<n)) {
			LOG(L_WARN,"WARNING:dialog:mod_init: hash_size is not a power "
				"of 2 as it should be -> rounding from %d to %d\n",
				dlg_hash_size, 1<<(n-1));
			dlg_hash_size = 1<<(n-1);
		}
	}
	if ( init_dlg_table(dlg_hash_size)<0 ) {
		LOG(L_ERR,"ERROR:dialog:mod_init: failed to create hash table\n");
		return -1;
	}

	if ( register_fifo_cmd( fifo_print_dlgs, "dlg_list",0)<0 ) {
		LOG(L_ERR,"ERROR:dialog:mod_init: failed to register fifo\n");
		return -1;
	}

	if(xl_add_extra("dlg_count", it_get_dlg_count, 100, 0 )!=0) {
		LOG(L_ERR,"ERROR:dialog:mod_init: failed to register pvar "
			"[dlg_no]\n");
		return -1;
	}

	return 0;
}


static void mod_destroy()
{
	destroy_dlg_timer();
	destroy_dlg_table();
	destroy_dlg_callbacks();
	destroy_dlg_handlers();
}

