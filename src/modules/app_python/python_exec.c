/*
 * Copyright (C) 2009 Sippy Software, Inc., http://www.sippysoft.com
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <Python.h>

#include "../../core/mem/mem.h"
#include "../../core/data_lump.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/dprint.h"
#include "../../core/action.h"
#include "../../core/config.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_uri.h"

#include "python_exec.h"
#include "app_python_mod.h"
#include "python_msgobj.h"
#include "python_support.h"

static sr_apy_env_t _sr_apy_env = {0};

/**
 *
 */
sr_apy_env_t *sr_apy_env_get()
{
	return &_sr_apy_env;
}

static int _sr_apy_exec_pid = 0;


#define PY_GIL_ENSURE gstate = PyGILState_Ensure()
#define PY_GIL_RELEASE PyGILState_Release(gstate)
#define LOCK_RELEASE \
	if(locked)       \
	_sr_apy_exec_pid = 0

extern int _sr_python_local_version;
extern int *_sr_python_reload_version;
extern int apy_reload_script(void);

/**
 *
 */
int apy_exec(sip_msg_t *_msg, char *fname, char *fparam, int emode)
{
	PyObject *pFunc, *pArgs, *pValue, *pResult;
	PyObject *pmsg;
	int rval = -1;
	sip_msg_t *bmsg;
	int mpid;
	int locked = 0;
	PyGILState_STATE gstate;
	int gstate_init = 0;

	bmsg = _sr_apy_env.msg;
	_sr_apy_env.msg = _msg;
	mpid = getpid();

	if(_sr_apy_exec_pid != mpid) {
		_sr_apy_exec_pid = mpid;
		locked = 1;

		if(_sr_python_reload_version
				&& *_sr_python_reload_version > _sr_python_local_version) {
			if(apy_reload_script()) {
				LM_ERR("Error on reloading script\n");
			} else {
				LM_INFO("Reloaded script version %d -> %d\n",
						_sr_python_local_version, *_sr_python_reload_version);
				_sr_python_local_version = *_sr_python_reload_version;
			}
		}
		PY_GIL_ENSURE;
		gstate_init = 1;
	}

	pFunc = PyObject_GetAttrString(_sr_apy_handler_obj, fname);
	if(pFunc == NULL || !PyCallable_Check(pFunc)) {
		if(emode == 1) {
			LM_ERR("%s not found or is not callable\n", fname);
		} else {
			LM_DBG("%s not found or is not callable\n", fname);
		}
		Py_XDECREF(pFunc);
		_sr_apy_env.msg = bmsg;
		if(emode == 1) {
			goto err;
		} else {
			rval = 1;
			goto err;
		}
	}

	pmsg = newmsgobject(_msg);
	if(pmsg == NULL) {
		LM_ERR("can't create MSGtype instance\n");
		Py_DECREF(pFunc);
		_sr_apy_env.msg = bmsg;
		goto err;
	}

	pArgs = PyTuple_New(fparam == NULL ? 1 : 2);
	if(pArgs == NULL) {
		LM_ERR("PyTuple_New() has failed\n");
		msg_invalidate(pmsg);
		Py_DECREF(pmsg);
		Py_DECREF(pFunc);
		_sr_apy_env.msg = bmsg;
		goto err;
	}
	PyTuple_SetItem(pArgs, 0, pmsg);
	/* Tuple steals pmsg */

	if(fparam != NULL) {
		pValue = PyString_FromString(fparam);
		if(pValue == NULL) {
			LM_ERR("PyString_FromString(%s) has failed\n", fparam);
			msg_invalidate(pmsg);
			Py_DECREF(pArgs);
			Py_DECREF(pFunc);
			_sr_apy_env.msg = bmsg;
			goto err;
		}
		PyTuple_SetItem(pArgs, 1, pValue);
		/* Tuple steals pValue */
	}

	pResult = PyObject_CallObject(pFunc, pArgs);
	msg_invalidate(pmsg);
	Py_DECREF(pArgs);
	Py_DECREF(pFunc);
	if(PyErr_Occurred()) {
		Py_XDECREF(pResult);
		python_handle_exception("apy_exec: %s(%s)", fname, fparam);
		_sr_apy_env.msg = bmsg;
		goto err;
	}

	if(pResult == NULL) {
		LM_ERR("PyObject_CallObject() returned NULL\n");
		if(locked) {
			_sr_apy_exec_pid = 0;
		}
		_sr_apy_env.msg = bmsg;
		goto err;
	}

	rval = PyInt_AsLong(pResult);
	Py_DECREF(pResult);
	_sr_apy_env.msg = bmsg;
err:
	if(gstate_init) {
		PY_GIL_RELEASE;
	}
	LOCK_RELEASE;
	return rval;
}

/**
 *
 */
int python_exec1(sip_msg_t *_msg, char *method_name, char *foobar)
{
	str method = STR_NULL;
	if(fixup_get_svalue(_msg, (gparam_t *)method_name, &method) < 0) {
		LM_ERR("cannot get the python method to be executed\n");
		return -1;
	}
	return apy_exec(_msg, method.s, NULL, 1);
}

/**
 *
 */
int python_exec2(sip_msg_t *_msg, char *method_name, char *mystr)
{
	str method = STR_NULL;
	str param = STR_NULL;
	if(fixup_get_svalue(_msg, (gparam_t *)method_name, &method) < 0) {
		LM_ERR("cannot get the python method to be executed\n");
		return -1;
	}
	if(fixup_get_svalue(_msg, (gparam_t *)mystr, &param) < 0) {
		LM_ERR("cannot get the parameter of the python method\n");
		return -1;
	}
	return apy_exec(_msg, method.s, param.s, 1);
}
