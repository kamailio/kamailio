/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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

#include <Python.h>
#include <frameobject.h>

#include "../../core/dprint.h"
#include "../../core/route.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../core/pvar.h"
#include "../../core/timer.h"
#include "../../core/mem/pkg.h"
#include "../../core/mem/shm.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "msgobj_struct.h"
#include "python_exec.h"
#include "apy_kemi_export.h"
#include "apy_kemi.h"

int *_sr_python_reload_version = NULL;
int _sr_python_local_version = 0;
extern str _sr_python_load_file;
extern int _apy_process_rank;

/**
 *
 */
int sr_kemi_config_engine_python(sip_msg_t *msg, int rtype, str *rname,
		str *rparam)
{
	int ret;

	ret = -1;
	if(rtype==REQUEST_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, 0);
		} else {
			ret = apy_exec(msg, "ksr_request_route", NULL, 1);
		}
	} else if(rtype==CORE_ONREPLY_ROUTE) {
		if(kemi_reply_route_callback.len>0) {
			ret = apy_exec(msg, kemi_reply_route_callback.s, NULL, 0);
		}
	} else if(rtype==BRANCH_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==BRANCH_FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==TM_ONREPLY_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==ONSEND_ROUTE) {
		if(kemi_onsend_route_callback.len>0) {
			ret = apy_exec(msg, kemi_onsend_route_callback.s, NULL, 0);
		}
		return 1;
	} else if(rtype==EVENT_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, 0);
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
PyObject *sr_kemi_apy_return_true(void)
{
	Py_INCREF(Py_True);
	return Py_True;
}

/**
 *
 */
PyObject *sr_kemi_apy_return_false(void)
{
	Py_INCREF(Py_False);
	return Py_False;
}

/**
 *
 */
PyObject *sr_apy_kemi_return_none(void)
{
	Py_INCREF(Py_None);
	return Py_None;
}

/**
 *
 */
PyObject *sr_kemi_apy_return_int(sr_kemi_t *ket, int rval)
{
	if(ket!=NULL && ket->rtype==SR_KEMIP_BOOL) {
		if(rval==SR_KEMI_TRUE) {
			return sr_kemi_apy_return_true();
		} else {
			return sr_kemi_apy_return_false();
		}
	}
	return PyInt_FromLong((long)rval);
}

/**
 *
 */
PyObject *sr_kemi_apy_return_long(sr_kemi_t *ket, long rval)
{
	return PyLong_FromLong(rval);
}

/**
 *
 */
PyObject *sr_apy_kemi_return_str(sr_kemi_t *ket, char *sval, int slen)
{
	return PyString_FromStringAndSize(sval, slen);
}

/**
 *
 */
PyObject *sr_kemi_apy_return_xval(sr_kemi_t *ket, sr_kemi_xval_t *rx)
{
	switch(rx->vtype) {
		case SR_KEMIP_NONE:
			return sr_apy_kemi_return_none();
		case SR_KEMIP_INT:
			return sr_kemi_apy_return_int(ket, rx->v.n);
		case SR_KEMIP_LONG:
			return sr_kemi_apy_return_long(ket, rx->v.l);
		case SR_KEMIP_STR:
			return sr_apy_kemi_return_str(ket, rx->v.s.s, rx->v.s.len);
		case SR_KEMIP_BOOL:
			if(rx->v.n!=SR_KEMI_FALSE) {
				return sr_kemi_apy_return_true();
			} else {
				return sr_kemi_apy_return_false();
			}
		case SR_KEMIP_ARRAY:
			LM_ERR("unsupported return type: array\n");
			sr_kemi_xval_free(rx);
			return sr_apy_kemi_return_none();
		case SR_KEMIP_DICT:
			LM_ERR("unsupported return type: map\n");
			sr_kemi_xval_free(rx);
			return sr_apy_kemi_return_none();
		case SR_KEMIP_XVAL:
			/* unknown content - return false */
			return sr_kemi_apy_return_false();
		case SR_KEMIP_NULL:
			return sr_apy_kemi_return_none();
		default:
			/* unknown type - return false */
			return sr_kemi_apy_return_false();
	}
}

/**
 *
 */
PyObject *sr_apy_kemi_exec_func_ex(sr_kemi_t *ket, PyObject *self,
			PyObject *args, int idx)
{
	str fname;
	int i;
	int ret;
	sr_kemi_xval_t vps[SR_KEMI_PARAMS_MAX];
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;
	sr_kemi_xval_t *xret;
	Py_ssize_t slen;
	Py_ssize_t alen;
	PyObject *pobj;

	env_P = sr_apy_env_get();

	if(env_P==NULL) {
		LM_ERR("invalid Python environment attributes\n");
		return sr_kemi_apy_return_false();
	}
	if(env_P->msg==NULL) {
		lmsg = faked_msg_next();
	} else {
		lmsg = env_P->msg;
	}

	if(ket->mname.len>0) {
		LM_DBG("execution of method: %.*s\n", ket->fname.len, ket->fname.s);
	} else {
		LM_DBG("execution of method: %.*s.%.*s\n",
				ket->mname.len, ket->mname.s,
				ket->fname.len, ket->fname.s);
	}
	fname = ket->fname;

	if(ket->ptypes[0]==SR_KEMIP_NONE) {
		if(ket->rtype==SR_KEMIP_XVAL) {
			xret = ((sr_kemi_xfm_f)(ket->func))(lmsg);
			return sr_kemi_apy_return_xval(ket, xret);
		} else {
			ret = ((sr_kemi_fm_f)(ket->func))(lmsg);
			return sr_kemi_apy_return_int(ket, ret);
		}
	}

	if(!PyTuple_Check(args)) {
		LM_ERR("invalid arguments from Python\n");
		return sr_kemi_apy_return_false();

	}
	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_xval_t));

	alen = PyTuple_Size(args);
	LM_DBG("number of arguments: %d\n", (int)alen);

	for(i=0; i<SR_KEMI_PARAMS_MAX; i++) {
		if(ket->ptypes[i]==SR_KEMIP_NONE) {
			break;
		}
		if(i>=alen) {
			LM_ERR("invalid number of parameters - idx: %d argc: %d\n", i, (int)alen);
			return sr_kemi_apy_return_false();
		}
		pobj = PyList_GetItem(args, i);
		if(pobj==NULL) {
			LM_ERR("null parameter - func: %.*s idx: %d argc: %d\n",
					fname.len, fname.s, i, (int)alen);
			return sr_kemi_apy_return_false();
		}
		if(ket->ptypes[i]==SR_KEMIP_STR) {
			if(!PyString_Check(pobj)) {
				LM_ERR("non-string parameter - func: %.*s idx: %d argc: %d\n",
						fname.len, fname.s, i, (int)alen);
				return sr_kemi_apy_return_false();
			}
			if(PyString_AsStringAndSize(pobj, &vps[i].v.s.s, &slen)<0) {
				LM_ERR("failed to get string parameter - func: %.*s idx: %d argc: %d\n",
						fname.len, fname.s, i, (int)alen);
				return sr_kemi_apy_return_false();
			}
			if(vps[i].v.s.s==NULL) {
				LM_ERR("null-string parameter - func: %.*s idx: %d argc: %d\n",
						fname.len, fname.s, i, (int)alen);
				return sr_kemi_apy_return_false();
			}
			vps[i].v.s.len = (int)slen;
			vps[i].vtype = SR_KEMIP_STR;
		} else if((ket->ptypes[i]==SR_KEMIP_INT) || (ket->ptypes[i]==SR_KEMIP_LONG)) {
			if(PyLong_Check(pobj)) {
				vps[i].v.l = PyLong_AsLong(pobj);
			} else if(PyInt_Check(pobj)) {
				vps[i].v.l = PyInt_AsLong(pobj);
			} else if(pobj==Py_True) {
				vps[i].v.l = 1;
			} else if(pobj==Py_False) {
				vps[i].v.l = 0;
			} else {
				LM_ERR("non-number parameter - func: %.*s idx: %d argc: %d\n",
						fname.len, fname.s, i, (int)alen);
				return sr_kemi_apy_return_false();
			}
			if(ket->ptypes[i]==SR_KEMIP_INT) {
				vps[i].vtype = SR_KEMIP_INT;
				ret = (int)vps[i].v.l;
				vps[i].v.n = ret;
			} else {
				vps[i].vtype = SR_KEMIP_LONG;
			}
		} else {
			LM_ERR("invalid parameter type - func: %.*s idx: %d argc: %d\n",
					fname.len, fname.s, i, (int)alen);
			return sr_kemi_apy_return_false();
		}
	}

	xret = sr_kemi_exec_func(ket, lmsg, i, vps);
	return sr_kemi_apy_return_xval(ket, xret);
}

/**
 *
 */
PyObject *sr_apy_kemi_exec_func(PyObject *self, PyObject *args, int idx)
{
	sr_kemi_t *ket = NULL;
	PyObject *ret = NULL;
	PyThreadState *pstate = NULL;
	PyFrameObject *pframe = NULL;
	struct timeval tvb = {0}, tve = {0};
	struct timezone tz;
	unsigned int tdiff;

	ket = sr_apy_kemi_export_get(idx);
	if(ket==NULL) {
		return sr_kemi_apy_return_false();
	}
	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tvb, &tz);
	}

	ret = sr_apy_kemi_exec_func_ex(ket, self, args, idx);

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tve, &tz);
		tdiff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
		if(tdiff >= cfg_get(core, core_cfg, latency_limit_action)) {
			pstate = PyThreadState_GET();
			if (pstate != NULL && pstate->frame != NULL) {
				pframe = pstate->frame;
			}

			LOG(cfg_get(core, core_cfg, latency_log),
					"alert - action KSR.%s%s%s(...)"
					" took too long [%u us] (file:%s func:%s line:%d)\n",
					(ket->mname.len>0)?ket->mname.s:"",
					(ket->mname.len>0)?".":"", ket->fname.s, tdiff,
					(pframe && pframe->f_code)?PyString_AsString(pframe->f_code->co_filename):"",
					(pframe && pframe->f_code)?PyString_AsString(pframe->f_code->co_name):"",
					(pframe && pframe->f_code)?PyCode_Addr2Line(pframe->f_code, pframe->f_lasti):0);
		}
	}

	return ret;
}

/**
 *
 */
PyObject *_sr_apy_ksr_module = NULL;
PyObject *_sr_apy_ksr_module_dict = NULL;
PyObject **_sr_apy_ksr_modules_list = NULL;

PyMethodDef *_sr_KSRMethods = NULL;
#define SR_APY_KSR_MODULES_SIZE	256
#define SR_APY_KSR_METHODS_SIZE	(SR_APY_KEMI_EXPORT_SIZE + SR_APY_KSR_MODULES_SIZE)

/**
 *
 */
static int sr_apy_kemi_f_ktest(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_DBG("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t _sr_apy_kemi_test[] = {
	{ str_init(""), str_init("ktest"),
		SR_KEMIP_NONE, sr_apy_kemi_f_ktest,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
static PyMethodDef _sr_apy_kemi_x_Methods[] = {
	{"modf", (PyCFunction)msg_call_function, METH_VARARGS,
		"Invoke function exported by the other module."},
	{NULL, NULL, 0, NULL} /* sentinel */
};

/**
 *
 */
int sr_apy_init_ksr(void)
{
	PyMethodDef *_sr_crt_KSRMethods = NULL;
	sr_kemi_module_t *emods = NULL;
	int emods_size = 0;
	int i;
	int k;
	int m;
	int n;
	char mname[128];

	/* init faked sip msg */
	if(faked_msg_init()<0)
	{
		LM_ERR("failed to init local faked sip msg\n");
		return -1;
	}

	_sr_KSRMethods = malloc(SR_APY_KSR_METHODS_SIZE * sizeof(PyMethodDef));
	if(_sr_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	_sr_apy_ksr_modules_list = malloc(SR_APY_KSR_MODULES_SIZE * sizeof(PyObject*));
	if(_sr_apy_ksr_modules_list==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	memset(_sr_KSRMethods, 0, SR_APY_KSR_METHODS_SIZE * sizeof(PyMethodDef));
	memset(_sr_apy_ksr_modules_list, 0, SR_APY_KSR_MODULES_SIZE * sizeof(PyObject*));

	emods_size = sr_kemi_modules_size_get();
	emods = sr_kemi_modules_get();

	n = 0;
	_sr_crt_KSRMethods = _sr_KSRMethods;
	if(emods_size==0 || emods[0].kexp==NULL) {
		LM_DBG("exporting KSR.%s(...)\n", _sr_apy_kemi_test[0].fname.s);
		_sr_crt_KSRMethods[0].ml_name = _sr_apy_kemi_test[0].fname.s;
		_sr_crt_KSRMethods[0].ml_meth = sr_apy_kemi_export_associate(&_sr_apy_kemi_test[0]);
		_sr_crt_KSRMethods[0].ml_flags = METH_VARARGS;
		_sr_crt_KSRMethods[0].ml_doc = NAME " exported function";
	} else {
		for(i=0; emods[0].kexp[i].func!=NULL; i++) {
			LM_DBG("exporting KSR.%s(...)\n", emods[0].kexp[i].fname.s);
			_sr_crt_KSRMethods[i].ml_name = emods[0].kexp[i].fname.s;
			_sr_crt_KSRMethods[i].ml_meth =
				sr_apy_kemi_export_associate(&emods[0].kexp[i]);
			if(_sr_crt_KSRMethods[i].ml_meth == NULL) {
				LM_ERR("failed to associate kemi function with python export\n");
				free(_sr_KSRMethods);
				_sr_KSRMethods = NULL;
				return -1;
			}
			_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
			_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
			n++;
		}
	}

	_sr_apy_ksr_module = Py_InitModule("KSR", _sr_crt_KSRMethods);
	_sr_apy_ksr_module_dict = PyModule_GetDict(_sr_apy_ksr_module);

	Py_INCREF(_sr_apy_ksr_module);

	m = 0;

	/* special sub-modules - x.modf() can have variable number of params */
	_sr_apy_ksr_modules_list[m] = Py_InitModule("KSR.x",
			_sr_apy_kemi_x_Methods);
	PyDict_SetItemString(_sr_apy_ksr_module_dict,
			"x", _sr_apy_ksr_modules_list[m]);
	Py_INCREF(_sr_apy_ksr_modules_list[m]);
	m++;

	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
			n++;
			_sr_crt_KSRMethods = _sr_KSRMethods + n;
			snprintf(mname, 128, "KSR.%s", emods[k].kexp[0].mname.s);
			for(i=0; emods[k].kexp[i].func!=NULL; i++) {
				LM_DBG("exporting %s.%s(...)\n", mname,
						emods[k].kexp[i].fname.s);
				_sr_crt_KSRMethods[i].ml_name = emods[k].kexp[i].fname.s;
				_sr_crt_KSRMethods[i].ml_meth =
					sr_apy_kemi_export_associate(&emods[k].kexp[i]);
				if(_sr_crt_KSRMethods[i].ml_meth == NULL) {
					LM_ERR("failed to associate kemi function with python export\n");
					free(_sr_KSRMethods);
					_sr_KSRMethods = NULL;
					return -1;
				}
				_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
				_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
				n++;
			}
			LM_DBG("initializing kemi sub-module: %s (%s)\n", mname,
					emods[k].kexp[0].mname.s);
			_sr_apy_ksr_modules_list[m] = Py_InitModule(mname, _sr_crt_KSRMethods);
			PyDict_SetItemString(_sr_apy_ksr_module_dict,
					emods[k].kexp[0].mname.s, _sr_apy_ksr_modules_list[m]);
			Py_INCREF(_sr_apy_ksr_modules_list[m]);
			m++;
		}
	}
	LM_DBG("module 'KSR' has been initialized\n");
	return 0;
}

/**
 *
 */
void sr_apy_destroy_ksr(void)
{
	if(_sr_apy_ksr_module!=NULL) {
		Py_XDECREF(_sr_apy_ksr_module);
		_sr_apy_ksr_module = NULL;
	}
	if(_sr_apy_ksr_module_dict!=NULL) {
		Py_XDECREF(_sr_apy_ksr_module_dict);
		_sr_apy_ksr_module_dict = NULL;
	}
	if(_sr_KSRMethods!=NULL) {
		free(_sr_KSRMethods);
		_sr_KSRMethods = NULL;
	}

	LM_DBG("module 'KSR' has been destroyed\n");
}

/**
 *
 */
int apy_sr_init_mod(void)
{
	if(_sr_python_reload_version == NULL) {
		_sr_python_reload_version = (int*)shm_malloc(sizeof(int));
		if(_sr_python_reload_version == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		*_sr_python_reload_version = 0;
	}

	return 0;
}

static const char* app_python_rpc_reload_doc[2] = {
	"Reload python file",
	0
};


static void app_python_rpc_reload(rpc_t* rpc, void* ctx)
{
	void *vh;

	if(_sr_python_load_file.s == NULL && _sr_python_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		rpc->fault(ctx, 500, "No script file");
		return;
	}
	if(_sr_python_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		rpc->fault(ctx, 500, "Reload not enabled");
		return;
	}

	*_sr_python_reload_version += 1;
	LM_INFO("marking for reload Python script file: %.*s (%d)\n",
				_sr_python_load_file.len, _sr_python_load_file.s,
				*_sr_python_reload_version);

	if (rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "dd",
			"old", *_sr_python_reload_version-1,
			"new", *_sr_python_reload_version);

	return;
}

static const char* app_python_rpc_api_list_doc[2] = {
	"List kemi exports to javascript",
	0
};

static void app_python_rpc_api_list(rpc_t* rpc, void* ctx)
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
	for(i=0; i<SR_APY_KSR_METHODS_SIZE ; i++) {
		ket = sr_apy_kemi_export_get(i);
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
	for(i=0; i<SR_APY_KSR_METHODS_SIZE; i++) {
		ket = sr_apy_kemi_export_get(i);
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

rpc_export_t app_python_rpc_cmds[] = {
	{"app_python.reload", app_python_rpc_reload,
		app_python_rpc_reload_doc, 0},
	{"app_python.api_list", app_python_rpc_api_list,
		app_python_rpc_api_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int app_python_init_rpc(void)
{
	if (rpc_register_array(app_python_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
