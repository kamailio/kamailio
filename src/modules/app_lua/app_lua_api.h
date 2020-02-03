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

#ifndef _APP_LUA_API_H_
#define _APP_LUA_API_H_

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../../core/parser/msg_parser.h"
#include "../../core/kemi.h"

#include "modapi.h"

/**
 * version variable stores a version counter for each script loaded.
 * This counter will be updated via RPC.
 */
typedef struct _sr_lua_script_ver
{
	unsigned int *version;
	unsigned int len; /* length of version array */
} sr_lua_script_ver_t;

typedef struct _sr_lua_load
{
	char *script;
	int version; /* child loaded version */
	struct _sr_lua_load *next;
} sr_lua_load_t;

int app_lua_openlibs_register(app_lua_openlibs_f rfunc);

sr_lua_env_t *sr_lua_env_get(void);

int lua_sr_initialized(void);
int lua_sr_init_mod(void);
int lua_sr_init_child(void);
void lua_sr_destroy(void);

int sr_lua_load_script(char *script);
int sr_lua_reload_module(unsigned int reload);

int app_lua_dostring(struct sip_msg *msg, char *script);
int app_lua_dofile(struct sip_msg *msg, char *script);
int app_lua_runstring(struct sip_msg *msg, char *script);
int app_lua_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
int app_lua_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3, int emode);

int sr_kemi_lua_exec_func(lua_State* L, int eidx);

int app_lua_init_rpc(void);
int bind_app_lua(app_lua_api_t* api);

#endif

