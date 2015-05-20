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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../route.h"
#include "../../ut.h"

#include "../../modules/sl/sl.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sqlops/sql_api.h"
#include "../../modules/rr/api.h"
#include "../../modules/auth/api.h"
#include "../../modules/auth_db/api.h"
#include "../../modules/maxfwd/api.h"
#include "../../modules/registrar/api.h"
#include "../../modules/dispatcher/api.h"
#include "../../modules/xhttp/api.h"
#include "../../modules/sdpops/api.h"
#include "../../modules/presence/bind_presence.h"
#include "../../modules/presence_xml/api.h"
#include "../../modules/textops/api.h"
#include "../../modules/pua_usrloc/api.h"
#include "../../modules/siputils/siputils.h"
#include "../../modules/rls/api.h"
#include "../../modules/alias_db/api.h"
#include "../../modules/msilo/api.h"
#include "../../modules/uac/api.h"
#include "../../modules/sanity/api.h"
#include "../../modules/cfgutils/api.h"
#include "../../modules/tmx/api.h"
#include "../../modules/mqueue/api.h"
#include "../../modules/ndb_mongodb/api.h"

#include "app_lua_api.h"

#define SR_LUA_EXP_MOD_SL         (1<<0)
#define SR_LUA_EXP_MOD_TM         (1<<1)
#define SR_LUA_EXP_MOD_SQLOPS     (1<<2)
#define SR_LUA_EXP_MOD_RR         (1<<3)
#define SR_LUA_EXP_MOD_AUTH       (1<<4)
#define SR_LUA_EXP_MOD_AUTH_DB    (1<<5)
#define SR_LUA_EXP_MOD_MAXFWD     (1<<6)
#define SR_LUA_EXP_MOD_REGISTRAR  (1<<7)
#define SR_LUA_EXP_MOD_DISPATCHER (1<<8)
#define SR_LUA_EXP_MOD_XHTTP      (1<<9)
#define SR_LUA_EXP_MOD_SDPOPS     (1<<10)
#define SR_LUA_EXP_MOD_PRESENCE   (1<<11)
#define SR_LUA_EXP_MOD_PRESENCE_XML (1<<12)
#define SR_LUA_EXP_MOD_TEXTOPS    (1<<13)
#define SR_LUA_EXP_MOD_PUA_USRLOC (1<<14)
#define SR_LUA_EXP_MOD_SIPUTILS   (1<<15)
#define SR_LUA_EXP_MOD_RLS        (1<<16)
#define SR_LUA_EXP_MOD_ALIAS_DB   (1<<17)
#define SR_LUA_EXP_MOD_MSILO      (1<<18)
#define SR_LUA_EXP_MOD_UAC        (1<<19)
#define SR_LUA_EXP_MOD_SANITY     (1<<20)
#define SR_LUA_EXP_MOD_CFGUTILS   (1<<21)
#define SR_LUA_EXP_MOD_TMX        (1<<22)
#define SR_LUA_EXP_MOD_MQUEUE     (1<<23)
#define SR_LUA_EXP_MOD_NDB_MONGODB (1<<24)

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
 * dispatcher
 */
static dispatcher_api_t _lua_dispatcherb;

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
static tm_api_t  _lua_tmb;
static tm_xapi_t _lua_xtmb;

/**
 * xhttp
 */
static xhttp_api_t _lua_xhttpb;

/**
 * sdpops
 */
static sdpops_api_t _lua_sdpopsb;

/**
 * presence
 */
static presence_api_t _lua_presenceb;

/**
 * presence_xml
 */
static presence_xml_api_t _lua_presence_xmlb;

/**
 * textops
 */
static textops_api_t _lua_textopsb;

/**
 * pua_usrloc
 */
static pua_usrloc_api_t _lua_pua_usrlocb;

/**
 * siputils
 */
static siputils_api_t _lua_siputilsb;

/**
 * rls
 */
static rls_api_t _lua_rlsb;

/**
 * alias_db
 */
static alias_db_api_t _lua_alias_dbb;

/**
 * msilo 
 */
static msilo_api_t _lua_msilob;

/**
 * uac
 */
static uac_api_t _lua_uacb;

/**
 * sanity
 */
static sanity_api_t _lua_sanityb;

/**
 * cfgutils
 */
static cfgutils_api_t _lua_cfgutilsb;

/**
 * tmx
 */
static tmx_api_t _lua_tmxb;

/**
 * mqueue
 */
static mq_api_t _lua_mqb;

/**
 * mqueue
 */
static ndb_mongodb_api_t _lua_ndb_mongodbb;


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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	code = lua_tointeger(L, -2);

	if(code<100 || code>=800)
		return app_lua_return_error(L);
	
	txt.s = (char*)lua_tostring(L, -1);
	if(txt.s==NULL || env_L->msg==NULL)
		return app_lua_return_error(L);

	txt.len = strlen(txt.s);
	ret = _lua_slb.freply(env_L->msg, code, &txt);
	return app_lua_return_int(L, ret);
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
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
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
static const luaL_Reg _sr_sl_Map [] = {
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	code = lua_tointeger(L, -2);

	if(code<100 || code>=800)
		return app_lua_return_error(L);

	txt = (char*)lua_tostring(L, -1);
	if(txt!=NULL && env_L->msg!=NULL)
	{
		ret = _lua_tmb.t_reply(env_L->msg, code, txt);
		return app_lua_return_int(L, ret);
	}
	return app_lua_return_error(L);
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	ret = _lua_tmb.t_relay(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_tm_t_on_failure(lua_State *L)
{
	char *name;
	int i;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	name = (char*)lua_tostring(L, -1);
	if(name==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}

	i = route_get(&failure_rt, name);
	if(failure_rt.rlist[i]==0)
	{
		LM_WARN("no actions in failure_route[%s]\n", name);
		return app_lua_return_error(L);
	}

	_lua_xtmb.t_on_failure((unsigned int)i);
	return app_lua_return_int(L, 1);
}

/**
 *
 */
static int lua_sr_tm_t_on_branch(lua_State *L)
{
	char *name;
	int i;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	name = (char*)lua_tostring(L, -1);
	if(name==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}

	i = route_get(&branch_rt, name);
	if(branch_rt.rlist[i]==0)
	{
		LM_WARN("no actions in branch_route[%s]\n", name);
		return app_lua_return_error(L);
	}

	_lua_xtmb.t_on_branch((unsigned int)i);
	return app_lua_return_int(L, 1);
}

/**
 *
 */
static int lua_sr_tm_t_on_reply(lua_State *L)
{
	char *name;
	int i;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	name = (char*)lua_tostring(L, -1);
	if(name==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}

	i = route_get(&onreply_rt, name);
	if(onreply_rt.rlist[i]==0)
	{
		LM_WARN("no actions in onreply_route[%s]\n", name);
		return app_lua_return_error(L);
	}

	_lua_xtmb.t_on_reply((unsigned int)i);
	return app_lua_return_int(L, 1);
}

/**
 *
 */
static int lua_sr_tm_t_check_trans(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_xtmb.t_check_trans(env_L->msg);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_tm_t_is_canceled(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_xtmb.t_is_canceled(env_L->msg);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_tm_t_newtran(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_tmb.t_newtran(env_L->msg);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_tm_t_release(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_tmb.t_release(env_L->msg);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_tm_t_replicate(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;
	str suri;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	suri.s = (char*)lua_tostring(L, -1);
	if(suri.s == NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	suri.len = strlen(suri.s);

	ret = _lua_tmb.t_replicate(env_L->msg, &suri);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
#define BRANCH_FAILURE_ROUTE_PREFIX "tm:branch-failure"
static int lua_sr_tm_t_on_branch_failure(lua_State *L)
{
	static str rt_name = {NULL, 0};
	char *name;
	int rt_name_len;
	int i;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	name = (char*)lua_tostring(L, -1);
	if(name==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	rt_name_len = strlen(BRANCH_FAILURE_ROUTE_PREFIX) + 1 + strlen(name);
	if (rt_name_len > rt_name.len)
	{
		if ((rt_name.s = pkg_realloc(rt_name.s, rt_name_len+1)) == NULL)
		{
			LM_ERR("No memory left in branch_failure fixup\n");
			return -1;
		}
		rt_name.len = rt_name_len;
	}
	sprintf(rt_name.s, "%s:%s", BRANCH_FAILURE_ROUTE_PREFIX, name);

	i = route_get(&event_rt, rt_name.s);
	if(i < 0 || event_rt.rlist[i]==0)
	{
		LM_WARN("no actions in branch_failure_route[%s]\n", name);
		return app_lua_return_error(L);
	}

	_lua_xtmb.t_on_branch_failure((unsigned int)i);
	return app_lua_return_int(L, 1);
}

/**
 *
 */
static int lua_sr_tm_t_load_contacts(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_tmb.t_load_contacts(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_tm_t_next_contacts(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TM))
	{
		LM_WARN("weird: tm function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_tmb.t_next_contacts(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_tm_Map [] = {
	{"t_reply",             lua_sr_tm_t_reply},
	{"t_relay",             lua_sr_tm_t_relay},
	{"t_on_failure",        lua_sr_tm_t_on_failure},
	{"t_on_branch",         lua_sr_tm_t_on_branch},
	{"t_on_reply",          lua_sr_tm_t_on_reply},
	{"t_check_trans",       lua_sr_tm_t_check_trans},
	{"t_is_canceled",       lua_sr_tm_t_is_canceled},
	{"t_newtran",           lua_sr_tm_t_newtran},
	{"t_release",           lua_sr_tm_t_release},
	{"t_replicate",         lua_sr_tm_t_replicate},
	{"t_on_branch_failure", lua_sr_tm_t_on_branch_failure},
	{"t_load_contacts",     lua_sr_tm_t_load_contacts},
	{"t_next_contacts",     lua_sr_tm_t_next_contacts},
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
	int ret;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	scon.s = (char*)lua_tostring(L, -3);
	squery.s = (char*)lua_tostring(L, -2);
	sres.s = (char*)lua_tostring(L, -1);
	if(scon.s == NULL || squery.s == NULL || sres.s == NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	scon.len = strlen(scon.s);
	squery.len = strlen(squery.s);
	sres.len = strlen(sres.s);

	ret = _lua_sqlopsb.query(&scon, &squery, &sres);
	return app_lua_return_int(L, ret);
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
static int lua_sr_sqlops_xquery(lua_State *L)
{
	str scon;
	str squery;
	str sres;
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SQLOPS))
	{
		LM_WARN("weird: sqlops function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	scon.s = (char*)lua_tostring(L, -3);
	squery.s = (char*)lua_tostring(L, -2);
	sres.s = (char*)lua_tostring(L, -1);
	if(scon.s == NULL || squery.s == NULL || sres.s == NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	scon.len = strlen(scon.s);
	squery.len = strlen(squery.s);
	sres.len = strlen(sres.s);

	ret = _lua_sqlopsb.xquery(env_L->msg, &scon, &squery, &sres);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_sqlops_Map [] = {
	{"query",   lua_sr_sqlops_query},
	{"value",   lua_sr_sqlops_value},
	{"is_null", lua_sr_sqlops_is_null},
	{"column",  lua_sr_sqlops_column},
	{"nrows",   lua_sr_sqlops_nrows},
	{"ncols",   lua_sr_sqlops_ncols},
	{"reset",   lua_sr_sqlops_reset},
	{"xquery",  lua_sr_sqlops_xquery},
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)==1)
	{
		sv.s = (char*)lua_tostring(L, -1);
		if(sv.s!=NULL)
			sv.len = strlen(sv.s);
	}
	ret = _lua_rrb.record_route(env_L->msg, (sv.len>0)?&sv:NULL);

	return app_lua_return_int(L, ret);
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	ret = _lua_rrb.loose_route(env_L->msg);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_rr_add_rr_param(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;
	str param;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RR))
	{
		LM_WARN("weird: rr function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters\n");
		return app_lua_return_error(L);
	}

	param.s = (char*)lua_tostring(L, -1);
	if(param.s!=NULL)
		param.len = strlen(param.s);

	ret = _lua_rrb.add_rr_param(env_L->msg, &param);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_rr_Map [] = {
	{"record_route",    lua_sr_rr_record_route},
	{"loose_route",     lua_sr_rr_loose_route},
	{"add_rr_param",    lua_sr_rr_add_rr_param},
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=2)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	realm.s = (char*)lua_tostring(L, -2);
	flags   = lua_tointeger(L, -1);
	if(flags<0 || realm.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=3)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	realm.s  = (char*)lua_tostring(L, -3);
	passwd.s = (char*)lua_tostring(L, -2);
	flags    = lua_tointeger(L, -1);
	if(flags<0 || realm.s==NULL || passwd.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	realm.len = strlen(realm.s);
	passwd.len = strlen(passwd.s);
	ret = _lua_authb.pv_authenticate(env_L->msg, &realm, &passwd, flags,
			hftype, &env_L->msg->first_line.u.request.method);

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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	ret = _lua_authb.consume_credentials(env_L->msg);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_auth_Map [] = {
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=2)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	realm.s  = (char*)lua_tostring(L, -2);
	table.s  = (char*)lua_tostring(L, -1);
	if(realm.s==NULL || table.s==NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	realm.len = strlen(realm.s);
	table.len = strlen(table.s);
	ret = _lua_auth_dbb.digest_authenticate(env_L->msg, &realm, &table,
			hftype, &env_L->msg->first_line.u.request.method);

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
static const luaL_Reg _sr_auth_db_Map [] = {
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
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	limit = lua_tointeger(L, -1);
	if(limit<0)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	ret = _lua_maxfwdb.process_maxfwd(env_L->msg, limit);

	return app_lua_return_int(L, ret);
}


/**
 *
 */
static const luaL_Reg _sr_maxfwd_Map [] = {
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
	str uri ={NULL, 0};
	sr_lua_env_t *env_L;

	flags = 0;
	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_REGISTRAR))
	{
		LM_WARN("weird: registrar function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)==1)
	{
		table  = (char*)lua_tostring(L, -1);
	} else if(lua_gettop(L)==2) {
		table  = (char*)lua_tostring(L, -2);
		flags = lua_tointeger(L, -1);
	} else if(lua_gettop(L)==3) {
		table  = (char*)lua_tostring(L, -3);
		flags = lua_tointeger(L, -2);
		uri.s = (char*)lua_tostring(L, -1);
		uri.len = strlen(uri.s);
	} else {
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	if(table==NULL || strlen(table)==0)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	if (lua_gettop(L)==3) {
		ret = _lua_registrarb.save_uri(env_L->msg, table, flags, &uri);
	} else {
		ret = _lua_registrarb.save(env_L->msg, table, flags);
	}

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_registrar_lookup(lua_State *L)
{
	int ret;
	char *table = NULL;
	str uri = {NULL, 0};
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_REGISTRAR))
	{
		LM_WARN("weird: registrar function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)==1)
	{
		table = (char*)lua_tostring(L, -1);
	}
	else if (lua_gettop(L)==2)
	{
		table = (char*)lua_tostring(L, -2);
		uri.s = (char*)lua_tostring(L, -1);
		uri.len = strlen(uri.s);
	} else
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	if(table==NULL || strlen(table)==0)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)==2)
	{
		ret = _lua_registrarb.lookup_uri(env_L->msg, table, &uri);
	} else {
		ret = _lua_registrarb.lookup(env_L->msg, table);
	}

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_registrar_registered(lua_State *L)
{
	int ret;
	char *table;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_REGISTRAR))
	{
		LM_WARN("weird: registrar function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	table  = (char*)lua_tostring(L, -1);
	if(table==NULL || strlen(table)==0)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	ret = _lua_registrarb.registered(env_L->msg, table);

	return app_lua_return_int(L, ret);
}


/**
 *
 */
static const luaL_Reg _sr_registrar_Map [] = {
	{"save",      lua_sr_registrar_save},
	{"lookup",    lua_sr_registrar_lookup},
	{"registered",lua_sr_registrar_registered},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_dispatcher_select(lua_State *L)
{
	int ret;
	int setid;
	int algid;
	int mode;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_DISPATCHER))
	{
		LM_WARN("weird: dispatcher function executed but module"
				" not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)==3)
	{
		setid = lua_tointeger(L, -3);
		algid = lua_tointeger(L, -2);
		mode  = lua_tointeger(L, -1);
	} else if(lua_gettop(L)==2) {
		setid = lua_tointeger(L, -2);
		algid = lua_tointeger(L, -1);
		mode  = 0;
	} else {
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	ret = _lua_dispatcherb.select(env_L->msg, setid, algid, mode);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_dispatcher_next(lua_State *L)
{
	int ret;
	int mode;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_DISPATCHER))
	{
		LM_WARN("weird: dispatcher function executed but module"
				" not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	mode = 0;
	if(lua_gettop(L)==1)
	{
		/* mode given as parameter */
		mode  = lua_tointeger(L, -1);
	}
	ret = _lua_dispatcherb.next(env_L->msg, mode);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_dispatcher_mark(lua_State *L)
{
	int ret;
	int mode;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_DISPATCHER))
	{
		LM_WARN("weird: dispatcher function executed but module"
				" not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	mode = 0;
	if(lua_gettop(L)==1)
	{
		/* mode given as parameter */
		mode  = lua_tointeger(L, -1);
	}
	ret = _lua_dispatcherb.mark(env_L->msg, mode);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_dispatcher_is_from(lua_State *L)
{
	int ret;
	int mode;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_DISPATCHER))
	{
		LM_WARN("weird: dispatcher function executed but module"
				" not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	mode = -1;
	if(lua_gettop(L)==1)
	{
		/* mode given as parameter */
		mode  = lua_tointeger(L, -1);
	}
	ret = _lua_dispatcherb.is_from(env_L->msg, mode);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_dispatcher_Map [] = {
	{"select",      lua_sr_dispatcher_select},
	{"next",        lua_sr_dispatcher_next},
	{"mark",        lua_sr_dispatcher_mark},
	{"is_from",     lua_sr_dispatcher_is_from},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_xhttp_reply(lua_State *L)
{
	int rcode;
	str reason;
	str ctype;
	str mbody;
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_XHTTP))
	{
		LM_WARN("weird: xhttp function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	rcode = lua_tointeger(L, -4);
	reason.s = (char*)lua_tostring(L, -3);
	ctype.s = (char*)lua_tostring(L, -2);
	mbody.s = (char*)lua_tostring(L, -1);
	if(reason.s == NULL || ctype.s == NULL || mbody.s == NULL)
	{
		LM_WARN("invalid parameters from Lua\n");
		return app_lua_return_error(L);
	}
	reason.len = strlen(reason.s);
	ctype.len = strlen(ctype.s);
	mbody.len = strlen(mbody.s);

	ret = _lua_xhttpb.reply(env_L->msg, rcode, &reason, &ctype, &mbody);
	return app_lua_return_int(L, ret);
}


/**
 *
 */
static const luaL_Reg _sr_xhttp_Map [] = {
	{"reply",       lua_sr_xhttp_reply},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_sdpops_with_media(lua_State *L)
{
	int ret;
	str media;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SDPOPS))
	{
		LM_WARN("weird: sdpops function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=1)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	media.s = (char*)lua_tostring(L, -1);
	media.len = strlen(media.s);

	ret = _lua_sdpopsb.sdp_with_media(env_L->msg, &media);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_sdpops_Map [] = {
	{"sdp_with_media",       lua_sr_sdpops_with_media},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_pres_auth_status(lua_State *L)
{
	str param[2];
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE))
	{
		LM_WARN("weird: presence function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=2)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -2);
	param[0].len = strlen(param[0].s);
	param[1].s = (char *) lua_tostring(L, -1);
	param[1].len = strlen(param[1].s);
	
	ret = _lua_presenceb.pres_auth_status(env_L->msg, param[0], param[1]);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_pres_handle_publish(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE))
	{
		LM_WARN("weird: presence function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=0)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	ret = _lua_presenceb.handle_publish(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_pres_handle_subscribe(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE))
	{
		LM_WARN("weird: presence function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)==0)
		ret = _lua_presenceb.handle_subscribe0(env_L->msg);
	else if (lua_gettop(L)==1)
	{
		str wuri;
		struct sip_uri parsed_wuri;

		wuri.s = (char *) lua_tostring(L, -1);
		wuri.len = strlen(wuri.s);
		if (parse_uri(wuri.s, wuri.len, &parsed_wuri))
		{
			LM_ERR("failed to parse watcher URI\n");
			return app_lua_return_error(L);
		}
		ret = _lua_presenceb.handle_subscribe(env_L->msg, parsed_wuri.user, parsed_wuri.host);
	}
	else
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_presence_Map [] = {
	{"pres_auth_status",       lua_sr_pres_auth_status},
	{"handle_publish",         lua_sr_pres_handle_publish},
	{"handle_subscribe",       lua_sr_pres_handle_subscribe},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_pres_check_basic(lua_State *L)
{
	str param[2];
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE_XML))
	{
		LM_WARN("weird: presence_xml function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=2)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -2);
	param[0].len = strlen(param[0].s);
	param[1].s = (char *) lua_tostring(L, -1);
	param[1].len = strlen(param[1].s);
	
	ret = _lua_presence_xmlb.pres_check_basic(env_L->msg, param[0], param[1]);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_pres_check_activities(lua_State *L)
{
	str param[2];
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE_XML))
	{
		LM_WARN("weird: presence_xml function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=2)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -2);
	param[0].len = strlen(param[0].s);
	param[1].s = (char *) lua_tostring(L, -1);
	param[1].len = strlen(param[1].s);
	
	ret = _lua_presence_xmlb.pres_check_activities(env_L->msg, param[0], param[1]);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_presence_xml_Map [] = {
	{"pres_check_basic",       lua_sr_pres_check_basic},
	{"pres_check_activities",  lua_sr_pres_check_activities},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_textops_is_privacy(lua_State *L)
{
	str param[1];
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TEXTOPS))
	{
		LM_WARN("weird: textops function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=1)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -1);
	param[0].len = strlen(param[0].s);
	
	ret = _lua_textopsb.is_privacy(env_L->msg, &param[0]);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_textops_Map [] = {
	{"is_privacy",       lua_sr_textops_is_privacy},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_pua_usrloc_set_publish(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PUA_USRLOC))
	{
		LM_WARN("weird: pua_usrloc function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=0)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	ret = _lua_pua_usrlocb.pua_set_publish(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_pua_usrloc_Map [] = {
	{"set_publish",            lua_sr_pua_usrloc_set_publish},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_siputils_has_totag(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SIPUTILS))
	{
		LM_WARN("weird: siputils function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=0)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	ret = _lua_siputilsb.has_totag(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_siputils_is_uri_user_e164(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;
	str param[1];

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SIPUTILS))
	{
		LM_WARN("weird: siputils function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=1)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -1);
	param[0].len = strlen(param[0].s);
	
	ret = _lua_siputilsb.is_uri_user_e164(&param[0]);
	if (ret < 0)
		return app_lua_return_false(L);

	return app_lua_return_true(L);
}

/**
 *
 */
static const luaL_Reg _sr_siputils_Map [] = {
	{"has_totag",            lua_sr_siputils_has_totag},
	{"is_uri_user_e164",     lua_sr_siputils_is_uri_user_e164},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_rls_handle_subscribe(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RLS))
	{
		LM_WARN("weird: rls function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)==0)
		ret = _lua_rlsb.rls_handle_subscribe0(env_L->msg);
	else if (lua_gettop(L)==1)
	{
		str wuri;
		struct sip_uri parsed_wuri;

		wuri.s = (char *) lua_tostring(L, -1);
		wuri.len = strlen(wuri.s);
		if (parse_uri(wuri.s, wuri.len, &parsed_wuri))
		{
			LM_ERR("failed to parse watcher URI\n");
			return app_lua_return_error(L);
		}
		ret = _lua_rlsb.rls_handle_subscribe(env_L->msg, parsed_wuri.user, parsed_wuri.host);
	}
	else
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_rls_handle_notify(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RLS))
	{
		LM_WARN("weird: rls function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=0)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	ret = _lua_rlsb.rls_handle_notify(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_rls_Map [] = {
	{"handle_subscribe",       lua_sr_rls_handle_subscribe},
	{"handle_notify",          lua_sr_rls_handle_notify},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_alias_db_lookup(lua_State *L)
{
	int ret;
	str param[1];
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();
	
	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_ALIAS_DB))
	{
		LM_WARN("weird: alias_db function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if(lua_gettop(L)!=1)
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -1);
	param[0].len = strlen(param[0].s);
	
	ret = _lua_alias_dbb.alias_db_lookup(env_L->msg, param[0]);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_alias_db_Map [] = {
	{"lookup",       lua_sr_alias_db_lookup},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_msilo_store(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if (!(_sr_lua_exp_reg_mods & SR_LUA_EXP_MOD_MSILO))
	{
		LM_WARN("weird: msilo function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if (env_L->msg == NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if (lua_gettop(L) == 0)
	{
		ret = _lua_msilob.m_store(env_L->msg, NULL);
	}
	else if (lua_gettop(L) == 1)
	{
		str owner;
		owner.s = (char*)lua_tostring(L, -1);
		if (owner.s == NULL)
		{
			return app_lua_return_error(L);
		}
		owner.len = strlen(owner.s);
		ret = _lua_msilob.m_store(env_L->msg, &owner);
	}
	else
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_msilo_dump(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if (!(_sr_lua_exp_reg_mods & SR_LUA_EXP_MOD_MSILO))
	{
		LM_WARN("weird: msilo function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if (env_L->msg == NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if (lua_gettop(L) == 0)
	{
		ret = _lua_msilob.m_dump(env_L->msg, NULL);
	}
	else if (lua_gettop(L) == 1)
	{
		str owner;
		owner.s = (char*)lua_tostring(L, -1);
		if (owner.s == NULL)
		{
			return app_lua_return_error(L);
		}
		owner.len = strlen(owner.s);
		ret = _lua_msilob.m_dump(env_L->msg, &owner);
	}
	else
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_msilo_Map [] = {
	{"store",       lua_sr_msilo_store},
	{"dump",        lua_sr_msilo_dump},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_uac_replace_x(lua_State *L, int htype)
{
	int ret;
	sr_lua_env_t *env_L;
	str param[2];

	env_L = sr_lua_env_get();

	if (!(_sr_lua_exp_reg_mods & SR_LUA_EXP_MOD_UAC))
	{
		LM_WARN("weird:uac function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if (env_L->msg == NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	if (lua_gettop(L) == 1)
	{
		param[0].s = "";
		param[0].len = 0;
		param[1].s = (char *) lua_tostring(L, -1);
		param[1].len = strlen(param[1].s);
	
	}
	else if (lua_gettop(L) == 2)
	{
		param[0].s = (char *) lua_tostring(L, -2);
		param[0].len = strlen(param[0].s);
		param[1].s = (char *) lua_tostring(L, -1);
		param[1].len = strlen(param[1].s);
	}
	else
	{
		LM_ERR("incorrect number of arguments\n");
		return app_lua_return_error(L);
	}

	if(htype==1) {
		ret = _lua_uacb.replace_to(env_L->msg, &param[0], &param[1]);
	} else {
		ret = _lua_uacb.replace_from(env_L->msg, &param[0], &param[1]);
	}
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_uac_replace_from(lua_State *L)
{
	return lua_sr_uac_replace_x(L, 0);
}

/**
 *
 */
static int lua_sr_uac_replace_to(lua_State *L)
{
	return lua_sr_uac_replace_x(L, 1);
}

/**
 *
 */
static int lua_sr_uac_req_send(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if (!(_sr_lua_exp_reg_mods & SR_LUA_EXP_MOD_UAC))
	{
		LM_WARN("weird:uac function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if (env_L->msg == NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_uacb.req_send();

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_uac_Map [] = {
	{"replace_from",lua_sr_uac_replace_from},
	{"replace_to",lua_sr_uac_replace_to},
	{"uac_req_send",lua_sr_uac_req_send},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_sanity_check(lua_State *L)
{
	int msg_checks, uri_checks;
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SANITY))
	{
		LM_WARN("weird: sanity function executed but module not registered\n");
		return app_lua_return_error(L);
	}

	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}
	msg_checks = lua_tointeger(L, -1);
	uri_checks = lua_tointeger(L, -2);

	ret = _lua_sanityb.check(env_L->msg, msg_checks, uri_checks);
	return app_lua_return_int(L, ret);
}


/**
 *
 */
static const luaL_Reg _sr_sanity_Map [] = {
	{"sanity_check",       lua_sr_sanity_check},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_cfgutils_lock(lua_State *L)
{
	int ret;
	str lkey;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_CFGUTILS))
	{
		LM_WARN("weird: cfgutils function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	lkey.s = (char*)lua_tostring(L, -1);
	lkey.len = strlen(lkey.s);
	ret = _lua_cfgutilsb.mlock(&lkey);

	return app_lua_return_int(L, ret);
}


/**
 *
 */
static int lua_sr_cfgutils_unlock(lua_State *L)
{
	int ret;
	str lkey;

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_CFGUTILS))
	{
		LM_WARN("weird: cfgutils function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	lkey.s = (char*)lua_tostring(L, -1);
	lkey.len = strlen(lkey.s);
	ret = _lua_cfgutilsb.munlock(&lkey);

	return app_lua_return_int(L, ret);
}


/**
 *
 */
static const luaL_Reg _sr_cfgutils_Map [] = {
	{"lock",      lua_sr_cfgutils_lock},
	{"unlock",    lua_sr_cfgutils_unlock},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_tmx_t_suspend(lua_State *L)
{
	int ret;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TMX))
	{
		LM_WARN("weird: tmx function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(env_L->msg==NULL)
	{
		LM_WARN("invalid parameters from Lua env\n");
		return app_lua_return_error(L);
	}

	ret = _lua_tmxb.t_suspend(env_L->msg, NULL, NULL);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_tmx_Map [] = {
	{"t_suspend", lua_sr_tmx_t_suspend},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_mq_add(lua_State *L)
{
	int ret;
	str param[3];

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MQUEUE))
	{
		LM_WARN("weird: mqueue function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=3)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}
	
	param[0].s = (char *) lua_tostring(L, -3);
	param[0].len = strlen(param[0].s);
	param[1].s = (char *) lua_tostring(L, -2);
	param[1].len = strlen(param[1].s);
	param[2].s = (char *) lua_tostring(L, -1);
	param[2].len = strlen(param[2].s);
	
	ret = _lua_mqb.add(&param[0], &param[1], &param[2]);
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_mqueue_Map [] = {
	{"add", lua_sr_mq_add},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_ndb_mongodb_cmd_x(lua_State *L, int ctype)
{
	int ret = 0;
	str param[6];

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_NDB_MONGODB))
	{
		LM_WARN("weird: ndb_mongodb function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=5)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -5);
	param[0].len = strlen(param[0].s);
	param[1].s = (char *) lua_tostring(L, -4);
	param[1].len = strlen(param[1].s);
	param[2].s = (char *) lua_tostring(L, -3);
	param[2].len = strlen(param[2].s);
	param[3].s = (char *) lua_tostring(L, -2);
	param[3].len = strlen(param[3].s);
	param[4].s = (char *) lua_tostring(L, -1);
	param[4].len = strlen(param[4].s);

	if(ctype==1) {
		ret = _lua_ndb_mongodbb.cmd_simple(&param[0], &param[1], &param[2], &param[3], &param[4]);
	} else if(ctype==2) {
		ret = _lua_ndb_mongodbb.find(&param[0], &param[1], &param[2], &param[3], &param[4]);
	} else if(ctype==3) {
		ret = _lua_ndb_mongodbb.find_one(&param[0], &param[1], &param[2], &param[3], &param[4]);
	} else {
		ret = _lua_ndb_mongodbb.cmd(&param[0], &param[1], &param[2], &param[3], &param[4]);
	}
	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_ndb_mongodb_cmd(lua_State *L)
{
	return lua_sr_ndb_mongodb_cmd_x(L, 0);
}

/**
 *
 */
static int lua_sr_ndb_mongodb_cmd_simple(lua_State *L)
{
	return lua_sr_ndb_mongodb_cmd_x(L, 1);
}

/**
 *
 */
static int lua_sr_ndb_mongodb_find(lua_State *L)
{
	return lua_sr_ndb_mongodb_cmd_x(L, 2);
}

/**
 *
 */
static int lua_sr_ndb_mongodb_find_one(lua_State *L)
{
	return lua_sr_ndb_mongodb_cmd_x(L, 3);
}

/**
 *
 */
static int lua_sr_ndb_mongodb_next_reply(lua_State *L)
{
	int ret = 0;
	str param[1];

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_NDB_MONGODB))
	{
		LM_WARN("weird: ndb_mongodb function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -1);
	param[0].len = strlen(param[0].s);

	ret = _lua_ndb_mongodbb.next_reply(&param[0]);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static int lua_sr_ndb_mongodb_free_reply(lua_State *L)
{
	int ret = 0;
	str param[1];

	if(!(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_NDB_MONGODB))
	{
		LM_WARN("weird: ndb_mongodb function executed but module not registered\n");
		return app_lua_return_error(L);
	}
	if(lua_gettop(L)!=1)
	{
		LM_WARN("invalid number of parameters from Lua\n");
		return app_lua_return_error(L);
	}

	param[0].s = (char *) lua_tostring(L, -1);
	param[0].len = strlen(param[0].s);

	ret = _lua_ndb_mongodbb.free_reply(&param[0]);

	return app_lua_return_int(L, ret);
}

/**
 *
 */
static const luaL_Reg _sr_ndb_mongodb_Map [] = {
	{"cmd", lua_sr_ndb_mongodb_cmd},
	{"cmd_simple", lua_sr_ndb_mongodb_cmd_simple},
	{"find", lua_sr_ndb_mongodb_find},
	{"find_one", lua_sr_ndb_mongodb_find_one},
	{"next_reply", lua_sr_ndb_mongodb_next_reply},
	{"free_reply", lua_sr_ndb_mongodb_free_reply},
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
		/* bind the TM XAPI */
		if (tm_load_xapi(&_lua_xtmb) < 0)
		{
			LM_ERR("cannot bind to TM XAPI\n");
			return -1;
		}
		LM_DBG("loaded tm xapi\n");
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
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_DISPATCHER)
	{
		/* bind the DISPATCHER API */
		if (dispatcher_load_api(&_lua_dispatcherb) < 0)
		{
			LM_ERR("cannot bind to DISPATCHER API\n");
			return -1;
		}
		LM_DBG("loaded dispatcher api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_XHTTP)
	{
		/* bind the XHTTP API */
		if (xhttp_load_api(&_lua_xhttpb) < 0)
		{
			LM_ERR("cannot bind to XHTTP API\n");
			return -1;
		}
		LM_DBG("loaded xhttp api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SDPOPS)
	{
		/* bind the SDPOPS API */
		if (sdpops_load_api(&_lua_sdpopsb) < 0)
		{
			LM_ERR("cannot bind to SDPOPS API\n");
			return -1;
		}
		LM_DBG("loaded sdpops api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE)
	{
		/* bind the PRESENCE API */
		if (presence_load_api(&_lua_presenceb) < 0)
		{
			LM_ERR("cannot bind to PRESENCE API\n");
			return -1;
		}
		LM_DBG("loaded presence api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE)
	{
		/* bind the PRESENCE_XML API */
		if (presence_xml_load_api(&_lua_presence_xmlb) < 0)
		{
			LM_ERR("cannot bind to PRESENCE_XML API\n");
			return -1;
		}
		LM_DBG("loaded presence_xml api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TEXTOPS)
	{
		/* bind the TEXTOPS API */
		if (load_textops_api(&_lua_textopsb) < 0)
		{
			LM_ERR("cannot bind to TEXTOPS API\n");
			return -1;
		}
		LM_DBG("loaded textops api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PUA_USRLOC)
	{
		/* bind the PUA_USRLOC API */
		if (pua_usrloc_load_api(&_lua_pua_usrlocb) < 0)
		{
			LM_ERR("cannot bind to PUA_USRLOC API\n");
			return -1;
		}
		LM_DBG("loaded pua_usrloc api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SIPUTILS)
	{
		/* bind the SIPUTILS API */
		if (siputils_load_api(&_lua_siputilsb) < 0)
		{
			LM_ERR("cannot bind to SIPUTILS API\n");
			return -1;
		}
		LM_DBG("loaded siputils api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RLS)
	{
		/* bind the RLS API */
		if (rls_load_api(&_lua_rlsb) < 0)
		{
			LM_ERR("cannot bind to RLS API\n");
			return -1;
		}
		LM_DBG("loaded rls api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_ALIAS_DB)
	{
		/* bind the ALIAS_DB API */
		if (alias_db_load_api(&_lua_alias_dbb) < 0)
		{
			LM_ERR("cannot bind to ALIAS_DB API\n");
			return -1;
		}
		LM_DBG("loaded alias_db api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MSILO)
	{
		/* bind the MSILO API */
		if (load_msilo_api(&_lua_msilob) < 0)
		{
			LM_ERR("cannot bind to MSILO API\n");
			return -1;
		}
		LM_DBG("loaded msilo api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_UAC)
	{
		/* bind the UAC API */
		if (load_uac_api(&_lua_uacb) < 0)
		{
			LM_ERR("cannot bind to UAC API\n");
			return -1;
		}
		LM_DBG("loaded uac api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SANITY)
	{
		/* bind the SANITY API */
		if (sanity_load_api(&_lua_sanityb) < 0)
		{
			LM_ERR("cannot bind to SANITY API\n");
			return -1;
		}
		LM_DBG("loaded sanity api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_CFGUTILS)
	{
		/* bind the CFGUTILS API */
		if (cfgutils_load_api(&_lua_cfgutilsb) < 0)
		{
			LM_ERR("cannot bind to CFGUTILS API\n");
			return -1;
		}
		LM_DBG("loaded cfgutils api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TMX)
	{
		/* bind the TMX API */
		if (load_tmx_api(&_lua_tmxb) < 0)
		{
			LM_ERR("cannot bind to TMX API\n");
			return -1;
		}
		LM_DBG("loaded tmx api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MQUEUE)
	{
		/* bind the MQUEUE API */
		if (load_mq_api(&_lua_mqb) < 0)
		{
			LM_ERR("cannot bind to MQUEUE API\n");
			return -1;
		}
		LM_DBG("loaded mqueue api\n");
	}
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_NDB_MONGODB)
	{
		/* bind the NDB_MONGODB API */
		if (ndb_mongodb_load_api(&_lua_ndb_mongodbb) < 0)
		{
			LM_ERR("cannot bind to NDB_MONGODB API\n");
			return -1;
		}
		LM_DBG("loaded ndb_mongodb api\n");
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
	} else 	if(len==10 && strcmp(mname, "dispatcher")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_DISPATCHER;
		return 0;
	} else 	if(len==5 && strcmp(mname, "xhttp")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_XHTTP;
		return 0;
	} else 	if(len==6 && strcmp(mname, "sdpops")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_SDPOPS;
		return 0;
	} else 	if(len==8 && strcmp(mname, "presence")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_PRESENCE;
		return 0;
	} else 	if(len==12 && strcmp(mname, "presence_xml")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_PRESENCE_XML;
		return 0;
	} else 	if(len==7 && strcmp(mname, "textops")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_TEXTOPS;
		return 0;
	} else 	if(len==10 && strcmp(mname, "pua_usrloc")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_PUA_USRLOC;
		return 0;
	} else 	if(len==8 && strcmp(mname, "siputils")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_SIPUTILS;
		return 0;
	} else 	if(len==3 && strcmp(mname, "rls")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_RLS;
		return 0;
	} else 	if(len==8 && strcmp(mname, "alias_db")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_ALIAS_DB;
		return 0;
	} else 	if(len==5 && strcmp(mname, "msilo")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_MSILO;
		return 0;
	} else 	if(len==3 && strcmp(mname, "uac")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_UAC;
		return 0;
	} else 	if(len==6 && strcmp(mname, "sanity")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_SANITY;
		return 0;
	} else 	if(len==8 && strcmp(mname, "cfgutils")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_CFGUTILS;
		return 0;
	} else 	if(len==3 && strcmp(mname, "tmx")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_TMX;
		return 0;
	} else 	if(len==6 && strcmp(mname, "mqueue")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_MQUEUE;
		return 0;
	} else 	if(len==11 && strcmp(mname, "ndb_mongodb")==0) {
		_sr_lua_exp_reg_mods |= SR_LUA_EXP_MOD_NDB_MONGODB;
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
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_DISPATCHER)
		luaL_openlib(L, "sr.dispatcher", _sr_dispatcher_Map,  0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_XHTTP)
		luaL_openlib(L, "sr.xhttp",      _sr_xhttp_Map,       0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SDPOPS)
		luaL_openlib(L, "sr.sdpops",     _sr_sdpops_Map,      0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE)
		luaL_openlib(L, "sr.presence",     _sr_presence_Map,  0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PRESENCE_XML)
		luaL_openlib(L, "sr.presence_xml", _sr_presence_xml_Map, 0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TEXTOPS)
		luaL_openlib(L, "sr.textops", _sr_textops_Map,        0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_PUA_USRLOC)
		luaL_openlib(L, "sr.pua_usrloc", _sr_pua_usrloc_Map,  0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SIPUTILS)
		luaL_openlib(L, "sr.siputils", _sr_siputils_Map,      0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_RLS)
		luaL_openlib(L, "sr.rls", _sr_rls_Map,                0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_ALIAS_DB)
		luaL_openlib(L, "sr.alias_db", _sr_alias_db_Map,      0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MSILO)
		luaL_openlib(L, "sr.msilo", _sr_msilo_Map,            0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_UAC)
		luaL_openlib(L, "sr.uac", _sr_uac_Map,                0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_SANITY)
		luaL_openlib(L, "sr.sanity", _sr_sanity_Map,          0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_CFGUTILS)
		luaL_openlib(L, "sr.cfgutils", _sr_cfgutils_Map,      0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_TMX)
		luaL_openlib(L, "sr.tmx", _sr_tmx_Map,                0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_MQUEUE)
		luaL_openlib(L, "sr.mq", _sr_mqueue_Map,              0);
	if(_sr_lua_exp_reg_mods&SR_LUA_EXP_MOD_NDB_MONGODB)
		luaL_openlib(L, "sr.ndb_mongodb", _sr_ndb_mongodb_Map, 0);
}

