/**
 * Copyright (C) 2010-2019 Daniel-Constantin Mierla (asipto.com)
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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"

#include "../../modules/app_lua/modapi.h"

#include "app_lua_sr_api.h"
#include "app_lua_sr_exp.h"

MODULE_VERSION

/** parameters */
static int mod_init(void);

static int app_lua_register_param(modparam_t type, void *val);

static int app_lua_sr_openlibs(lua_State *L);

int _ksr_app_lua_log_mode = 0;

app_lua_api_t _app_lua_api = {0};

static param_export_t params[]={
	{"register", PARAM_STRING|USE_FUNC_PARAM, (void*)app_lua_register_param},
	{0, 0, 0}
};


struct module_exports exports = {
	"app_lua_sr",
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,			/*·exported·functions·*/
	params,		/*·exported·params·*/
	0,			/*·exported·RPC·methods·*/
	0,			/* exported pseudo-variables */
	0,			/*·response·function·*/
	mod_init,	/* initialization module */
	0,			/* per child init function */
	0			/* destroy function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(app_lua_load_api(&_app_lua_api)<0) {
		return -1;
	}
	if(lua_sr_exp_init_mod()<0) {
		return -1;
	}

	_app_lua_api.openlibs_register_f(app_lua_sr_openlibs);
	return 0;
}

static int app_lua_register_param(modparam_t type, void *val)
{
	if(val==NULL) {
		return -1;
	}

	if(lua_sr_exp_register_mod((char*)val)==0)
		return 0;
	return -1;
}

static int app_lua_sr_openlibs(lua_State *L)
{
	lua_sr_core_openlibs(L);
	lua_sr_exp_openlibs(L);
	return 0;
}