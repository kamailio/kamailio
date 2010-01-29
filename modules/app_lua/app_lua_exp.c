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
#include <sys/stat.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "../../modules_k/sl/sl_api.h"

#include "app_lua_api.h"

#define SR_LUA_EXP_MOD_SL	(1<<0)

/**
 *
 */
static unsigned int _sr_lua_exp_reg_mods = 0;

/**
 *
 */
static struct sl_binds _lua_slb;

/**
 *
 */
static int lua_sr_sl_send_reply (lua_State *L)
{
	str txt;
	int code;
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SL))
	{
		LM_WARN("weird: sl function executed but module not registered\n");
		return 0;
	}

	code = lua_tointeger(L, -2);

	if(code<100 || code>=700)
		return 0;
	
	txt.s = (char*)lua_tostring(L, -1);
	if(txt.s!=NULL || env_L->msg==NULL)
	{
		txt.len = strlen(txt.s);
		ret = _lua_slb.send_reply(env_L->msg, code, &txt);
		if(ret<0)
		{
			LM_WARN("sl send_reply returned false\n");
			return 0;
		}
	}
	return 0;
}

/**
 *
 */
static int lua_sr_sl_get_reply_totag (lua_State *L)
{
	str txt;
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SL))
	{
		LM_WARN("weird: sl function executed but module not registered\n");
		return 0;
	}
	ret = _lua_slb.get_reply_totag(env_L->msg, &txt);
	if(ret<0)
	{
		LM_WARN("sl get_reply_totag returned false\n");
		return 0;
	}
	lua_pushlstring(L, txt.s, txt.len);
	return 1;
}

/**
 *
 */
static const luaL_reg _sr_sl_Map [] = {
	{"send_reply",      lua_sr_sl_send_reply},
	{"get_reply_totag", lua_sr_sl_get_reply_totag},
	{NULL, NULL}
};

/**
 *
 */
int lua_sr_exp_init_mod(void)
{
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SL)
	{
		/* load SL API */
		if(load_sl_api(&_lua_slb)==-1)
		{
			LM_ERR("cannot load sl api\n");
			return -1;
		}
		LM_DBG("loaded sl api\n");
	}
	return 0;
}

/**
 *
 */
int lua_sr_exp_register_mod(char *mname)
{
	int len;

	len = strlen(mname);

	if(len==2 && strcmp(mname, "sl")==0)
	{
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_SL;
		return 0;
	}
	return -1;
}

/**
 *
 */
void lua_sr_exp_openlibs(lua_State *L)
{
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SL)
		luaL_openlib(L, "sr.sl",   _sr_sl_Map,   0);
}


