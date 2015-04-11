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
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../locking.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../lib/kcore/cmpapi.h"

#include "app_lua_api.h"
#include "app_lua_sr.h"
#include "app_lua_exp.h"


#define SRVERSION "1.0"

/**
 * reload enabled param
 * default: 0 (off)
 */
static unsigned int _app_lua_sr_reload = 0;
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
		LM_ERR("cannot allocate shm memory\n");
		return -1;
	}

	sr_lua_script_ver->version = (unsigned int *) shm_malloc(sizeof(unsigned int)*size);
	if(sr_lua_script_ver->version==NULL)
	{
		LM_ERR("cannot allocate shm memory\n");
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
		LM_ERR("no more pkg\n");
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
int sr_lua_register_module(char *mname)
{
	if(lua_sr_exp_register_mod(mname)==0)
		return 0;
	return -1;
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
	}
	return 0;
}

/**
 *
 */
void lua_sr_openlibs(lua_State *L)
{
	lua_sr_core_openlibs(L);
	lua_sr_exp_openlibs(L);
}

/**
 *
 */
int lua_sr_init_mod(void)
{
	/* allocate shm */
	if(lua_sr_alloc_script_ver()<0)
	{
		LM_CRIT("failed to alloc shm for version\n");
		return -1;
	}

	memset(&_sr_L_env, 0, sizeof(sr_lua_env_t));
	if(lua_sr_exp_init_mod()<0)
		return -1;

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
	if(luaL_dostring(L, "sr.probe()")!=0)
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

	/* set SR lib version */
#if LUA_VERSION_NUM >= 502
	lua_pushstring(_sr_L_env.L, SRVERSION);
	lua_setglobal(_sr_L_env.L, "SRVERSION");
#else
	lua_pushstring(_sr_L_env.L, "SRVERSION");
	lua_pushstring(_sr_L_env.L, SRVERSION);
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
		lua_pushstring(_sr_L_env.L, SRVERSION);
		lua_setglobal(_sr_L_env.L, "SRVERSION");
#else
		lua_pushstring(_sr_L_env.LL, "SRVERSION");
		lua_pushstring(_sr_L_env.LL, SRVERSION);
		lua_settable(_sr_L_env.LL, LUA_GLOBALSINDEX);
#endif
		/* force loading lua lib now */
		if(luaL_dostring(_sr_L_env.LL, "sr.probe()")!=0)
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
			LM_ERR("no more pkg memory\n");
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

/**
 *
 */
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

/**
 *
 */
int app_lua_runstring(struct sip_msg *msg, char *script)
{
	int ret;
	char *txt;

	if(_sr_L_env.LL==NULL)
	{
		LM_ERR("lua loading state not initialized (call: %s)\n", script);
		return -1;
	}

	LM_DBG("running Lua string: [[%s]]\n", script);
	LM_DBG("lua top index is: %d\n", lua_gettop(_sr_L_env.LL));
	_sr_L_env.msg = msg;
	ret = luaL_dostring(_sr_L_env.LL, script);
	if(ret!=0)
	{
		txt = (char*)lua_tostring(_sr_L_env.LL, -1);
		LM_ERR("error from Lua: %s\n", (txt)?txt:"unknown");
		lua_pop (_sr_L_env.LL, 1);
	}
	_sr_L_env.msg = 0;
	return (ret==0)?1:-1;
}

/**
 *
 */
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
