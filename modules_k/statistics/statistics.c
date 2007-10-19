/*
 * $Id$
 *
 * statistics module - script interface to internal statistics manager
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *  2006-03-14  initial version (bogdan)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../statistics.h"
#include "../../mem/mem.h"
#include "stats_funcs.h"

MODULE_VERSION

static int reg_param_stat( modparam_t type, void* val);
static int mod_init(void);
static int w_update_stat(struct sip_msg* msg, char* stat, char* n);
static int w_reset_stat(struct sip_msg* msg, char* stat, char* foo);
static int fixup_stat(void** param, int param_no);


static cmd_export_t cmds[]={
	{"update_stat",  w_update_stat,  2, fixup_stat, 0,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"reset_stat",   w_reset_stat,    1, fixup_stat, 0,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t mod_params[]={
	{ "variable",  STR_PARAM|USE_FUNC_PARAM, (void*)reg_param_stat },
	{ 0,0,0 }
};


struct module_exports exports= {
	"statistics", /* module's name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,         /* exported functions */
	mod_params,   /* param exports */
	0,            /* exported statistics */
	0,            /* exported MI functions */
	0,            /* exported pseudo-variables */
	0,            /* extra processes */
	mod_init,     /* module initialization function */
	0,            /* reply processing function */
	0,            /* module destroy function */
	0             /* per-child init function */
};



static int reg_param_stat( modparam_t type, void* val)
{
	return reg_statistic( (char*)val);
}



static int mod_init(void)
{
	LM_INFO("initializing\n");

	if (register_all_mod_stats()!=0) {
		LM_ERR("failed to register statistic variables\n");
		return E_UNSPEC;
	}
	return 0;
}



static int fixup_stat(void** param, int param_no)
{
	stat_var *stat;
	str s;
	long n;
	int err;

	s.s = (char*)*param;
	s.len = strlen(s.s);
	if (param_no==1) {
		/* var name - string */
		stat = get_stat( &s );
		if (stat==0) {
			LM_ERR("fixup_stat: variable <%s> not "
				"defined\n", s.s);
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*)stat;
		return 0;
	} else if (param_no==2) {
		/* update value - integer */
		if (s.s[0]=='-' || s.s[0]=='+') {
			n = str2s( s.s+1, s.len-1, &err);
			if (s.s[0]=='-')
				n = -n;
		} else {
			n = str2s( s.s, s.len, &err);
		}
		if (err==0){
			if (n==0) {
				LM_ERR("update with 0 has no sense\n");
				return E_CFG;
			}
			pkg_free(*param);
			*param=(void*)n;
			return 0;
		}else{
			LM_ERR("bad update number <%s>\n",(char*)(*param));
			return E_CFG;
		}
	}
	return 0;
}


static int w_update_stat(struct sip_msg *msg, char *stat, char *n)
{
	update_stat( (stat_var*)stat, (long)n);
	return 1;
}


static int w_reset_stat(struct sip_msg *msg, char* stat, char *foo)
{
	reset_stat( (stat_var*)stat );
	return 1;
}


