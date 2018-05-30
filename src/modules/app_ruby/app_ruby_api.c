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

str _sr_ruby_load_file = STR_NULL;

static int *_sr_ruby_reload_version = NULL;
static int _sr_ruby_local_version = 0;

/**
 *
 */
sr_ruby_env_t *app_ruby_sr_env_get(void)
{
	return &_sr_R_env;
}

/**
 * 
 */
int ruby_sr_init_mod(void)
{
	if(_sr_ruby_load_file.s == NULL || _sr_ruby_load_file.len<=0) {
		LM_ERR("no ruby script file to load was provided\n");
		return -1;
	}
	if(_sr_ruby_reload_version == NULL) {
		_sr_ruby_reload_version = (int*)shm_malloc(sizeof(int));
		if(_sr_ruby_reload_version == NULL) {
			LM_ERR("failed to allocated reload version\n");
			return -1;
		}
		*_sr_ruby_reload_version = 0;
	}
	memset(&_sr_R_env, 0, sizeof(sr_ruby_env_t));
	return 0;
}

static void app_ruby_print_last_exception()
{
	VALUE rException, rExceptStr;

	rException = rb_errinfo();         /* get last exception */
	rb_set_errinfo(Qnil);              /* clear last exception */
	rExceptStr = rb_funcall(rException, rb_intern("to_s"), 0, Qnil);
	LM_ERR("exception: %s\n", StringValuePtr(rExceptStr));
	return;
}

/**
 *
 */
int app_ruby_kemi_load_script(void)
{
	int state = 0;
	VALUE script;

	script  = rb_str_new_cstr(_sr_ruby_load_file.s);

	/* handle exceptions like rb_eval_string_protect() */
	rb_load_protect(script, 0, &state);

	if (state) {
		/* got exception */
		app_ruby_print_last_exception();
		LM_ERR("failed to load rb script file: %.*s (%d)\n",
				_sr_ruby_load_file.len, _sr_ruby_load_file.s, state);
		// return -1;
	}
	LM_DBG("rb script loaded: %s\n", _sr_ruby_load_file.s);

	return 0;
}

/**
 *
 */
int app_ruby_kemi_reload_script(void)
{
	int v;
	if(_sr_ruby_load_file.s == NULL && _sr_ruby_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		return -1;
	}
	if(_sr_ruby_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		return -1;
	}
	if(_sr_R_env.rinit == 0) {
		LM_ERR("load ruby context not created\n");
		return -1;
	}

	v = *_sr_ruby_reload_version;
	if(v == _sr_ruby_local_version) {
		/* same version */
		return 0;
	}
	LM_DBG("reloading ruby script file: %.*s (%d => %d)\n",
				_sr_ruby_load_file.len, _sr_ruby_load_file.s,
				_sr_ruby_local_version, v);
	app_ruby_kemi_load_script();
	_sr_ruby_local_version = v;
	return 0;
}

/**
 * 
 */
int ruby_sr_init_child(void)
{
	int state = 0;
	VALUE result;

	/* construct the VM */
	ruby_init();
	ruby_init_loadpath();
	ruby_script(_sr_ruby_load_file.s);

	/* Ruby goes here */
	result = rb_eval_string_protect("puts 'Hello " NAME "!'", &state);

	if (state) {
		/* handle exception */
		app_ruby_print_last_exception();
		LM_ERR("test execution with error\n");
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
void ruby_sr_destroy(void)
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
int ruby_sr_initialized(void)
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
	if(ket->rtype==SR_KEMIP_INT) {
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
static VALUE ksr_ruby_exec_callback(VALUE ptr)
{
	ksr_ruby_data_t *data = (ksr_ruby_data_t *)ptr;
	return rb_funcall2(data->robj, data->metid, data->nargs, data->vargs);
}

/**
 * 
 */
VALUE sr_kemi_ruby_exec_func(ksr_ruby_context_t *R, int eidx, int argc,
		VALUE* argv, VALUE self)
{
	sr_kemi_val_t vps[SR_KEMI_PARAMS_MAX];
	sr_ruby_env_t *env_R;
	sr_kemi_t *ket;
	str *fname;
	str *mname;
	int i;
	int ret = -1;

	env_R = app_ruby_sr_env_get();
	ket = sr_kemi_ruby_export_get(eidx);

	LM_DBG("executing %p eidx %d\n", ket, eidx);
	if(env_R==NULL || env_R->msg==NULL || ket==NULL) {
		LM_ERR("invalid ruby environment attributes or parameters\n");
		return Qfalse;
	}

	if(argc==0 && ket->ptypes[0]==SR_KEMIP_NONE) {
		ret = ((sr_kemi_fm_f)(ket->func))(env_R->msg);
		return sr_kemi_ruby_return_int(ket, ret);
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
			vps[i].s.s = StringValuePtr(argv[i]);
			vps[i].s.len = strlen(vps[i].s.s);
			LM_DBG("param[%d] for: %.*s.%.*s is str: %.*s\n", i,
				mname->len, mname->s, fname->len, fname->s, vps[i].s.len, vps[i].s.s);
		} else if(ket->ptypes[i]==SR_KEMIP_INT) {
			if(!RB_INTEGER_TYPE_P(argv[i])) {
				LM_ERR("invalid int parameter type %d (%d)\n", ket->ptypes[i], i);
				return Qfalse;
			}
			vps[i].n = NUM2INT(argv[i]);
			LM_DBG("param[%d] for: %.*s.%.*s is int: %d\n", i,
				mname->len, mname->s, fname->len, fname->s, vps[i].n);
		} else {
			LM_ERR("unknown parameter type %d (%d)\n", ket->ptypes[i], i);
			return Qfalse;
		}
	}

	switch(i) {
		case 1:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmn_f)(ket->func))(env_R->msg, vps[0].n);
				return sr_kemi_ruby_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fms_f)(ket->func))(env_R->msg, &vps[0].s);
				return sr_kemi_ruby_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return Qfalse;
			}
		break;
		case 2:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmnn_f)(ket->func))(env_R->msg, vps[0].n, vps[1].n);
					return sr_kemi_ruby_return_int(ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmns_f)(ket->func))(env_R->msg, vps[0].n, &vps[1].s);
					return sr_kemi_ruby_return_int(ket, ret);
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return Qfalse;
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmsn_f)(ket->func))(env_R->msg, &vps[0].s, vps[1].n);
					return sr_kemi_ruby_return_int(ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmss_f)(ket->func))(env_R->msg, &vps[0].s, &vps[1].s);
					return sr_kemi_ruby_return_int(ket, ret);
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return Qfalse;
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return Qfalse;
			}
		break;
		default:
			LM_ERR("invalid parameters for: %.*s\n",
					fname->len, fname->s);
			return Qfalse;
	}
}

/**
 * 
 */
int app_ruby_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
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

	rbdata.robj = rb_mKernel;
	rbdata.nargs = 0;
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
		app_ruby_print_last_exception();
		LM_ERR("ruby exception (%d) on callback for: %s\n", rberr, func);
		return -1;
	}

	return 1;
}

/**
 * 
 */
int app_ruby_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return app_ruby_run_ex(msg, func, p1, p2, p3, 0);
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

	/* registered kemi modules */
	m = 0;
	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
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
			LM_DBG("initializing kemi sub-module: KSR.%s\n", rmname);
		}
	}
	LM_DBG("module 'KSR' has been initialized\n");

	return 1;
}

/**
 * 
 */
int app_ruby_init_rpc(void)
{
	return 0;
}