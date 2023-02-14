/**
 * Copyright (C) 2018 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
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
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "app_ruby_papi.h"

MODULE_VERSION

static int app_ruby_init_mod(void);
static int app_ruby_init_rpc(void);

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

static int w_app_ruby_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_ruby_run0(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_ruby_run1(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_ruby_run2(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_ruby_run3(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);

static int fixup_ruby_run(void** param, int param_no);

static str _app_ruby_load_file = STR_NULL;
static int _app_ruby_xval_mode = 1;

static int *_app_ruby_reload_version = NULL;
static str _app_ruby_modproc = str_init("app_ruby_proc.so");

static void *_app_ruby_dlhandle = NULL;
static app_ruby_papi_t _app_ruby_papi = {0};

/* clang-format off */
static cmd_export_t cmds[]={
	{"ruby_run", (cmd_function)w_app_ruby_run0, 1, fixup_ruby_run,
		0, ANY_ROUTE},
	{"ruby_run", (cmd_function)w_app_ruby_run1, 2, fixup_ruby_run,
		0, ANY_ROUTE},
	{"ruby_run", (cmd_function)w_app_ruby_run2, 3, fixup_ruby_run,
		0, ANY_ROUTE},
	{"ruby_run", (cmd_function)w_app_ruby_run3, 4, fixup_ruby_run,
		0, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"load", PARAM_STR, &_app_ruby_load_file},
	{"modproc", PARAM_STR, &_app_ruby_modproc},
	{"xval_mode", PARAM_INT, &_app_ruby_xval_mode},
	{0, 0, 0}
};

struct module_exports exports = {
	"app_ruby",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* exported rpc functions */
	0,               /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module initialization function */
	child_init,      /* per child init function */
	mod_destroy      /* module destroy function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	if(app_ruby_init_mod()<0)
		return -1;

	if(app_ruby_init_rpc()<0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(_app_ruby_dlhandle!=0) {
		dlclose(_app_ruby_dlhandle);
		_app_ruby_dlhandle = NULL;
	}

	return 0;
}

/**
 * init module children
 */
static int child_init(int rank)
{
	char *errstr = NULL;
	char *modpath = NULL;
	app_ruby_proc_bind_f bind_f = NULL;

	if(rank==PROC_MAIN || rank==PROC_TCP_MAIN || rank==PROC_INIT) {
		LM_DBG("skipping child init for rank: %d\n", rank);
		return 0;
	}

	if(ksr_locate_module(_app_ruby_modproc.s, &modpath)<0) {
		return -1;
	}

	LM_DBG("trying to load <%s>\n", modpath);

#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif
	_app_ruby_dlhandle = dlopen(modpath, RTLD_NOW); /* resolve all symbols now */
	if (_app_ruby_dlhandle==0) {
		LM_ERR("could not open module <%s>: %s\n", modpath, dlerror());
		goto error;
	}
	/* launch register */
	bind_f = (app_ruby_proc_bind_f)dlsym(_app_ruby_dlhandle, "app_ruby_proc_bind");
	if (((errstr=(char*)dlerror())==NULL) && bind_f!=NULL) {
		/* version control */
		if (!ksr_version_control(_app_ruby_dlhandle, modpath)) {
			goto error;
		}
		/* no error - call it */
		if(bind_f(&_app_ruby_papi)<0) {
			LM_ERR("filed to bind the api of proc module: %s\n", modpath);
			goto error;
		}
		LM_DBG("bound to proc module: <%s>\n", modpath);
	} else {
		LM_ERR("failure - func: %p - error: %s\n", bind_f, (errstr)?errstr:"none");
		goto error;
	}
	if(_app_ruby_modproc.s != modpath) {
		pkg_free(modpath);
	}

	if(_app_ruby_papi.AppRubyOptSetS("LoadFile", &_app_ruby_load_file)!=0) {
		return -1;
	}
	if(_app_ruby_papi.AppRubyOptSetN("XValMode", _app_ruby_xval_mode)!=0) {
		return -1;
	}
	if(_app_ruby_papi.AppRubyOptSetP("ReloadVersionPtr", _app_ruby_reload_version)!=0) {
		return -1;
	}

	return _app_ruby_papi.AppRubyInitChild();

error:
	if(_app_ruby_modproc.s != modpath) {
		pkg_free(modpath);
	}
	return -1;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	if(_app_ruby_papi.AppRubyModDestroy) {
		_app_ruby_papi.AppRubyModDestroy();
	}
}

/**
 *
 */
int sr_kemi_config_engine_ruby(sip_msg_t *msg, int rtype, str *rname,
		str *rparam)
{
	int ret;

	ret = -1;
	if(rtype==REQUEST_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, NULL, NULL, 0);
		} else {
			ret = _app_ruby_papi.AppRubyRunEx(msg, "ksr_request_route", NULL, NULL, NULL, 1);
		}
	} else if(rtype==CORE_ONREPLY_ROUTE) {
		if(kemi_reply_route_callback.len>0) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, kemi_reply_route_callback.s, NULL,
						NULL, NULL, 0);
		}
	} else if(rtype==BRANCH_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==BRANCH_FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==TM_ONREPLY_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==ONSEND_ROUTE) {
		if(kemi_onsend_route_callback.len>0) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, kemi_onsend_route_callback.s,
					NULL, NULL, NULL, 0);
		}
		return 1;
	} else if(rtype==EVENT_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = _app_ruby_papi.AppRubyRunEx(msg, rname->s,
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
 *
 */
static int app_ruby_init_mod(void)
{
	if(_app_ruby_load_file.s == NULL || _app_ruby_load_file.len<=0) {
		LM_ERR("no ruby script file to load was provided\n");
		return -1;
	}
	if(_app_ruby_reload_version == NULL) {
		_app_ruby_reload_version = (int*)shm_malloc(sizeof(int));
		if(_app_ruby_reload_version == NULL) {
			LM_ERR("failed to allocated reload version\n");
			return -1;
		}
		*_app_ruby_reload_version = 0;
	}
	return 0;
}


/**
 *
 */
int app_ruby_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return _app_ruby_papi.AppRubyRunEx(msg, func, p1, p2, p3, 0);
}

#define RUBY_BUF_STACK_SIZE	1024
static char _ruby_buf_stack[4][RUBY_BUF_STACK_SIZE];


/**
 *
 */
static int w_app_ruby_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	str s;
	if(!_app_ruby_papi.AppRubyInitialized())
	{
		LM_ERR("ruby env not initialized");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)func, &s)<0)
	{
		LM_ERR("cannot get the function\n");
		return -1;
	}
	if(s.len>=RUBY_BUF_STACK_SIZE-1)
	{
		LM_ERR("function too long %d\n", s.len);
		return -1;
	}
	memcpy(_ruby_buf_stack[0], s.s, s.len);
	_ruby_buf_stack[0][s.len] = '\0';

	if(p1!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)p1, &s)<0)
		{
			LM_ERR("cannot get p1\n");
			return -1;
		}
		if(s.len>=RUBY_BUF_STACK_SIZE-1)
		{
			LM_ERR("p1 too long %d\n", s.len);
			return -1;
		}
		memcpy(_ruby_buf_stack[1], s.s, s.len);
		_ruby_buf_stack[1][s.len] = '\0';

		if(p2!=NULL)
		{
			if(fixup_get_svalue(msg, (gparam_p)p2, &s)<0)
			{
				LM_ERR("cannot get p2\n");
				return -1;
			}
			if(s.len>=RUBY_BUF_STACK_SIZE-1)
			{
				LM_ERR("p2 too long %d\n", s.len);
				return -1;
			}
			memcpy(_ruby_buf_stack[2], s.s, s.len);
			_ruby_buf_stack[2][s.len] = '\0';

			if(p3!=NULL)
			{
				if(fixup_get_svalue(msg, (gparam_p)p3, &s)<0)
				{
					LM_ERR("cannot get p3\n");
					return -1;
				}
				if(s.len>=RUBY_BUF_STACK_SIZE-1)
				{
					LM_ERR("p3 too long %d\n", s.len);
					return -1;
				}
				memcpy(_ruby_buf_stack[3], s.s, s.len);
				_ruby_buf_stack[3][s.len] = '\0';
			}
		} else {
			p3 = NULL;
		}
	} else {
		p2 = NULL;
		p3 = NULL;
	}

	return app_ruby_run(msg, _ruby_buf_stack[0],
			(p1!=NULL)?_ruby_buf_stack[1]:NULL,
			(p2!=NULL)?_ruby_buf_stack[2]:NULL,
			(p3!=NULL)?_ruby_buf_stack[3]:NULL);
}

static int w_app_ruby_run0(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_ruby_run(msg, func, NULL, NULL, NULL);
}

static int w_app_ruby_run1(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_ruby_run(msg, func, p1, NULL, NULL);
}

static int w_app_ruby_run2(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_ruby_run(msg, func, p1, p2, NULL);
}

static int w_app_ruby_run3(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_ruby_run(msg, func, p1, p2, p3);
}

static int fixup_ruby_run(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
}

/**
 *
 */
static int ki_app_ruby_run(sip_msg_t *msg, str *func)
{
	if(func==NULL || func->s==NULL || func->len<0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	return app_ruby_run(msg, func->s, NULL, NULL, NULL);

}

/**
 *
 */
static int ki_app_ruby_run_p1(sip_msg_t *msg, str *func, str *p1)
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
	return app_ruby_run(msg, func->s, p1->s, NULL, NULL);
}

/**
 *
 */
static int ki_app_ruby_run_p2(sip_msg_t *msg, str *func, str *p1, str *p2)
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
	return app_ruby_run(msg, func->s, p1->s, p2->s, NULL);
}

/**
 *
 */
static int ki_app_ruby_run_p3(sip_msg_t *msg, str *func, str *p1, str *p2, str *p3)
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
	return app_ruby_run(msg, func->s, p1->s, p2->s, p3->s);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_app_ruby_exports[] = {
	{ str_init("app_ruby"), str_init("run"),
		SR_KEMIP_INT, ki_app_ruby_run,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_ruby"), str_init("run_p1"),
		SR_KEMIP_INT, ki_app_ruby_run_p1,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_ruby"), str_init("run_p2"),
		SR_KEMIP_INT, ki_app_ruby_run_p2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_ruby"), str_init("run_p3"),
		SR_KEMIP_INT, ki_app_ruby_run_p3,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */


static const char* app_ruby_rpc_reload_doc[2] = {
	"Reload javascript file",
	0
};


static void app_ruby_rpc_reload(rpc_t* rpc, void* ctx)
{
	int v;
	void *vh;
	int lversion;

	if(_app_ruby_load_file.s == NULL && _app_ruby_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		rpc->fault(ctx, 500, "No script file");
		return;
	}
	if(_app_ruby_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		rpc->fault(ctx, 500, "Reload not enabled");
		return;
	}

	v = *_app_ruby_reload_version;
	*_app_ruby_reload_version += 1;
	lversion = _app_ruby_papi.AppRubyLocalVersion();
	LM_INFO("marking for reload ruby script file: %.*s (%d / %d => %d)\n",
				_app_ruby_load_file.len, _app_ruby_load_file.s,
				lversion, v, *_app_ruby_reload_version);

	if (rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "dd",
			"old", v,
			"new", *_app_ruby_reload_version);
}

static const char* app_ruby_rpc_api_list_doc[2] = {
	"List kemi exports to ruby",
	0
};

static void app_ruby_rpc_api_list(rpc_t* rpc, void* ctx)
{
	int i;
	int n;
	int esz;
	sr_kemi_t *ket;
	void* th;
	void* sh;
	void* ih;

	if (rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	n = 0;
	esz = _app_ruby_papi.AppRubyGetExportSize();
	for(i=0; i<esz; i++) {
		ket = _app_ruby_papi.AppRubyGetExport(i);
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
	for(i=0; i<esz; i++) {
		ket = _app_ruby_papi.AppRubyGetExport(i);
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

/**
 *
 */
rpc_export_t app_ruby_rpc_cmds[] = {
	{"app_ruby.reload", app_ruby_rpc_reload,
		app_ruby_rpc_reload_doc, 0},
	{"app_ruby.api_list", app_ruby_rpc_api_list,
		app_ruby_rpc_api_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 *
 */
static int app_ruby_init_rpc(void)
{
	if (rpc_register_array(app_ruby_rpc_cmds)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}


/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	str ename = str_init("ruby");

	*dlflags = RTLD_NOW | RTLD_GLOBAL;

	sr_kemi_eng_register(&ename, sr_kemi_config_engine_ruby);
	sr_kemi_modules_add(sr_kemi_app_ruby_exports);

	return 0;
}
