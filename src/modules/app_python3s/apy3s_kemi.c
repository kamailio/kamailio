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
#include "../../core/locking.h"
#include "../../core/pvar.h"
#include "../../core/timer.h"
#include "../../core/mem/pkg.h"
#include "../../core/mem/shm.h"
#include "../../core/sr_module.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "app_python3s_mod.h"
#include "apy3s_exception.h"
#include "apy3s_kemi_export.h"
#include "apy3s_kemi.h"

int *_sr_python_reload_version = NULL;
int _sr_python_local_version = 0;
gen_lock_t* _sr_python_reload_lock = NULL;
extern str _sr_python_load_file;
extern int _apy3s_process_rank;

int apy_reload_script(void);

static sr_apy_env_t _sr_apy_env = {0};

/**
 *
 */
sr_apy_env_t *sr_apy_env_get()
{
	return &_sr_apy_env;
}

/**
 *
 */
int apy3s_exec_func(sip_msg_t *_msg, char *fname, char *fparam, int emode)
{
    PyObject *pFunc, *pArgs, *pValue;
    sip_msg_t *bmsg;
    int rval = -1;
	int locked = 0;

    if (_sr_apy3s_handler_script == NULL) {
    	LM_ERR("interpreter not properly initialized\n");
    	return -1;
	}

	if (lock_try(_sr_python_reload_lock) == 0) {
		if(_sr_python_reload_version && *_sr_python_reload_version != _sr_python_local_version) {
			LM_INFO("Reloading script %d->%d\n", _sr_python_local_version, *_sr_python_reload_version);
			if (apy_reload_script()) {
				LM_ERR("Error reloading script\n");
			} else {
				_sr_python_local_version = *_sr_python_reload_version;
			}
		}
		locked = 1;
	}

	bmsg = _sr_apy_env.msg;
	_sr_apy_env.msg = _msg;

	pFunc = PyObject_GetAttrString(_sr_apy3s_handler_script, fname);
	/* pFunc is a new reference */

	if (pFunc == NULL || !PyCallable_Check(pFunc)) {
		if(emode==1) {
			LM_ERR("%s not found or is not callable\n", fname);
		} else {
			LM_DBG("%s not found or is not callable\n", fname);
		}
		Py_XDECREF(pFunc);
		_sr_apy_env.msg = bmsg;
		if(emode==1) {
			goto error;
		} else {
			rval = 1;
			goto error;
		}
	}
	if(fparam) {
		pArgs = PyTuple_New(1);
		if (pArgs == NULL) {
			LM_ERR("PyTuple_New() failed\n");
			Py_DECREF(pFunc);
			_sr_apy_env.msg = bmsg;
			goto error;
		}
		pValue = PyUnicode_FromString(fparam);
		if (pValue == NULL) {
			LM_ERR("PyUnicode_FromString(%s) failed\n", fparam);
			Py_DECREF(pArgs);
			Py_DECREF(pFunc);
			_sr_apy_env.msg = bmsg;
			goto error;
		}
		PyTuple_SetItem(pArgs, 0, pValue);
	} else {
		pArgs = PyTuple_New(0);
	}

	pValue = PyObject_CallObject(pFunc, pArgs);
	Py_XDECREF(pArgs);
	Py_DECREF(pFunc);

	if (PyErr_Occurred()) {
		Py_XDECREF(pValue);
		LM_ERR("error exception occurred\n");
		apy3s_handle_exception("apy3s_exec_func: %s(%s)", fname, fparam);
		_sr_apy_env.msg = bmsg;
		goto error;
	}
	if (pValue == NULL) {
		LM_ERR("PyObject_CallObject() returned NULL\n");
		_sr_apy_env.msg = bmsg;
		goto error;
	}
	rval = (int)PyLong_AsLong(pValue);

	Py_DECREF(pValue);
	_sr_apy_env.msg = bmsg;

error:
	if(locked) {
		lock_release(_sr_python_reload_lock);
	}
    return rval;
}


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
			ret = apy3s_exec_func(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, 0);
		} else {
			ret = apy3s_exec_func(msg, "ksr_request_route", NULL, 1);
		}
	} else if(rtype==CORE_ONREPLY_ROUTE) {
		if(kemi_reply_route_callback.len>0) {
			ret = apy3s_exec_func(msg, kemi_reply_route_callback.s, NULL, 0);
		}
	} else if(rtype==BRANCH_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy3s_exec_func(msg, rname->s, NULL, 0);
		}
	} else if(rtype==FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy3s_exec_func(msg, rname->s, NULL, 0);
		}
	} else if(rtype==BRANCH_FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy3s_exec_func(msg, rname->s, NULL, 0);
		}
	} else if(rtype==TM_ONREPLY_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy3s_exec_func(msg, rname->s, NULL, 0);
		}
	} else if(rtype==ONSEND_ROUTE) {
		if(kemi_onsend_route_callback.len>0) {
			ret = apy3s_exec_func(msg, kemi_onsend_route_callback.s, NULL, 0);
		}
		return 1;
	} else if(rtype==EVENT_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy3s_exec_func(msg, rname->s,
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
	return PyLong_FromLong((long)rval);
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
	return PyUnicode_FromStringAndSize(sval, slen);
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
PyObject *sr_apy_kemi_exec_func_ex(sr_kemi_t *ket, PyObject *self, PyObject *args, int idx)
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
		LM_DBG("execution of method: %.*s.%.*s\n",
				ket->mname.len, ket->mname.s,
				ket->fname.len, ket->fname.s);
	} else {
		LM_DBG("execution of method: %.*s\n", ket->fname.len, ket->fname.s);
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
		pobj = PyTuple_GetItem(args, i);
		if(pobj==NULL) {
			LM_ERR("null parameter - func: %.*s idx: %d argc: %d\n",
					fname.len, fname.s, i, (int)alen);
			return sr_kemi_apy_return_false();
		}
		if(ket->ptypes[i]==SR_KEMIP_STR) {
			if(!PyUnicode_Check(pobj)) {
				LM_ERR("non-string parameter - func: %.*s idx: %d argc: %d\n",
						fname.len, fname.s, i, (int)alen);
				return sr_kemi_apy_return_false();
			}
			vps[i].v.s.s = (char*)PyUnicode_AsUTF8AndSize(pobj, &slen);
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
				vps[i].v.n = (int)vps[i].v.l;
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
#if PY_VERSION_HEX >= 0x03100000
	PyCodeObject *pcode = NULL;
#endif
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
			if (pstate != NULL) {
#if PY_VERSION_HEX >= 0x03100000
				pframe = PyThreadState_GetFrame(pstate);
				if(pframe != NULL) {
					pcode = PyFrame_GetCode(pframe);
				}
#else
				pframe = pstate->frame;
#endif
			}

#if PY_VERSION_HEX >= 0x03100000
			LOG(cfg_get(core, core_cfg, latency_log),
					"alert - action KSR.%s%s%s(...)"
					" took too long [%u us] (file:%s func:%s line:%d)\n",
					(ket->mname.len>0)?ket->mname.s:"",
					(ket->mname.len>0)?".":"", ket->fname.s, tdiff,
					(pcode)?PyUnicode_AsUTF8(pcode->co_filename):"",
					(pcode)?PyUnicode_AsUTF8(pcode->co_name):"",
					(pframe)?PyFrame_GetLineNumber(pframe):0);
#else
			LOG(cfg_get(core, core_cfg, latency_log),
					"alert - action KSR.%s%s%s(...)"
					" took too long [%u us] (file:%s func:%s line:%d)\n",
					(ket->mname.len>0)?ket->mname.s:"",
					(ket->mname.len>0)?".":"", ket->fname.s, tdiff,
					(pframe && pframe->f_code)?PyUnicode_AsUTF8(pframe->f_code->co_filename):"",
					(pframe && pframe->f_code)?PyUnicode_AsUTF8(pframe->f_code->co_name):"",
					(pframe && pframe->f_code)?PyCode_Addr2Line(pframe->f_code, pframe->f_lasti):0);
#endif
		}
	}

	return ret;
}

PyObject *apy3s_kemi_modx(PyObject *self, PyObject *args)
{
	int i, rval;
	char *fname, *arg1, *arg2;
	ksr_cmd_export_t* fexport;
	struct action *act;
	struct run_act_ctx ra_ctx;

	if (_sr_apy_env.msg == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "msg is NULL");
		return NULL;
	}

	i = PySequence_Size(args);
	if (i < 1 || i > 3) {
		PyErr_SetString(PyExc_RuntimeError, "call_function() should " \
				"have from 1 to 3 arguments");
		return NULL;
	}

	if(!PyArg_ParseTuple(args, "s|ss:call_function", &fname, &arg1, &arg2))
		return NULL;

	fexport = find_export_record(fname, i - 1, 0);
	if (fexport == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "no such function");
		return NULL;
	}

	act = mk_action(MODULE2_T, 4 /* number of (type, value) pairs */,
			MODEXP_ST, fexport, /* function */
			NUMBER_ST, 2,       /* parameter number */
			STRING_ST, arg1,    /* param. 1 */
			STRING_ST, arg2     /* param. 2 */
			);

	if (act == NULL) {
		PyErr_SetString(PyExc_RuntimeError,
				"action structure could not be created");
		return NULL;
	}

	if (fexport->fixup != NULL) {
		if (i >= 3) {
			rval = fexport->fixup(&(act->val[3].u.data), 2);
			if (rval < 0) {
				pkg_free(act);
				PyErr_SetString(PyExc_RuntimeError, "Error in fixup (2)");
				return NULL;
			}
			act->val[3].type = MODFIXUP_ST;
		}
		if (i >= 2) {
			rval = fexport->fixup(&(act->val[2].u.data), 1);
			if (rval < 0) {
				pkg_free(act);
				PyErr_SetString(PyExc_RuntimeError, "Error in fixup (1)");
				return NULL;
			}
			act->val[2].type = MODFIXUP_ST;
		}
		if (i == 1) {
			rval = fexport->fixup(0, 0);
			if (rval < 0) {
				pkg_free(act);
				PyErr_SetString(PyExc_RuntimeError, "Error in fixup (0)");
				return NULL;
			}
		}
	}

	init_run_actions_ctx(&ra_ctx);
	rval = do_action(&ra_ctx, act, _sr_apy_env.msg);

	if ((act->val[3].type == MODFIXUP_ST) && (act->val[3].u.data)) {
		pkg_free(act->val[3].u.data);
	}

	if ((act->val[2].type == MODFIXUP_ST) && (act->val[2].u.data)) {
		pkg_free(act->val[2].u.data);
	}

	pkg_free(act);

	return PyLong_FromLong(rval);
}


/**
 *
 */
PyObject *_sr_apy_ksr_module = NULL;
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

static struct PyModuleDef KSR_moduledef = {
        PyModuleDef_HEAD_INIT,
        "KSR",
        NULL,
        -1,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};

/**
 *
 */
static PyMethodDef _sr_apy_kemi_x_Methods[] = {
	{"modf", (PyCFunction)apy3s_kemi_modx, METH_VARARGS,
		"Invoke function exported by the other module."},
	{NULL, NULL, 0, NULL} /* sentinel */
};

static struct PyModuleDef KSR_x_moduledef = {
        PyModuleDef_HEAD_INIT,
        "KSR.x",
        NULL,
        -1,
        _sr_apy_kemi_x_Methods,
        NULL,
        NULL,
        NULL,
        NULL
};

/* forward */
static PyObject* init_KSR(void);
int sr_apy3s_init_ksr(void) {
	PyImport_AppendInittab("KSR", &init_KSR);
	return 0;
}
/**
 *
 */
static
PyObject* init_KSR(void)
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
		return NULL;
	}

	_sr_KSRMethods = malloc(SR_APY_KSR_METHODS_SIZE * sizeof(PyMethodDef));
	if(_sr_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return NULL;
	}
	_sr_apy_ksr_modules_list = malloc(SR_APY_KSR_MODULES_SIZE * sizeof(PyObject*));
	if(_sr_apy_ksr_modules_list==NULL) {
		LM_ERR("no more pkg memory\n");
		return NULL;
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
				return NULL;
			}
			_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
			_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
			n++;
		}
	}

	KSR_moduledef.m_methods = _sr_crt_KSRMethods;
	_sr_apy_ksr_module = PyModule_Create(&KSR_moduledef);

	Py_INCREF(_sr_apy_ksr_module);

	m = 0;

	/* special sub-modules - x.modf() can have variable number of params */
	_sr_apy_ksr_modules_list[m] = PyModule_Create(&KSR_x_moduledef);
	PyModule_AddObject(_sr_apy_ksr_module, "x", _sr_apy_ksr_modules_list[m]);
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
					return NULL;
				}
				_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
				_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
				n++;
			}
			LM_DBG("initializing kemi sub-module: %s (%s)\n", mname,
					emods[k].kexp[0].mname.s);

			PyModuleDef *mmodule  = malloc(sizeof(PyModuleDef));
			memset(mmodule, 0, sizeof(PyModuleDef));
			mmodule->m_name = strndup(mname, 127);
			mmodule->m_methods = _sr_crt_KSRMethods;
			mmodule->m_size = -1;

			_sr_apy_ksr_modules_list[m] = PyModule_Create(mmodule);
			PyModule_AddObject(_sr_apy_ksr_module, emods[k].kexp[0].mname.s, _sr_apy_ksr_modules_list[m]);
			Py_INCREF(_sr_apy_ksr_modules_list[m]);
			m++;
		}
	}
	LM_DBG("module 'KSR' has been initialized\n");
	return _sr_apy_ksr_module;
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
			LM_ERR("failed to allocated reload version\n");
			return -1;
		}
		*_sr_python_reload_version = 0;
	}
	_sr_python_reload_lock = lock_alloc();
	lock_init(_sr_python_reload_lock);
	return 0;
}

static const char* app_python3s_rpc_reload_doc[2] = {
	"Reload python file",
	0
};


static void app_python3s_rpc_reload(rpc_t* rpc, void* ctx)
{
	int v;
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

	_sr_python_local_version = v = *_sr_python_reload_version;
	*_sr_python_reload_version += 1;
	LM_INFO("marking for reload Python script file: %.*s (%d => %d)\n",
		_sr_python_load_file.len, _sr_python_load_file.s,
		v,
		*_sr_python_reload_version);

	if (rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "dd",
			"old", v,
			"new", *_sr_python_reload_version);

	return;
}

static const char* app_python3s_rpc_api_list_doc[2] = {
	"List kemi exports to javascript",
	0
};

static void app_python3s_rpc_api_list(rpc_t* rpc, void* ctx)
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

rpc_export_t app_python3s_rpc_cmds[] = {
	{"app_python3s.reload", app_python3s_rpc_reload,
		app_python3s_rpc_reload_doc, 0},
	{"app_python3s.api_list", app_python3s_rpc_api_list,
		app_python3s_rpc_api_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int app_python3s_init_rpc(void)
{
	if (rpc_register_array(app_python3s_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
