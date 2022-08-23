/**
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

#include "../cfgt/cfgt.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_param.h"
#include "../../core/shm_init.h"
#include "../../core/script_cb.h"
#include "../../core/msg_translator.h"
#include "../../core/kemi.h"

#include "debugger_api.h"
#include "debugger_config.h"
#include "debugger_json.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

static int w_dbg_breakpoint(struct sip_msg* msg, char* point, char* str2);
static int fixup_dbg_breakpoint(void** param, int param_no);
static int dbg_mod_level_param(modparam_t type, void *val);
static int dbg_mod_facility_param(modparam_t type, void *val);

static int fixup_dbg_pv_dump(void** param, int param_no);
static int w_dbg_dump(struct sip_msg* msg, char* mask, char* level);

static struct action *dbg_fixup_get_action(void **param, int param_no);
static int fixup_dbg_sip_msg(void** param, int param_no);
static int w_dbg_sip_msg(struct sip_msg* msg, char *level, char *facility);

extern char* dump_lump_list(struct lump *list, int s_offset, char *s_buf);

/* parameters */
extern int _dbg_cfgtrace;
extern int _dbg_cfgtrace_format;
extern int _dbg_cfgpkgcheck;
extern int _dbg_breakpoint;
extern int _dbg_cfgtrace_level;
extern int _dbg_cfgtrace_facility;
extern char *_dbg_cfgtrace_prefix;
extern char *_dbg_cfgtrace_lname;
extern int _dbg_step_usleep;
extern int _dbg_step_loops;
extern int _dbg_reset_msgid;
extern int _dbg_cfgtest;

/* cfgt api */
extern cfgt_api_t _dbg_cfgt;

static int _dbg_sip_msg_cline;
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
	{"dbg_sip_msg", (cmd_function)w_dbg_sip_msg, 0,
		fixup_dbg_sip_msg, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"dbg_sip_msg", (cmd_function)w_dbg_sip_msg, 1,
		fixup_dbg_sip_msg, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"dbg_sip_msg", (cmd_function)w_dbg_sip_msg, 2,
		fixup_dbg_sip_msg, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"cfgtrace",          INT_PARAM, &_dbg_cfgtrace},
	{"cfgtrace_format",   INT_PARAM, &_dbg_cfgtrace_format},
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
	{"mod_facility_mode", INT_PARAM, &default_dbg_cfg.mod_facility_mode},
	{"mod_facility",      PARAM_STRING|USE_FUNC_PARAM, (void*)dbg_mod_facility_param},
	{"reset_msgid",       INT_PARAM, &_dbg_reset_msgid},
	{"cfgpkgcheck",       INT_PARAM, &_dbg_cfgpkgcheck},
	{"cfgtest",           INT_PARAM, &_dbg_cfgtest},
	{0, 0, 0}
};

struct module_exports exports = {
	"debugger",      /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};


/**
 * init module function
 */
static int mod_init(void)
{
	int fl;
	bind_cfgt_t bind_cfgt;

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

	/* anyhow, should fail before */
	if (!dbg_cfg) {
		return -1;
	}

	LM_DBG("cfg level_mode:%d facility_mode:%d hash_size:%d\n",
		cfg_get(dbg, dbg_cfg, mod_level_mode),
		cfg_get(dbg, dbg_cfg, mod_facility_mode),
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
	if(_dbg_cfgtest==1)
	{
		bind_cfgt = (bind_cfgt_t)find_export("cfgt_bind_cfgt", 1, 0);
		if (!bind_cfgt) {
			LM_ERR("can't find cfgt module\n");
			return -1;
		}

		if (bind_cfgt(&_dbg_cfgt) < 0) {
			return -1;
		}
		LM_INFO("bind to cfgt module\n");
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
		dbg_enable_mod_facilities();
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
	dbg_cfg = NULL;
	dbg_destroy_mod_levels();
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

	if (!dbg_cfg) {
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

	if(dbg_set_mod_debug_level(s.s, s.len, &l)<0)
	{
		LM_ERR("cannot store parameter: %s\n", (char*)val);
		return -1;
	}

	return 0;

}

static int dbg_mod_facility_param(modparam_t type, void *val)
{
	char *p;
	str s;
	int fl;
	if(val==NULL)
		return -1;

	p = strchr((char*)val, '=');
	if(p==NULL) {
		LM_ERR("invalid parameter value: %s\n", (char*)val);
		return -1;
	}
	s.s = p + 1;
	s.len = strlen(s.s);

	if ((fl = str2facility(s.s)) == -1) {
		LM_ERR("invalid parameter - facility value: %s\n", (char*)val);
		return -1;
	}

	s.s = (char*)val;
	s.len = p - s.s;

	if (!dbg_cfg) {
		return -1;
	}

	LM_DBG("cfg facility_mode:%d hash_size:%d\n",
		cfg_get(dbg, dbg_cfg, mod_facility_mode),
		cfg_get(dbg, dbg_cfg, mod_hash_size));

	if(dbg_init_mod_levels(cfg_get(dbg, dbg_cfg, mod_hash_size))<0)
	{
		LM_ERR("failed to init per module log level\n");
		return -1;
	}

	if(dbg_set_mod_debug_facility(s.s, s.len, &fl)<0)
	{
		LM_ERR("cannot store parameter: %s\n", (char*)val);
		return -1;
	}

	return 0;
}

static int fixup_dbg_sip_msg(void** param, int param_no)
{
	int facility;
	int level;
	struct action *dbg_sip_msg_action;

	LM_DBG("dbg_sip_msg() called with %d params\n", param_no);

	switch(param_no)
	{
		case 2:
			facility = str2facility((char*)*(param));
			if (facility == -1) {
				LM_ERR("invalid log facility configured");
				return E_UNSPEC;
			}

			*param = (void*)(long)facility;
			break;

		case 1:
			switch(((char*)(*param))[2])
			{
				/* add L_OFFSET because L_WARN is consdered null pointer */
				case 'A': level = L_ALERT + L_OFFSET; break;
				case 'B': level = L_BUG + L_OFFSET; break;
				case 'C': level = L_CRIT2 + L_OFFSET; break;
				case 'E': level = L_ERR + L_OFFSET; break;
				case 'W': level = L_WARN + L_OFFSET; break;
				case 'N': level = L_NOTICE + L_OFFSET; break;
				case 'I': level = L_INFO + L_OFFSET; break;
				case 'D': level = L_DBG + L_OFFSET; break;
				default:
					LM_ERR("unknown log level\n");
					return E_UNSPEC;
			}

			*param = (void*)(long)level;
			break;

		case 0:
			_dbg_sip_msg_cline = -1;
			return 0;

		default:
			// should not reach here
			_dbg_sip_msg_cline = -1;
			return -1;
	}

	/* save the config line where this config function was called */
	dbg_sip_msg_action = dbg_fixup_get_action(param, param_no);
	_dbg_sip_msg_cline = dbg_sip_msg_action->cline;

	return 0;
}

/**
  * dump current SIP message and a diff lump list
  * part of the code taken from msg_apply_changes_f
  */
static int w_dbg_sip_msg(struct sip_msg* msg, char *level, char *facility)
{
	int ilevel = cfg_get(core, core_cfg, debug);
	int ifacility= cfg_get(core, core_cfg, log_facility);
	int flag = FLAG_MSG_LUMPS_ONLY; // copy lumps only, not the whole message
	unsigned int new_buf_offs=0, orig_offs = 0;
	char *hdr_lumps = NULL;
	char *bdy_lumps = NULL;
	const char *start_txt = "------------------------- START OF SIP message debug --------------------------\n";
	const char *hdr_txt =   "------------------------------ SIP header diffs -------------------------------\n";
	const char *bdy_txt =   "------------------------------- SIP body diffs --------------------------------\n";
	const char *end_txt =   "-------------------------- END OF SIP message debug ---------------------------\n\n";
	struct dest_info send_info;
	str obuf;

	if (msg->first_line.type != SIP_REPLY && get_route_type() != REQUEST_ROUTE) {
		LM_ERR("invalid usage - not in request route\n");
		return -1;
	}

	if (level != NULL) {
		/* substract L_OFFSET previously added */
		ilevel = (int)(long)level - L_OFFSET;
	}

	if (facility != NULL) {
		ifacility = (int)(long)facility;
	}

	/* msg_apply_changes_f code needed to get the current msg */
	init_dest_info(&send_info);
	send_info.proto = PROTO_UDP;
	if(msg->first_line.type == SIP_REPLY) {
		obuf.s = generate_res_buf_from_sip_res(msg,
				(unsigned int*)&obuf.len, BUILD_NO_VIA1_UPDATE);
	} else {
		obuf.s = build_req_buf_from_sip_req(msg,
				(unsigned int*)&obuf.len, &send_info,
				BUILD_NO_PATH|BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
	}

	if(obuf.s == NULL)
	{
		LM_ERR("couldn't update msg buffer content\n");
		return -1;
	}

	if(obuf.len >= BUF_SIZE)
	{
		LM_ERR("new buffer overflow (%d)\n", obuf.len);
		pkg_free(obuf.s);
		return -1;
	}

	/* skip original uri */
	if(msg->first_line.type == SIP_REQUEST) {
		if(msg->new_uri.s) {
			orig_offs = msg->first_line.u.request.uri.s - msg->buf;
			orig_offs += msg->first_line.u.request.uri.len;
		}
	}

	/* alloc private mem and copy lumps */
	hdr_lumps = pkg_malloc(BUF_SIZE);
	bdy_lumps = pkg_malloc(BUF_SIZE);

	new_buf_offs = 0;
	process_lumps(msg, msg->add_rm, hdr_lumps, &new_buf_offs, &orig_offs, &send_info, flag);

	new_buf_offs = 0;
	process_lumps(msg, msg->body_lumps, bdy_lumps, &new_buf_offs, &orig_offs, &send_info, flag);

	/* do the print */
	if (_dbg_sip_msg_cline < 0 ) {
		LOG_FC(ifacility, ilevel, "CONFIG LINE unknown\n%s%.*s%s%s%s%s%s",
			start_txt,
			obuf.len, obuf.s,
			hdr_txt, hdr_lumps,
			bdy_txt, bdy_lumps,
			end_txt);
	} else {
		LOG_FC(ifacility, ilevel, "CONFIG LINE %d\n%s%.*s%s%s%s%s%s",
			_dbg_sip_msg_cline,
			start_txt,
			obuf.len, obuf.s,
			hdr_txt, hdr_lumps,
			bdy_txt, bdy_lumps,
			end_txt);
	}

	/* free lumps */
	if (hdr_lumps) {
		pkg_free(hdr_lumps);
	}

	if (bdy_lumps) {
		pkg_free(bdy_lumps);
	}

	return 1;
}

/**
 * dump pv_cache contents as json with default parameters
 */
static int ki_dbg_pv_dump(sip_msg_t* msg)
{
	dbg_dump_json(msg, DBG_DP_ALL, L_DBG);
	return 1;
}

/**
 * dump pv_cache contents as json with explicit parameters
 */
static int ki_dbg_pv_dump_ex(sip_msg_t* msg, int mask, int level)
{
	dbg_dump_json(msg, (unsigned int)mask, level);
	return 1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_debugger_exports[] = {
	{ str_init("debugger"), str_init("dbg_pv_dump"),
		SR_KEMIP_INT, ki_dbg_pv_dump,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("debugger"), str_init("dbg_pv_dump_ex"),
		SR_KEMIP_INT, ki_dbg_pv_dump_ex,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_debugger_exports);
	return 0;
}
