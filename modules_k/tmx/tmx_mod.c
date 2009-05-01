/**
 * $Id$
 *
 * Copyright (C) 2009
 *
 * This file is part of SIP-Router.org, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../modules/tm/tm_load.h"

#include "t_var.h"
#include "t_mi.h"

MODULE_VERSION


/** TM bind */
struct tm_binds _tmx_tmb;

/** parameters */

/** module functions */
static int mod_init(void);
void destroy(void);

/* statistic variables */
stat_var *tm_rcv_rpls;
stat_var *tm_rld_rpls;
stat_var *tm_loc_rpls;
stat_var *tm_uas_trans;
stat_var *tm_uac_trans;
stat_var *tm_trans_2xx;
stat_var *tm_trans_3xx;
stat_var *tm_trans_4xx;
stat_var *tm_trans_5xx;
stat_var *tm_trans_6xx;
stat_var *tm_trans_inuse;

#ifdef STATISTICS
static stat_export_t mod_stats[] = {
	{"received_replies" ,    0,              &tm_rcv_rpls    },
	{"relayed_replies" ,     0,              &tm_rld_rpls    },
	{"local_replies" ,       0,              &tm_loc_rpls    },
	{"UAS_transactions" ,    0,              &tm_uas_trans   },
	{"UAC_transactions" ,    0,              &tm_uac_trans   },
	{"2xx_transactions" ,    0,              &tm_trans_2xx   },
	{"3xx_transactions" ,    0,              &tm_trans_3xx   },
	{"4xx_transactions" ,    0,              &tm_trans_4xx   },
	{"5xx_transactions" ,    0,              &tm_trans_5xx   },
	{"6xx_transactions" ,    0,              &tm_trans_6xx   },
	{"inuse_transactions" ,  STAT_NO_RESET,  &tm_trans_inuse },
	{0,0,0}
};
#endif

/**
 * pseudo-variables exported by TM module
 */
static pv_export_t mod_pvs[] = {
	{ {"T_branch_idx", sizeof("T_branch_idx")-1}, PVT_OTHER,
		pv_get_tm_branch_idx, 0,
		 0, 0, 0, 0 },
	{ {"T_reply_code", sizeof("T_reply_code")-1}, PVT_OTHER,
		pv_get_tm_reply_code, 0,
		 0, 0, 0, 0 },
	{ {"T_req", sizeof("T_req")-1}, PVT_OTHER, pv_get_t_var_req, 0,
		pv_parse_t_var_name, 0, 0, 0 },
	{ {"T_rpl", sizeof("T_rpl")-1}, PVT_OTHER, pv_get_t_var_rpl, 0,
		pv_parse_t_var_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static mi_export_t mi_cmds [] = {
	{MI_TM_UAC,     mi_tm_uac_dlg,   MI_ASYNC_RPL_FLAG,  0,  0 },
	{MI_TM_CANCEL,  mi_tm_cancel,    0,                  0,  0 },
	{MI_TM_HASH,    mi_tm_hash,      MI_NO_INPUT_FLAG,   0,  0 },
	{MI_TM_REPLY,   mi_tm_reply,     0,                  0,  0 },
	{0,0,0,0,0}
};


static cmd_export_t cmds[]={
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"tmx",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
#ifdef STATISTICS
	mod_stats,  /* exported statistics */
#else
	0,
#endif
	mi_cmds,    /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	/* load the TM API */
	if (load_tm_api(&_tmx_tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register statistics\n");
		return -1;
	}
#endif
	return 0;
}

/**
 * destroy function
 */
void destroy(void)
{
	return;
}


