/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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
#include "../../mod_fix.h"
#include "../../parser/parse_param.h"
#include "../../shm_init.h"
#include "../../script_cb.h"

#include "debugger_api.h"
#include "debugger_config.h"

MODULE_VERSION

static int  mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

static int w_dbg_breakpoint(struct sip_msg* msg, char* point, char* str2);
static int fixup_dbg_breakpoint(void** param, int param_no);
static int dbg_mod_level_param(modparam_t type, void *val);

static int fixup_dbg_pv_dump(void** param, int param_no);
static int w_dbg_dump(struct sip_msg* msg, char* mask, char* level);

/* parameters */
extern int _dbg_cfgtrace;
extern int _dbg_cfgpkgcheck;
extern int _dbg_breakpoint;
extern int _dbg_cfgtrace_level;
extern int _dbg_cfgtrace_facility;
extern char *_dbg_cfgtrace_prefix;
extern char *_dbg_cfgtrace_lname;
extern int _dbg_step_usleep;
extern int _dbg_step_loops;
extern int _dbg_reset_msgid;

static char * _dbg_cfgtrace_facility_str = 0;
static int _dbg_log_assign = 0;

static cmd_export_t cmds[]={
	{"dbg_breakpoint", (cmd_function)w_dbg_breakpoint, 1,
		fixup_dbg_breakpoint, 0, ANY_ROUTE},
	{"dbg_pv_dump", (cmd_function)w_dbg_dump, 0,
		fixup_dbg_pv_dump, 0, ANY_ROUTE},
	{"dbg_pv_dump", (cmd_function)w_dbg_dump, 1,
		fixup_dbg_pv_dump, 0, ANY_ROUTE},
	{"dbg_pv_dump", (cmd_function)w_dbg_dump, 2,
		fixup_dbg_pv_dump, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"cfgtrace",          INT_PARAM, &_dbg_cfgtrace},
	{"breakpoint",        INT_PARAM, &_dbg_breakpoint},
	{"log_level",         INT_PARAM, &_dbg_cfgtrace_level},
	{"log_facility",      PARAM_STRING, &_dbg_cfgtrace_facility_str},
	{"log_prefix",        PARAM_STRING, &_dbg_cfgtrace_prefix},
	{"log_level_name",    PARAM_STRING, &_dbg_cfgtrace_lname},
	{"log_assign",        INT_PARAM, &_dbg_log_assign},
	{"step_usleep",       INT_PARAM, &_dbg_step_usleep},
	{"step_loops",        INT_PARAM, &_dbg_step_loops},
	{"mod_hash_size",     INT_PARAM, &default_dbg_cfg.mod_hash_size},
	{"mod_level_mode",    INT_PARAM, &default_dbg_cfg.mod_level_mode},
	{"mod_level",         PARAM_STRING|USE_FUNC_PARAM, (void*)dbg_mod_level_param},
	{"reset_msgid",       INT_PARAM, &_dbg_reset_msgid},
	{"cfgpkgcheck",       INT_PARAM, &_dbg_cfgpkgcheck},
	{0, 0, 0}
};

struct module_exports exports = {
	"debugger",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
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
	int fl;
	if (_dbg_cfgtrace_facility_str!=NULL)
	{
		fl = str2facility(_dbg_cfgtrace_facility_str);
		if (fl != -1)
		{
			_dbg_cfgtrace_facility = fl;
		} else {
			LM_ERR("invalid log facility configured");
			return -1;
		}
	}

	if(dbg_init_rpc()!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(cfg_declare("dbg", dbg_cfg_def, &default_dbg_cfg, cfg_sizeof(dbg), &dbg_cfg))
	{
		LM_ERR("Fail to declare the configuration\n");
		return -1;
	}
	LM_DBG("cfg level_mode:%d hash_size:%d\n",
		cfg_get(dbg, dbg_cfg, mod_level_mode),
		cfg_get(dbg, dbg_cfg, mod_hash_size));

	if(dbg_init_mod_levels(cfg_get(dbg, dbg_cfg, mod_hash_size))<0)
	{
		LM_ERR("failed to init per module log level\n");
		return -1;
	}

	if(_dbg_log_assign>0)
	{
		if(dbg_init_pvcache()!=0)
		{
			LM_ERR("failed to create pvcache\n");
			return -1;
		}
	}
	if(_dbg_reset_msgid==1)
	{
		unsigned int ALL = REQUEST_CB+FAILURE_CB+ONREPLY_CB
		  +BRANCH_CB+ONSEND_CB+ERROR_CB+LOCAL_CB+EVENT_CB+BRANCH_FAILURE_CB;
		if (register_script_cb(dbg_msgid_filter, PRE_SCRIPT_CB|ALL, 0) != 0) {
			LM_ERR("could not insert callback");
			return -1;
		}
	}
	return dbg_init_bp_list();
}

/**
 * child init function
 */
static int child_init(int rank)
{
	LM_DBG("rank is (%d)\n", rank);
	if (rank==PROC_INIT) {
		dbg_enable_mod_levels();
		dbg_enable_log_assign();
		return dbg_init_pid_list();
	}
	return dbg_init_mypid();
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 * cfg wrapper to set breakpoint (not implemented yet)
 */
static int w_dbg_breakpoint(struct sip_msg* msg, char* point, char* str2)
{
	return 1;
}

/**
 * fixup for cfg dbg_pv_dump
 */
static int fixup_dbg_pv_dump(void** param, int param_no)
{
	unsigned int mask;
	int level;
	str s = STR_NULL;

	switch(param_no)
	{
		case 2:
			switch(((char*)(*param))[2])
			{
				case 'A': level = L_ALERT; break;
				case 'B': level = L_BUG; break;
				case 'C': level = L_CRIT2; break;
				case 'E': level = L_ERR; break;
				case 'W': level = L_WARN; break;
				case 'N': level = L_NOTICE; break;
				case 'I': level = L_INFO; break;
				case 'D': level = L_DBG; break;
				default:
					LM_ERR("unknown log level\n");
					return E_UNSPEC;
			}
			*param = (void*)(long)level;
		break;
		case 1:
			s.s = *param;
			s.len = strlen(s.s);
			if(str2int(&s, &mask) == 0) {
				*param = (void*)(long)mask;
			}
			else return E_UNSPEC;
		break;
	}

    return 0;
}

/**
 * dump pv_cache contents as json
 */
static int w_dbg_dump(struct sip_msg* msg, char* mask, char* level)
{
	unsigned int umask = DBG_DP_ALL;
	int ilevel = L_DBG;
	if(level!=NULL){
		ilevel = (int)(long)level;
	}
	if(mask!=NULL){
		umask = (unsigned int)(unsigned long)mask;
	}
	dbg_dump_json(msg, umask, ilevel);
	return 1;
}

/**
 * get the pointer to action structure
 */
static struct action *dbg_fixup_get_action(void **param, int param_no)
{
	struct action *ac, ac2;
	action_u_t *au, au2;
	/* param points to au->u.string, get pointer to au */
	au = (void*) ((char *)param - ((char *)&au2.u.string-(char *)&au2));
	au = au - 1 - param_no;
	ac = (void*) ((char *)au - ((char *)&ac2.val-(char *)&ac2));
	return ac;
}


/**
 * fixup for cfg set breakpoint function
 */
static int fixup_dbg_breakpoint(void** param, int param_no)
{
	struct action *a;
	char *p;

	if(param_no!=1)
		return -1;
	a = dbg_fixup_get_action(param, param_no);
	p = (char*)(*param);

    return dbg_add_breakpoint(a, (*p=='0')?0:1);
}

static int dbg_mod_level_param(modparam_t type, void *val)
{
	char *p;
	str s;
	int l;
	if(val==NULL)
		return -1;

	p = strchr((char*)val, '=');
	if(p==NULL) {
		LM_ERR("invalid parameter value: %s\n", (char*)val);
		return -1;
	}
	s.s = p + 1;
	s.len = strlen(s.s);

	if(str2sint(&s, &l)<0) {
		LM_ERR("invalid parameter - level value: %s\n", (char*)val);
		return -1;
	}
	s.s = (char*)val;
	s.len = p - s.s;
	LM_DBG("cfg level_mode:%d hash_size:%d\n",
		cfg_get(dbg, dbg_cfg, mod_level_mode),
		cfg_get(dbg, dbg_cfg, mod_hash_size));
	if(dbg_init_mod_levels(cfg_get(dbg, dbg_cfg, mod_hash_size))<0)
	{
		LM_ERR("failed to init per module log level\n");
		return -1;
	}
	if(dbg_set_mod_debug_level(s.s, s.len, &l)<0)
	{
		LM_ERR("cannot store parameter: %s\n", (char*)val);
		return -1;
	}
	return 0;

}

