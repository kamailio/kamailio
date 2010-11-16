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
#include "../../modules_k/sqlops/sql_api.h"

#include "app_lua_api.h"

#define SR_LUA_EXP_MOD_SL	(1<<0)
#define SR_LUA_EXP_MOD_TM	(1<<1)
#define SR_LUA_EXP_MOD_SQLOPS	(1<<2)

/**
 *
 */
static unsigned int _sr_lua_exp_reg_mods = 0;

/**
 * sl
 */
static sl_api_t _lua_slb;

/**
 * tm
 */
static tm_api_t _lua_tmb;

/**
 * sqlops
 */
static sqlops_api_t _lua_sqlopsb;

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
static int lua_sr_sqlops_query(lua_State *L)
{
	str scon;
	str squery;
	str sres;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_false(L);
	}

	scon.s = (char*)lua_tostring(L, -3);
	squery.s = (char*)lua_tostring(L, -2);
	sres.s = (char*)lua_tostring(L, -1);
	if(scon.s == NULL || squery.s == NULL || sres.s == NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	scon.len = strlen(scon.s);
	squery.len = strlen(squery.s);
	sres.len = strlen(sres.s);

	if(_lua_sqlopsb.query(&scon, &squery, &sres)<0)
		return app_lua_return_false(L);
	return app_lua_return_true(L);
}

/**
 *
 */
static int lua_sr_sqlops_value(lua_State *L)
{
	str sres;
	int col;
	int row;
	sql_val_t *val;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	sres.s = (char*)lua_tostring(L, -3);
	row = lua_tointeger(L, -2);
	col = lua_tointeger(L, -1);
	if(row<0 || col<0 || sres.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	sres.len = strlen(sres.s);
	if(_lua_sqlopsb.value(&sres, row, col, &val)<0)
		return app_lua_return_false(L);
	if(val->flags&PV_VAL_NULL)
	{
		lua_pushinteger(L, 0);
		return 1;
	}

	if(val->flags&PV_VAL_INT)
	{
		lua_pushinteger(L, val->value.n);
		return 1;
	}
	lua_pushlstring(L, val->value.s.s, val->value.s.len);
	return 1;
}

/**
 *
 */
static int lua_sr_sqlops_is_null(lua_State *L)
{
	str sres;
	int col;
	int row;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	sres.s = (char*)lua_tostring(L, -3);
	row = lua_tointeger(L, -2);
	col = lua_tointeger(L, -1);
	if(row<0 || col<0 || sres.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	sres.len = strlen(sres.s);
	if(_lua_sqlopsb.is_null(&sres, row, col)==1)
		return app_lua_return_true(L);
	return app_lua_return_false(L);
}

/**
 *
 */
static int lua_sr_sqlops_column(lua_State *L)
{
	str sres;
	int col;
	str name = {0, 0};

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	sres.s = (char*)lua_tostring(L, -2);
	col = lua_tointeger(L, -1);
	if(col<0 || sres.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	sres.len = strlen(sres.s);
	if(_lua_sqlopsb.column(&sres, col, &name)<0)
		return app_lua_return_false(L);
	lua_pushlstring(L, name.s, name.len);
	return 1;
}

/**
 *
 */
static int lua_sr_sqlops_nrows(lua_State *L)
{
	str sres;
	int rows;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	sres.s = (char*)lua_tostring(L, -1);
	if(sres.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	sres.len = strlen(sres.s);
	rows = _lua_sqlopsb.nrows(&sres);
	if(rows<0)
		return app_lua_return_false(L);
	lua_pushinteger(L, rows);
	return 1;
}

/**
 *
 */
static int lua_sr_sqlops_ncols(lua_State *L)
{
	str sres;
	int cols;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	sres.s = (char*)lua_tostring(L, -1);
	if(sres.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	sres.len = strlen(sres.s);
	cols = _lua_sqlopsb.ncols(&sres);
	if(cols<0)
		return app_lua_return_false(L);
	lua_pushinteger(L, cols);
	return 1;
}

/**
 *
 */
static int lua_sr_sqlops_reset(lua_State *L)
{
	str sres;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	sres.s = (char*)lua_tostring(L, -1);
	if(sres.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	sres.len = strlen(sres.s);
	_lua_sqlopsb.reset(&sres);
	return app_lua_return_true(L);
}

/**
 *
 */
static const luaL_reg _sr_sqlops_Map [] = {
	{"query",   lua_sr_sqlops_query},
	{"value",   lua_sr_sqlops_value},
	{"is_null", lua_sr_sqlops_is_null},
	{"column",  lua_sr_sqlops_column},
	{"nrows",   lua_sr_sqlops_nrows},
	{"ncols",   lua_sr_sqlops_ncols},
	{"reset",   lua_sr_sqlops_reset},
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
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS)
	{
		/* bind the SQLOPS API */
		if (sqlops_load_api(&_lua_sqlopsb) == -1)
		{
			LM_ERR("cannot bind to SQLOPS API\n");
			return -1;
		}
		LM_DBG("loaded sqlops api\n");
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
	} else 	if(len==6 && strcmp(mname, "sqlops")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_SQLOPS;
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
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS)
		luaL_openlib(L, "sr.sqlops",   _sr_sqlops_Map,   0);
}


