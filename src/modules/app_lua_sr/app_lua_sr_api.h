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

#ifndef _APP_LUA_SR_H_
#define _APP_LUA_SR_H_

#include <lua.h>

int app_lua_return_int(lua_State *L, int v);
int app_lua_return_error(lua_State *L);
int app_lua_return_boolean(lua_State *L, int b);
int app_lua_return_false(lua_State *L);
int app_lua_return_true(lua_State *L);

void lua_sr_core_openlibs(lua_State *L);
void lua_sr_kemi_register_libs(lua_State *L);

int sr_kemi_lua_exec_func(lua_State* L, int eidx);

#endif

