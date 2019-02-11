/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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

/**
 * this file is generated - do not edit
 */

#ifndef __APP_LUA_KEMI_EXPORT_H__
#define __APP_LUA_KEMI_EXPORT_H__

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../../core/kemi.h"

#define SR_KEMI_LUA_EXPORT_SIZE	1024

typedef struct sr_kemi_lua_export {
	lua_CFunction pfunc;
	sr_kemi_t *ket;
} sr_kemi_lua_export_t;

sr_kemi_t *sr_kemi_lua_export_get(int idx);
lua_CFunction sr_kemi_lua_export_associate(sr_kemi_t *ket);

#endif
