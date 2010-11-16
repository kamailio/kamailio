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
#include "../../modules_k/rr/api.h"
#include "../../modules/auth/api.h"
#include "../../modules_k/auth_db/api.h"
#include "../../modules_k/maxfwd/api.h"
#include "../../modules_k/registrar/api.h"

#include "app_lua_api.h"

#define SR_LUA_EXP_MOD_SL         (1<<0)
#define SR_LUA_EXP_MOD_TM         (1<<1)
#define SR_LUA_EXP_MOD_SQLOPS     (1<<2)
#define SR_LUA_EXP_MOD_RR         (1<<3)
#define SR_LUA_EXP_MOD_AUTH       (1<<4)
#define SR_LUA_EXP_MOD_AUTH_DB    (1<<5)
#define SR_LUA_EXP_MOD_MAXFWD     (1<<6)
#define SR_LUA_EXP_MOD_REGISTRAR  (1<<7)

/**
 *
 */
static unsigned int _sr_lua_exp_reg_mods = 0;

/**
 * auth
 */
static auth_api_s_t _lua_authb;

/**
 * auth_db
 */
static auth_db_api_t _lua_auth_dbb;

/**
 * maxfwd
 */
static maxfwd_api_t _lua_maxfwdb;

/**
 * registrar
 */
static registrar_api_t _lua_registrarb;

/**
 * rr
 */
static rr_api_t _lua_rrb;

/**
 * sqlops
 */
static sqlops_api_t _lua_sqlopsb;

/**
 * sl
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
static int lua_sr_rr_record_route(lua_State *L)
{
	int ret;
	str sv = {0, 0};
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RR))
	{
		LM_WARN("weird: rr function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	if(lua_gettop(L)==1)
	{
		sv.s = (char*)lua_tostring(L, -1);
		if(sv.s!=NULL)
			sv.len = strlen(sv.s);
	}
	ret = _lua_rrb.record_route(env_L->msg, (sv.len>0)?&sv:NULL);

	if(ret<0)
		return app_lua_return_false(L);
	return app_lua_return_true(L);
}

/**
 *
 */
static int lua_sr_rr_loose_route(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RR))
	{
		LM_WARN("weird: rr function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	ret = _lua_rrb.loose_route(env_L->msg);

	if(ret<0)
		return app_lua_return_false(L);
	return app_lua_return_true(L);
}

/**
 *
 */
static const luaL_reg _sr_rr_Map [] = {
	{"record_route",    lua_sr_rr_record_route},
	{"loose_route",     lua_sr_rr_loose_route},
	{NULL, NULL}
};


static int lua_sr_auth_challenge(lua_State *L, int hftype)
{
	int ret;
	str realm = {0, 0};
	int flags;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH))
	{
		LM_WARN("weird: auth function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	if(lua_gettop(L)!=2)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_false(L);
	}
	realm.s = (char*)lua_tostring(L, -2);
	flags   = lua_tointeger(L, -1);
	if(flags<0 || realm.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	realm.len = strlen(realm.s);
	ret = _lua_authb.auth_challenge(env_L->msg, &realm, flags, hftype);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_auth_www_challenge(lua_State *L)
{
	return lua_sr_auth_challenge(L, HDR_AUTHORIZATION_T);
}

/**
 *
 */
static int lua_sr_auth_proxy_challenge(lua_State *L)
{
	return lua_sr_auth_challenge(L, HDR_PROXYAUTH_T);
}

/**
 *
 */
static int lua_sr_auth_pv_authenticate(lua_State *L, int hftype)
{
	int ret;
	str realm  = {0, 0};
	str passwd = {0, 0};
	int flags;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH))
	{
		LM_WARN("weird: auth function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	if(lua_gettop(L)!=3)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_false(L);
	}
	realm.s  = (char*)lua_tostring(L, -3);
	passwd.s = (char*)lua_tostring(L, -2);
	flags    = lua_tointeger(L, -1);
	if(flags<0 || realm.s==NULL || passwd.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	realm.len = strlen(realm.s);
	passwd.len = strlen(passwd.s);
	ret = _lua_authb.pv_authenticate(env_L->msg, &realm, &passwd, flags,
			hftype);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_auth_pv_www_authenticate(lua_State *L)
{
	return lua_sr_auth_pv_authenticate(L, HDR_AUTHORIZATION_T);
}

/**
 *
 */
static int lua_sr_auth_pv_proxy_authenticate(lua_State *L)
{
	return lua_sr_auth_pv_authenticate(L, HDR_PROXYAUTH_T);
}

/**
 *
 */
static int lua_sr_auth_consume_credentials(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH))
	{
		LM_WARN("weird: auth function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	ret = _lua_authb.consume_credentials(env_L->msg);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_reg _sr_auth_Map [] = {
	{"www_challenge",            lua_sr_auth_www_challenge},
	{"proxy_challenge",          lua_sr_auth_proxy_challenge},
	{"pv_www_authenticate",      lua_sr_auth_pv_www_authenticate},
	{"pv_proxy_authenticate",    lua_sr_auth_pv_proxy_authenticate},
	{"consume_credentials",      lua_sr_auth_consume_credentials},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_auth_db_authenticate(lua_State *L, hdr_types_t hftype)
{
	int ret;
	str realm = {0, 0};
	str table = {0, 0};
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH_DB))
	{
		LM_WARN("weird: auth function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	if(lua_gettop(L)!=2)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_false(L);
	}
	realm.s  = (char*)lua_tostring(L, -2);
	table.s  = (char*)lua_tostring(L, -1);
	if(realm.s==NULL || table.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	realm.len = strlen(realm.s);
	table.len = strlen(table.s);
	ret = _lua_auth_dbb.digest_authenticate(env_L->msg, &realm, &table,
			hftype);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_auth_db_www_authenticate(lua_State *L)
{
	return lua_sr_auth_db_authenticate(L, HDR_AUTHORIZATION_T);
}

/**
 *
 */
static int lua_sr_auth_db_proxy_authenticate(lua_State *L)
{
	return lua_sr_auth_db_authenticate(L, HDR_PROXYAUTH_T);
}

/**
 *
 */
static const luaL_reg _sr_auth_db_Map [] = {
	{"www_authenticate",      lua_sr_auth_db_www_authenticate},
	{"proxy_authenticate",    lua_sr_auth_db_proxy_authenticate},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_maxfwd_process_maxfwd(lua_State *L)
{
	int ret;
	int limit;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MAXFWD))
	{
		LM_WARN("weird: maxfwd function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_false(L);
	}
	limit = lua_tointeger(L, -1);
	if(limit<0)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	ret = _lua_maxfwdb.process_maxfwd(env_L->msg, limit);

	return app_lua_return_int(L, ret);
}


/**
 *
 */
static const luaL_reg _sr_maxfwd_Map [] = {
	{"process_maxfwd",      lua_sr_maxfwd_process_maxfwd},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_registrar_save(lua_State *L)
{
	int ret;
	int flags;
	char *table;
	sr_lua_env_t *env_L;

	flags = 0;
	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MAXFWD))
	{
		LM_WARN("weird: maxfwd function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	if(lua_gettop(L)==1)
	{
		table  = (char*)lua_tostring(L, -1);
	} else if(lua_gettop(L)==2) {
		table  = (char*)lua_tostring(L, -2);
		flags = lua_tointeger(L, -1);
	} else {
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_false(L);
	}
	if(table==NULL || strlen(table)==0)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	ret = _lua_registrarb.save(env_L->msg, table, flags);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_registrar_lookup(lua_State *L)
{
	int ret;
	char *table;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MAXFWD))
	{
		LM_WARN("weird: maxfwd function executed but module not registered\n");
		return app_lua_return_false(L);
	}
	if(env_L->msg!=NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_false(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_false(L);
	}
	table  = (char*)lua_tostring(L, -1);
	if(table==NULL || strlen(table)==0)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_false(L);
	}
	ret = _lua_registrarb.lookup(env_L->msg, table);

	return app_lua_return_int(L, ret);
}


/**
 *
 */
static const luaL_reg _sr_registrar_Map [] = {
	{"save",      lua_sr_registrar_save},
	{"lookup",    lua_sr_registrar_lookup},
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
		if (sl_load_api(&_lua_slb) < 0) {
			LM_ERR("cannot bind to SL API\n");
			return -1;
		}
		LM_DBG("loaded sl api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM)
	{
		/* bind the TM API */
		if (tm_load_api(&_lua_tmb) < 0)
		{
			LM_ERR("cannot bind to TM API\n");
			return -1;
		}
		LM_DBG("loaded tm api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS)
	{
		/* bind the SQLOPS API */
		if (sqlops_load_api(&_lua_sqlopsb) < 0)
		{
			LM_ERR("cannot bind to SQLOPS API\n");
			return -1;
		}
		LM_DBG("loaded sqlops api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RR)
	{
		/* bind the RR API */
		if (rr_load_api(&_lua_rrb) < 0)
		{
			LM_ERR("cannot bind to RR API\n");
			return -1;
		}
		LM_DBG("loaded rr api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH)
	{
		/* bind the AUTH API */
		if (auth_load_api(&_lua_authb) < 0)
		{
			LM_ERR("cannot bind to AUTH API\n");
			return -1;
		}
		LM_DBG("loaded auth api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH_DB)
	{
		/* bind the AUTH_DB API */
		if (auth_db_load_api(&_lua_auth_dbb) < 0)
		{
			LM_ERR("cannot bind to AUTH_DB API\n");
			return -1;
		}
		LM_DBG("loaded auth_db api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MAXFWD)
	{
		/* bind the MAXFWD API */
		if (maxfwd_load_api(&_lua_maxfwdb) < 0)
		{
			LM_ERR("cannot bind to MAXFWD API\n");
			return -1;
		}
		LM_DBG("loaded maxfwd api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_REGISTRAR)
	{
		/* bind the REGISTRAR API */
		if (registrar_load_api(&_lua_registrarb) < 0)
		{
			LM_ERR("cannot bind to REGISTRAR API\n");
			return -1;
		}
		LM_DBG("loaded registrar api\n");
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
	} else 	if(len==2 && strcmp(mname, "rr")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_RR;
		return 0;
	} else 	if(len==4 && strcmp(mname, "auth")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_AUTH;
		return 0;
	} else 	if(len==7 && strcmp(mname, "auth_db")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_AUTH_DB;
		return 0;
	} else 	if(len==6 && strcmp(mname, "maxfwd")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_MAXFWD;
		return 0;
	} else 	if(len==9 && strcmp(mname, "registrar")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_REGISTRAR;
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
		luaL_openlib(L, "sr.sl",         _sr_sl_Map,          0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM)
		luaL_openlib(L, "sr.tm",         _sr_tm_Map,          0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS)
		luaL_openlib(L, "sr.sqlops",     _sr_sqlops_Map,      0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RR)
		luaL_openlib(L, "sr.rr",         _sr_rr_Map,          0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH)
		luaL_openlib(L, "sr.auth",       _sr_auth_Map,        0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_AUTH_DB)
		luaL_openlib(L, "sr.auth_db",    _sr_auth_db_Map,     0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MAXFWD)
		luaL_openlib(L, "sr.maxfwd",     _sr_maxfwd_Map,      0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_REGISTRAR)
		luaL_openlib(L, "sr.registrar",  _sr_registrar_Map,   0);
}

