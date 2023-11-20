/*
 * Copyright (C) 2022 Daniel-Constatin Mierla (daniel@asipto.com)
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
#include <libgen.h>

#include "../../core/str.h"
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/cfg/cfg_struct.h"

#include "app_python3s_mod.h"

#include "apy3s_exception.h"
#include "apy3s_kemi.h"

MODULE_VERSION


str _sr_python_load_file = str_init("/usr/local/etc/" NAME "/" NAME ".py");
str _sr_apy3s_script_init = str_init("");
str _sr_apy3s_script_child_init = str_init("");

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

int w_app_python3s_exec1(sip_msg_t *_msg, char *pmethod, char *p2);
int w_app_python3s_exec2(sip_msg_t *_msg, char *pmethod, char *pparam);

PyObject *_sr_apy3s_handler_script = NULL;
PyObject *_sr_apy3s_format_exc_obj = NULL;

char *_sr_apy3s_dname = NULL;
char *_sr_apy3s_bname = NULL;

int _apy3s_process_rank = 0;

PyThreadState *myThreadState = NULL;

int apy3s_script_init_exec(PyObject *pModule, str *fname, int *vparam);

/** module parameters */
static param_export_t params[] = {{"load", PARAM_STR, &_sr_python_load_file},
		{"script_init", PARAM_STR, &_sr_apy3s_script_init},
		{"script_child_init", PARAM_STR, &_sr_apy3s_script_child_init},

		{0, 0, 0}};

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
		{"app_python3s_exec", (cmd_function)w_app_python3s_exec1, 1,
				fixup_spve_null, 0, ANY_ROUTE},
		{"app_python3s_exec", (cmd_function)w_app_python3s_exec2, 2,
				fixup_spve_spve, 0, ANY_ROUTE},

		{0, 0, 0, 0, 0, 0}};

/** module exports */
struct module_exports exports = {
		"app_python3s",			/* module name */
		RTLD_NOW | RTLD_GLOBAL, /* dlopen flags */
		cmds,					/* exported functions */
		params,					/* exported parameters */
		0,						/* exported rpc functions */
		0,						/* exported pseudo-variables */
		0,						/* response handling function */
		mod_init,				/* module init function */
		child_init,				/* per-child init function */
		mod_destroy				/* destroy function */
};


/**
 *
 */
static int mod_init(void)
{
	char *dname_src, *bname_src;
	int i;

	/*
	 * register the need to be called post-fork of all children
	 * with the special rank PROC_POSTCHILDINIT
	 */
	ksr_module_set_flag(KSRMOD_FLAG_POSTCHILDINIT);

	if(apy_sr_init_mod() < 0) {
		LM_ERR("failed to init the sr mod\n");
		return -1;
	}
	if(app_python3s_init_rpc() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	dname_src = as_asciiz(&_sr_python_load_file);
	bname_src = as_asciiz(&_sr_python_load_file);

	if(dname_src == NULL || bname_src == NULL) {
		LM_ERR("no more pkg memory\n");
		if(dname_src)
			pkg_free(dname_src);
		if(bname_src)
			pkg_free(bname_src);
		return -1;
	}

	_sr_apy3s_dname = strdup(dirname(dname_src));
	if(_sr_apy3s_dname == NULL) {
		LM_ERR("no more system memory\n");
		pkg_free(dname_src);
		pkg_free(bname_src);
		return -1;
	}
	if(strlen(_sr_apy3s_dname) == 0) {
		free(_sr_apy3s_dname);
		_sr_apy3s_dname = malloc(2);
		if(_sr_apy3s_dname == NULL) {
			LM_ERR("no more system memory\n");
			pkg_free(dname_src);
			pkg_free(bname_src);
			return -1;
		}
		_sr_apy3s_dname[0] = '.';
		_sr_apy3s_dname[1] = '\0';
	}
	_sr_apy3s_bname = strdup(basename(bname_src));
	i = strlen(_sr_apy3s_bname);
	if(_sr_apy3s_bname[i - 1] == 'c' || _sr_apy3s_bname[i - 1] == 'o')
		i -= 1;
	if(_sr_apy3s_bname[i - 3] == '.' && _sr_apy3s_bname[i - 2] == 'p'
			&& _sr_apy3s_bname[i - 1] == 'y') {
		_sr_apy3s_bname[i - 3] = '\0';
	} else {
		LM_ERR("%s: script_name doesn't look like a python script\n",
				_sr_python_load_file.s);
		pkg_free(dname_src);
		pkg_free(bname_src);
		return -1;
	}

	if(apy_load_script() < 0) {
		pkg_free(dname_src);
		pkg_free(bname_src);
		LM_ERR("failed to load python script\n");
		return -1;
	}

	pkg_free(dname_src);
	pkg_free(bname_src);
	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	if(rank == PROC_INIT) {
		/*
		 * this is called before any process is forked
		 * so the Python internal state handler
		 * should be called now.
		 */
#if PY_VERSION_HEX >= 0x03070000
		PyOS_BeforeFork();
#endif
		return 0;
	}
	if(rank == PROC_POSTCHILDINIT) {
		/*
		 * this is called after forking of all child
		 * processes
		 */
#if PY_VERSION_HEX >= 0x03070000
		PyOS_AfterFork_Parent();
#endif
		return 0;
	}
	_apy3s_process_rank = rank;

	if(!_ksr_is_main) {
#if PY_VERSION_HEX >= 0x03070000
		PyOS_AfterFork_Child();
#else
		PyOS_AfterFork();
#endif
	}
	if(cfg_child_init()) {
		return -1;
	}
	return apy3s_script_init_exec(
			_sr_apy3s_handler_script, &_sr_apy3s_script_child_init, &rank);
}

/**
 *
 */
static void mod_destroy(void)
{
	if(_sr_apy3s_dname)
		free(_sr_apy3s_dname); // _sr_apy3s_dname was strdup'ed
	if(_sr_apy3s_bname)
		free(_sr_apy3s_bname); // _sr_apy3s_bname was strdup'ed
}


/**
 *
 */
int w_app_python3s_exec1(sip_msg_t *_msg, char *pmethod, char *p2)
{
	str method = STR_NULL;
	if(fixup_get_svalue(_msg, (gparam_t *)pmethod, &method) < 0) {
		LM_ERR("cannot get the python method to be executed\n");
		return -1;
	}
	return apy3s_exec_func(_msg, method.s, NULL, 1);
}

/**
 *
 */
int w_app_python3s_exec2(sip_msg_t *_msg, char *pmethod, char *pparam)
{
	str method = STR_NULL;
	str param = STR_NULL;
	if(fixup_get_svalue(_msg, (gparam_t *)pmethod, &method) < 0) {
		LM_ERR("cannot get the python method to be executed\n");
		return -1;
	}
	if(fixup_get_svalue(_msg, (gparam_t *)pparam, &param) < 0) {
		LM_ERR("cannot get the parameter of the python method\n");
		return -1;
	}
	return apy3s_exec_func(_msg, method.s, param.s, 1);
}

/**
 *
 */
int apy3s_script_init_exec(PyObject *pModule, str *fname, int *vparam)
{
	PyObject *pFunc, *pArgs, *pHandler, *pValue;
	PyGILState_STATE gstate;
	int rval = -1;

	if(fname == NULL || fname->len <= 0) {
		return 0;
	}
	LM_DBG("script init callback: %.*s()\n", fname->len, fname->s);

	gstate = PyGILState_Ensure();
	pFunc = PyObject_GetAttrString(pModule, fname->s);
	/* pFunc is a new reference */

	if(pFunc == NULL || !PyCallable_Check(pFunc)) {
		if(!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"'module' object '%s' has no attribute '%s'",
					_sr_apy3s_bname, fname->s);
		apy3s_handle_exception("script_init");
		Py_XDECREF(pFunc);
		goto error;
	}

	if(vparam == NULL) {
		pArgs = PyTuple_New(0);
		if(pArgs == NULL) {
			apy3s_handle_exception("script_init");
			Py_DECREF(pFunc);
			goto error;
		}
	} else {
		pArgs = PyTuple_New(1);
		if(pArgs == NULL) {
			apy3s_handle_exception("script_init");
			Py_DECREF(pFunc);
			goto error;
		}

		pValue = PyLong_FromLong((long)(*vparam));
		if(pValue == NULL) {
			apy3s_handle_exception("script_init");
			Py_DECREF(pArgs);
			Py_DECREF(pFunc);
			goto error;
		}
		/* pValue moved to pArgs - no direct dec ref */
		PyTuple_SetItem(pArgs, 0, pValue);
	}

	pHandler = PyObject_CallObject(pFunc, pArgs);

	Py_XDECREF(pFunc);
	Py_XDECREF(pArgs);

	if(PyErr_Occurred()) {
		LM_ERR("error exception occurred\n");
		apy3s_handle_exception("script_init");
		if(pHandler != NULL) {
			Py_DECREF(pHandler);
		}
		goto error;
	}

	if(pHandler == NULL) {
		LM_ERR("PyObject_CallObject() returned NULL but no exception!\n");
		if(!PyErr_Occurred())
			PyErr_Format(PyExc_TypeError,
					"Function '%s' of module '%s' has not returned"
					" an object. Should be a class instance.",
					fname->s, _sr_apy3s_bname);
		apy3s_handle_exception("script_init");
		goto error;
	}
	Py_XDECREF(pHandler);
	rval = 0;
error:
	PyGILState_Release(gstate);
	return rval;
}

/**
 *
 */
int apy_reload_script(void)
{
	PyGILState_STATE gstate;
	int rval = -1;

	gstate = PyGILState_Ensure();
	PyObject *pModule = PyImport_ReloadModule(_sr_apy3s_handler_script);
	if(!pModule) {
		if(!PyErr_Occurred())
			PyErr_Format(
					PyExc_ImportError, "Reload module '%s'", _sr_apy3s_bname);
		apy3s_handle_exception("reload_script");
		Py_DECREF(_sr_apy3s_format_exc_obj);
		goto error;
	}
	if(apy3s_script_init_exec(pModule, &_sr_apy3s_script_init, NULL)) {
		LM_ERR("Error calling mod_init on reload\n");
		Py_DECREF(pModule);
		goto error;
	}
	Py_DECREF(_sr_apy3s_handler_script);
	_sr_apy3s_handler_script = pModule;

	if(apy3s_script_init_exec(
			   pModule, &_sr_apy3s_script_child_init, &_apy3s_process_rank)
			< 0) {
		LM_ERR("Failed to run child init callback\n");
		goto error;
	}
	rval = 0;

error:
	PyGILState_Release(gstate);
	return rval;
}

#define INTERNAL_VERSION "1008\n"

/**
 *
 */
int apy_load_script(void)
{
	PyObject *sys_path, *pDir, *pModule;
	PyGILState_STATE gstate;
	int rc, rval = -1;

	if(sr_apy3s_init_ksr() != 0) {
		return -1;
	}

	Py_Initialize();
#if PY_VERSION_HEX < 0x03070000
	PyEval_InitThreads();
#endif
	myThreadState = PyThreadState_Get();

	gstate = PyGILState_Ensure();

	// Py3 does not create a package-like hierarchy of modules
	// make legacy modules importable using Py2 syntax
	// import Router.Logger

	rc = PyRun_SimpleString("import sys\n"
							"import KSR\n"
							"KSR.__version__ = " INTERNAL_VERSION
							"sys.modules['KSR.pv'] = KSR.pv\n"
							"sys.modules['KSR.x'] = KSR.x\n");
	if(rc) {
		LM_ERR("Early imports of modules failed\n");
		goto err;
	}

	_sr_apy3s_format_exc_obj = InitTracebackModule();

	if(_sr_apy3s_format_exc_obj == NULL
			|| !PyCallable_Check(_sr_apy3s_format_exc_obj)) {
		Py_XDECREF(_sr_apy3s_format_exc_obj);
		goto err;
	}

	sys_path = PySys_GetObject("path");
	/* PySys_GetObject doesn't pass reference! No need to DEREF */
	if(sys_path == NULL) {
		if(!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"'module' object 'sys' has no attribute 'path'");
		apy3s_handle_exception("load_script");
		Py_DECREF(_sr_apy3s_format_exc_obj);
		goto err;
	}

	pDir = PyUnicode_FromString(_sr_apy3s_dname);
	if(pDir == NULL) {
		if(!PyErr_Occurred())
			PyErr_Format(
					PyExc_AttributeError, "PyUnicode_FromString() has failed");
		apy3s_handle_exception("load_script");
		Py_DECREF(_sr_apy3s_format_exc_obj);
		goto err;
	}

	PyList_Insert(sys_path, 0, pDir);
	Py_DECREF(pDir);

	pModule = PyImport_ImportModule(_sr_apy3s_bname);
	if(pModule == NULL) {
		if(!PyErr_Occurred())
			PyErr_Format(
					PyExc_ImportError, "No module named '%s'", _sr_apy3s_bname);
		apy3s_handle_exception("load_script");
		Py_DECREF(_sr_apy3s_format_exc_obj);
		goto err;
	}
	if(apy3s_script_init_exec(pModule, &_sr_apy3s_script_init, NULL) != 0) {
		LM_ERR("failed calling script init callback function\n");
		Py_DECREF(pModule);
		goto err;
	}
	_sr_apy3s_handler_script = pModule;

	rval = 0;
err:
	PyGILState_Release(gstate);
	return rval;
}

/**
 *
 */
static int ki_app_python_exec(sip_msg_t *msg, str *method)
{
	if(method == NULL || method->s == NULL || method->len <= 0) {
		LM_ERR("invalid method name\n");
		return -1;
	}
	if(method->s[method->len] != '\0') {
		LM_ERR("invalid terminated method name\n");
		return -1;
	}
	return apy3s_exec_func(msg, method->s, NULL, 1);
}

/**
 *
 */
static int ki_app_python_exec_p1(sip_msg_t *msg, str *method, str *p1)
{
	if(method == NULL || method->s == NULL || method->len <= 0) {
		LM_ERR("invalid method name\n");
		return -1;
	}
	if(method->s[method->len] != '\0') {
		LM_ERR("invalid terminated method name\n");
		return -1;
	}
	if(p1 == NULL || p1->s == NULL || p1->len < 0) {
		LM_ERR("invalid p1 value\n");
		return -1;
	}
	if(p1->s[p1->len] != '\0') {
		LM_ERR("invalid terminated p1 value\n");
		return -1;
	}

	return apy3s_exec_func(msg, method->s, p1->s, 1);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_app_python_exports[] = {
	{ str_init("app_python3s"), str_init("exec"),
		SR_KEMIP_INT, ki_app_python_exec,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_python3s"), str_init("execx"),
		SR_KEMIP_INT, ki_app_python_exec,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_python3s"), str_init("exec_p1"),
		SR_KEMIP_INT, ki_app_python_exec_p1,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	str ename = str_init("python");

	sr_kemi_eng_register(&ename, sr_kemi_config_engine_python);
	sr_kemi_modules_add(sr_kemi_app_python_exports);

	return 0;
}
