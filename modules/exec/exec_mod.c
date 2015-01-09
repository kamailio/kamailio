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

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../parser/parse_uri.h"

#include "exec.h"
#include "kill.h"
#include "exec_hf.h"

MODULE_VERSION

unsigned int time_to_kill=0;
int exec_bash_safety=1;

static int mod_init( void );

inline static int w_exec_dset(struct sip_msg* msg, char* cmd, char* foo);
inline static int w_exec_msg(struct sip_msg* msg, char* cmd, char* foo);
inline static int w_exec_avp(struct sip_msg* msg, char* cmd, char* avpl);

static int exec_avp_fixup(void** param, int param_no);

inline static void exec_shutdown(void);

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
	{"exec_avp",  (cmd_function)w_exec_avp,  2, exec_avp_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
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
	"exec",
	DEFAULT_DLFLAGS,/* dlopen flags */
	cmds,           /* Exported functions */
	params,         /* Exported parameters */
	0,              /* exported statistics */
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* initialization module */
	0,              /* response function */
	exec_shutdown,  /* destroy function */
	0               /* per-child init function */
};

void exec_shutdown(void)
{
	if (time_to_kill) destroy_kill();
}


static int mod_init( void )
{
	if (time_to_kill) initialize_kill();
	return 0;
}

inline static int w_exec_dset(struct sip_msg* msg, char* cmd, char* foo)
{
	str *uri;
	environment_t *backup;
	int ret;
	str command;
	
	if(msg==0 || cmd==0)
		return -1;
	
	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LM_ERR("no env created\n");
			return -1;
		}
	}

	if (msg->new_uri.s && msg->new_uri.len)
		uri=&msg->new_uri;
	else
		uri=&msg->first_line.u.request.uri;
	
	if(fixup_get_svalue(msg, (gparam_p)cmd, &command)!=0)
	{
		LM_ERR("invalid command parameter");
		return -1;
	}
	
	LM_DBG("executing [%s]\n", command.s);

	ret=exec_str(msg, command.s, uri->s, uri->len);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}


inline static int w_exec_msg(struct sip_msg* msg, char* cmd, char* foo)
{
	environment_t *backup;
	int ret;
	str command;
	
	if(msg==0 || cmd==0)
		return -1;

	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LM_ERR("no env created\n");
			return -1;
		}
	}
	
	if(fixup_get_svalue(msg, (gparam_p)cmd, &command)!=0)
	{
		LM_ERR("invalid command parameter");
		return -1;
	}
	
	LM_DBG("executing [%s]\n", command.s);
	
	ret=exec_msg(msg, command.s);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}

inline static int w_exec_avp(struct sip_msg* msg, char* cmd, char* avpl)
{
	environment_t *backup;
	int ret;
	str command;
	
	if(msg==0 || cmd==0)
		return -1;
	
	backup=0;
	if (setvars) {
		backup=set_env(msg);
		if (!backup) {
			LM_ERR("no env created\n");
			return -1;
		}
	}

	if(fixup_get_svalue(msg, (gparam_p)cmd, &command)!=0)
	{
		LM_ERR("invalid command parameter");
		return -1;
	}
	
	LM_DBG("executing [%s]\n", command.s);

	ret=exec_avp(msg, command.s, (pvname_list_p)avpl);
	if (setvars) {
		unset_env(backup);
	}
	return ret;
}

static int exec_avp_fixup(void** param, int param_no)
{
	pvname_list_t *anlist = NULL;
	str s;

	s.s = (char*)(*param);
	if (param_no==1)
	{
		if(s.s==NULL)
		{
			LM_ERR("null format in P%d\n", param_no);
			return E_UNSPEC;
		}
		return fixup_spve_null(param, 1);
	} else if(param_no==2) {
		if(s.s==NULL)
		{
			LM_ERR("null format in P%d\n", param_no);
			return E_UNSPEC;
		}
		s.len =  strlen(s.s);
		anlist = parse_pvname_list(&s, PVT_AVP);
		if(anlist==NULL)
		{
			LM_ERR("bad format in P%d [%s]\n", param_no, s.s);
			return E_UNSPEC;
		}
		*param = (void*)anlist;
		return 0;
	}

	return 0;
}


