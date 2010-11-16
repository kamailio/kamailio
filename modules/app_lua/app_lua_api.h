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

#ifndef _APP_LUA_API_H_
#define _APP_LUA_API_H_

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../../parser/msg_parser.h"

typedef struct _sr_lua_env
{
	lua_State *L;
	lua_State *LL;
	struct sip_msg *msg;
	unsigned int flags;
} sr_lua_env_t;

sr_lua_env_t *sr_lua_env_get(void);

int lua_sr_initialized(void);
int lua_sr_init_mod(void);
int lua_sr_init_child(void);
void lua_sr_destroy(void);
int lua_sr_init_probe(void);

int sr_lua_load_script(char *script);
int sr_lua_register_module(char *mname);

int app_lua_dostring(struct sip_msg *msg, char *script);
int app_lua_dofile(struct sip_msg *msg, char *script);
int app_lua_runstring(struct sip_msg *msg, char *script);
int app_lua_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3);

#define SRLUA_FALSE	0
#define SRLUA_TRUE	1
int app_lua_return_boolean(lua_State *L, int b);
int app_lua_return_false(lua_State *L);
int app_lua_return_true(lua_State *L);
int app_lua_return_int(lua_State *L, int v);

void app_lua_dump_stack(lua_State *L);

#endif

