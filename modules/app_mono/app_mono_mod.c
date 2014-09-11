/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mod_fix.h"

#include "app_mono_api.h"

MODULE_VERSION

/** parameters */

/* List of allowed chars for a prefix*/
static int  mod_init(void);
static void mod_destroy(void);
static int  child_init(int rank);

static int w_app_mono_exec(struct sip_msg *msg, char *script, char *mparam);
static int w_app_mono_run(struct sip_msg *msg, char *mparam, char *extra);

static int fixup_mono_exec(void** param, int param_no);

int app_mono_load_param(modparam_t type, void *val);
int app_mono_register_param(modparam_t type, void *val);

static param_export_t params[]={
	{"load",     PARAM_STRING|USE_FUNC_PARAM, (void*)app_mono_load_param},
	{"register", PARAM_STRING|USE_FUNC_PARAM, (void*)app_mono_register_param},
	{0, 0, 0}
};

static cmd_export_t cmds[]={
	{"mono_exec", (cmd_function)w_app_mono_exec, 1, fixup_mono_exec,
		0, ANY_ROUTE},
	{"mono_exec", (cmd_function)w_app_mono_exec, 2, fixup_mono_exec,
		0, ANY_ROUTE},
	{"mono_run",  (cmd_function)w_app_mono_run,  0, 0,
		0, ANY_ROUTE},
	{"mono_run", (cmd_function)w_app_mono_run,   1, fixup_spve_null,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"app_mono",
	RTLD_NOW | RTLD_GLOBAL, /* dlopen flags */
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
	mono_sr_init_mod();
	return 0;
}


/* each child get a new connection to the database */
static int child_init(int rank)
{
	if(rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if (rank==PROC_INIT)
	{
		/* do a probe before forking */
		if(mono_sr_init_probe()!=0)
			return -1;
		return 0;
	}
	if(mono_sr_init_child()<0)
		return -1;
	if(mono_sr_init_load()<0)
		return -1;
	return 0;
}


static void mod_destroy(void)
{
	mono_sr_destroy();
}

char _mono_buf_stack[2][512];

/**
 *
 */
static int w_app_mono_exec(struct sip_msg *msg, char *script, char *mparam)
{
	str s;
	str p;

	if(!mono_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)script, &s)<0)
	{
		LM_ERR("cannot get the script\n");
		return -1;
	}
	if(s.len>=511)
	{
		LM_ERR("script too long %d\n", s.len);
		return -1;
	}
	if(mparam!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)mparam, &p)<0)
		{
			LM_ERR("cannot get the parameter\n");
			return -1;
		}
		if(p.len>=511)
		{
			LM_ERR("parameter value too long %d\n", p.len);
			return -1;
		}
		memcpy(_mono_buf_stack[1], p.s, p.len);
		_mono_buf_stack[1][p.len] = '\0';
	}
	memcpy(_mono_buf_stack[0], s.s, s.len);
	_mono_buf_stack[0][s.len] = '\0';
	return app_mono_exec(msg, _mono_buf_stack[0],
			(mparam)?_mono_buf_stack[1]:NULL);
}

/**
 *
 */
static int w_app_mono_run(struct sip_msg *msg, char *mparam, char *extra)
{
	str p;

	if(!mono_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	if(mparam!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)mparam, &p)<0)
		{
			LM_ERR("cannot get the parameter\n");
			return -1;
		}
		if(p.len>=511)
		{
			LM_ERR("parameter value too long %d\n", p.len);
			return -1;
		}
		memcpy(_mono_buf_stack[0], p.s, p.len);
		_mono_buf_stack[0][p.len] = '\0';
	}
	return app_mono_run(msg, (mparam)?_mono_buf_stack[0]:NULL);
}


int app_mono_load_param(modparam_t type, void *val)
{
	if(val==NULL)
		return -1;
	return sr_mono_load_script((char*)val);
}

int app_mono_register_param(modparam_t type, void *val)
{
	if(val==NULL)
		return -1;
	return sr_mono_register_module((char*)val);
}

static int fixup_mono_exec(void** param, int param_no)
{
	if(sr_mono_assembly_loaded())
	{
		LM_ERR("cannot use lua_exec(...) when an assembly is loaded\n");
		return -1;
	}
	return fixup_spve_null(param, 1);
}

