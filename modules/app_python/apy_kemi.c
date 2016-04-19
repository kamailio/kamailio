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

#include "../../dprint.h"
#include "../../route.h"
#include "../../kemi.h"
#include "../../mem/pkg.h"

#include "python_exec.h"
#include "apy_kemi_export.h"
#include "apy_kemi.h"

/**
 *
 */
int sr_kemi_config_engine_python(sip_msg_t *msg, int rtype, str *rname)
{
	int ret;

	ret = -1;
	if(rtype==REQUEST_ROUTE) {
		ret = apy_exec(msg, "ksr_request_route", NULL, 1);
	} else if(rtype==CORE_ONREPLY_ROUTE) {
		ret = apy_exec(msg, "ksr_reply_route", NULL, 0);
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
		ret = apy_exec(msg, "ksr_onsend_route", NULL, 0);
	} else if(rtype==EVENT_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
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
PyObject *sr_apy_kemi_exec_func(PyObject *self, PyObject *args, int idx)
{
	str fname;
	int i;
	sr_kemi_t *ket = NULL;
	sr_kemi_val_t vps[SR_KEMI_PARAMS_MAX];
	sr_apy_env_t *env_P;

	env_P = sr_apy_env_get();

	if(env_P==NULL || env_P->msg==NULL) {
		LM_ERR("invalid Python environment attributes\n");
		Py_INCREF(Py_None);
		return Py_None;
	}

	ket = sr_apy_kemi_export_get(idx);
	if(ket==NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	if(ket->mname.len>0) {
		LM_DBG("execution of method: %.*s\n", ket->fname.len, ket->fname.s);
	} else {
		LM_DBG("execution of method: %.*s.%.*s\n",
				ket->mname.len, ket->mname.s,
				ket->fname.len, ket->fname.s);
	}
	fname = ket->fname;

	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_val_t));
	for(i=0; i<SR_KEMI_PARAMS_MAX; i++) {
		if(ket->ptypes[i]==SR_KEMIP_NONE) {
			break;
		} else if(ket->ptypes[i]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "s:kemi-param", &vps[i].s.s)) {
				LM_ERR("unable to retrieve str param %d\n", i);
				Py_INCREF(Py_None);
				return Py_None;
			}
			vps[i].s.len = strlen(vps[i].s.s);
			LM_DBG("param[%d] for: %.*s is str: %.*s\n", i,
				fname.len, fname.s, vps[i].s.len, vps[i].s.s);
		} else if(ket->ptypes[i]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "i:kemi-param", &vps[i].n)) {
				LM_ERR("unable to retrieve int param %d\n", i);
				Py_INCREF(Py_None);
				return Py_None;
			}
			LM_DBG("param[%d] for: %.*s is int: %d\n", i,
				fname.len, fname.s, vps[i].n);
		} else {
			LM_ERR("unknown parameter type %d (%d)\n", ket->ptypes[i], i);
			Py_INCREF(Py_None);
			return Py_None;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 *
 */
PyObject *_sr_apy_ksr_module = NULL;
PyObject *_sr_apy_ksr_module_dict = NULL;

PyMethodDef *_sr_KSRMethods = NULL;
#define SR_APY_KSR_METHOS_SIZE	256

/**
 *
 */
static int sr_apy_kemi_f_dbg(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_DBG("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
static sr_kemi_t _sr_apy_kemi_test[] = {
	{ str_init("test"), str_init("dbg"),
		SR_KEMIP_NONE, sr_apy_kemi_f_dbg,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};


/**
 *
 */
int sr_apy_init_ksr(void)
{
	_sr_KSRMethods = pkg_malloc(SR_APY_KSR_METHOS_SIZE * sizeof(PyMethodDef));
	if(_sr_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	memset(_sr_KSRMethods, 0, SR_APY_KSR_METHOS_SIZE * sizeof(PyMethodDef));

	_sr_KSRMethods[0].ml_name = _sr_apy_kemi_test[0].fname.s;
	_sr_KSRMethods[0].ml_meth = sr_apy_kemi_export_associate(&_sr_apy_kemi_test[0]);
	_sr_KSRMethods[0].ml_flags = METH_VARARGS;
	_sr_KSRMethods[0].ml_doc = "Kamailio function";

	_sr_apy_ksr_module = Py_InitModule("KSR", _sr_KSRMethods);
	_sr_apy_ksr_module_dict = PyModule_GetDict(_sr_apy_ksr_module);

	Py_INCREF(_sr_apy_ksr_module);

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
		pkg_free(_sr_KSRMethods);
		_sr_KSRMethods = NULL;
	}

	LM_DBG("module 'KSR' has been destroyed\n");
}
