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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mem/mem.h"
#include "../../core/locking.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/strutils.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "app_lua_api.h"
#include "app_lua_kemi_export.h"

#define KSRVERSION "2.0"

#define KSR_APP_LUA_LOG_EXPORTS (1<<0)

extern int _ksr_app_lua_log_mode;

void lua_sr_kemi_register_libs(lua_State *L);

static app_lua_openlibs_f app_lua_openlibs_cb = NULL;

/**
 *
 */
int app_lua_openlibs_register(app_lua_openlibs_f rfunc)
{
	app_lua_openlibs_cb = rfunc;
	return 0;
}

/**
 * reload enabled param
 * default: 0 (off)
 */
static unsigned int _app_lua_sr_reload = 1;
/**
 *
 */
static sr_lua_env_t _sr_L_env;

/**
 *
 */
static int *_app_lua_sv = NULL;

/**
 * @return the static Lua env
 */
sr_lua_env_t *sr_lua_env_get(void)
{
	return &_sr_L_env;
}

/**
 *
 */
static sr_lua_load_t *_sr_lua_load_list = NULL;
/**
 * set of locks to manage the shared variable.
 */
static gen_lock_set_t *sr_lua_locks = NULL;
static sr_lua_script_ver_t *sr_lua_script_ver = NULL;


int lua_sr_alloc_script_ver(void)
{
	int size = _sr_L_env.nload;

	sr_lua_script_ver = (sr_lua_script_ver_t *) shm_malloc(sizeof(sr_lua_script_ver_t));
	if(sr_lua_script_ver==NULL)
	{
		SHM_MEM_ERROR;
		return -1;
	}

	sr_lua_script_ver->version = (unsigned int *) shm_malloc(sizeof(unsigned int)*size);
	if(sr_lua_script_ver->version==NULL)
	{
		SHM_MEM_ERROR;
		goto error;
	}
	memset(sr_lua_script_ver->version, 0, sizeof(unsigned int)*size);
	sr_lua_script_ver->len = size;

	if((sr_lua_locks=lock_set_alloc(size))==0)
	{
		LM_CRIT("failed to alloc lock set\n");
		goto error;
	}
	if(lock_set_init(sr_lua_locks)==0 )
	{
		LM_CRIT("failed to init lock set\n");
		goto error;
	}

	return 0;
error:
	if(sr_lua_script_ver!=NULL)
	{
		if(sr_lua_script_ver->version!=NULL)
		{
			shm_free(sr_lua_script_ver->version);
			sr_lua_script_ver->version = NULL;
		}
		shm_free(sr_lua_script_ver);
		sr_lua_script_ver = NULL;
	}
	if(sr_lua_locks!=NULL)
	{
		lock_set_destroy( sr_lua_locks );
		lock_set_dealloc( sr_lua_locks );
		sr_lua_locks = NULL;
	}
	return -1;
}

/**
 *
 */
int sr_lua_load_script(char *script)
{
	sr_lua_load_t *li;

	li = (sr_lua_load_t*)pkg_malloc(sizeof(sr_lua_load_t));
	if(li==NULL)
	{
		PKG_MEM_ERROR;
		return -1;
	}
	memset(li, 0, sizeof(sr_lua_load_t));
	li->script = script;
	li->version = 0;
	li->next = _sr_lua_load_list;
	_sr_lua_load_list = li;
	_sr_L_env.nload += 1;
	LM_DBG("loaded script:[%s].\n", script);
	LM_DBG("Now there are %d scripts loaded\n", _sr_L_env.nload);

	return 0;
}

/**
 *
 */
int sr_lua_reload_module(unsigned int reload)
{
	LM_DBG("reload:%d\n", reload);
	if(reload!=0) {
		_app_lua_sr_reload = 1;
		LM_DBG("reload param activated!\n");
	} else {
		_app_lua_sr_reload = 0;
		LM_DBG("reload param inactivated!\n");
	}
	return 0;
}

/**
 *
 */
void lua_sr_openlibs(lua_State *L)
{
	if(app_lua_openlibs_cb!=NULL) {
		app_lua_openlibs_cb(L);
	}
	lua_sr_kemi_register_libs(L);
}

/**
 *
 */
int lua_sr_init_mod(void)
{
	/* allocate shm */
	if(lua_sr_alloc_script_ver()<0) {
		LM_CRIT("failed to alloc shm for version\n");
		return -1;
	}

	memset(&_sr_L_env, 0, sizeof(sr_lua_env_t));

	return 0;

}

/**
 *
 */
int lua_sr_init_probe(void)
{
	lua_State *L;
	char *txt;
	sr_lua_load_t *li;
	struct stat sbuf;

	L = luaL_newstate();
	if(L==NULL)
	{
		LM_ERR("cannot open lua\n");
		return -1;
	}
	luaL_openlibs(L);
	lua_sr_openlibs(L);

	/* force loading lua lib now */
	if(luaL_dostring(L, "KSR.x.probe()")!=0)
	{
		txt = (char*)lua_tostring(L, -1);
		LM_ERR("error initializing Lua: %s\n", (txt)?txt:"unknown");
		lua_pop(L, 1);
		lua_close(L);
		return -1;
	}

	/* test if files to be loaded exist */
	if(_sr_lua_load_list != NULL)
	{
		li = _sr_lua_load_list;
		while(li)
		{
			if(stat(li->script, &sbuf)!=0)
			{
				/* file does not exist */
				LM_ERR("cannot find script: %s (wrong path?)\n",
						li->script);
				lua_close(L);
				return -1;
			}
			li = li->next;
		}
	}
	lua_close(L);
	LM_DBG("Lua probe was ok!\n");
	return 0;
}

/**
 *
 */
int lua_sr_init_child(void)
{
	sr_lua_load_t *li;
	int ret;
	char *txt;

	memset(&_sr_L_env, 0, sizeof(sr_lua_env_t));
	_sr_L_env.L = luaL_newstate();
	if(_sr_L_env.L==NULL)
	{
		LM_ERR("cannot open lua\n");
		return -1;
	}
	luaL_openlibs(_sr_L_env.L);
	lua_sr_openlibs(_sr_L_env.L);

	/* set KSR lib version */
#if LUA_VERSION_NUM >= 502
	lua_pushstring(_sr_L_env.L, KSRVERSION);
	lua_setglobal(_sr_L_env.L, "KSRVERSION");
#else
	lua_pushstring(_sr_L_env.L, "KSRVERSION");
	lua_pushstring(_sr_L_env.L, KSRVERSION);
	lua_settable(_sr_L_env.L, LUA_GLOBALSINDEX);
#endif
	if(_sr_lua_load_list != NULL)
	{
		_sr_L_env.LL = luaL_newstate();
		if(_sr_L_env.LL==NULL)
		{
			LM_ERR("cannot open lua loading state\n");
			return -1;
		}
		luaL_openlibs(_sr_L_env.LL);
		lua_sr_openlibs(_sr_L_env.LL);

		/* set SR lib version */
#if LUA_VERSION_NUM >= 502
		lua_pushstring(_sr_L_env.LL, KSRVERSION);
		lua_setglobal(_sr_L_env.LL, "KSRVERSION");
#else
		lua_pushstring(_sr_L_env.LL, "KSRVERSION");
		lua_pushstring(_sr_L_env.LL, KSRVERSION);
		lua_settable(_sr_L_env.LL, LUA_GLOBALSINDEX);
#endif
		/* force loading lua lib now */
		if(luaL_dostring(_sr_L_env.LL, "KSR.x.probe()")!=0)
		{
			txt = (char*)lua_tostring(_sr_L_env.LL, -1);
			LM_ERR("error initializing Lua: %s\n", (txt)?txt:"unknown");
			lua_pop(_sr_L_env.LL, 1);
			lua_sr_destroy();
			return -1;
		}

		li = _sr_lua_load_list;
		while(li)
		{
			ret = luaL_dofile(_sr_L_env.LL, (const char*)li->script);
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
			li = li->next;
		}
	}
	LM_DBG("Lua initialized!\n");
	return 0;
}

/**
 *
 */
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

	if(sr_lua_script_ver!=NULL)
	{
		shm_free(sr_lua_script_ver->version);
		shm_free(sr_lua_script_ver);
	}

	if (sr_lua_locks!=NULL)
	{
		lock_set_destroy( sr_lua_locks );
		lock_set_dealloc( sr_lua_locks );
		sr_lua_locks = 0;
	}

	if(_app_lua_sv!=NULL) {
		pkg_free(_app_lua_sv);
		_app_lua_sv = 0;
	}
}

/**
 *
 */
int lua_sr_list_script(sr_lua_load_t **list)
{
	*list = _sr_lua_load_list;
	return 0;
}

/**
 * Mark script in pos to be reloaded
 * pos -1: reload all scritps
 */
int lua_sr_reload_script(int pos)
{
	int i, len = sr_lua_script_ver->len;
	if(_sr_lua_load_list!= NULL)
	{
		if (!sr_lua_script_ver)
		{
			LM_CRIT("shm for version not allocated\n");
			return -1;
		}
		if (_app_lua_sr_reload==0)
		{
			LM_ERR("reload is not activated\n");
			return -3;
		}
		if (pos<0)
		{
			// let's mark all the scripts to be reloaded
			for (i=0;i<len;i++)
			{
				lock_set_get(sr_lua_locks, i);
				sr_lua_script_ver->version[i] += 1;
				lock_set_release(sr_lua_locks, i);
			}
		}
		else
		{
			if (pos>=0 && pos<len)
			{
				lock_set_get(sr_lua_locks, pos);
				sr_lua_script_ver->version[pos] += 1;
				lock_set_release(sr_lua_locks, pos);
				LM_DBG("pos: %d set to reloaded\n", pos);
			}
			else
			{
				LM_ERR("pos out of range\n");
				return -2;
			}
		}
		return 0;
	}
	LM_ERR("No script loaded\n");
	return -1;
}

/**
 * Checks if loaded version matches the shared
 * counter. If not equal reloads the script.
 */
int sr_lua_reload_script(void)
{
	sr_lua_load_t *li = _sr_lua_load_list;
	int ret, i;
	char *txt;
	int sv_len = sr_lua_script_ver->len;

	if(li==NULL)
	{
		LM_DBG("No script loaded\n");
		return 0;
	}

	if(_app_lua_sv==NULL) {
		_app_lua_sv = (int *) pkg_malloc(sizeof(int)*sv_len);
		if(_app_lua_sv==NULL)
		{
			PKG_MEM_ERROR;
			return -1;
		}
	}

	for(i=0;i<sv_len;i++)
	{
		lock_set_get(sr_lua_locks, i);
		_app_lua_sv[i] = sr_lua_script_ver->version[i];
		lock_set_release(sr_lua_locks, i);

		if(li->version!=_app_lua_sv[i])
		{
			LM_DBG("loaded version:%d needed: %d Let's reload <%s>\n",
				li->version, _app_lua_sv[i], li->script);
			ret = luaL_dofile(_sr_L_env.LL, (const char*)li->script);
			if(ret!=0)
			{
				LM_ERR("failed to load Lua script: %s (err: %d)\n",
						li->script, ret);
				txt = (char*)lua_tostring(_sr_L_env.LL, -1);
				LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
				lua_pop(_sr_L_env.LL, 1);
				return -1;
			}
			li->version = _app_lua_sv[i];
			LM_DBG("<%s> set to version %d\n", li->script, li->version);
		}
		else LM_DBG("No need to reload [%s] is version %d\n",
			li->script, li->version);
		li = li->next;
	}
	return 1;
}

/**
 *
 */
int lua_sr_initialized(void)
{
	if(_sr_L_env.L==NULL)
		return 0;

	return 1;
}

/**
 *
 */
int app_lua_return_int(lua_State *L, int v)
{
	lua_pushinteger(L, v);
	return 1;
}

/**
 *
 */
int app_lua_return_error(lua_State *L)
{
	lua_pushinteger(L, -1);
	return 1;
}

/**
 *
 */
int app_lua_return_boolean(lua_State *L, int b)
{
	if(b==SRLUA_FALSE)
		lua_pushboolean(L, SRLUA_FALSE);
	else
		lua_pushboolean(L, SRLUA_TRUE);
	return 1;
}

/**
 *
 */
int app_lua_return_false(lua_State *L)
{
	lua_pushboolean(L, SRLUA_FALSE);
	return 1;
}

/**
 *
 */
int app_lua_return_true(lua_State *L)
{
	lua_pushboolean(L, SRLUA_TRUE);
	return 1;
}

/**
 *
 */
int app_lua_dostring(sip_msg_t *msg, char *script)
{
	int ret;
	char *txt;
	sip_msg_t *bmsg;

	LM_DBG("executing Lua string: [[%s]]\n", script);
	LM_DBG("lua top index is: %d\n", lua_gettop(_sr_L_env.L));
	bmsg = _sr_L_env.msg;
	_sr_L_env.msg = msg;
	ret = luaL_dostring(_sr_L_env.L, script);
	if(ret!=0)
	{
		txt = (char*)lua_tostring(_sr_L_env.L, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		lua_pop (_sr_L_env.L, 1);
	}
	_sr_L_env.msg = bmsg;
	return (ret==0)?1:-1;
}

/**
 *
 */
int app_lua_dofile(sip_msg_t *msg, char *script)
{
	int ret;
	char *txt;
	sip_msg_t *bmsg;

	LM_DBG("executing Lua file: [[%s]]\n", script);
	LM_DBG("lua top index is: %d\n", lua_gettop(_sr_L_env.L));
	bmsg = _sr_L_env.msg;
	_sr_L_env.msg = msg;
	ret = luaL_dofile(_sr_L_env.L, script);
	if(ret!=0)
	{
		txt = (char*)lua_tostring(_sr_L_env.L, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		lua_pop(_sr_L_env.L, 1);
	}
	_sr_L_env.msg = bmsg;
	return (ret==0)?1:-1;
}

/**
 *
 */
int app_lua_runstring(sip_msg_t *msg, char *script)
{
	int ret;
	char *txt;
	sip_msg_t *bmsg;

	if(_sr_L_env.LL==NULL)
	{
		LM_ERR("lua loading state not initialized (call: %s)\n", script);
		return -1;
	}

	LM_DBG("running Lua string: [[%s]]\n", script);
	LM_DBG("lua top index is: %d\n", lua_gettop(_sr_L_env.LL));
	bmsg = _sr_L_env.msg;
	_sr_L_env.msg = msg;
	ret = luaL_dostring(_sr_L_env.LL, script);
	if(ret!=0)
	{
		txt = (char*)lua_tostring(_sr_L_env.LL, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		lua_pop (_sr_L_env.LL, 1);
	}
	_sr_L_env.msg = bmsg;
	return (ret==0)?1:-1;
}

/**
 *
 */
static str _sr_kemi_lua_exit_string = str_init("~~ksr~exit~~");

/**
 *
 */
str* sr_kemi_lua_exit_string_get(void)
{
	return &_sr_kemi_lua_exit_string;
}

/**
 *
 */
int app_lua_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3, int emode)
{
	int n;
	int ret;
	str txt;
	sip_msg_t *bmsg;
	int ltop;

	if(_sr_L_env.LL==NULL)
	{
		LM_ERR("lua loading state not initialized (call: %s)\n", func);
		return -1;
	}
	if(_app_lua_sr_reload!=0)
	{
		/* check the script version loaded */
		if(!sr_lua_reload_script())
		{
			LM_ERR("lua reload failed\n");
			return -1;
		}
	}
	else LM_DBG("reload deactivated\n");
	LM_DBG("executing Lua function: [[%s]]\n", func);
	ltop = lua_gettop(_sr_L_env.LL);
	LM_DBG("lua top index is: %d\n", ltop);
	lua_getglobal(_sr_L_env.LL, func);
	if(!lua_isfunction(_sr_L_env.LL, -1))
	{
		if(emode) {
			LM_ERR("no such function [%s] in lua scripts\n", func);
			LM_ERR("top stack type [%d - %s]\n",
				lua_type(_sr_L_env.LL, -1),
				lua_typename(_sr_L_env.LL,lua_type(_sr_L_env.LL, -1)));
			txt.s = (char*)lua_tostring(_sr_L_env.LL, -1);
			LM_ERR("error from Lua: %s\n", (txt.s)?txt.s:"unknown");
			/* restores the original stack size */
			lua_settop(_sr_L_env.LL, ltop);
			return -1;
		} else {
			/* restores the original stack size */
			lua_settop(_sr_L_env.LL, ltop);
			return 1;
		}
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
	bmsg = _sr_L_env.msg;
	_sr_L_env.msg = msg;
	ret = lua_pcall(_sr_L_env.LL, n, 0, 0);
	_sr_L_env.msg = bmsg;
	if(ret!=0)
	{
		txt.s = (char*)lua_tostring(_sr_L_env.LL, -1);
		n = 0;
		if(txt.s!=NULL) {
			for(n=0; txt.s[n]!='\0' && _sr_kemi_lua_exit_string.s[n]!='\0';
					n++) {
				if(txt.s[n] != _sr_kemi_lua_exit_string.s[n])
					break;
			}
			if(txt.s[n]!='\0' || _sr_kemi_lua_exit_string.s[n]!='\0') {
				LM_ERR("error from Lua: %s\n", txt.s);
				n = 0;
			} else {
				LM_DBG("ksr error call from Lua: %s\n", txt.s);
				n = 1;
			}
		} else {
			LM_ERR("error from Lua: unknown\n");
		}
		lua_pop(_sr_L_env.LL, 1);
		if(n==1) {
			/* restores the original stack size */
			lua_settop(_sr_L_env.LL, ltop);
			return 1;
		} else {
			LM_ERR("error executing: %s (err: %d)\n", func, ret);
			/* restores the original stack size */
			lua_settop(_sr_L_env.LL, ltop);
			return -1;
		}
	}

	/* restores the original stack size */
	lua_settop(_sr_L_env.LL, ltop);

	return 1;
}

/**
 *
 */
int app_lua_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return app_lua_run_ex(msg, func, p1, p2, p3, 1);
}

void app_lua_dump_stack(lua_State *L)
{
	int i;
	int t;
	int top;

	top = lua_gettop(L);

	LM_DBG("lua stack top index: %d\n", top);
	for (i = 1; i <= top; i++)
	{
		t = lua_type(L, i);
		switch (t)
		{
			case LUA_TSTRING:  /* strings */
				LM_DBG("[%i:s> %s\n", i, lua_tostring(L, i));
			break;
			case LUA_TBOOLEAN:  /* booleans */
				LM_DBG("[%i:b> %s\n", i,
					lua_toboolean(L, i) ? "true" : "false");
			break;
			case LUA_TNUMBER:  /* numbers */
				LM_DBG("[%i:n> %g\n", i, lua_tonumber(L, i));
			break;
			default:  /* other values */
				LM_DBG("[%i:t> %s\n", i, lua_typename(L, t));
			break;
		}
	}
}

/**
 *
 */
int sr_kemi_lua_return_int(lua_State* L, sr_kemi_t *ket, int rc)
{
	if(ket->rtype==SR_KEMIP_INT) {
		lua_pushinteger(L, rc);
		return 1;
	}
	if(ket->rtype==SR_KEMIP_BOOL && rc!=SR_KEMI_FALSE) {
		return app_lua_return_true(L);
	}
	return app_lua_return_false(L);
}

void sr_kemi_lua_push_dict_item(lua_State *L, sr_kemi_dict_item_t *item);

/**
 * creates and push a table to the lua stack with
 * the elements of the list
 */
void sr_kemi_lua_push_array(lua_State *L, sr_kemi_dict_item_t *item) {
	int i = 1;
	sr_kemi_dict_item_t *k;
	if(!item) {
		LM_CRIT("BUG: dict field empty\n");
		return;
	}
	if (item->vtype == SR_KEMIP_ARRAY) {
		k = item->v.dict;
	} else {
		k = item;
	}
	if(k) {
		lua_newtable(L);
	}
	while(k){
		lua_pushnumber(L, i++);
		sr_kemi_lua_push_dict_item(L, k);
		lua_settable(L, -3);
		k = k->next;
	}
}

void sr_kemi_lua_push_dict(lua_State *L, sr_kemi_dict_item_t *item) {
	sr_kemi_dict_item_t *k = item;
	if(!item) {
		LM_CRIT("BUG: dict field empty\n");
		return;
	}
	lua_newtable(L);
	while(k){
		sr_kemi_lua_push_dict_item(L, k->v.dict);
		lua_setfield(L, -2, k->name.s);
		k = k->next;
	}
}

void
sr_kemi_lua_push_dict_item(lua_State *L, sr_kemi_dict_item_t *item)
{
	switch(item->vtype) {
		case SR_KEMIP_NONE:
			LM_CRIT("BUG: vtype is NONE\n");
			lua_pushnil(L);
		break;
		case SR_KEMIP_INT:
			lua_pushinteger(L, item->v.n);
		break;
		case SR_KEMIP_STR:
			lua_pushlstring(L, item->v.s.s, item->v.s.len);
		break;
		case SR_KEMIP_BOOL:
			if(item->v.n!=SR_KEMI_FALSE) {
				lua_pushboolean(L, SRLUA_TRUE);
			} else {
				lua_pushboolean(L, SRLUA_FALSE);
			}
		break;
		case SR_KEMIP_NULL:
			lua_pushnil(L);
		break;
		case SR_KEMIP_ARRAY:
			sr_kemi_lua_push_array(L, item);
		break;
		case SR_KEMIP_DICT:
			sr_kemi_lua_push_dict(L, item);
		break;
		default:
			LM_DBG("unknown type:%d\n", item->vtype);
			/* unknown type - return false */
			lua_pushboolean(L, SRLUA_FALSE);
	}
}

/**
 *
 */
int sr_kemi_lua_return_xval(lua_State* L, sr_kemi_t *ket, sr_kemi_xval_t *rx)
{
	switch(rx->vtype) {
		case SR_KEMIP_NONE:
			return 0;
		case SR_KEMIP_INT:
			lua_pushinteger(L, rx->v.n);
			return 1;
		case SR_KEMIP_STR:
			lua_pushlstring(L, rx->v.s.s, rx->v.s.len);
			return 1;
		case SR_KEMIP_BOOL:
			if(rx->v.n!=SR_KEMI_FALSE) {
				lua_pushboolean(L, SRLUA_TRUE);
			} else {
				lua_pushboolean(L, SRLUA_FALSE);
			}
			return 1;
		case SR_KEMIP_XVAL:
			/* unknown content - return false */
			lua_pushboolean(L, SRLUA_FALSE);
			return 1;
		case SR_KEMIP_NULL:
			lua_pushnil(L);
			return 1;
		case SR_KEMIP_ARRAY:
			sr_kemi_lua_push_array(L, rx->v.dict);
			sr_kemi_xval_free(rx);
			return 1;
		case SR_KEMIP_DICT:
			sr_kemi_lua_push_dict_item(L, rx->v.dict);
			sr_kemi_xval_free(rx);
			return 1;
		default:
			/* unknown type - return false */
			lua_pushboolean(L, SRLUA_FALSE);
			return 1;
	}
}

/**
 *
 */
int sr_kemi_lua_exec_func_ex(lua_State* L, sr_kemi_t *ket, int pdelta)
{
	int i;
	int argc;
	int ret;
	str *fname;
	str *mname;
	sr_kemi_val_t vps[SR_KEMI_PARAMS_MAX];
	sr_lua_env_t *env_L;
	sr_kemi_xval_t *xret;

	env_L = sr_lua_env_get();

	if(env_L==NULL || env_L->msg==NULL || ket==NULL) {
		LM_ERR("invalid Lua environment attributes or parameters\n");
		return app_lua_return_false(L);
	}

	fname = &ket->fname;
	mname = &ket->mname;

	argc = lua_gettop(L);
	if(argc==pdelta && ket->ptypes[0]==SR_KEMIP_NONE) {
		if(ket->rtype==SR_KEMIP_XVAL) {
			xret = ((sr_kemi_xfm_f)(ket->func))(env_L->msg);
			return sr_kemi_lua_return_xval(L, ket, xret);
		} else {
			ret = ((sr_kemi_fm_f)(ket->func))(env_L->msg);
			return sr_kemi_lua_return_int(L, ket, ret);
		}
	}
	if(argc==pdelta && ket->ptypes[0]!=SR_KEMIP_NONE) {
		LM_ERR("invalid number of parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return app_lua_return_false(L);
	}

	if(argc>SR_KEMI_PARAMS_MAX+pdelta) {
		LM_ERR("too many parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return app_lua_return_false(L);
	}

	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_val_t));
	for(i=0; i<SR_KEMI_PARAMS_MAX; i++) {
		if(ket->ptypes[i]==SR_KEMIP_NONE) {
			break;
		}
		if(argc<i+pdelta+1) {
			LM_ERR("not enough parameters for: %.*s.%.*s\n",
					mname->len, mname->s, fname->len, fname->s);
			return app_lua_return_false(L);
		}
		if(ket->ptypes[i]==SR_KEMIP_STR) {
			vps[i].s.s = (char*)lua_tostring(L, i+pdelta+1);
			if(vps[i].s.s!=NULL) {
				if(lua_isstring(L, i+pdelta+1)) {
#if LUA_VERSION_NUM > 501
					vps[i].s.len = lua_rawlen(L, i+pdelta+1);
#else
					vps[i].s.len = lua_strlen(L, i+pdelta+1);
#endif
				} else {
					vps[i].s.len = strlen(vps[i].s.s);
				}
			} else {
				vps[i].s.len = 0;
			}
			LM_DBG("param[%d] for: %.*s is str: %.*s\n", i,
				fname->len, fname->s, vps[i].s.len, vps[i].s.s);
		} else if(ket->ptypes[i]==SR_KEMIP_INT) {
			vps[i].n = lua_tointeger(L, i+pdelta+1);
			LM_DBG("param[%d] for: %.*s is int: %d\n", i,
				fname->len, fname->s, vps[i].n);
		} else {
			LM_ERR("unknown parameter type %d (%d)\n", ket->ptypes[i], i);
			return app_lua_return_false(L);
		}
	}

	switch(i) {
		case 1:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmn_f)(ket->func))(env_L->msg, vps[0].n);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmn_f)(ket->func))(env_L->msg, vps[0].n);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfms_f)(ket->func))(env_L->msg, &vps[0].s);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fms_f)(ket->func))(env_L->msg, &vps[0].s);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_lua_return_false(L);
			}
		break;
		case 2:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmnn_f)(ket->func))(env_L->msg, vps[0].n, vps[1].n);
						return sr_kemi_lua_return_xval(L, ket, xret);
					} else {
						ret = ((sr_kemi_fmnn_f)(ket->func))(env_L->msg, vps[0].n, vps[1].n);
						return sr_kemi_lua_return_int(L, ket, ret);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmns_f)(ket->func))(env_L->msg, vps[0].n, &vps[1].s);
						return sr_kemi_lua_return_xval(L, ket, xret);
					} else {
						ret = ((sr_kemi_fmns_f)(ket->func))(env_L->msg, vps[0].n, &vps[1].s);
						return sr_kemi_lua_return_int(L, ket, ret);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_lua_return_false(L);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmsn_f)(ket->func))(env_L->msg, &vps[0].s, vps[1].n);
						return sr_kemi_lua_return_xval(L, ket, xret);
					} else {
						ret = ((sr_kemi_fmsn_f)(ket->func))(env_L->msg, &vps[0].s, vps[1].n);
						return sr_kemi_lua_return_int(L, ket, ret);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmss_f)(ket->func))(env_L->msg, &vps[0].s, &vps[1].s);
						return sr_kemi_lua_return_xval(L, ket, xret);
					} else {
						ret = ((sr_kemi_fmss_f)(ket->func))(env_L->msg, &vps[0].s, &vps[1].s);
						return sr_kemi_lua_return_int(L, ket, ret);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_lua_return_false(L);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_lua_return_false(L);
			}
		break;
		case 3:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmsss_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmsss_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmssn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmssn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmsns_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmsns_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmsnn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmsnn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnss_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmnss_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnsn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmnsn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnns_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmnns_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnnn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n);
					return sr_kemi_lua_return_xval(L, ket, xret);
				} else {
					ret = ((sr_kemi_fmnnn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n);
					return sr_kemi_lua_return_int(L, ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname->len, fname->s);
				return app_lua_return_false(L);
			}
		break;
		case 4:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssss_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsssn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssns_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnss_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnsn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnns_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsss_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnssn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsns_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnss_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnsn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnns_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname->len, fname->s);
				return app_lua_return_false(L);
			}
		break;
		case 5:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssss_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssssn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssns_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsssnn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssnss_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnsn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssnns_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnnn_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnsss_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnssn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnsns_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnsnn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnnss_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnsn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnnns_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnnn_f)(ket->func))(env_L->msg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnssss_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsssn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnssns_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnssnn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsnss_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnsn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsnns_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnnn_f)(ket->func))(env_L->msg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnsss_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnssn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnsns_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnsnn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnnss_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnsn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnnns_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnnn_f)(ket->func))(env_L->msg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname->len, fname->s);
				return app_lua_return_false(L);
			}
		break;
		case 6:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR
					&& ket->ptypes[5]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssssss_f)(ket->func))(env_L->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s, &vps[5].s);
				return sr_kemi_lua_return_int(L, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_lua_return_false(L);
			}
		break;
		default:
			LM_ERR("invalid parameters for: %.*s\n",
					fname->len, fname->s);
			return app_lua_return_false(L);
	}
}

/**
 *
 */
int sr_kemi_exec_func(lua_State* L, str *mname, int midx, str *fname)
{
	int pdelta;
	sr_kemi_t *ket = NULL;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(env_L==NULL || env_L->msg==NULL) {
		LM_ERR("invalid Lua environment attributes\n");
		return app_lua_return_false(L);
	}

	ket = sr_kemi_lookup(mname, midx, fname);
	if(ket==NULL) {
		LM_ERR("cannot find function (%d): %.*s.%.*s\n", midx,
				(mname && mname->len>0)?mname->len:0,
				(mname && mname->len>0)?mname->s:"",
				fname->len, fname->s);
		return app_lua_return_false(L);
	}
	if(mname->len<=0) {
		pdelta = 1;
	} else {
		pdelta = 3;
	}
	return sr_kemi_lua_exec_func_ex(L, ket, pdelta);
}

/**
 *
 */
int sr_kemi_lua_exec_func(lua_State* L, int eidx)
{
	sr_kemi_t *ket;
	int ret;
	struct timeval tvb = {0}, tve = {0};
	struct timezone tz;
	unsigned int tdiff;
	lua_Debug dinfo;

	ket = sr_kemi_lua_export_get(eidx);
	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tvb, &tz);
	}

	ret = sr_kemi_lua_exec_func_ex(L, ket, 0);

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tve, &tz);
		tdiff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
		if(tdiff >= cfg_get(core, core_cfg, latency_limit_action)) {
			memset(&dinfo, 0, sizeof(lua_Debug));
			if(lua_getstack(L, 1, &dinfo)>0
						&& lua_getinfo(L, "nSl", &dinfo)>0) {
				LOG(cfg_get(core, core_cfg, latency_log),
						"alert - action KSR.%s%s%s(...)"
						" took too long [%u us] (%s:%d - %s [%s])\n",
						(ket->mname.len>0)?ket->mname.s:"",
						(ket->mname.len>0)?".":"", ket->fname.s,
						tdiff,
						(dinfo.short_src[0])?dinfo.short_src:"<unknown>",
						dinfo.currentline,
						(dinfo.name)?dinfo.name:"<unknown>",
						(dinfo.what)?dinfo.what:"<unknown>");
			} else {
				LOG(cfg_get(core, core_cfg, latency_log),
						"alert - action KSR.%s%s%s(...)"
						" took too long [%u us]\n",
						(ket->mname.len>0)?ket->mname.s:"",
						(ket->mname.len>0)?".":"", ket->fname.s,
						tdiff);
			}
		}
	}

	return ret;
}

/**
 *
 */
static int sr_kemi_lua_modf (lua_State *L)
{
	int ret;
	char *luav[MAX_ACTIONS];
	char *argv[MAX_ACTIONS];
	int argc;
	int i;
	int mod_type;
	struct run_act_ctx ra_ctx;
	struct action *act;
	ksr_cmd_export_t* expf;
	sr_lua_env_t *env_L;

	ret = 1;
	act = NULL;
	argc = 0;
	memset(luav, 0, MAX_ACTIONS*sizeof(char*));
	memset(argv, 0, MAX_ACTIONS*sizeof(char*));
	env_L = sr_lua_env_get();
	if(env_L->msg==NULL)
		goto error;

#if 0
	app_lua_dump_stack(L);
#endif
	argc = lua_gettop(L);
	if(argc==0)
	{
		LM_ERR("name of module function not provided\n");
		goto error;
	}
	if(argc>=MAX_ACTIONS)
	{
		LM_ERR("too many parameters\n");
		goto error;
	}
	/* first is function name, then parameters */
	for(i=1; i<=argc; i++)
	{
		if (!lua_isstring(L, i))
		{
			LM_ERR("invalid parameter type (%d)\n", i);
			goto error;
		}
		luav[i-1] = (char*)lua_tostring(L, i);
	}
	/* pkg copy only parameters */
	for(i=1; i<MAX_ACTIONS; i++)
	{
		if(luav[i]!=NULL)
		{
			argv[i] = (char*)pkg_malloc(strlen(luav[i])+1);
			if(argv[i]==NULL)
			{
				PKG_MEM_ERROR;
				goto error;
			}
			strcpy(argv[i], luav[i]);
		}
	}

	expf = find_export_record(luav[0], argc-1, 0);
	if (expf==NULL) {
		LM_ERR("function '%s' is not available\n", luav[0]);
		goto error;
	}
	/* check fixups */
	if (expf->fixup!=NULL && expf->free_fixup==NULL) {
		LM_ERR("function '%s' has fixup - cannot be used\n", luav[0]);
		goto error;
	}
	switch(expf->param_no) {
		case 0:
			mod_type = MODULE0_T;
			break;
		case 1:
			mod_type = MODULE1_T;
			break;
		case 2:
			mod_type = MODULE2_T;
			break;
		case 3:
			mod_type = MODULE3_T;
			break;
		case 4:
			mod_type = MODULE4_T;
			break;
		case 5:
			mod_type = MODULE5_T;
			break;
		case 6:
			mod_type = MODULE6_T;
			break;
		case VAR_PARAM_NO:
			mod_type = MODULEX_T;
			break;
		default:
			LM_ERR("unknown/bad definition for function '%s' (%d params)\n",
					luav[0], expf->param_no);
			goto error;
	}

	act = mk_action(mod_type,  argc+1   /* number of (type, value) pairs */,
					MODEXP_ST, expf,    /* function */
					NUMBER_ST, argc-1,  /* parameter number */
					STRING_ST, argv[1], /* param. 1 */
					STRING_ST, argv[2], /* param. 2 */
					STRING_ST, argv[3], /* param. 3 */
					STRING_ST, argv[4], /* param. 4 */
					STRING_ST, argv[5], /* param. 5 */
					STRING_ST, argv[6]  /* param. 6 */
			);

	if (act==NULL) {
		LM_ERR("action structure could not be created for '%s'\n", luav[0]);
		goto error;
	}

	/* handle fixups */
	if (expf->fixup) {
		if(argc==1)
		{ /* no parameters */
			if(expf->fixup(0, 0)<0)
			{
				LM_ERR("Error in fixup (0) for '%s'\n", luav[0]);
				goto error;
			}
		} else {
			for(i=1; i<argc; i++)
			{
				if(expf->fixup(&(act->val[i+1].u.data), i)<0)
				{
					LM_ERR("Error in fixup (%d) for '%s'\n", i, luav[0]);
					goto error;
				}
				act->val[i+1].type = MODFIXUP_ST;
			}
		}
	}
	init_run_actions_ctx(&ra_ctx);
	ret = do_action(&ra_ctx, act, env_L->msg);

	/* free fixups */
	if (expf->fixup) {
		for(i=1; i<argc; i++)
		{
			if ((act->val[i+1].type == MODFIXUP_ST) && (act->val[i+1].u.data))
			{
				expf->free_fixup(&(act->val[i+1].u.data), i);
			}
		}
	}
	pkg_free(act);
	for(i=0; i<MAX_ACTIONS; i++)
	{
		if(argv[i]!=NULL) pkg_free(argv[i]);
		argv[i] = 0;
	}
	lua_pushinteger(L, ret);
	return 1;

error:
	if(act!=NULL)
		pkg_free(act);
	for(i=0; i<MAX_ACTIONS; i++)
	{
		if(argv[i]!=NULL) pkg_free(argv[i]);
		argv[i] = 0;
	}
	lua_pushinteger(L, -1);
	return 1;
}

/**
 *
 */
static int sr_kemi_lua_exit (lua_State *L)
{
	str *s;

	LM_DBG("script exit call\n");
	s = sr_kemi_lua_exit_string_get();
	lua_getglobal(L, "error");
	lua_pushstring(L, s->s);
	lua_call(L, 1, 0);
	return 0;
}

/**
 *
 */
static int sr_kemi_lua_drop (lua_State *L)
{
	str *s;

	LM_DBG("script drop call\n");
	sr_kemi_core_set_drop(NULL);
	s = sr_kemi_lua_exit_string_get();
	lua_getglobal(L, "error");
	lua_pushstring(L, s->s);
	lua_call(L, 1, 0);
	return 0;
}

/**
 *
 */
static int sr_kemi_lua_probe (lua_State *L)
{
	LM_DBG("someone probing from lua\n");
	return 0;
}

/**
 *
 */
static const luaL_Reg _sr_kemi_x_Map [] = {
	{"modf",      sr_kemi_lua_modf},
	{"exit",      sr_kemi_lua_exit},
	{"drop",      sr_kemi_lua_drop},
	{"probe",     sr_kemi_lua_probe},
	{NULL, NULL}
};


/**
 *
 */
luaL_Reg *_sr_KSRMethods = NULL;

#define SR_LUA_KSR_MODULES_SIZE	256
#define SR_LUA_KSR_METHODS_SIZE	(SR_KEMI_LUA_EXPORT_SIZE + SR_LUA_KSR_MODULES_SIZE)

/**
 *
 */
void lua_sr_kemi_register_libs(lua_State *L)
{
	luaL_Reg *_sr_crt_KSRMethods = NULL;
	sr_kemi_module_t *emods = NULL;
	int emods_size = 0;
	int i;
	int k;
	int n;
	char mname[128];

#if 0
	/* dynamic lookup on function name */
	lua_sr_kemi_register_core(L);
	lua_sr_kemi_register_modules(L);
#endif

	_sr_KSRMethods = malloc(SR_LUA_KSR_METHODS_SIZE * sizeof(luaL_Reg));
	if(_sr_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return;
	}
	memset(_sr_KSRMethods, 0, SR_LUA_KSR_METHODS_SIZE * sizeof(luaL_Reg));

	emods_size = sr_kemi_modules_size_get();
	emods = sr_kemi_modules_get();

	n = 0;
	_sr_crt_KSRMethods = _sr_KSRMethods;
	if(emods_size==0 || emods[0].kexp==NULL) {
		LM_ERR("no kemi exports registered\n");
		return;
	}

	for(i=0; emods[0].kexp[i].func!=NULL; i++) {
		if(_ksr_app_lua_log_mode & KSR_APP_LUA_LOG_EXPORTS) {
			LM_DBG("exporting KSR.%s(...)\n", emods[0].kexp[i].fname.s);
		}
		_sr_crt_KSRMethods[i].name = emods[0].kexp[i].fname.s;
		_sr_crt_KSRMethods[i].func =
			sr_kemi_lua_export_associate(&emods[0].kexp[i]);
		if(_sr_crt_KSRMethods[i].func == NULL) {
			LM_ERR("failed to associate kemi function with lua export\n");
			free(_sr_KSRMethods);
			_sr_KSRMethods = NULL;
			return;
		}
		n++;
	}

	luaL_openlib(L, "KSR", _sr_crt_KSRMethods, 0);

	luaL_openlib(L, "KSR.x",  _sr_kemi_x_Map, 0);

	/* registered kemi modules */
	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
			n++;
			_sr_crt_KSRMethods = _sr_KSRMethods + n;
			snprintf(mname, 128, "KSR.%s", emods[k].kexp[0].mname.s);
			for(i=0; emods[k].kexp[i].func!=NULL; i++) {
				if(_ksr_app_lua_log_mode & KSR_APP_LUA_LOG_EXPORTS) {
					LM_DBG("exporting %s.%s(...)\n", mname,
							emods[k].kexp[i].fname.s);
				}
				_sr_crt_KSRMethods[i].name = emods[k].kexp[i].fname.s;
				_sr_crt_KSRMethods[i].func =
					sr_kemi_lua_export_associate(&emods[k].kexp[i]);
				if(_sr_crt_KSRMethods[i].func == NULL) {
					LM_ERR("failed to associate kemi function with func export\n");
					free(_sr_KSRMethods);
					_sr_KSRMethods = NULL;
					return;
				}
				n++;
			}
			if(!lua_checkstack(L, i+8)) {
				LM_ERR("not enough Lua stack capacity\n");
				exit(-1);
			}
			luaL_openlib(L, mname, _sr_crt_KSRMethods, 0);
			if(_ksr_app_lua_log_mode & KSR_APP_LUA_LOG_EXPORTS) {
				LM_DBG("initializing kemi sub-module: %s (%s) (%d/%d/%d)\n",
						mname, emods[k].kexp[0].mname.s, i, k, n);
			}
		}
	}
	LM_DBG("module 'KSR' has been initialized (%d/%d)\n", emods_size, n);
}

static const char* app_lua_rpc_api_list_doc[2] = {
	"list kemi exports to lua",
	0
};

static void app_lua_rpc_api_list(rpc_t* rpc, void* ctx)
{
	int i;
	int n;
	sr_kemi_t *ket;
	void* th;
	void* sh;
	void* ih;

	if (rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}

	/* count the number of exported functions */
	n = 0;
	for(i=0; i<SR_KEMI_LUA_EXPORT_SIZE; i++) {
		ket = sr_kemi_lua_export_get(i);
		if(ket==NULL) continue;
		n++;
	}

	if(rpc->struct_add(th, "d[",
				"msize", n,
				"methods",  &ih)<0)
	{
		rpc->fault(ctx, 500, "Internal error array structure");
		return;
	}
	for(i=0; i<SR_KEMI_LUA_EXPORT_SIZE; i++) {
		ket = sr_kemi_lua_export_get(i);
		if(ket==NULL) continue;
		if(rpc->struct_add(ih, "{", "func", &sh)<0) {
			rpc->fault(ctx, 500, "Internal error internal structure");
			return;
		}
		if(rpc->struct_add(sh, "SSSS",
				"ret", sr_kemi_param_map_get_name(ket->rtype),
				"module", &ket->mname,
				"name", &ket->fname,
				"params", sr_kemi_param_map_get_params(ket->ptypes))<0) {
			LM_ERR("failed to add the structure with attributes (%d)\n", i);
			rpc->fault(ctx, 500, "Internal error creating dest struct");
			return;
		}
	}
}

/*** RPC implementation ***/

static const char* app_lua_rpc_reload_doc[2] = {
	"Reload lua script",
	0
};

static const char* app_lua_rpc_list_doc[2] = {
	"list lua scripts",
	0
};

static void app_lua_rpc_reload(rpc_t* rpc, void* ctx)
{
	int pos = -1;

	rpc->scan(ctx, "*d", &pos);
	LM_DBG("selected index: %d\n", pos);
	if(lua_sr_reload_script(pos)<0)
		rpc->fault(ctx, 500, "Reload Failed");
	return;
}

static void app_lua_rpc_list(rpc_t* rpc, void* ctx)
{
	int i;
	sr_lua_load_t *list = NULL, *li;
	if(lua_sr_list_script(&list)<0)
	{
		LM_ERR("Can't get loaded scripts\n");
		return;
	}
	if(list)
	{
		li = list;
		i = 0;
		while(li)
		{
			rpc->rpl_printf(ctx, "%d: [%s]", i, li->script);
			li = li->next;
			i += 1;
		}
	}
	else {
		rpc->rpl_printf(ctx,"No scripts loaded");
	}
	return;
}

rpc_export_t app_lua_rpc_cmds[] = {
	{"app_lua.reload", app_lua_rpc_reload,
		app_lua_rpc_reload_doc, 0},
	{"app_lua.list", app_lua_rpc_list,
		app_lua_rpc_list_doc, 0},
	{"app_lua.api_list", app_lua_rpc_api_list,
		app_lua_rpc_api_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int app_lua_init_rpc(void)
{
	if (rpc_register_array(app_lua_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int bind_app_lua(app_lua_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->env_get_f = sr_lua_env_get;
	api->openlibs_register_f = app_lua_openlibs_register;
	return 0;
}
