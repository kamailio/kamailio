/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#include "../../core/dprint.h"
#include "../../core/pvar.h"
#include "../../core/sr_module.h"
#include "../../core/mem/shm.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "duktape.h"
#include "duk_module_node.h"
#include "app_jsdt_kemi_export.h"
#include "app_jsdt_api.h"

#define SRJSDT_FALSE 0
#define SRJSDT_TRUE 1

void jsdt_sr_kemi_register_libs(duk_context *J);

typedef struct _sr_jsdt_env
{
	duk_context *J;
	duk_context *JJ;
	sip_msg_t *msg;
	unsigned int flags;
	unsigned int nload; /* number of scripts loaded */
} sr_jsdt_env_t;

static sr_jsdt_env_t _sr_J_env = {0};

str _sr_jsdt_load_file = STR_NULL;
int _sr_jsdt_mode = 1;

static int *_sr_jsdt_reload_version = NULL;
static int _sr_jsdt_local_version = 0;

/**
 *
 */
sr_jsdt_env_t *jsdt_sr_env_get(void)
{
	return &_sr_J_env;
}

/**
 *
 */
int jsdt_sr_initialized(void)
{
	if(_sr_J_env.J==NULL)
		return 0;

	return 1;
}

/**
 *
 */
#define JSDT_SR_EXIT_THROW_STR "~~ksr~exit~~"
#define JSDT_SR_EXIT_EXEC_STR "throw '" JSDT_SR_EXIT_THROW_STR "';"

static str _sr_kemi_jsdt_exit_string = str_init(JSDT_SR_EXIT_THROW_STR);

/**
 *
 */
str* sr_kemi_jsdt_exit_string_get(void)
{
	return &_sr_kemi_jsdt_exit_string;
}

/**
 *
 */
int app_jsdt_return_int(duk_context *J, int v)
{
	duk_push_int(J, v);
	return 1;
}

/**
 *
 */
int app_jsdt_return_error(duk_context *J)
{
	duk_push_int(J, -1);
	return 1;
}

/**
 *
 */
int app_jsdt_return_boolean(duk_context *J, int b)
{
	if(b==SRJSDT_FALSE)
		duk_push_boolean(J, SRJSDT_FALSE);
	else
		duk_push_boolean(J, SRJSDT_TRUE);
	return 1;
}

/**
 *
 */
int app_jsdt_return_false(duk_context *J)
{
	duk_push_boolean(J, SRJSDT_FALSE);
	return 1;
}

/**
 *
 */
int app_jsdt_return_true(duk_context *J)
{
	duk_push_boolean(J, SRJSDT_TRUE);
	return 1;
}

/**
 *
 */
int sr_kemi_jsdt_return_int(duk_context *J, sr_kemi_t *ket, int rc)
{
	if(ket->rtype==SR_KEMIP_INT) {
		duk_push_int(J, rc);
		return 1;
	}
	if(ket->rtype==SR_KEMIP_BOOL && rc!=SR_KEMI_FALSE) {
		return app_jsdt_return_true(J);
	}
	return app_jsdt_return_false(J);
}

/**
 *
 */
int sr_kemi_jsdt_return_xval(duk_context *J, sr_kemi_t *ket, sr_kemi_xval_t *rx)
{
	switch(rx->vtype) {
		case SR_KEMIP_NONE:
			return 0;
		case SR_KEMIP_INT:
			duk_push_int(J, rx->v.n);
			return 1;
		case SR_KEMIP_STR:
			duk_push_lstring(J, rx->v.s.s, rx->v.s.len);
			return 1;
		case SR_KEMIP_BOOL:
			if(rx->v.n!=SR_KEMI_FALSE) {
				duk_push_boolean(J, SRJSDT_TRUE);
			} else {
				duk_push_boolean(J, SRJSDT_FALSE);
			}
			return 1;
		case SR_KEMIP_ARRAY:
			LM_ERR("unsupported return type: array\n");
			sr_kemi_xval_free(rx);
			duk_push_string(J, NULL);
			return 1;
		case SR_KEMIP_DICT:
			LM_ERR("unsupported return type: map\n");
			sr_kemi_xval_free(rx);
			duk_push_string(J, NULL);
			return 1;
		case SR_KEMIP_XVAL:
			/* unknown content - return false */
			duk_push_boolean(J, SRJSDT_FALSE);
			return 1;
		case SR_KEMIP_NULL:
			duk_push_string(J, NULL);
			return 1;
		default:
			/* unknown type - return false */
			duk_push_boolean(J, SRJSDT_FALSE);
			return 1;
	}
}


/**
 *
 */
static int jsdt_sr_exit (duk_context *J)
{
	duk_eval_string_noresult(J, JSDT_SR_EXIT_EXEC_STR);
	return 0;
}

/**
 *
 */
static int jsdt_sr_drop (duk_context *J)
{
	sr_kemi_core_set_drop(NULL);
	duk_eval_string_noresult(J, JSDT_SR_EXIT_EXEC_STR);
	return 0;
}


/**
 *
 */
static int jsdt_sr_modf (duk_context *J)
{
	int ret;
	char *jsdtv[MAX_ACTIONS];
	char *argv[MAX_ACTIONS];
	int argc;
	int i;
	int mod_type;
	struct run_act_ctx ra_ctx;
	struct action *act;
	ksr_cmd_export_t* expf;
	sr_jsdt_env_t *env_J;

	ret = 1;
	act = NULL;
	argc = 0;
	memset(jsdtv, 0, MAX_ACTIONS*sizeof(char*));
	memset(argv, 0, MAX_ACTIONS*sizeof(char*));
	env_J = jsdt_sr_env_get();
	if(env_J->msg==NULL)
		goto error;

	argc = duk_get_top(J);
	if(argc==0) {
		LM_ERR("name of module function not provided\n");
		goto error;
	}
	if(argc>=MAX_ACTIONS) {
		LM_ERR("too many parameters\n");
		goto error;
	}
	/* first is function name, then parameters */
	for(i=0; i<argc; i++) {
		if (!duk_is_string(J, i)) {
			LM_ERR("invalid parameter type (%d)\n", i);
			goto error;
		}
		jsdtv[i] = (char*)duk_to_string(J, i);
	}
	LM_ERR("request to execute cfg function '%s'\n", jsdtv[0]);
	/* pkg copy only parameters */
	for(i=1; i<MAX_ACTIONS; i++) {
		if(jsdtv[i]!=NULL) {
			argv[i] = (char*)pkg_malloc(strlen(jsdtv[i])+1);
			if(argv[i]==NULL) {
				PKG_MEM_ERROR;
				goto error;
			}
			strcpy(argv[i], jsdtv[i]);
		}
	}

	expf = find_export_record(jsdtv[0], argc-1, 0);
	if (expf==NULL) {
		LM_ERR("function '%s' is not available\n", jsdtv[0]);
		goto error;
	}
	/* check fixups */
	if (expf->fixup!=NULL && expf->free_fixup==NULL) {
		LM_ERR("function '%s' has fixup - cannot be used\n", jsdtv[0]);
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
					jsdtv[0], expf->param_no);
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
		LM_ERR("action structure could not be created for '%s'\n", jsdtv[0]);
		goto error;
	}

	/* handle fixups */
	if (expf->fixup) {
		if(argc==1) {
			/* no parameters */
			if(expf->fixup(0, 0)<0) {
				LM_ERR("Error in fixup (0) for '%s'\n", jsdtv[0]);
				goto error;
			}
		} else {
			for(i=1; i<argc; i++) {
				if(expf->fixup(&(act->val[i+1].u.data), i)<0) {
					LM_ERR("Error in fixup (%d) for '%s'\n", i, jsdtv[0]);
					goto error;
				}
				act->val[i+1].type = MODFIXUP_ST;
			}
		}
	}
	init_run_actions_ctx(&ra_ctx);
	ret = do_action(&ra_ctx, act, env_J->msg);

	/* free fixups */
	if (expf->fixup) {
		for(i=1; i<argc; i++) {
			if ((act->val[i+1].type == MODFIXUP_ST) && (act->val[i+1].u.data)) {
				expf->free_fixup(&(act->val[i+1].u.data), i);
			}
		}
	}
	pkg_free(act);
	for(i=0; i<MAX_ACTIONS; i++) {
		if(argv[i]!=NULL) pkg_free(argv[i]);
		argv[i] = 0;
	}
	duk_push_int(J, ret);
	return 1;

error:
	if(act!=NULL)
		pkg_free(act);
	for(i=0; i<MAX_ACTIONS; i++) {
		if(argv[i]!=NULL) pkg_free(argv[i]);
		argv[i] = 0;
	}
	duk_push_int(J, -1);
	return 1;
}


const duk_function_list_entry _sr_kemi_x_J_Map[] = {
	{ "exit", jsdt_sr_exit, 0 /* 0 args */ },
	{ "drop", jsdt_sr_drop, 0 /* 0 args */ },
	{ "modf", jsdt_sr_modf, DUK_VARARGS /* var args */ },
	{ NULL, NULL, 0 }
};

/**
 * load a JS file into context
 */
static int jsdt_load_file(duk_context *ctx, const char *filename)
{
	FILE *f;
	size_t len;
#define JSDT_SCRIPT_MAX_SIZE 128*1024
	char buf[JSDT_SCRIPT_MAX_SIZE];

	f = fopen(filename, "rb");
	if (f) {
		len = fread((void *) buf, 1, sizeof(buf), f);
		fclose(f);
		if(len>0) {
			duk_push_lstring(ctx, (const char *)buf, (duk_size_t)len);
		} else {
			LM_ERR("empty content\n");
			return -1;
		}
	} else {
		LM_ERR("cannot open file\n");
		return -1;
	}
	return 0;
}
/**
 *
 */
int jsdt_sr_init_mod(void)
{
	if(_sr_jsdt_reload_version == NULL) {
		_sr_jsdt_reload_version = (int*)shm_malloc(sizeof(int));
		if(_sr_jsdt_reload_version == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		*_sr_jsdt_reload_version = 0;
	}
	memset(&_sr_J_env, 0, sizeof(sr_jsdt_env_t));

	return 0;
}

/**
 *
 */
int jsdt_kemi_load_script(void)
{
	if(jsdt_load_file(_sr_J_env.JJ, _sr_jsdt_load_file.s)<0) {
		LM_ERR("failed to load js script file: %.*s\n",
				_sr_jsdt_load_file.len, _sr_jsdt_load_file.s);
		return -1;
	}
	if (duk_peval(_sr_J_env.JJ) != 0) {
		LM_ERR("failed running: %s\n", duk_safe_to_string(_sr_J_env.JJ, -1));
		duk_pop(_sr_J_env.JJ);  /* ignore result */
		return -1;
	}
	duk_pop(_sr_J_env.JJ);  /* ignore result */
	return 0;
}

/**
 *
 */
int jsdt_sr_init_child(int rank)
{
	memset(&_sr_J_env, 0, sizeof(sr_jsdt_env_t));
	_sr_J_env.J = duk_create_heap_default();
	if(_sr_J_env.J==NULL) {
		LM_ERR("cannot create JS context (exec)\n");
		return -1;
	}
	jsdt_sr_kemi_register_libs(_sr_J_env.J);
	if(_sr_jsdt_load_file.s != NULL && _sr_jsdt_load_file.len>0) {
		_sr_J_env.JJ = duk_create_heap_default();
		if(_sr_J_env.JJ==NULL) {
			LM_ERR("cannot create load JS context (load)\n");
			return -1;
		}
		if(_sr_jsdt_mode == 1) {
			duk_push_object(_sr_J_env.JJ);
			duk_push_c_function(_sr_J_env.JJ, cb_resolve_module, DUK_VARARGS);
			duk_put_prop_string(_sr_J_env.JJ, -2, "resolve");
			duk_push_c_function(_sr_J_env.JJ, cb_load_module, DUK_VARARGS);
			duk_put_prop_string(_sr_J_env.JJ, -2, "load");
			duk_module_node_init(_sr_J_env.JJ);
		}
		jsdt_sr_kemi_register_libs(_sr_J_env.JJ);
		LM_DBG("loading js script file: %.*s\n",
				_sr_jsdt_load_file.len, _sr_jsdt_load_file.s);
		if(jsdt_kemi_load_script()<0) {
			return -1;
		}
	}
	LM_DBG("JS initialized!\n");
	return 0;
}

/**
 *
 */
void jsdt_sr_destroy(void)
{
	if(_sr_J_env.J!=NULL) {
		duk_destroy_heap(_sr_J_env.J);
		_sr_J_env.J = NULL;
	}
	if(_sr_J_env.JJ!=NULL) {
		duk_destroy_heap(_sr_J_env.JJ);
		_sr_J_env.JJ = NULL;
	}
	memset(&_sr_J_env, 0, sizeof(sr_jsdt_env_t));
}

/**
 *
 */
int jsdt_kemi_reload_script(void)
{
	int v;
	if(_sr_jsdt_load_file.s == NULL && _sr_jsdt_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		return -1;
	}
	if(_sr_jsdt_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		return -1;
	}
	if(_sr_J_env.JJ==NULL) {
		LM_ERR("load JS context not created\n");
		return -1;
	}

	v = *_sr_jsdt_reload_version;
	if(v == _sr_jsdt_local_version) {
		/* same version */
		return 0;
	}
	LM_DBG("reloading js script file: %.*s (%d => %d)\n",
				_sr_jsdt_load_file.len, _sr_jsdt_load_file.s,
				_sr_jsdt_local_version, v);
	jsdt_kemi_load_script();
	_sr_jsdt_local_version = v;
	return 0;
}

/**
 *
 */
int app_jsdt_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3, int emode)
{
	int n;
	int ret;
	str txt;
	sip_msg_t *bmsg;
	duk_idx_t jtop;

	if(_sr_J_env.JJ==NULL) {
		LM_ERR("js loading state not initialized (call: %s)\n", func);
		return -1;
	}
	/* check the script version loaded */
	jsdt_kemi_reload_script();

	LM_DBG("executing js function: [[%s]]\n", func);
	jtop = duk_get_top(_sr_J_env.JJ);
	LM_DBG("js top index is: %d\n", (int)jtop);
	duk_get_global_string(_sr_J_env.JJ, func);
	if(!duk_is_function(_sr_J_env.JJ, -1))
	{
		if(emode) {
			LM_ERR("no such function [%s] in js scripts\n", func);
			LM_ERR("top stack type [%d]\n",
				duk_get_type(_sr_J_env.JJ, -1));
			txt.s = (char*)duk_to_string(_sr_J_env.JJ, -1);
			LM_ERR("error from JS: %s\n", (txt.s)?txt.s:"unknown");
			duk_set_top(_sr_J_env.JJ, jtop);
			return -1;
		} else {
			duk_set_top(_sr_J_env.JJ, jtop);
			return 1;
		}
	}
	n = 0;
	if(p1!=NULL)
	{
		duk_push_string(_sr_J_env.JJ, p1);
		n++;
		if(p2!=NULL)
		{
			duk_push_string(_sr_J_env.JJ, p2);
			n++;
			if(p3!=NULL)
			{
				duk_push_string(_sr_J_env.JJ, p3);
				n++;
			}
		}
	}
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	ret = duk_pcall(_sr_J_env.JJ, n);
	_sr_J_env.msg = bmsg;
	if(ret!=DUK_EXEC_SUCCESS)
	{
		n = 0;
		if (duk_is_error(_sr_J_env.JJ, -1)) {
			duk_get_prop_string(_sr_J_env.JJ, -1, "stack");
			LM_ERR("error stack from js: %s\n", duk_safe_to_string(_sr_J_env.JJ, -1));
			duk_pop(_sr_J_env.JJ);
		} else {
			txt.s = (char*)duk_safe_to_string(_sr_J_env.JJ, -1);
			if(txt.s!=NULL) {
				for(n=0; txt.s[n]!='\0' && _sr_kemi_jsdt_exit_string.s[n]!='\0';
						n++) {
					if(txt.s[n] != _sr_kemi_jsdt_exit_string.s[n])
						break;
				}
				if(txt.s[n]!='\0' || _sr_kemi_jsdt_exit_string.s[n]!='\0') {
					LM_ERR("error from js: %s\n", txt.s);
					n = 0;
				} else {
					LM_DBG("ksr error call from js: %s\n", txt.s);
					n = 1;
				}
			} else {
				LM_ERR("error from js: unknown\n");
			}
		}
		if(n==1) {
			duk_set_top(_sr_J_env.JJ, jtop);
			return 1;
		} else {
			LM_ERR("error executing: %s (err: %d)\n", func, ret);
			duk_set_top(_sr_J_env.JJ, jtop);
			return -1;
		}
	}
	duk_set_top(_sr_J_env.JJ, jtop);

	return 1;
}

/**
 *
 */
int app_jsdt_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return app_jsdt_run_ex(msg, func, p1, p2, p3, 1);
}

/**
 *
 */
int app_jsdt_runstring(sip_msg_t *msg, char *script)
{
	int ret;
	sip_msg_t *bmsg;

	if(_sr_J_env.JJ==NULL) {
		LM_ERR("js loading state not initialized (call: %s)\n", script);
		return -1;
	}

	jsdt_kemi_reload_script();

	LM_DBG("running js string: [[%s]]\n", script);
	LM_DBG("js top index is: %d\n", duk_get_top(_sr_J_env.JJ));
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	duk_push_string(_sr_J_env.JJ, script);
	ret = duk_peval(_sr_J_env.JJ);
	if(ret != 0) {
		LM_ERR("JS failed running: %s\n", duk_safe_to_string(_sr_J_env.JJ, -1));
	}
	duk_pop(_sr_J_env.JJ);  /* ignore result */

	_sr_J_env.msg = bmsg;
	return (ret==0)?1:-1;
}

/**
 *
 */
int app_jsdt_dostring(sip_msg_t *msg, char *script)
{
	int ret;
	sip_msg_t *bmsg;

	LM_DBG("executing js string: [[%s]]\n", script);
	LM_DBG("JS top index is: %d\n", duk_get_top(_sr_J_env.J));
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	duk_push_string(_sr_J_env.J, script);
	ret = duk_peval(_sr_J_env.J);
	if(ret != 0) {
		LM_ERR("JS failed running: %s\n", duk_safe_to_string(_sr_J_env.J, -1));
	}
	duk_pop(_sr_J_env.J);  /* ignore result */
	_sr_J_env.msg = bmsg;
	return (ret==0)?1:-1;
}

/**
 *
 */
int app_jsdt_dofile(sip_msg_t *msg, char *script)
{
	int ret;
	sip_msg_t *bmsg;

	LM_DBG("executing js file: [[%s]]\n", script);
	LM_DBG("JS top index is: %d\n", duk_get_top(_sr_J_env.J));
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	if(jsdt_load_file(_sr_J_env.J, script)<0) {
		LM_ERR("failed to load js script file: %s\n", script);
		return -1;
	}
	ret = duk_peval(_sr_J_env.J);
	if(ret != 0) {
		LM_ERR("JS failed running: %s\n", duk_safe_to_string(_sr_J_env.J, -1));
	}
	duk_pop(_sr_J_env.J);  /* ignore result */

	_sr_J_env.msg = bmsg;
	return (ret==0)?1:-1;
}

/**
 *
 */
int sr_kemi_jsdt_exec_func_ex(duk_context *J, sr_kemi_t *ket)
{
	int i;
	int argc;
	int ret;
	sr_kemi_xval_t *xret;
	str *fname;
	str *mname;
	sr_kemi_val_t vps[SR_KEMI_PARAMS_MAX];
	sr_jsdt_env_t *env_J;

	env_J = jsdt_sr_env_get();

	if(env_J==NULL || env_J->msg==NULL || ket==NULL) {
		LM_ERR("invalid JS environment attributes or parameters\n");
		return app_jsdt_return_false(J);
	}

	fname = &ket->fname;
	mname = &ket->mname;

	argc = duk_get_top(J);
	if(argc==0 && ket->ptypes[0]==SR_KEMIP_NONE) {
		if(ket->rtype==SR_KEMIP_XVAL) {
			xret = ((sr_kemi_xfm_f)(ket->func))(env_J->msg);
			return sr_kemi_jsdt_return_xval(J, ket, xret);
		} else {
			ret = ((sr_kemi_fm_f)(ket->func))(env_J->msg);
			return sr_kemi_jsdt_return_int(J, ket, ret);
		}
	}
	if(argc==0 && ket->ptypes[0]!=SR_KEMIP_NONE) {
		LM_ERR("invalid number of parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return app_jsdt_return_false(J);
	}

	if(argc>SR_KEMI_PARAMS_MAX) {
		LM_ERR("too many parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return app_jsdt_return_false(J);
	}

	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_val_t));
	for(i=0; i<SR_KEMI_PARAMS_MAX; i++) {
		if(ket->ptypes[i]==SR_KEMIP_NONE) {
			break;
		} else if(ket->ptypes[i]==SR_KEMIP_STR) {
			vps[i].s.s = (char*)duk_to_string(J, i);
			vps[i].s.len = strlen(vps[i].s.s);
			LM_DBG("param[%d] for: %.*s is str: %.*s\n", i,
				fname->len, fname->s, vps[i].s.len, vps[i].s.s);
		} else if(ket->ptypes[i]==SR_KEMIP_INT) {
			vps[i].n = duk_to_int(J, i);
			LM_DBG("param[%d] for: %.*s is int: %d\n", i,
				fname->len, fname->s, vps[i].n);
		} else {
			LM_ERR("unknown parameter type %d (%d)\n", ket->ptypes[i], i);
			return app_jsdt_return_false(J);
		}
	}

	switch(i) {
		case 1:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmn_f)(ket->func))(env_J->msg, vps[0].n);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmn_f)(ket->func))(env_J->msg, vps[0].n);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfms_f)(ket->func))(env_J->msg, &vps[0].s);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fms_f)(ket->func))(env_J->msg, &vps[0].s);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_jsdt_return_false(J);
			}
		break;
		case 2:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmnn_f)(ket->func))(env_J->msg, vps[0].n, vps[1].n);
						return sr_kemi_jsdt_return_xval(J, ket, xret);
					} else {
						ret = ((sr_kemi_fmnn_f)(ket->func))(env_J->msg, vps[0].n, vps[1].n);
						return sr_kemi_jsdt_return_int(J, ket, ret);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmns_f)(ket->func))(env_J->msg, vps[0].n, &vps[1].s);
						return sr_kemi_jsdt_return_xval(J, ket, xret);
					} else {
						ret = ((sr_kemi_fmns_f)(ket->func))(env_J->msg, vps[0].n, &vps[1].s);
						return sr_kemi_jsdt_return_int(J, ket, ret);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_jsdt_return_false(J);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmsn_f)(ket->func))(env_J->msg, &vps[0].s, vps[1].n);
						return sr_kemi_jsdt_return_xval(J, ket, xret);
					} else {
						ret = ((sr_kemi_fmsn_f)(ket->func))(env_J->msg, &vps[0].s, vps[1].n);
						return sr_kemi_jsdt_return_int(J, ket, ret);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmss_f)(ket->func))(env_J->msg, &vps[0].s, &vps[1].s);
						return sr_kemi_jsdt_return_xval(J, ket, xret);
					} else {
						ret = ((sr_kemi_fmss_f)(ket->func))(env_J->msg, &vps[0].s, &vps[1].s);
						return sr_kemi_jsdt_return_int(J, ket, ret);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_jsdt_return_false(J);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_jsdt_return_false(J);
			}
		break;
		case 3:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmsss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmsss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmssn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmssn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmsns_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmsns_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmsnn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmsnn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnss_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmnss_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnsn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmnsn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnns_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmnns_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmnnn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n);
					return sr_kemi_jsdt_return_xval(J, ket, xret);
				} else {
					ret = ((sr_kemi_fmnnn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n);
					return sr_kemi_jsdt_return_int(J, ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname->len, fname->s);
				return app_jsdt_return_false(J);
			}
		break;
		case 4:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsssn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssns_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnss_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnsn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnns_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsss_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnssn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsns_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnss_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnsn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnns_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname->len, fname->s);
				return app_jsdt_return_false(J);
			}
		break;
		case 5:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssssn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssns_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsssnn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssnss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnsn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssnns_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnnn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnsss_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnssn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnsns_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnsnn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnnss_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnsn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnnns_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnnn_f)(ket->func))(env_J->msg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnssss_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsssn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnssns_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnssnn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsnss_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnsn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsnns_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnnn_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnsss_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnssn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnsns_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnsnn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnnss_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnsn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnnns_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnnn_f)(ket->func))(env_J->msg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname->len, fname->s);
				return app_jsdt_return_false(J);
			}
		break;
		case 6:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR
					&& ket->ptypes[5]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssssss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s, &vps[5].s);
				return sr_kemi_jsdt_return_int(J, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_jsdt_return_false(J);
			}
		break;
		default:
			LM_ERR("invalid parameters for: %.*s\n",
					fname->len, fname->s);
			return app_jsdt_return_false(J);
	}
}

/**
 *
 */
int sr_kemi_jsdt_exec_func(duk_context *J, int eidx)
{
	sr_kemi_t *ket;
	int ret;
	struct timeval tvb ={0}, tve = {0};
	struct timezone tz;
	unsigned int tdiff;
	int sline = 0;

	ket = sr_kemi_jsdt_export_get(eidx);
	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tvb, &tz);
	}

	ret = sr_kemi_jsdt_exec_func_ex(J, ket);

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tve, &tz);
		tdiff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
		if(tdiff >= cfg_get(core, core_cfg, latency_limit_action)) {
			duk_inspect_callstack_entry(J, -1);
			duk_get_prop_string(J, -1, "lineNumber");
			sline = (int)duk_to_int(J, -1);
			duk_pop_2(J);
			LOG(cfg_get(core, core_cfg, latency_log),
					"alert - action KSR.%s%s%s(...)"
					" took too long [%u us] (line: %d)\n",
					(ket->mname.len>0)?ket->mname.s:"",
					(ket->mname.len>0)?".":"", ket->fname.s, tdiff, sline);
		}
	}

	return ret;
}

/**
 *
 */
duk_function_list_entry *_sr_J_KSRMethods = NULL;
#define SR_JSDT_KSR_MODULES_SIZE	256
#define SR_JSDT_KSR_METHODS_SIZE	(SR_KEMI_JSDT_EXPORT_SIZE + SR_JSDT_KSR_MODULES_SIZE)


/**
 *
 */
duk_ret_t dukopen_KSR(duk_context *J)
{
	duk_function_list_entry *_sr_crt_J_KSRMethods = NULL;
	sr_kemi_module_t *emods = NULL;
	int emods_size = 0;
	int i;
	int k;
	int n;
	char mname[128];
	char malias[256];

	_sr_J_KSRMethods = malloc(SR_JSDT_KSR_METHODS_SIZE * sizeof(duk_function_list_entry));
	if(_sr_J_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return 0;
	}
	memset(_sr_J_KSRMethods, 0, SR_JSDT_KSR_METHODS_SIZE * sizeof(duk_function_list_entry));

	emods_size = sr_kemi_modules_size_get();
	emods = sr_kemi_modules_get();

	n = 0;
	_sr_crt_J_KSRMethods = _sr_J_KSRMethods;
	if(emods_size==0 || emods[0].kexp==NULL) {
		LM_ERR("no kemi exports registered\n");
		return 0;
	}

	for(i=0; emods[0].kexp[i].func!=NULL; i++) {
		LM_DBG("exporting KSR.%s(...)\n", emods[0].kexp[i].fname.s);
		_sr_crt_J_KSRMethods[i].key = emods[0].kexp[i].fname.s;
		_sr_crt_J_KSRMethods[i].value =
			sr_kemi_jsdt_export_associate(&emods[0].kexp[i]);
		if(_sr_crt_J_KSRMethods[i].value == NULL) {
			LM_ERR("failed to associate kemi function with js export\n");
			free(_sr_J_KSRMethods);
			_sr_J_KSRMethods = NULL;
			return 0;
		}
		_sr_crt_J_KSRMethods[i].nargs = DUK_VARARGS;
		n++;
	}

	duk_push_global_object(J);
	duk_push_object(J);  /* -> [ ... global obj ] */
	duk_put_function_list(J, -1, _sr_crt_J_KSRMethods);
	duk_put_prop_string(J, -2, "KSR");  /* -> [ ... global ] */
	duk_pop(J);

	duk_push_global_object(J);
	duk_push_object(J);  /* -> [ ... global obj ] */
	duk_put_function_list(J, -1, _sr_kemi_x_J_Map);
	duk_put_prop_string(J, -2, "KSR_x");  /* -> [ ... global ] */
	duk_pop(J);
	duk_eval_string_noresult(J, "KSR.x = KSR_x;");

	/* registered kemi modules */
	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
			n++;
			_sr_crt_J_KSRMethods = _sr_J_KSRMethods + n;
			snprintf(mname, 128, "KSR_%s", emods[k].kexp[0].mname.s);
			for(i=0; emods[k].kexp[i].func!=NULL; i++) {
				LM_DBG("exporting %s.%s(...)\n", mname,
						emods[k].kexp[i].fname.s);
				_sr_crt_J_KSRMethods[i].key = emods[k].kexp[i].fname.s;
				_sr_crt_J_KSRMethods[i].value =
					sr_kemi_jsdt_export_associate(&emods[k].kexp[i]);
				if(_sr_crt_J_KSRMethods[i].value == NULL) {
					LM_ERR("failed to associate kemi function with func export\n");
					free(_sr_J_KSRMethods);
					_sr_J_KSRMethods = NULL;
					return 0;
				}
				_sr_crt_J_KSRMethods[i].nargs = DUK_VARARGS;
				n++;
			}

			duk_push_global_object(J);
			duk_push_object(J);  /* -> [ ... global obj ] */
			duk_put_function_list(J, -1, _sr_crt_J_KSRMethods);
			duk_put_prop_string(J, -2, mname);  /* -> [ ... global ] */
			duk_pop(J);
			snprintf(malias, 256, "KSR.%s = KSR_%s;", emods[k].kexp[0].mname.s,
					emods[k].kexp[0].mname.s);
			duk_eval_string_noresult(J, malias);

			LM_DBG("initializing kemi sub-module: %s (%s)\n", mname,
					emods[k].kexp[0].mname.s);
		}
	}
	LM_DBG("module 'KSR' has been initialized\n");
	return 1;
}

/**
 *
 */
void jsdt_sr_kemi_register_libs(duk_context *J)
{
	int ret;

	duk_push_c_function(J, dukopen_KSR, 0 /*nargs*/);
	ret = duk_pcall(J, 0);
	if(ret!=DUK_EXEC_SUCCESS) {
		LM_ERR("failed to initialize KSR module\n");
	}
}

static const char* app_jsdt_rpc_reload_doc[2] = {
	"Reload javascript file",
	0
};


static void app_jsdt_rpc_reload(rpc_t* rpc, void* ctx)
{
	int v;
	void *vh;

	if(_sr_jsdt_load_file.s == NULL && _sr_jsdt_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		rpc->fault(ctx, 500, "No script file");
		return;
	}
	if(_sr_jsdt_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		rpc->fault(ctx, 500, "Reload not enabled");
		return;
	}

	v = *_sr_jsdt_reload_version;
	LM_INFO("marking for reload js script file: %.*s (%d => %d)\n",
				_sr_jsdt_load_file.len, _sr_jsdt_load_file.s,
				_sr_jsdt_local_version, v);
	*_sr_jsdt_reload_version += 1;

	if (rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "dd",
			"old", v,
			"new", *_sr_jsdt_reload_version);
}

static const char* app_jsdt_rpc_api_list_doc[2] = {
	"List kemi exports to javascript",
	0
};

static void app_jsdt_rpc_api_list(rpc_t* rpc, void* ctx)
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
	n = 0;
	for(i=0; i<SR_KEMI_JSDT_EXPORT_SIZE; i++) {
		ket = sr_kemi_jsdt_export_get(i);
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
	for(i=0; i<SR_KEMI_JSDT_EXPORT_SIZE; i++) {
		ket = sr_kemi_jsdt_export_get(i);
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

rpc_export_t app_jsdt_rpc_cmds[] = {
	{"app_jsdt.reload", app_jsdt_rpc_reload,
		app_jsdt_rpc_reload_doc, 0},
	{"app_jsdt.api_list", app_jsdt_rpc_api_list,
		app_jsdt_rpc_api_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int app_jsdt_init_rpc(void)
{
	if (rpc_register_array(app_jsdt_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
*  Duktape - duk_module_node - resolve
*/
duk_ret_t cb_resolve_module(duk_context *JJ) {
	const char *requested_id = duk_get_string(JJ, 0);
	const char *parent_id = duk_get_string(JJ, 1);

	char requested_path[PATH_MAX];
	if (requested_id[0] == '/') {
		// absolute
		strcpy(requested_path, requested_id);
	} else if (strncmp(requested_id, "./", 2)
			  || strncmp(requested_id, "../", 3)) {
		if (strlen(parent_id)) {
			// relative to parent
			strcpy(requested_path, parent_id);
		} else {
			// no parent so relative to jsdt_load_file
			strcpy(requested_path, _sr_jsdt_load_file.s);
		}
		char *ptr = strrchr(requested_path, '/');
		if (ptr) {
			ptr++;
			*ptr = '\0';
		}
		strcat(requested_path, requested_id);
	} else {
		LM_INFO("cb_resolve_module - TODO resolve pathless module names");
		goto error;
	}
	// if missing add .js ext
	if (strcmp(strrchr(requested_path, '\0') - 3, ".js")){
		strcat(requested_path, ".js");
	}
	char resolved_id[PATH_MAX];
	if (realpath(requested_path, resolved_id)) {
		duk_push_string(JJ, resolved_id);
		return 1;  /*nrets*/
	} else {
		goto error;
	}

error:
	return duk_generic_error(JJ, "Could not resolve module '%s'", requested_id);
}

/**
*  Duktape - duk_module_node - node
*/
duk_ret_t cb_load_module(duk_context *JJ) {
	const char *resolved_id = duk_get_string(JJ, 0);
	if (0 > jsdt_load_file(JJ, resolved_id)) {
		return duk_generic_error(JJ, "Could not load module '%s'", resolved_id);
	}
	return 1;  /*nrets*/
}
