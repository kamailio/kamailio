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
#include "../../route_struct.h"
#include "../../action.h"
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
static int lua_sr_probe (lua_State *L)
{
	LM_DBG("someone probing from lua\n");
	return 0;
}

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
static int lua_sr_modf (lua_State *L)
{
	int ret;
	char *luav[MAX_ACTIONS];
	char *argv[MAX_ACTIONS];
	int argc;
	int i;
	int mod_type;
	struct run_act_ctx ra_ctx;
	unsigned modver;
	struct action *act;
	sr31_cmd_export_t* expf;
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
				LM_ERR("no more pkg\n");
				goto error;
			}
			strcpy(argv[i], luav[i]);
		}
	}

	expf = find_export_record(luav[0], argc-1, 0, &modver);
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
			for(i=1; i<=argc; i++)
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
		for(i=1; i<=argc; i++)
		{
			if ((act->val[i+1].type == MODFIXUP_ST) && (act->val[i+1].u.data))
			{
				expf->free_fixup(&(act->val[i+1].u.data), i);
			}
		}
	}
	pkg_free(act);
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
static const luaL_reg _sr_core_Map [] = {
	{"probe", lua_sr_probe},
	{"dbg",   lua_sr_dbg},
	{"err",   lua_sr_err},
	{"log",   lua_sr_log},
	{"modf",  lua_sr_modf},
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


