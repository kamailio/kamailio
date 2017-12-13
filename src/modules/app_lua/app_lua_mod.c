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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"

#include "app_lua_api.h"
#include "app_lua_kemi_export.h"

MODULE_VERSION

/** parameters */

/* List of allowed chars for a prefix*/
static int  mod_init(void);
static void mod_destroy(void);
static int  child_init(int rank);

static int app_lua_init_rpc(void);

static int w_app_lua_dostring(struct sip_msg *msg, char *script, char *extra);
static int w_app_lua_dofile(struct sip_msg *msg, char *script, char *extra);
static int w_app_lua_runstring(struct sip_msg *msg, char *script, char *extra);
static int w_app_lua_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_lua_run0(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_lua_run1(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_lua_run2(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_lua_run3(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3);

static int fixup_lua_run(void** param, int param_no);

int app_lua_load_param(modparam_t type, void *val);
int app_lua_register_param(modparam_t type, void *val);
int app_lua_reload_param(modparam_t type, void *val);

static param_export_t params[]={
	{"load",     PARAM_STRING|USE_FUNC_PARAM, (void*)app_lua_load_param},
	{"register", PARAM_STRING|USE_FUNC_PARAM, (void*)app_lua_register_param},
	{"reload",   INT_PARAM|USE_FUNC_PARAM, (void*)app_lua_reload_param},
	{0, 0, 0}
};

static cmd_export_t cmds[]={
	{"lua_dostring", (cmd_function)w_app_lua_dostring, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"lua_dofile", (cmd_function)w_app_lua_dofile, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"lua_runstring", (cmd_function)w_app_lua_runstring, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run0, 1, fixup_lua_run,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run1, 2, fixup_lua_run,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run2, 3, fixup_lua_run,
		0, ANY_ROUTE},
	{"lua_run", (cmd_function)w_app_lua_run3, 4, fixup_lua_run,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"app_lua",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};

/**
 *
 */
int sr_kemi_config_engine_lua(sip_msg_t *msg, int rtype, str *rname,
		str *rparam)
{
	int ret;

	ret = -1;
	if(rtype==REQUEST_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_lua_run_ex(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, NULL, NULL, 0);
		} else {
			ret = app_lua_run_ex(msg, "ksr_request_route", NULL, NULL, NULL, 1);
		}
	} else if(rtype==CORE_ONREPLY_ROUTE) {
		ret = app_lua_run_ex(msg, "ksr_reply_route", NULL, NULL, NULL, 0);
	} else if(rtype==BRANCH_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_lua_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_lua_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==BRANCH_FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_lua_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==TM_ONREPLY_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_lua_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==ONSEND_ROUTE) {
		ret = app_lua_run_ex(msg, "ksr_onsend_route", NULL, NULL, NULL, 0);
	} else if(rtype==EVENT_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_lua_run_ex(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, NULL, NULL, 0);
		}
	} else {
		if(rname!=NULL) {
			LM_ERR("route type %d with name [%.*s] not implemented\n",
				rtype, rname->len, rname->s);
		} else {
			LM_ERR("route type %d with no name not implemented\n",
				rtype);
		}
	}

	if(rname!=NULL) {
		LM_DBG("execution of route type %d with name [%.*s] returned %d\n",
				rtype, rname->len, rname->s, ret);
	} else {
		LM_DBG("execution of route type %d with no name returned %d\n",
			rtype, ret);
	}

	return 1;
}


/**
 * init module function
 */
static int mod_init(void)
{
	if(lua_sr_init_mod()<0)
		return -1;

	if(app_lua_init_rpc()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}


/* each child get a new connection to the database */
static int child_init(int rank)
{
	if(rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if (rank==PROC_INIT)
	{
		/* do a probe before forking */
		if(lua_sr_init_probe()!=0)
			return -1;
		return 0;
	}
	return lua_sr_init_child();
}


static void mod_destroy(void)
{
	lua_sr_destroy();
}

#define LUA_BUF_STACK_SIZE 1024
static char _lua_buf_stack[4][LUA_BUF_STACK_SIZE];

/**
 *
 */
static int ki_app_lua_dostring(sip_msg_t *msg, str *script)
{
	if(script==NULL || script->s==NULL || script->len>=LUA_BUF_STACK_SIZE-1) {
		LM_ERR("script too short or too long %d\n", (script)?script->len:0);
		return -1;
	}
	if(!lua_sr_initialized()) {
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	memcpy(_lua_buf_stack[0], script->s, script->len);
	_lua_buf_stack[0][script->len] = '\0';
	return app_lua_dostring(msg, _lua_buf_stack[0]);
}

/**
 *
 */
static int w_app_lua_dostring(struct sip_msg *msg, char *script, char *extra)
{
	str s;

	if(fixup_get_svalue(msg, (gparam_p)script, &s)<0)
	{
		LM_ERR("cannot get the script\n");
		return -1;
	}
	return ki_app_lua_dostring(msg, &s);
}

/**
 *
 */
static int ki_app_lua_dofile(sip_msg_t *msg, str *script)
{
	if(script==NULL || script->s==NULL || script->len>=LUA_BUF_STACK_SIZE-1) {
		LM_ERR("script too short or too long %d\n", (script)?script->len:0);
		return -1;
	}
	if(!lua_sr_initialized()) {
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	memcpy(_lua_buf_stack[0], script->s, script->len);
	_lua_buf_stack[0][script->len] = '\0';
	return app_lua_dofile(msg, _lua_buf_stack[0]);
}

/**
 *
 */
static int w_app_lua_dofile(struct sip_msg *msg, char *script, char *extra)
{
	str s;
	if(fixup_get_svalue(msg, (gparam_p)script, &s)<0) {
		LM_ERR("cannot get the script\n");
		return -1;
	}
	return ki_app_lua_dofile(msg, &s);
}

/**
 *
 */
static int ki_app_lua_runstring(sip_msg_t *msg, str *script)
{
	if(script==NULL || script->s==NULL || script->len>=LUA_BUF_STACK_SIZE-1) {
		LM_ERR("script too short or too long %d\n", (script)?script->len:0);
		return -1;
	}
	if(!lua_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	memcpy(_lua_buf_stack[0], script->s, script->len);
	_lua_buf_stack[0][script->len] = '\0';
	return app_lua_runstring(msg, _lua_buf_stack[0]);
}

/**
 *
 */
static int w_app_lua_runstring(struct sip_msg *msg, char *script, char *extra)
{
	str s;
	if(!lua_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)script, &s)<0)
	{
		LM_ERR("cannot get the script\n");
		return -1;
	}
	return ki_app_lua_runstring(msg, &s);
}

/**
 *
 */
static int w_app_lua_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	str s;
	if(!lua_sr_initialized())
	{
		LM_ERR("Lua env not intitialized");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)func, &s)<0)
	{
		LM_ERR("cannot get the function\n");
		return -1;
	}
	if(s.len>=LUA_BUF_STACK_SIZE-1)
	{
		LM_ERR("function too long %d\n", s.len);
		return -1;
	}
	memcpy(_lua_buf_stack[0], s.s, s.len);
	_lua_buf_stack[0][s.len] = '\0';

	if(p1!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)p1, &s)<0)
		{
			LM_ERR("cannot get p1\n");
			return -1;
		}
		if(s.len>=LUA_BUF_STACK_SIZE-1)
		{
			LM_ERR("p1 too long %d\n", s.len);
			return -1;
		}
		memcpy(_lua_buf_stack[1], s.s, s.len);
		_lua_buf_stack[1][s.len] = '\0';

		if(p2!=NULL)
		{
			if(fixup_get_svalue(msg, (gparam_p)p2, &s)<0)
			{
				LM_ERR("cannot get p2\n");
				return -1;
			}
			if(s.len>=LUA_BUF_STACK_SIZE-1)
			{
				LM_ERR("p2 too long %d\n", s.len);
				return -1;
			}
			memcpy(_lua_buf_stack[2], s.s, s.len);
			_lua_buf_stack[2][s.len] = '\0';

			if(p3!=NULL)
			{
				if(fixup_get_svalue(msg, (gparam_p)p3, &s)<0)
				{
					LM_ERR("cannot get p3\n");
					return -1;
				}
				if(s.len>=LUA_BUF_STACK_SIZE-1)
				{
					LM_ERR("p3 too long %d\n", s.len);
					return -1;
				}
				memcpy(_lua_buf_stack[3], s.s, s.len);
				_lua_buf_stack[3][s.len] = '\0';
			}
		} else {
			p3 = NULL;
		}
	} else {
		p2 = NULL;
		p3 = NULL;
	}

	return app_lua_run(msg, _lua_buf_stack[0],
			(p1!=NULL)?_lua_buf_stack[1]:NULL,
			(p2!=NULL)?_lua_buf_stack[2]:NULL,
			(p3!=NULL)?_lua_buf_stack[3]:NULL);
}

static int w_app_lua_run0(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_lua_run(msg, func, NULL, NULL, NULL);
}

static int w_app_lua_run1(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_lua_run(msg, func, p1, NULL, NULL);
}

static int w_app_lua_run2(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_lua_run(msg, func, p1, p2, NULL);
}

static int w_app_lua_run3(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_lua_run(msg, func, p1, p2, p3);
}

/**
 *
 */
static int ki_app_lua_run(sip_msg_t *msg, str *func)
{
	if(func==NULL || func->s==NULL || func->len<0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	return app_lua_run(msg, func->s, NULL, NULL, NULL);

}

/**
 *
 */
static int ki_app_lua_run_p1(sip_msg_t *msg, str *func, str *p1)
{
	if(func==NULL || func->s==NULL || func->len<=0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	if(p1==NULL || p1->s==NULL || p1->len<0) {
		LM_ERR("invalid p1 value\n");
		return -1;
	}
	if(p1->s[p1->len]!='\0') {
		LM_ERR("invalid terminated p1 value\n");
		return -1;
	}
	return app_lua_run(msg, func->s, p1->s, NULL, NULL);
}

/**
 *
 */
static int ki_app_lua_run_p2(sip_msg_t *msg, str *func, str *p1, str *p2)
{
	if(func==NULL || func->s==NULL || func->len<=0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	if(p1==NULL || p1->s==NULL || p1->len<0) {
		LM_ERR("invalid p1 value\n");
		return -1;
	}
	if(p1->s[p1->len]!='\0') {
		LM_ERR("invalid terminated p1 value\n");
		return -1;
	}
	if(p2==NULL || p2->s==NULL || p2->len<0) {
		LM_ERR("invalid p2 value\n");
		return -1;
	}
	if(p2->s[p2->len]!='\0') {
		LM_ERR("invalid terminated p2 value\n");
		return -1;
	}
	return app_lua_run(msg, func->s, p1->s, p2->s, NULL);
}

/**
 *
 */
static int ki_app_lua_run_p3(sip_msg_t *msg, str *func, str *p1, str *p2, str *p3)
{
	if(func==NULL || func->s==NULL || func->len<=0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	if(p1==NULL || p1->s==NULL || p1->len<0) {
		LM_ERR("invalid p1 value\n");
		return -1;
	}
	if(p1->s[p1->len]!='\0') {
		LM_ERR("invalid terminated p1 value\n");
		return -1;
	}
	if(p2==NULL || p2->s==NULL || p2->len<0) {
		LM_ERR("invalid p2 value\n");
		return -1;
	}
	if(p2->s[p2->len]!='\0') {
		LM_ERR("invalid terminated p2 value\n");
		return -1;
	}
	if(p3==NULL || p3->s==NULL || p3->len<0) {
		LM_ERR("invalid p3 value\n");
		return -1;
	}
	if(p3->s[p3->len]!='\0') {
		LM_ERR("invalid terminated p3 value\n");
		return -1;
	}
	return app_lua_run(msg, func->s, p1->s, p2->s, p3->s);
}

int app_lua_load_param(modparam_t type, void *val)
{
	if(val==NULL)
		return -1;
	return sr_lua_load_script((char*)val);
}

int app_lua_register_param(modparam_t type, void *val)
{
	if(val==NULL)
		return -1;
	return sr_lua_register_module((char*)val);
}

int app_lua_reload_param(modparam_t type, void *val)
{
	return sr_lua_reload_module((unsigned int)(long) (int *)val);
}

static int fixup_lua_run(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
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

/**
 * only prototypes of special kemi exports in order to list via rpc
 */
/* clang-format off */
static sr_kemi_t sr_kemi_app_lua_rpc_exports[] = {
	{ str_init("pv"), str_init("get"),
		SR_KEMIP_STR, NULL,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("seti"),
		SR_KEMIP_NONE, NULL,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("sets"),
		SR_KEMIP_NONE, NULL,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("unset"),
		SR_KEMIP_NONE, NULL,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("pv"), str_init("is_null"),
		SR_KEMIP_BOOL, NULL,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("x"), str_init("modf"),
		SR_KEMIP_INT, NULL,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("x"), str_init("exit"),
		SR_KEMIP_NONE, NULL,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("x"), str_init("drop"),
		SR_KEMIP_NONE, NULL,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

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
	for(i=0; sr_kemi_app_lua_rpc_exports[i].fname.s!=NULL; i++) {
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
	for(i=0; sr_kemi_app_lua_rpc_exports[i].fname.s!=NULL; i++) {
		ket = &sr_kemi_app_lua_rpc_exports[i];
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
static int app_lua_init_rpc(void)
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
/* clang-format off */
static sr_kemi_t sr_kemi_app_lua_exports[] = {
	{ str_init("app_lua"), str_init("dostring"),
		SR_KEMIP_INT, ki_app_lua_dostring,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_lua"), str_init("dofile"),
		SR_KEMIP_INT, ki_app_lua_dofile,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_lua"), str_init("runstring"),
		SR_KEMIP_INT, ki_app_lua_runstring,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_lua"), str_init("run"),
		SR_KEMIP_INT, ki_app_lua_run,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_lua"), str_init("run_p1"),
		SR_KEMIP_INT, ki_app_lua_run_p1,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_lua"), str_init("run_p2"),
		SR_KEMIP_INT, ki_app_lua_run_p2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_lua"), str_init("run_p3"),
		SR_KEMIP_INT, ki_app_lua_run_p3,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	str ename = str_init("lua");

	*dlflags = RTLD_NOW | RTLD_GLOBAL;

	sr_kemi_eng_register(&ename, sr_kemi_config_engine_lua);
	sr_kemi_modules_add(sr_kemi_app_lua_exports);

	return 0;
}
