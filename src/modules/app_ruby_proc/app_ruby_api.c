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
#include <ctype.h>

#include "../../core/dprint.h"
#include "../../core/pvar.h"
#include "../../core/sr_module.h"
#include "../../core/mem/shm.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "app_ruby_api.h"
#include "app_ruby_kemi_export.h"

/* ruby.h defines xmalloc macro, replacing the shm.xmalloc field name */
#undef xmalloc
#undef xfree

int _app_ruby_proc_xval_mode = 0;

int app_ruby_kemi_export_libs(void);

typedef struct _sr_ruby_env
{
	ksr_ruby_context_t *R;
	sip_msg_t *msg;
	int rinit;
	unsigned int flags;
	unsigned int nload; /* number of scripts loaded */
} sr_ruby_env_t;

typedef struct ksr_ruby_data {
	VALUE robj;
	ID metid;
	int nargs;
	VALUE vargs[4];
} ksr_ruby_data_t;

static sr_ruby_env_t _sr_R_env = {0};

static str _app_ruby_proc_load_file = STR_NULL;

static int *_app_ruby_proc_reload_version = NULL;
static int _app_ruby_proc_local_version = 0;

/**
 *
 */
sr_ruby_env_t *app_ruby_sr_env_get(void)
{
	return &_sr_R_env;
}


static void ksr_ruby_error(int error)
{
	VALUE lasterr;
	VALUE inclass;
	VALUE message;
	VALUE ary;
	long c;

	if (error == 0)
		return;

	lasterr = rb_gv_get("$!"); /* NOTRANSLATE */
	inclass = rb_class_path(CLASS_OF(lasterr));
	message = rb_obj_as_string(lasterr);
	LM_ERR("error ruby script: class=%s, message=%s\n",
			RSTRING_PTR(inclass), RSTRING_PTR(message));

	if (!NIL_P(rb_errinfo())) {
		ary = rb_funcall(rb_errinfo(), rb_intern("backtrace"), 0);
		for (c=0; c<RARRAY_LEN(ary); ++c) {
			LM_ERR("backtrace from %s\n",
					RSTRING_PTR(RARRAY_PTR(ary)[c]));
		}
	}
}

static int app_ruby_print_last_exception()
{
	VALUE rException, rExceptStr;

	rException = rb_errinfo();         /* get last exception */
	rb_set_errinfo(Qnil);              /* clear last exception */
	rExceptStr = rb_funcall(rException, rb_intern("to_s"), 0, Qnil);
	if(RSTRING_LEN(rExceptStr)!=4
			|| strncmp(RSTRING_PTR(rExceptStr), "exit", 4)!=0) {
		LM_ERR("exception: %.*s\n", (int)RSTRING_LEN(rExceptStr),
				RSTRING_PTR(rExceptStr));
		return 0;
	}
	return 1;
}

/**
 *
 */
int app_ruby_kemi_load_script(void)
{
	int state = 0;
	VALUE script;

	script  = rb_str_new_cstr(_app_ruby_proc_load_file.s);

	/* handle exceptions like rb_eval_string_protect() */
	rb_load_protect(script, 0, &state);

	if (state) {
		/* got exception */
		//app_ruby_print_last_exception();
		ksr_ruby_error(state);
		LM_ERR("failed to load rb script file: %.*s (%d)\n",
				_app_ruby_proc_load_file.len, _app_ruby_proc_load_file.s, state);
		// return -1;
	}
	LM_DBG("rb script loaded: %s\n", _app_ruby_proc_load_file.s);

	return 0;
}

/**
 *
 */
int app_ruby_kemi_reload_script(void)
{
	int v;
	if(_app_ruby_proc_load_file.s == NULL && _app_ruby_proc_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		return -1;
	}
	if(_app_ruby_proc_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		return -1;
	}
	if(_sr_R_env.rinit == 0) {
		LM_ERR("load ruby context not created\n");
		return -1;
	}

	v = *_app_ruby_proc_reload_version;
	if(v == _app_ruby_proc_local_version) {
		/* same version */
		return 0;
	}
	LM_DBG("reloading ruby script file: %.*s (%d => %d)\n",
				_app_ruby_proc_load_file.len, _app_ruby_proc_load_file.s,
				_app_ruby_proc_local_version, v);
	app_ruby_kemi_load_script();
	_app_ruby_proc_local_version = v;
	return 0;
}

/**
 *
 */
int app_ruby_proc_init_child(void)
{
	int state = 0;
	VALUE rbres;

	/* construct the VM */
	ruby_init();
	ruby_init_loadpath();
	ruby_script(_app_ruby_proc_load_file.s);

	/* Ruby goes here */
	rbres = rb_eval_string_protect("puts 'Hello " NAME "!'", &state);

	if (state) {
		/* handle exception */
		// app_ruby_print_last_exception();
		ksr_ruby_error(state);
		LM_ERR("test execution with error (res type: %d)\n", TYPE(rbres));
		return -1;
	} else {
		LM_DBG("test execution without error\n");
	}

	if(app_ruby_kemi_export_libs()<0) {
		return -1;
	}

	if(app_ruby_kemi_load_script()<0) {
		return -1;
	}

	_sr_R_env.rinit = 1;

	return 0;
}

/**
 *
 */
void app_ruby_proc_mod_destroy(void)
{
	if(_sr_R_env.rinit == 1) {
		return;
	}
	memset(&_sr_R_env, 0, sizeof(sr_ruby_env_t));
	/* destruct the VM */
	ruby_cleanup(0);
	return;
}

/**
 *
 */
int app_ruby_proc_initialized(void)
{
	if(_sr_R_env.rinit==1) {
		return 1;
	}
	return 0;
}

/**
 *
 */
int sr_kemi_ruby_return_int(sr_kemi_t *ket, int rc)
{
	if(ket->rtype==SR_KEMIP_INT || ket->rtype==SR_KEMIP_XVAL) {
		return INT2NUM(rc);
	}
	if(ket->rtype==SR_KEMIP_BOOL && rc!=SR_KEMI_FALSE) {
		return Qtrue;
	}
	return Qfalse;
}

/**
 *
 */
static VALUE sr_kemi_ruby_return_none(int rmode)
{
	if(rmode==1) {
		return rb_str_new_cstr("<<null>>");
	} else if(rmode==2) {
		return rb_str_new_cstr("");
	}
	return Qnil;
}

/**
 *
 */
static VALUE app_ruby_pv_get_mode(int argc, VALUE* argv, VALUE self, int rmode)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_ruby_env_t *env_R;
	int pl;

	env_R = app_ruby_sr_env_get();

	if(env_R==NULL || env_R->msg==NULL || argc!=1) {
		LM_ERR("invalid ruby environment attributes or parameters\n");
		return sr_kemi_ruby_return_none(rmode);
	}

	if(!RB_TYPE_P(argv[0], T_STRING)) {
		LM_ERR("invalid parameter type\n");
		return sr_kemi_ruby_return_none(rmode);
	}

	pvn.s = StringValuePtr(argv[0]);
	if(pvn.s==NULL)
		return sr_kemi_ruby_return_none(rmode);
	pvn.len = strlen(pvn.s);

	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return sr_kemi_ruby_return_none(rmode);
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return sr_kemi_ruby_return_none(rmode);
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_R->msg, pvs, &val) != 0) {
		LM_ERR("unable to get pv value for [%s]\n", pvn.s);
		return sr_kemi_ruby_return_none(rmode);
	}
	if(val.flags&PV_VAL_NULL) {
		return sr_kemi_ruby_return_none(rmode);
	}
	if(val.flags&PV_TYPE_INT) {
		return INT2NUM(val.ri);
	}
	return rb_str_new(val.rs.s, val.rs.len);
}

/**
 *
 */
static VALUE app_ruby_pv_get(int argc, VALUE* argv, VALUE self)
{
	return app_ruby_pv_get_mode(argc, argv, self, 0);
}

/**
 *
 */
static VALUE app_ruby_pv_getw(int argc, VALUE* argv, VALUE self)
{
	return app_ruby_pv_get_mode(argc, argv, self, 1);
}

/**
 *
 */
static VALUE app_ruby_pv_gete(int argc, VALUE* argv, VALUE self)
{
	return app_ruby_pv_get_mode(argc, argv, self, 2);
}

/**
 *
 */
static VALUE app_ruby_pv_seti(int argc, VALUE* argv, VALUE self)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_ruby_env_t *env_R;
	int pl;

	env_R = app_ruby_sr_env_get();

	if(env_R==NULL || env_R->msg==NULL || argc!=2) {
		LM_ERR("invalid ruby environment attributes or parameters\n");
		return Qfalse;
	}

	if(!RB_TYPE_P(argv[0], T_STRING)) {
		LM_ERR("invalid pv name parameter type\n");
		return Qfalse;
	}

	if(!RB_TYPE_P(argv[1], T_FIXNUM)) {
		LM_ERR("invalid pv val parameter type\n");
		return Qfalse;
	}

	pvn.s = StringValuePtr(argv[0]);
	if(pvn.s==NULL)
		return Qfalse;
	pvn.len = strlen(pvn.s);

	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return Qfalse;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return Qfalse;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.ri = NUM2INT(argv[1]);
	val.flags |= PV_TYPE_INT|PV_VAL_INT;

	if(pv_set_spec_value(env_R->msg, pvs, 0, &val)<0) {
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return Qfalse;
	}

	return Qtrue;
}

/**
 *
 */
static VALUE app_ruby_pv_sets(int argc, VALUE* argv, VALUE self)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_ruby_env_t *env_R;
	int pl;

	env_R = app_ruby_sr_env_get();

	if(env_R==NULL || env_R->msg==NULL || argc!=2) {
		LM_ERR("invalid ruby environment attributes or parameters\n");
		return Qfalse;
	}

	if(!RB_TYPE_P(argv[0], T_STRING)) {
		LM_ERR("invalid pv name parameter type\n");
		return Qfalse;
	}

	if(!RB_TYPE_P(argv[1], T_STRING)) {
		LM_ERR("invalid pv val parameter type\n");
		return Qfalse;
	}

	pvn.s = StringValuePtr(argv[0]);
	if(pvn.s==NULL)
		return Qfalse;
	pvn.len = strlen(pvn.s);

	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return Qfalse;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return Qfalse;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.rs.s = StringValuePtr(argv[1]);
	if(val.rs.s==NULL) {
		LM_ERR("invalid str value\n");
		return Qfalse;
	}
	val.rs.len = strlen(val.rs.s);
	val.flags |= PV_VAL_STR;

	if(pv_set_spec_value(env_R->msg, pvs, 0, &val)<0) {
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return Qfalse;
	}

	return Qtrue;
}

/**
 *
 */
static VALUE app_ruby_pv_unset(int argc, VALUE* argv, VALUE self)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_ruby_env_t *env_R;
	int pl;

	env_R = app_ruby_sr_env_get();

	if(env_R==NULL || env_R->msg==NULL || argc!=1) {
		LM_ERR("invalid ruby environment attributes or parameters\n");
		return Qfalse;
	}

	if(!RB_TYPE_P(argv[0], T_STRING)) {
		LM_ERR("invalid parameter type\n");
		return Qfalse;
	}

	pvn.s = StringValuePtr(argv[0]);
	if(pvn.s==NULL)
		return Qfalse;
	pvn.len = strlen(pvn.s);

	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return Qfalse;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return Qfalse;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_NULL;
	if(pv_set_spec_value(env_R->msg, pvs, 0, &val)<0)
	{
		LM_ERR("unable to unset pv [%s]\n", pvn.s);
		return Qfalse;
	}

	return Qtrue;
}

/**
 *
 */
static VALUE app_ruby_pv_is_null(int argc, VALUE* argv, VALUE self)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_ruby_env_t *env_R;
	int pl;

	env_R = app_ruby_sr_env_get();

	if(env_R==NULL || env_R->msg==NULL || argc!=1) {
		LM_ERR("invalid ruby environment attributes or parameters\n");
		return Qfalse;
	}

	if(!RB_TYPE_P(argv[0], T_STRING)) {
		LM_ERR("invalid parameter type\n");
		return Qfalse;
	}

	pvn.s = StringValuePtr(argv[0]);
	if(pvn.s==NULL)
		return Qfalse;
	pvn.len = strlen(pvn.s);

	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return Qfalse;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return Qfalse;
	}

	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_R->msg, pvs, &val) != 0) {
		LM_NOTICE("unable to get pv value for [%s]\n", pvn.s);
		return Qtrue;
	}
	if(val.flags&PV_VAL_NULL) {
		return Qtrue;
	} else {
		pv_value_destroy(&val);
		return Qfalse;
	}
}

/**
 *
 */
static ksr_ruby_export_t _sr_kemi_pv_R_Map[] = {
	{"PV", "get", app_ruby_pv_get},
	{"PV", "getw", app_ruby_pv_getw},
	{"PV", "gete", app_ruby_pv_gete},
	{"PV", "seti", app_ruby_pv_seti},
	{"PV", "sets", app_ruby_pv_sets},
	{"PV", "unset", app_ruby_pv_unset},
	{"PV", "is_null", app_ruby_pv_is_null},
	{0, 0, 0}
};

/**
 *
 */
static VALUE app_ruby_sr_modf(int argc, VALUE* argv, VALUE self)
{
	int ret;
	char *rbv[MAX_ACTIONS];
	char *paramv[MAX_ACTIONS];
	int i;
	int mod_type;
	struct run_act_ctx ra_ctx;
	struct action *act;
	ksr_cmd_export_t* expf;
	sr_ruby_env_t *env_R;

	ret = 1;
	act = NULL;
	memset(rbv, 0, MAX_ACTIONS*sizeof(char*));
	memset(paramv, 0, MAX_ACTIONS*sizeof(char*));
	env_R = app_ruby_sr_env_get();
	if(env_R->msg==NULL)
		goto error;

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
		if(!RB_TYPE_P(argv[i], T_STRING)) {
			LM_ERR("invalid parameter type (%d)\n", i);
			return INT2NUM(-1);
		}
		rbv[i] = (char*)StringValuePtr(argv[i]);
	}
	LM_ERR("request to execute cfg function '%s'\n", rbv[0]);
	/* pkg copy only parameters */
	for(i=1; i<MAX_ACTIONS; i++) {
		if(rbv[i]!=NULL) {
			paramv[i] = (char*)pkg_malloc(strlen(rbv[i])+1);
			if(paramv[i]==NULL) {
				LM_ERR("no more pkg\n");
				goto error;
			}
			strcpy(paramv[i], rbv[i]);
		}
	}

	expf = find_export_record(rbv[0], argc-1, 0);
	if (expf==NULL) {
		LM_ERR("function '%s' is not available\n", rbv[0]);
		goto error;
	}
	/* check fixups */
	if (expf->fixup!=NULL && expf->free_fixup==NULL) {
		LM_ERR("function '%s' has fixup - cannot be used\n", rbv[0]);
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
					rbv[0], expf->param_no);
			goto error;
	}

	act = mk_action(mod_type,  argc+1   /* number of (type, value) pairs */,
					MODEXP_ST, expf,    /* function */
					NUMBER_ST, argc-1,  /* parameter number */
					STRING_ST, paramv[1], /* param. 1 */
					STRING_ST, paramv[2], /* param. 2 */
					STRING_ST, paramv[3], /* param. 3 */
					STRING_ST, paramv[4], /* param. 4 */
					STRING_ST, paramv[5], /* param. 5 */
					STRING_ST, paramv[6]  /* param. 6 */
			);

	if (act==NULL) {
		LM_ERR("action structure could not be created for '%s'\n", rbv[0]);
		goto error;
	}

	/* handle fixups */
	if (expf->fixup) {
		if(argc==1) {
			/* no parameters */
			if(expf->fixup(0, 0)<0) {
				LM_ERR("Error in fixup (0) for '%s'\n", rbv[0]);
				goto error;
			}
		} else {
			for(i=1; i<argc; i++) {
				if(expf->fixup(&(act->val[i+1].u.data), i)<0) {
					LM_ERR("Error in fixup (%d) for '%s'\n", i, rbv[0]);
					goto error;
				}
				act->val[i+1].type = MODFIXUP_ST;
			}
		}
	}
	init_run_actions_ctx(&ra_ctx);
	ret = do_action(&ra_ctx, act, env_R->msg);

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
		if(paramv[i]!=NULL) pkg_free(paramv[i]);
		paramv[i] = 0;
	}
	return INT2NUM(ret);

error:
	if(act!=NULL)
		pkg_free(act);
	for(i=0; i<MAX_ACTIONS; i++) {
		if(paramv[i]!=NULL) pkg_free(paramv[i]);
		paramv[i] = 0;
	}
	return INT2NUM(-1);
}

/**
 *
 */
static ksr_ruby_export_t _sr_kemi_x_R_Map[] = {
	{"X", "modf", app_ruby_sr_modf},
	{0, 0, 0}
};

/**
 *
 */
static VALUE ksr_ruby_exec_callback(VALUE ptr)
{
	ksr_ruby_data_t *data = (ksr_ruby_data_t *)ptr;
	return rb_funcall2(data->robj, data->metid, data->nargs, data->vargs);
}

/**
 *
 */
VALUE sr_kemi_ruby_return_xval(sr_kemi_t *ket, sr_kemi_xval_t *rx)
{
	switch(rx->vtype) {
		case SR_KEMIP_NONE:
			return Qnil;
		case SR_KEMIP_INT:
			return INT2NUM(rx->v.n);
		case SR_KEMIP_LONG:
			return LONG2NUM(rx->v.l);
		case SR_KEMIP_STR:
			if(_app_ruby_proc_xval_mode==0) {
				LM_ERR("attempt to return xval str - support disabled - returning null\n");
				return Qnil;
			} else {
				return rb_str_new(rx->v.s.s, rx->v.s.len);
			}
		case SR_KEMIP_BOOL:
			if(rx->v.n!=SR_KEMI_FALSE) {
				return Qtrue;
			} else {
				return Qfalse;
			}
		case SR_KEMIP_ARRAY:
			LM_ERR("unsupported return type: array\n");
			sr_kemi_xval_free(rx);
			return Qnil;
		case SR_KEMIP_DICT:
			LM_ERR("unsupported return type: map\n");
			sr_kemi_xval_free(rx);
			return Qnil;
		case SR_KEMIP_XVAL:
			/* unknown content - return false */
			return Qfalse;
		case SR_KEMIP_NULL:
			return Qnil;
		default:
			/* unknown type - return false */
			return Qfalse;
	}
}

/**
 *
 */
VALUE sr_kemi_ruby_exec_func_ex(ksr_ruby_context_t *R, sr_kemi_t *ket, int argc,
		VALUE* argv, VALUE self)
{
	sr_kemi_xval_t vps[SR_KEMI_PARAMS_MAX];
	sr_ruby_env_t *env_R;
	str *fname;
	str *mname;
	int i;
	int ret = -1;
	sr_kemi_xval_t *xret;

	env_R = app_ruby_sr_env_get();
	if(env_R==NULL || env_R->msg==NULL || ket==NULL) {
		LM_ERR("invalid ruby environment attributes or parameters (%p/%p/%p)\n",
				env_R, env_R->msg, ket);
		return Qfalse;
	}

	if(argc==0 && ket->ptypes[0]==SR_KEMIP_NONE) {
		if(ket->rtype==SR_KEMIP_XVAL) {
			xret = ((sr_kemi_xfm_f)(ket->func))(env_R->msg);
			return sr_kemi_ruby_return_xval(ket, xret);
		} else {
			ret = ((sr_kemi_fm_f)(ket->func))(env_R->msg);
			return sr_kemi_ruby_return_int(ket, ret);
		}
	}
	fname = &ket->fname;
	mname = &ket->mname;
	if(argc==0 && ket->ptypes[0]!=SR_KEMIP_NONE) {
		LM_ERR("invalid number of parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return Qfalse;
	}

	if(argc>SR_KEMI_PARAMS_MAX) {
		LM_ERR("too many parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return Qfalse;
	}

	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_val_t));
	for(i=0; i<SR_KEMI_PARAMS_MAX; i++) {
		if(ket->ptypes[i]==SR_KEMIP_NONE) {
			break;
		} else if(ket->ptypes[i]==SR_KEMIP_STR) {
			if(!RB_TYPE_P(argv[i], T_STRING)) {
				LM_ERR("invalid str parameter type %d (%d)\n", ket->ptypes[i], i);
				return Qfalse;
			}
			vps[i].vtype = SR_KEMIP_STR;
			vps[i].v.s.s = StringValuePtr(argv[i]);
			vps[i].v.s.len = strlen(vps[i].v.s.s);
			LM_DBG("param[%d] for: %.*s.%.*s is str: %.*s\n", i,
				mname->len, mname->s, fname->len, fname->s, vps[i].v.s.len, vps[i].v.s.s);
		} else if(ket->ptypes[i]==SR_KEMIP_INT) {
			if(!RB_TYPE_P(argv[i], T_FIXNUM)) {
				LM_ERR("invalid int parameter type %d (%d)\n", ket->ptypes[i], i);
				return Qfalse;
			}
			vps[i].vtype = SR_KEMIP_INT;
			vps[i].v.n = NUM2INT(argv[i]);
			LM_DBG("param[%d] for: %.*s.%.*s is int: %d\n", i,
				mname->len, mname->s, fname->len, fname->s, vps[i].v.n);
		} else if(ket->ptypes[i]==SR_KEMIP_LONG) {
			if(!RB_TYPE_P(argv[i], T_FIXNUM)) {
				LM_ERR("invalid int parameter type %d (%d)\n", ket->ptypes[i], i);
				return Qfalse;
			}
			vps[i].vtype = SR_KEMIP_LONG;
			vps[i].v.l = NUM2LONG(argv[i]);
			LM_DBG("param[%d] for: %.*s.%.*s is long int: %ld\n", i,
				mname->len, mname->s, fname->len, fname->s, vps[i].v.l);
		} else {
			LM_ERR("unknown parameter type %d (%d)\n", ket->ptypes[i], i);
			return Qfalse;
		}
	}

	xret = sr_kemi_exec_func(ket, env_R->msg, i, vps);
	return sr_kemi_ruby_return_xval(ket, xret);
}

/**
 *
 */
VALUE sr_kemi_ruby_exec_func(ksr_ruby_context_t *R, int eidx, int argc,
		VALUE* argv, VALUE self)
{
	sr_kemi_t *ket;
	int ret;
	struct timeval tvb = {0}, tve = {0};
	struct timezone tz;
	unsigned int tdiff;

	ket = sr_kemi_ruby_export_get(eidx);

	LM_DBG("executing %p eidx %d\n", ket, eidx);

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tvb, &tz);
	}

	ret = sr_kemi_ruby_exec_func_ex(R, ket, argc, argv, self);

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tve, &tz);
		tdiff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
		if(tdiff >= cfg_get(core, core_cfg, latency_limit_action)) {
			LOG(cfg_get(core, core_cfg, latency_log),
						"alert - action KSR.%s%s%s(...)"
						" took too long [%u us]\n",
						(ket->mname.len>0)?ket->mname.s:"",
						(ket->mname.len>0)?".":"", ket->fname.s,
						tdiff);
		}
	}

	return ret;
}

/**
 *
 */
int app_ruby_proc_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3, int emode)
{
	sip_msg_t *bmsg;
	ksr_ruby_data_t rbdata;
    int rberr = 0;
    VALUE rbres;

	if(_sr_R_env.rinit==0) {
		LM_ERR("js loading state not initialized (call: %s)\n", func);
		return -1;
	}
	/* check the script version loaded */
	app_ruby_kemi_reload_script();

	memset(&rbdata, 0, sizeof(ksr_ruby_data_t));
	rbdata.robj = rb_mKernel;
	rbdata.metid = rb_intern(func);

	LM_DBG("executing ruby function: [[%s]]\n", func);
	bmsg = _sr_R_env.msg;
	_sr_R_env.msg = msg;
	if(p1!=NULL) {
		rbdata.vargs[rbdata.nargs] = rb_str_new_cstr(p1);
		rbdata.nargs++;
		if(p2!=NULL) {
			rbdata.vargs[rbdata.nargs] = rb_str_new_cstr(p2);
			rbdata.nargs++;
			if(p3!=NULL) {
				rbdata.vargs[rbdata.nargs] = rb_str_new_cstr(p3);
				rbdata.nargs++;
			}
		}
	}

	rbres = rb_protect(ksr_ruby_exec_callback, (VALUE)&rbdata, &rberr);

	_sr_R_env.msg = bmsg;

	if (rberr) {
		if(app_ruby_print_last_exception()==0) {
			LM_ERR("ruby exception (%d) on callback for: %s (res type: %d)\n",
					rberr, func, TYPE(rbres));
			return -1;
		}
	}

	return 1;
}

/**
 *
 */
int app_ruby_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return app_ruby_proc_run_ex(msg, func, p1, p2, p3, 0);
}

/**
 *
 */
int app_ruby_runstring(sip_msg_t *msg, char *script)
{
	LM_ERR("not implemented\n");
	return -1;
}

/**
 *
 */
int app_ruby_dostring(sip_msg_t *msg, char *script)
{
	LM_ERR("not implemented\n");
	return -1;
}

/**
 *
 */
int app_ruby_dofile(sip_msg_t *msg, char *script)
{
	LM_ERR("not implemented\n");
	return -1;
}

ksr_ruby_export_t *_sr_R_KSRMethods = NULL;
#define SR_RUBY_KSR_MODULES_SIZE	256
#define SR_RUBY_KSR_METHODS_SIZE	(SR_KEMI_RUBY_EXPORT_SIZE + SR_RUBY_KSR_MODULES_SIZE)

static VALUE _ksr_mKSR;
static VALUE _ksr_mSMD[SR_RUBY_KSR_MODULES_SIZE];

/**
 *
 */
void ksr_app_ruby_toupper(char *bin, char *bout)
{
	int i;
	for(i=0; bin[i]!='\0'; i++) {
		bout[i] = (char)toupper(bin[i]);
	}
	bout[i] = '\0';
}
/**
 *
 */
int app_ruby_kemi_export_libs(void)
{
	ksr_ruby_export_t *_sr_crt_R_KSRMethods = NULL;
	sr_kemi_module_t *emods = NULL;
	int emods_size = 0;
	int i;
	int k;
	int n;
	int m;
	char rmname[128];

	_sr_R_KSRMethods = malloc(SR_RUBY_KSR_METHODS_SIZE * sizeof(ksr_ruby_export_t));
	if(_sr_R_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return 0;
	}
	memset(_sr_R_KSRMethods, 0, SR_RUBY_KSR_METHODS_SIZE * sizeof(ksr_ruby_export_t));

	emods_size = sr_kemi_modules_size_get();
	emods = sr_kemi_modules_get();

	n = 0;
	_sr_crt_R_KSRMethods = _sr_R_KSRMethods;
	if(emods_size==0 || emods[0].kexp==NULL) {
		LM_ERR("no kemi exports registered\n");
		return 0;
	}

	/* toplevel module KSR */
	_ksr_mKSR = rb_define_module("KSR");

	for(i=0; emods[0].kexp[i].func!=NULL; i++) {
		LM_DBG("exporting KSR.%s(...)\n", emods[0].kexp[i].fname.s);
		_sr_crt_R_KSRMethods[i].mname = "";
		_sr_crt_R_KSRMethods[i].fname = emods[0].kexp[i].fname.s;
		_sr_crt_R_KSRMethods[i].func =
			sr_kemi_ruby_export_associate(&emods[0].kexp[i]);
		if(_sr_crt_R_KSRMethods[i].func == NULL) {
			LM_ERR("failed to associate kemi function with ruby export\n");
			free(_sr_R_KSRMethods);
			_sr_R_KSRMethods = NULL;
			return 0;
		}

		rb_define_singleton_method(_ksr_mKSR, _sr_crt_R_KSRMethods[i].fname,
						_sr_crt_R_KSRMethods[i].func, -1);

		n++;
	}

	m = 0;

	if(_app_ruby_proc_xval_mode==0) {
		/* pv submodule */
		_ksr_mSMD[m] = rb_define_module_under(_ksr_mKSR, "PV");
		for(i=0; _sr_kemi_pv_R_Map[i].fname!=0; i++) {
			LM_DBG("exporting KSR.PV.%s(...)\n", _sr_kemi_pv_R_Map[i].fname);
			rb_define_singleton_method(_ksr_mSMD[m], _sr_kemi_pv_R_Map[i].fname,
					_sr_kemi_pv_R_Map[i].func, -1);
		}
		LM_DBG("initialized kemi sub-module: KSR.PV\n");
		m++;
	}

	/* x submodule */
	_ksr_mSMD[m] = rb_define_module_under(_ksr_mKSR, "X");
	for(i=0; _sr_kemi_x_R_Map[i].fname!=0; i++) {
		LM_DBG("exporting KSR.X.%s(...)\n", _sr_kemi_x_R_Map[i].fname);
		rb_define_singleton_method(_ksr_mSMD[m], _sr_kemi_x_R_Map[i].fname,
				_sr_kemi_x_R_Map[i].func, -1);
	}
	LM_DBG("initialized kemi sub-module: KSR.X\n");
	m++;

	/* registered kemi modules */
	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
			if((_app_ruby_proc_xval_mode==0) && emods[k].kexp[0].mname.len==2
					&& strncasecmp(emods[k].kexp[0].mname.s, "pv", 2)==0) {
				LM_DBG("skipping external pv sub-module\n");
				continue;
			}
			n++;
			_sr_crt_R_KSRMethods = _sr_R_KSRMethods + n;
			ksr_app_ruby_toupper(emods[k].kexp[0].mname.s, rmname);
			_ksr_mSMD[m] = rb_define_module_under(_ksr_mKSR, rmname);
			for(i=0; emods[k].kexp[i].func!=NULL; i++) {
				_sr_crt_R_KSRMethods[i].mname = emods[k].kexp[0].mname.s;
				_sr_crt_R_KSRMethods[i].fname = emods[k].kexp[i].fname.s;
				_sr_crt_R_KSRMethods[i].func =
					sr_kemi_ruby_export_associate(&emods[k].kexp[i]);
				LM_DBG("exporting KSR.%s.%s(...)\n", rmname,
						emods[k].kexp[i].fname.s);
				if(_sr_crt_R_KSRMethods[i].func == NULL) {
					LM_ERR("failed to associate kemi function with func export\n");
					free(_sr_R_KSRMethods);
					_sr_R_KSRMethods = NULL;
					return 0;
				}

				rb_define_singleton_method(_ksr_mSMD[m], _sr_crt_R_KSRMethods[i].fname,
						_sr_crt_R_KSRMethods[i].func, -1);

				n++;
			}
			m++;
			LM_DBG("initialized kemi sub-module: KSR.%s\n", rmname);
		}
	}
	LM_DBG("module 'KSR' has been initialized\n");

	return 1;
}

/**
 *
 */
int app_ruby_proc_opt_set_s(char* optName, str* optVal)
{
	LM_DBG("trying to set option: %s\n", optName);
	if(strcasecmp(optName, "LoadFile")==0) {
		_app_ruby_proc_load_file = *optVal;
	} else {
		LM_ERR("unknown option: %s\n", optName);
		return -1;
	}
	return 0;
}

/**
 *
 */
int app_ruby_proc_opt_set_n(char* optName, int optVal)
{
	LM_DBG("trying to set option: %s\n", optName);
	if(strcasecmp(optName, "XValMode")==0) {
		_app_ruby_proc_xval_mode = optVal;
	} else {
		LM_ERR("unknown option: %s\n", optName);
		return -1;
	}
	return 0;
}

/**
 *
 */
int app_ruby_proc_opt_set_p(char* optName, void* optVal)
{
	LM_DBG("trying to set option: %s\n", optName);
	if(strcasecmp(optName, "ReloadVersionPtr")==0) {
		_app_ruby_proc_reload_version = optVal;
	} else {
		LM_ERR("unknown option: %s\n", optName);
		return -1;
	}
	return 0;
}

/**
 *
 */
int app_ruby_proc_get_export_size(void)
{
	return SR_KEMI_RUBY_EXPORT_SIZE;
}

/**
 *
 */
sr_kemi_t* app_ruby_proc_get_export(int idx)
{
	return sr_kemi_ruby_export_get(idx);
}

/**
 *
 */
int app_ruby_proc_local_version(void)
{
	return _app_ruby_proc_local_version;
}
