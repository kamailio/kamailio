/*
 * execution module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \file
 * \brief Exec module:: Module interface
 * \ingroup exec
 * Module: \ref exec
 */


#include <stdio.h>

#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/kemi.h"

#include "exec.h"
#include "kill.h"
#include "exec_hf.h"

MODULE_VERSION

unsigned int time_to_kill = 0;
int exec_bash_safety = 1;

static int mod_init(void);

static int w_exec_dset(struct sip_msg *msg, char *cmd, char *foo);
static int w_exec_msg(struct sip_msg *msg, char *cmd, char *foo);
static int w_exec_avp(struct sip_msg *msg, char *cmd, char *avpl);
static int w_exec_cmd(struct sip_msg *msg, char *cmd, char *foo);

static int exec_avp_fixup(void **param, int param_no);

inline static void exec_shutdown(void);

/* clang-format off */
/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"exec_dset", (cmd_function)w_exec_dset, 1, fixup_spve_null,  0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"exec_msg",  (cmd_function)w_exec_msg,  1, fixup_spve_null,  0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"exec_avp",  (cmd_function)w_exec_avp,  1, fixup_spve_null,  0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"exec_avp",  (cmd_function)w_exec_avp,  2, exec_avp_fixup,   0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"exec_cmd",  (cmd_function)w_exec_cmd,  1, fixup_spve_null,  0,
		ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"time_to_kill", INT_PARAM, &time_to_kill},
	{"setvars",      INT_PARAM, &setvars     },
	{"bash_safety",  INT_PARAM, &exec_bash_safety     },
	{0, 0, 0}
};


struct module_exports exports= {
	"exec",				/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	0,					/* per-child init function */
	exec_shutdown		/* module destroy function */
};
/* clang-format on */

void exec_shutdown(void)
{
	if(time_to_kill)
		destroy_kill();
}


static int mod_init(void)
{
	if(time_to_kill)
		initialize_kill();
	return 0;
}

static int ki_exec_dset(struct sip_msg *msg, str *cmd)
{
	str *uri;
	environment_t *backup;
	int ret;

	if(msg == 0 || cmd == 0)
		return -1;

	backup = 0;
	if(setvars) {
		backup = set_env(msg);
		if(!backup) {
			LM_ERR("no env created\n");
			return -1;
		}
	}

	if(msg->new_uri.s && msg->new_uri.len)
		uri = &msg->new_uri;
	else
		uri = &msg->first_line.u.request.uri;

	LM_DBG("executing [%s]\n", cmd->s);

	ret = exec_str(msg, cmd->s, uri->s, uri->len);
	if(setvars) {
		unset_env(backup);
	}
	return ret;
}

static int w_exec_dset(struct sip_msg *msg, char *cmd, char *foo)
{
	str command;
	if(fixup_get_svalue(msg, (gparam_p)cmd, &command) != 0) {
		LM_ERR("invalid command parameter");
		return -1;
	}
	return ki_exec_dset(msg, &command);
}

static int ki_exec_msg(struct sip_msg *msg, str *cmd)
{
	environment_t *backup;
	int ret;

	if(msg == 0 || cmd == 0)
		return -1;

	backup = 0;
	if(setvars) {
		backup = set_env(msg);
		if(!backup) {
			LM_ERR("no env created\n");
			return -1;
		}
	}

	LM_DBG("executing [%s]\n", cmd->s);

	ret = exec_msg(msg, cmd->s);
	if(setvars) {
		unset_env(backup);
	}
	return ret;
}

static int w_exec_msg(struct sip_msg *msg, char *cmd, char *foo)
{
	str command;

	if(fixup_get_svalue(msg, (gparam_p)cmd, &command) != 0) {
		LM_ERR("invalid command parameter");
		return -1;
	}
	return ki_exec_msg(msg, &command);
}

static int w_exec_avp_helper(sip_msg_t *msg, str *cmd, pvname_list_t *avpl)
{
	environment_t *backup;
	int ret;

	if(msg == 0 || cmd == 0)
		return -1;

	backup = 0;
	if(setvars) {
		backup = set_env(msg);
		if(!backup) {
			LM_ERR("no env created\n");
			return -1;
		}
	}

	LM_DBG("executing [%s]\n", cmd->s);

	ret = exec_avp(msg, cmd->s, avpl);
	if(setvars) {
		unset_env(backup);
	}
	return ret;
}

static int w_exec_avp(struct sip_msg *msg, char *cmd, char *avpl)
{
	str command;

	if(fixup_get_svalue(msg, (gparam_p)cmd, &command) != 0) {
		LM_ERR("invalid command parameter");
		return -1;
	}
	return w_exec_avp_helper(msg, &command, (pvname_list_t *)avpl);
}

static int ki_exec_avp(sip_msg_t *msg, str *cmd)
{
	return w_exec_avp_helper(msg, cmd, NULL);
}

static int exec_avp_fixup(void **param, int param_no)
{
	pvname_list_t *anlist = NULL;
	str s;

	s.s = (char *)(*param);
	if(param_no == 1) {
		if(s.s == NULL) {
			LM_ERR("null format in P%d\n", param_no);
			return E_UNSPEC;
		}
		return fixup_spve_null(param, 1);
	} else if(param_no == 2) {
		if(s.s == NULL) {
			LM_ERR("null format in P%d\n", param_no);
			return E_UNSPEC;
		}
		s.len = strlen(s.s);
		anlist = parse_pvname_list(&s, PVT_AVP);
		if(anlist == NULL) {
			LM_ERR("bad format in P%d [%s]\n", param_no, s.s);
			return E_UNSPEC;
		}
		*param = (void *)anlist;
		return 0;
	}

	return 0;
}

static int ki_exec_cmd(sip_msg_t *msg, str *cmd)
{
	int ret;

	if(cmd == 0 || cmd->s == 0)
		return -1;

	LM_DBG("executing [%s]\n", cmd->s);

	ret = exec_cmd(msg, cmd->s);

	LM_DBG("execution return code: %d\n", ret);

	return (ret == 0) ? 1 : ret;
}

static int w_exec_cmd(struct sip_msg *msg, char *cmd, char *foo)
{
	str command;

	if(fixup_get_svalue(msg, (gparam_p)cmd, &command) != 0) {
		LM_ERR("invalid command parameter");
		return -1;
	}
	return ki_exec_cmd(msg, &command);
}


/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_exec_exports[] = {
	{ str_init("exec"), str_init("exec_dset"),
		SR_KEMIP_INT, ki_exec_dset,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("exec"), str_init("exec_msg"),
		SR_KEMIP_INT, ki_exec_msg,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("exec"), str_init("exec_avp"),
		SR_KEMIP_INT, ki_exec_avp,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("exec"), str_init("exec_cmd"),
		SR_KEMIP_INT, ki_exec_cmd,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_exec_exports);
	return 0;
}
