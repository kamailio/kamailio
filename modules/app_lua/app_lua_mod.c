/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mod_fix.h"

#include "app_lua_sr.h"

MODULE_VERSION

/** parameters */

/* List of allowed chars for a prefix*/
static int  mod_init(void);
static void mod_destroy(void);
static int  child_init(int rank);

static int w_app_lua_dostring(struct sip_msg *msg, char *script, char *extra);
static int w_app_lua_dofile(struct sip_msg *msg, char *script, char *extra);
static int w_app_lua_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3);

int app_lua_load_param(modparam_t type, void *val);

static param_export_t params[]={
	{"load",          STR_PARAM|USE_FUNC_PARAM, (void*)app_lua_load_param},
	{0, 0, 0}
};

static cmd_export_t cmds[]={
	{"lua_dostring", (cmd_function)w_app_lua_dostring, 1, 0,
		0, ANY_ROUTE},
	{"lua_dofile", (cmd_function)w_app_lua_dofile, 1, 0,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run, 1, 0,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run, 2, 0,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run, 3, 0,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run, 4, 0,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"app_lua",
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
	lua_sr_init_mod();
	return 0;
}


/* each child get a new connection to the database */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	return lua_sr_init_child();
}


static void mod_destroy(void)
{
	lua_sr_destroy();
}

static int w_app_lua_dostring(struct sip_msg *msg, char *script, char *extra)
{
	if(!lua_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	return app_lua_dostring(msg, script);
}

static int w_app_lua_dofile(struct sip_msg *msg, char *script, char *extra)
{
	if(!lua_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	return app_lua_dofile(msg, script);
}

static int w_app_lua_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	if(!lua_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	return app_lua_run(msg, func, p1, p2, p3);
}

int app_lua_load_param(modparam_t type, void *val)
{
	if(val==NULL)
		return -1;
	return sr_lua_load_script((char*)val);
}

