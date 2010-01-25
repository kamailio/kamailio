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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../lib/kcore/cmpapi.h"

#include "app_lua_sr.h"

#define SRVERSION "1.0"

typedef struct _sr_lua_env
{
	lua_State *L;
	lua_State *LL;
	struct sip_msg *msg;
	unsigned int flags;
} sr_lua_env_t;

typedef struct _sr_lua_load
{
	char *script;
	struct _sr_lua_load *next;
} sr_lua_load_t;

static sr_lua_env_t _sr_L_env;

static sr_lua_load_t *_sr_lua_load_list = NULL;

static int lua_sr_dbg (lua_State *L)
{
	char *txt;
	txt = (char*)lua_tostring(L, -1);
	if(txt!=NULL)
		LM_DBG("%s", txt);
	return 0;
}

static int lua_sr_err (lua_State *L)
{
	char *txt;
	txt = (char*)lua_tostring(L, -1);
	if(txt!=NULL)
		LM_ERR("%s", txt);
	return 0;
}

static int lua_sr_log (lua_State *L)
{
	char *txt;
	char *level;
	level = (char*)lua_tostring(L, -2);
	txt = (char*)lua_tostring(L, -1);
	if(txt!=NULL)
	{
		if(level==NULL)
		{
			LM_ERR("%s", txt);
		} else {
			if(strcasecmp(level, "warn")==0) {
				LM_WARN("%s", txt);
			} else if(strcasecmp(level, "crit")==0) {
				LM_CRIT("%s", txt);
			} else {
				LM_ERR("%s", txt);
			}
		}
	}
	return 0;
}

static const luaL_reg _sr_core_Map [] = {
	{"dbg", lua_sr_dbg},
	{"err", lua_sr_err},
	{"log", lua_sr_log},
	{NULL, NULL}
};

static int lua_sr_hdr_append (lua_State *L)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *txt;
	int len;
	char *hdr;

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || _sr_L_env.msg==NULL)
		return 0;

	LM_DBG("append hf: %s\n", txt);
	if (parse_headers(_sr_L_env.msg, HDR_EOH_F, 0) == -1)
	{
		LM_ERR("error while parsing message\n");
		return 0;
	}

	hf = _sr_L_env.msg->last_header;
	len = strlen(txt);
	hdr = (char*)pkg_malloc(len);
	if(hdr==NULL)
	{
		LM_ERR("no pkg memory left\n");
		return 0;
	}
	memcpy(hdr, txt, len);
	anchor = anchor_lump(_sr_L_env.msg,
				hf->name.s + hf->len - _sr_L_env.msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return 0;
	}
	return 0;
}

static int lua_sr_hdr_remove (lua_State *L)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *txt;
	str hname;

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || _sr_L_env.msg==NULL)
		return 0;

	LM_DBG("remove hf: %s\n", txt);
	if (parse_headers(_sr_L_env.msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return 0;
	}

	hname.s = txt;
	hname.len = strlen(txt);
	for (hf=_sr_L_env.msg->headers; hf; hf=hf->next)
	{
		if (cmp_hdrname_str(&hf->name, &hname)==0)
		{
			anchor=del_lump(_sr_L_env.msg,
					hf->name.s - _sr_L_env.msg->buf, hf->len, 0);
			if (anchor==0)
			{
				LM_ERR("cannot remove hdr %s\n", txt);
				return 0;
			}
		}
	}
	return 0;
}

static int lua_sr_hdr_insert (lua_State *L)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *txt;
	int len;
	char *hdr;

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || _sr_L_env.msg==NULL)
		return 0;

	LM_DBG("insert hf: %s\n", txt);
	hf = _sr_L_env.msg->headers;
	len = strlen(txt);
	hdr = (char*)pkg_malloc(len);
	if(hdr==NULL)
	{
		LM_ERR("no pkg memory left\n");
		return 0;
	}
	memcpy(hdr, txt, len);
	anchor = anchor_lump(_sr_L_env.msg,
				hf->name.s + hf->len - _sr_L_env.msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return 0;
	}
	return 0;
}

static int lua_sr_hdr_append_to_reply (lua_State *L)
{
	char *txt;
	int len;

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || _sr_L_env.msg==NULL)
		return 0;

	LM_DBG("append to reply: %s\n", txt);
	len = strlen(txt);

	if(add_lump_rpl(_sr_L_env.msg, txt, len, LUMP_RPL_HDR)==0)
	{
		LM_ERR("unable to add reply lump\n");
		return 0;
	}

	return 0;
}


static const luaL_reg _sr_hdr_Map [] = {
	{"append", lua_sr_hdr_append},
	{"remove", lua_sr_hdr_remove},
	{"insert", lua_sr_hdr_insert},
	{"append_to_reply", lua_sr_hdr_append_to_reply},
	{NULL, NULL}
};


static int lua_sr_pv_get (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;

	pvn.s = (char*)lua_tostring(L, -1);
	if(pvn.s==NULL || _sr_L_env.msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv get: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(_sr_L_env.msg, &pvs, &val) != 0)
	{
		LM_ERR("unable to get pv value for [%s]\n", pvn.s);
		return 0;
	}
	if(val.flags&PV_VAL_NULL)
	{
		return 0;
	}
	if(val.flags&PV_TYPE_INT)
	{
		lua_pushinteger(L, val.ri);
		return 1;
	}
	lua_pushlstring(L, val.rs.s, val.rs.len);
	return 1;
}

static int lua_sr_pv_seti (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;

	if(lua_gettop(L)<2)
	{
		LM_ERR("to few parameters [%d]\n",lua_gettop(L));
		return 0;
	}
	if(!lua_isnumber(L, -1))
	{
		LM_ERR("invalid int parameter\n");
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.ri = lua_tointeger(L, -1);
	val.flags |= PV_TYPE_INT|PV_VAL_INT;
	
	pvn.s = (char*)lua_tostring(L, -2);
	if(pvn.s==NULL || _sr_L_env.msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	if(pv_set_spec_value(_sr_L_env.msg, &pvs, 0, &val)<0)
	{
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

static int lua_sr_pv_sets (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;

	if(lua_gettop(L)<2)
	{
		LM_ERR("to few parameters [%d]\n",lua_gettop(L));
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.rs.s = (char*)lua_tostring(L, -1);
	val.rs.len = strlen(val.rs.s);
	val.flags |= PV_VAL_STR;
	
	pvn.s = (char*)lua_tostring(L, -2);
	if(pvn.s==NULL || _sr_L_env.msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	if(pv_set_spec_value(_sr_L_env.msg, &pvs, 0, &val)<0)
	{
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

static int lua_sr_pv_unset (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;

	pvn.s = (char*)lua_tostring(L, -1);
	if(pvn.s==NULL || _sr_L_env.msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv unset: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_NULL;
	if(pv_set_spec_value(_sr_L_env.msg, &pvs, 0, &val)<0)
	{
		LM_ERR("unable to unset pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

static int lua_sr_pv_is_null (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;

	pvn.s = (char*)lua_tostring(L, -1);
	if(pvn.s==NULL || _sr_L_env.msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv is null: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(_sr_L_env.msg, &pvs, &val) != 0)
	{
		LM_ERR("unable to get pv value for [%s]\n", pvn.s);
		return 0;
	}
	if(val.flags&PV_VAL_NULL)
	{
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

static const luaL_reg _sr_pv_Map [] = {
	{"get",      lua_sr_pv_get},
	{"seti",     lua_sr_pv_seti},
	{"sets",     lua_sr_pv_sets},
	{"unset",    lua_sr_pv_unset},
	{"is_null",  lua_sr_pv_is_null},
	{NULL, NULL}
};

void lua_sr_openlibs(lua_State *L)
{
	luaL_openlib(L, "sr",      _sr_core_Map, 0);
	luaL_openlib(L, "sr.hdr",  _sr_hdr_Map,  0);
	luaL_openlib(L, "sr.pv",   _sr_pv_Map,   0);
}

int sr_lua_load_script(char *script)
{
	sr_lua_load_t *li;

	li = (sr_lua_load_t*)pkg_malloc(sizeof(sr_lua_load_t));
	if(li==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(li, 0, sizeof(sr_lua_load_t));
	li->script = script;
	li->next = _sr_lua_load_list;
	_sr_lua_load_list = li;
	return 0;
}


int lua_sr_init_mod(void)
{
	memset(&_sr_L_env, 0, sizeof(sr_lua_env_t));
	return 0;
}

int lua_sr_init_child(void)
{
	sr_lua_load_t *li;
	int ret;
	char *txt;
	struct stat sbuf;

	memset(&_sr_L_env, 0, sizeof(sr_lua_env_t));
	_sr_L_env.L = lua_open();
	if(_sr_L_env.L==NULL)
	{
		LM_ERR("cannot open lua\n");
		return -1;
	}
	luaL_openlibs(_sr_L_env.L);
	lua_sr_openlibs(_sr_L_env.L);

	/* set SR lib version */
	lua_pushstring(_sr_L_env.L, "SRVERSION");
	lua_pushstring(_sr_L_env.L, SRVERSION);
	lua_settable(_sr_L_env.L, LUA_GLOBALSINDEX);

	if(_sr_lua_load_list != NULL)
	{
		_sr_L_env.LL = lua_open();
		if(_sr_L_env.LL==NULL)
		{
			LM_ERR("cannot open lua loading state\n");
			return -1;
		}
		luaL_openlibs(_sr_L_env.LL);
		lua_sr_openlibs(_sr_L_env.LL);

		/* set SR lib version */
		lua_pushstring(_sr_L_env.LL, "SRVERSION");
		lua_pushstring(_sr_L_env.LL, SRVERSION);
		lua_settable(_sr_L_env.LL, LUA_GLOBALSINDEX);

		li = _sr_lua_load_list;
		while(li)
		{
			if(stat(li->script, &sbuf)!=0)
			{
				/* file does not exist */
				LM_ERR("cannot find script: %s (wrong path?)\n",
						li->script);
				lua_sr_destroy();
				return -1;
			}
 			ret = luaL_loadfile(_sr_L_env.LL, (const char*)li->script);
			if(ret!=0)
			{
				LM_ERR("failed to load Lua script: %s (err: %d)\n",
						li->script, ret);
				txt = (char*)lua_tostring(_sr_L_env.LL, -1);
				LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
				lua_pop(_sr_L_env.LL, 1);
				lua_sr_destroy();
				return -1;
			}
			ret = lua_pcall(_sr_L_env.LL, 0, 0, 0);
			if(ret!=0)
			{
				LM_ERR("failed to init Lua script: %s (err: %d)\n",
						li->script, ret);
				txt = (char*)lua_tostring(_sr_L_env.LL, -1);
				LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
				lua_pop(_sr_L_env.LL, 1);
				lua_sr_destroy();
				return -1;
			}
			li = li->next;
		}
	}
	LM_DBG("Lua initialized!\n");
	return 0;
}

void lua_sr_destroy(void)
{
	if(_sr_L_env.L!=NULL)
	{
		lua_close(_sr_L_env.L);
		_sr_L_env.L = NULL;
	}
	if(_sr_L_env.LL!=NULL)
	{
		lua_close(_sr_L_env.LL);
		_sr_L_env.LL = NULL;
	}
	memset(&_sr_L_env, 0, sizeof(sr_lua_env_t));
}

int lua_sr_initialized(void)
{
	if(_sr_L_env.L==NULL)
		return 0;

	return 1;
}

int app_lua_dostring(struct sip_msg *msg, char *script)
{
	int ret;
	char *txt;

	LM_DBG("executing Lua string: [[%s]]\n", script);
	LM_DBG("lua top index is: %d\n", lua_gettop(_sr_L_env.L));
	_sr_L_env.msg = msg;
	ret = luaL_dostring(_sr_L_env.L, script);
	if(ret!=0)
	{
		txt = (char*)lua_tostring(_sr_L_env.L, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		lua_pop (_sr_L_env.L, 1);
	}
	_sr_L_env.msg = 0;
	return (ret==0)?1:-1;
}

int app_lua_dofile(struct sip_msg *msg, char *script)
{
	int ret;
	char *txt;

	LM_DBG("executing Lua file: [[%s]]\n", script);
	LM_DBG("lua top index is: %d\n", lua_gettop(_sr_L_env.L));
	_sr_L_env.msg = msg;
	ret = luaL_dofile(_sr_L_env.L, script);
	if(ret!=0)
	{
		txt = (char*)lua_tostring(_sr_L_env.L, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		lua_pop(_sr_L_env.L, 1);
	}
	_sr_L_env.msg = 0;
	return (ret==0)?1:-1;
}

int app_lua_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	int n;
	int ret;
	char *txt;

	if(_sr_L_env.LL==NULL)
	{
		LM_ERR("lua loading state not initialized (call: %s)\n", func);
		return -1;
	}

	LM_DBG("executing Lua function: [[%s]]\n", func);
	LM_DBG("lua top index is: %d\n", lua_gettop(_sr_L_env.LL));
	lua_getglobal(_sr_L_env.LL, func);
	if(!lua_isfunction(_sr_L_env.LL, -1))
	{
		LM_ERR("no such function [%s] in lua scripts\n", func);
		LM_ERR("top stack type [%d - %s]\n",
				lua_type(_sr_L_env.LL, -1),
				lua_typename(_sr_L_env.LL,lua_type(_sr_L_env.LL, -1)));
		txt = (char*)lua_tostring(_sr_L_env.LL, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		return -1;
	}
	n = 0;
	if(p1!=NULL)
	{
		lua_pushstring(_sr_L_env.LL, p1);
		n++;
		if(p2!=NULL)
		{
			lua_pushstring(_sr_L_env.LL, p2);
			n++;
			if(p3!=NULL)
			{
				lua_pushstring(_sr_L_env.LL, p3);
				n++;
			}
		}
	}
	_sr_L_env.msg = msg;
	ret = lua_pcall(_sr_L_env.LL, n, 0, 0);
	_sr_L_env.msg = 0;
	if(ret!=0)
	{
		LM_ERR("error executing: %s (err: %d)\n", func, ret);
		txt = (char*)lua_tostring(_sr_L_env.LL, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		lua_pop(_sr_L_env.LL, 1);
		return -1;
	}

	return 1;
}

