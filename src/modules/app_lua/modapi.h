/**
 * Copyright (C) 2010-2016 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _APP_LUA_MODAPI_H_
#define _APP_LUA_MODAPI_H_

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../../core/sr_module.h"

#define SRLUA_FALSE	0
#define SRLUA_TRUE	1

typedef struct _sr_lua_env
{
	lua_State *L;
	lua_State *LL;
	struct sip_msg *msg;
	unsigned int flags;
	unsigned int nload; /* number of scripts loaded */
} sr_lua_env_t;

typedef int (*app_lua_openlibs_f)(lua_State *L);

typedef sr_lua_env_t* (*app_lua_env_get_f)(void);
typedef int (*app_lua_openlibs_register_f)(app_lua_openlibs_f rfunc);

typedef struct app_lua_api {
	app_lua_env_get_f env_get_f;
	app_lua_openlibs_register_f openlibs_register_f;
} app_lua_api_t;

typedef int (*bind_app_lua_f)(app_lua_api_t* api);
int bind_app_lua(app_lua_api_t* api);

/**
 * @brief Load the app_lua API
 */
static inline int app_lua_load_api(app_lua_api_t *api)
{
	bind_app_lua_f bindapplua;

	bindapplua = (bind_app_lua_f)find_export("bind_app_lua", 0, 0);
	if(bindapplua == 0) {
		LM_ERR("cannot find bind_app_lua\n");
		return -1;
	}
	if(bindapplua(api)<0) {
		LM_ERR("cannot bind app_lua api\n");
		return -1;
	}
	return 0;
}

#endif
