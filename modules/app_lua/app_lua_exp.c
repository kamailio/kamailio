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

#include "../../modules/sl/sl.h"
#include "../../modules/tm/tm_load.h"

#include "app_lua_api.h"

#define SR_LUA_EXP_MOD_SL	(1<<0)
#define SR_LUA_EXP_MOD_TM	(1<<1)

/**
 *
 */
static unsigned int _sr_lua_exp_reg_mods = 0;

/**
 *
 */
static sl_api_t _lua_slb;

/**
 * tm
 */
static tm_api_t _lua_tmb;

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
		return app_lua_return_false(L);
	}

	code = lua_tointeger(L, -2);

	if(code<100 || code>=800)
		return app_lua_return_false(L);
	
	txt.s = (char*)lua_tostring(L, -1);
	if(txt.s!=NULL && env_L->msg!=NULL)
	{
		txt.len = strlen(txt.s);
		ret = _lua_slb.freply(env_L->msg, code, &txt);
		if(ret<0)
		{
			LM_WARN("sl send_reply returned false\n");
			return app_lua_return_false(L);
		}
		return app_lua_return_true(L);
	}
	return app_lua_return_false(L);
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
		return app_lua_return_false(L);
	}
	ret = _lua_slb.get_reply_totag(env_L->msg, &txt);
	if(ret<0)
	{
		LM_WARN("sl get_reply_totag returned false\n");
		return app_lua_return_false(L);
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
static int lua_sr_tm_t_reply(lua_State *L)
{
	char *txt;
	int code;
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_false(L);
	}

	code = lua_tointeger(L, -2);

	if(code<100 || code>=800)
		return app_lua_return_false(L);

	txt = (char*)lua_tostring(L, -1);
	if(txt!=NULL && env_L->msg!=NULL)
	{
		ret = _lua_tmb.t_reply(env_L->msg, code, txt);
		if(ret<0)
		{
			LM_WARN("tm t_reply returned false\n");
			/* shall push FALSE to Lua ?!? */
			return app_lua_return_false(L);
		}
		return app_lua_return_true(L);
	}
	return app_lua_return_false(L);
}

/**
 *
 */
static int lua_sr_tm_t_relay(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	ret = _lua_tmb.t_relay(env_L->msg, NULL, NULL);
	if(ret<0)
	{
		LM_WARN("tm t_relay returned false\n");
		return app_lua_return_false(L);
	}
	return app_lua_return_true(L);
}


/**
 *
 */
static const luaL_reg _sr_tm_Map [] = {
	{"t_reply", lua_sr_tm_t_reply},
	{"t_relay", lua_sr_tm_t_relay},
	{NULL, NULL}
};


/**
 *
 */
int lua_sr_exp_init_mod(void)
{
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SL)
	{
		/* bind the SL API */
		if (sl_load_api(&_lua_slb)!=0) {
			LM_ERR("cannot bind to SL API\n");
			return -1;
		}
		LM_DBG("loaded sl api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM)
	{
		/* bind the TM API */
		if (tm_load_api(&_lua_tmb) == -1)
		{
			LM_ERR("cannot bind to TM API\n");
			return -1;
		}
		LM_DBG("loaded tm api\n");
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
	} else 	if(len==2 && strcmp(mname, "tm")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_TM;
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
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM)
		luaL_openlib(L, "sr.tm",   _sr_tm_Map,   0);
}


