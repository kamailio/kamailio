/*
 * statistics module - script interface to internal statistics manager
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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

/*!
 * \defgroup statistics The statistics module
 */
/*!
 * \brief Script interface
 * \ingroup statistics
 * \author bogdan
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mod_fix.h"
#include "../../lib/kcore/statistics.h"
#include "../../mem/mem.h"
#include "stats_funcs.h"

MODULE_VERSION

static int reg_param_stat( modparam_t type, void* val);
static int mod_init(void);
static int w_update_stat(struct sip_msg* msg, char* stat, char* n);
static int w_reset_stat(struct sip_msg* msg, char* stat, char* foo);
static int fixup_stat(void** param, int param_no);

struct stat_or_pv {
	stat_var   *stat;
	pv_spec_t  *pv;
};

struct long_or_pv {
	long   val;
	pv_spec_t  *pv;
};



static cmd_export_t cmds[]={
	{"update_stat",  (cmd_function)w_update_stat,  2, fixup_stat, 0,
		ANY_ROUTE},
	{"reset_stat",   (cmd_function)w_reset_stat,    1, fixup_stat, 0,
		REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t mod_params[]={
	{ "variable",  PARAM_STRING|USE_FUNC_PARAM, (void*)reg_param_stat },
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
	if (register_all_mod_stats()!=0) {
		LM_ERR("failed to register statistic variables\n");
		return E_UNSPEC;
	}
	return 0;
}



static int fixup_stat(void** param, int param_no)
{
	struct stat_or_pv *sopv;
	struct long_or_pv *lopv;
	str s;
	long n=0;
	int err=0;

	s.s = (char*)*param;
	s.len = strlen(s.s);
	if (param_no==1) {
		/* var name - string or pv */
		sopv = (struct stat_or_pv *)pkg_malloc(sizeof(struct stat_or_pv));
		if (sopv==NULL) {
			LM_ERR("no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( sopv, 0 , sizeof(struct stat_or_pv) );
		/* is it pv? */
		if (s.s[0]=='$') {
			if (fixup_pvar_null(param, 1)!=0) {
				LM_ERR("invalid pv %s as parameter\n",s.s);
				return E_CFG;
			}
			sopv->pv = (pv_spec_t*)(*param);
		} else {
			/* it is string */
			sopv->stat = get_stat( &s );
			if (sopv->stat==0) {
				LM_ERR("variable <%s> not defined\n", s.s);
				return E_CFG;
			}
		}
		pkg_free(s.s);
		*param=(void*)sopv;
		return 0;
	} else if (param_no==2) {
		lopv = (struct long_or_pv *) pkg_malloc(sizeof(struct long_or_pv));
		if (lopv == NULL) {
			LM_ERR("no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset(lopv, 0, sizeof(struct long_or_pv));
		/* is it pv? */
		if (s.s[0] == '$') {
			if (fixup_pvar_pvar(param, 2) != 0) {
				LM_ERR("invalid pv %s as parameter\n",s.s);
				return E_CFG;
			}
			lopv->pv = (pv_spec_t*) (*param);
		} else {
			/* it is string */
			/* update value - integer */
			if (s.s[0] == '-' || s.s[0] == '+') {
				n = str2s(s.s + 1, s.len - 1, &err);
				if (s.s[0] == '-')
					n = -n;
			} else {
				n = str2s(s.s, s.len, &err);
			}
			lopv->val = n;
		}

		if (err==0){
			if (n==0 && (s.s[0]!='$')) {	//we can't check the value of the pvar so have to ignore this check if it is a pvar
				LM_ERR("update with 0 has no sense\n");
				return E_CFG;
			}
			pkg_free(s.s);
			*param=(void*)lopv;
			return 0;
		}else{
			LM_ERR("bad update number <%s>\n",(char*)(*param));
			return E_CFG;
		}
	}
	return 0;
}


static int w_update_stat(struct sip_msg *msg, char *stat_p, char *long_p)
{
	struct stat_or_pv *sopv = (struct stat_or_pv *)stat_p;
	struct long_or_pv *lopv = (struct long_or_pv *)long_p;
	pv_value_t pv_val;
	stat_var *stat;
	long n = 0;
	int err;

	if (lopv->val) {
		n=lopv->val;
	} else {
		if (pv_get_spec_value(msg, lopv->pv, &pv_val) != 0 || (pv_val.flags & PV_VAL_STR) == 0) {
			LM_ERR("failed to get pv string value\n");
			return -1;
		}
		str s = pv_val.rs;
		/* it is string */
		/* update value - integer */
		if (s.s[0] == '-' || s.s[0] == '+') {
			n = str2s(s.s + 1, s.len - 1, &err);
			if (s.s[0] == '-')
				n = -n;
		} else {
			n = str2s(s.s, s.len, &err);
		}
	}

	if (sopv->stat) {
		update_stat( sopv->stat, (long)n);
	} else {
		if (pv_get_spec_value(msg, sopv->pv, &pv_val)!=0 ||
		(pv_val.flags & PV_VAL_STR)==0 ) {
			LM_ERR("failed to get pv string value\n");
			return -1;
		}
		stat = get_stat( &(pv_val.rs) );
		if ( stat == 0 ) {
			LM_ERR("variable <%.*s> not defined\n",
				pv_val.rs.len, pv_val.rs.s);
			return -1;
		}
		update_stat( stat, (long)n);
	}

	return 1;
}


static int w_reset_stat(struct sip_msg *msg, char* stat_p, char *foo)
{
	struct stat_or_pv *sopv = (struct stat_or_pv *)stat_p;
	pv_value_t pv_val;
	stat_var *stat;

	if (sopv->stat) {
		reset_stat( sopv->stat );
	} else {
		if (pv_get_spec_value(msg, sopv->pv, &pv_val)!=0 ||
		(pv_val.flags & PV_VAL_STR)==0 ) {
			LM_ERR("failed to get pv string value\n");
			return -1;
		}
		stat = get_stat( &(pv_val.rs) );
		if ( stat == 0 ) {
			LM_ERR("variable <%.*s> not defined\n",
				pv_val.rs.len, pv_val.rs.s);
			return -1;
		}
		reset_stat( stat );
	}


	return 1;
}


