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
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../lib/kcore/cmpapi.h"

#include "app_lua_api.h"
#include "app_lua_sr.h"

/**
 *
 */
static int lua_sr_dbg (lua_State *L)
{
	char *txt;
	txt = (char*)lua_tostring(L, -1);
	if(txt!=NULL)
		LM_DBG("%s", txt);
	return 0;
}

/**
 *
 */
static int lua_sr_err (lua_State *L)
{
	char *txt;
	txt = (char*)lua_tostring(L, -1);
	if(txt!=NULL)
		LM_ERR("%s", txt);
	return 0;
}

/**
 *
 */
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

/**
 *
 */
static const luaL_reg _sr_core_Map [] = {
	{"dbg", lua_sr_dbg},
	{"err", lua_sr_err},
	{"log", lua_sr_log},
	{NULL, NULL}
};

/**
 *
 */
static int lua_sr_hdr_append (lua_State *L)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *txt;
	int len;
	char *hdr;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || env_L->msg==NULL)
		return 0;

	LM_DBG("append hf: %s\n", txt);
	if (parse_headers(env_L->msg, HDR_EOH_F, 0) == -1)
	{
		LM_ERR("error while parsing message\n");
		return 0;
	}

	hf = env_L->msg->last_header;
	len = strlen(txt);
	hdr = (char*)pkg_malloc(len);
	if(hdr==NULL)
	{
		LM_ERR("no pkg memory left\n");
		return 0;
	}
	memcpy(hdr, txt, len);
	anchor = anchor_lump(env_L->msg,
				hf->name.s + hf->len - env_L->msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return 0;
	}
	return 0;
}

/**
 *
 */
static int lua_sr_hdr_remove (lua_State *L)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *txt;
	str hname;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || env_L->msg==NULL)
		return 0;

	LM_DBG("remove hf: %s\n", txt);
	if (parse_headers(env_L->msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return 0;
	}

	hname.s = txt;
	hname.len = strlen(txt);
	for (hf=env_L->msg->headers; hf; hf=hf->next)
	{
		if (cmp_hdrname_str(&hf->name, &hname)==0)
		{
			anchor=del_lump(env_L->msg,
					hf->name.s - env_L->msg->buf, hf->len, 0);
			if (anchor==0)
			{
				LM_ERR("cannot remove hdr %s\n", txt);
				return 0;
			}
		}
	}
	return 0;
}

/**
 *
 */
static int lua_sr_hdr_insert (lua_State *L)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *txt;
	int len;
	char *hdr;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || env_L->msg==NULL)
		return 0;

	LM_DBG("insert hf: %s\n", txt);
	hf = env_L->msg->headers;
	len = strlen(txt);
	hdr = (char*)pkg_malloc(len);
	if(hdr==NULL)
	{
		LM_ERR("no pkg memory left\n");
		return 0;
	}
	memcpy(hdr, txt, len);
	anchor = anchor_lump(env_L->msg,
				hf->name.s + hf->len - env_L->msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		return 0;
	}
	return 0;
}

/**
 *
 */
static int lua_sr_hdr_append_to_reply (lua_State *L)
{
	char *txt;
	int len;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	txt = (char*)lua_tostring(L, -1);
	if(txt==NULL || env_L->msg==NULL)
		return 0;

	LM_DBG("append to reply: %s\n", txt);
	len = strlen(txt);

	if(add_lump_rpl(env_L->msg, txt, len, LUMP_RPL_HDR)==0)
	{
		LM_ERR("unable to add reply lump\n");
		return 0;
	}

	return 0;
}


/**
 *
 */
static const luaL_reg _sr_hdr_Map [] = {
	{"append", lua_sr_hdr_append},
	{"remove", lua_sr_hdr_remove},
	{"insert", lua_sr_hdr_insert},
	{"append_to_reply", lua_sr_hdr_append_to_reply},
	{NULL, NULL}
};


/**
 *
 */
static int lua_sr_pv_get (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	pvn.s = (char*)lua_tostring(L, -1);
	if(pvn.s==NULL || env_L->msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv get: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_L->msg, &pvs, &val) != 0)
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

/**
 *
 */
static int lua_sr_pv_seti (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	if(lua_gettop(L)<2)
	{
		LM_ERR("to few parameters [%d]\n", lua_gettop(L));
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
	if(pvn.s==NULL || env_L->msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	if(pv_set_spec_value(env_L->msg, &pvs, 0, &val)<0)
	{
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

/**
 *
 */
static int lua_sr_pv_sets (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

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
	if(pvn.s==NULL || env_L->msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	if(pv_set_spec_value(env_L->msg, &pvs, 0, &val)<0)
	{
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

/**
 *
 */
static int lua_sr_pv_unset (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	pvn.s = (char*)lua_tostring(L, -1);
	if(pvn.s==NULL || env_L->msg==NULL)
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
	if(pv_set_spec_value(env_L->msg, &pvs, 0, &val)<0)
	{
		LM_ERR("unable to unset pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

/**
 *
 */
static int lua_sr_pv_is_null (lua_State *L)
{
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;
	sr_lua_env_t *env_L;

	env_L = sr_lua_env_get();

	pvn.s = (char*)lua_tostring(L, -1);
	if(pvn.s==NULL || env_L->msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv is null: %s\n", pvn.s);
	if(pv_parse_spec(&pvn, &pvs)<0)
	{
		LM_ERR("unable to parse pv [%s]\n", pvn.s);
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_L->msg, &pvs, &val) != 0)
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

/**
 *
 */
static const luaL_reg _sr_pv_Map [] = {
	{"get",      lua_sr_pv_get},
	{"seti",     lua_sr_pv_seti},
	{"sets",     lua_sr_pv_sets},
	{"unset",    lua_sr_pv_unset},
	{"is_null",  lua_sr_pv_is_null},
	{NULL, NULL}
};

/**
 *
 */
void lua_sr_core_openlibs(lua_State *L)
{
	luaL_openlib(L, "sr",      _sr_core_Map, 0);
	luaL_openlib(L, "sr.hdr",  _sr_hdr_Map,  0);
	luaL_openlib(L, "sr.pv",   _sr_pv_Map,   0);
}


