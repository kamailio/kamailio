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
#include <libgen.h>

#include "../../core/str.h"
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/cfg/cfg_struct.h"

#include "python_exec.h"
#include "python_iface.h"
#include "python_msgobj.h"
#include "python_support.h"
#include "app_python_mod.h"

#include "mod_Router.h"
#include "mod_Core.h"
#include "mod_Ranks.h"
#include "mod_Logger.h"

#include "apy_kemi.h"

MODULE_VERSION


str _sr_python_load_file = str_init("/usr/local/etc/" NAME "/handler.py");
static str mod_init_fname = str_init("mod_init");
static str child_init_mname = str_init("child_init");

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

PyObject *_sr_apy_handler_obj;

char *dname = NULL, *bname = NULL;

int _apy_process_rank = 0;

PyThreadState *myThreadState;

/** module parameters */
static param_export_t params[]={
	{"script_name",        PARAM_STR, &_sr_python_load_file },
	{"load",               PARAM_STR, &_sr_python_load_file },
	{"mod_init_function",  PARAM_STR, &mod_init_fname },
	{"child_init_method",  PARAM_STR, &child_init_mname },
	{0,0,0}
};

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{ "python_exec", (cmd_function)python_exec1, 1,  fixup_spve_null,
		0,	ANY_ROUTE },
	{ "python_exec", (cmd_function)python_exec2, 2,  fixup_spve_spve,
		0,	ANY_ROUTE },
	{ 0, 0, 0, 0, 0, 0 }
};

/** module exports */
struct module_exports exports = {
	"app_python",                   /* module name */
	RTLD_NOW | RTLD_GLOBAL,         /* dlopen flags */
	cmds,                           /* exported functions */
	params,                         /* exported parameters */
	0,                              /* exported statistics */
	0,                              /* exported MI functions */
	0,                              /* exported pseudo-variables */
	0,                              /* extra processes */
	mod_init,                       /* module initialization function */
	(response_function) NULL,       /* response handling function */
	(destroy_function) mod_destroy, /* destroy function */
	child_init                      /* per-child init function */
};


/**
 *
 */
static int mod_init(void)
{
	char *dname_src, *bname_src;
	int i;

	if(apy_sr_init_mod()<0) {
		LM_ERR("failed to init the sr mod\n");
		return -1;
	}
	if(app_python_init_rpc()<0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	dname_src = as_asciiz(&_sr_python_load_file);
	bname_src = as_asciiz(&_sr_python_load_file);

	if(dname_src==NULL || bname_src==NULL)
	{
		LM_ERR("no more pkg memory\n");
		if(dname_src) pkg_free(dname_src);
		if(bname_src) pkg_free(bname_src);
		return -1;
	}

	dname = strdup(dirname(dname_src));
	if(dname==NULL) {
		LM_ERR("no more system memory\n");
		pkg_free(dname_src);
		pkg_free(bname_src);
		return -1;
	}
	if (strlen(dname) == 0) {
		free(dname);
		dname = malloc(2);
		if(dname==NULL) {
			LM_ERR("no more system memory\n");
			pkg_free(dname_src);
			pkg_free(bname_src);
			return -1;
		}
		dname[0] = '.';
		dname[1] = '\0';
	}
	bname = strdup(basename(bname_src));
	i = strlen(bname);
	if (bname[i - 1] == 'c' || bname[i - 1] == 'o')
		i -= 1;
	if (bname[i - 3] == '.' && bname[i - 2] == 'p' && bname[i - 1] == 'y') {
		bname[i - 3] = '\0';
	} else {
		LM_ERR("%s: script_name doesn't look like a python script\n",
				_sr_python_load_file.s);
		pkg_free(dname_src);
		pkg_free(bname_src);
		return -1;
	}

	if(apy_load_script()<0) {
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
	_apy_process_rank = rank;
	if (cfg_child_init()) {
		return -1;
	}
	return apy_init_script(rank);
}

/**
 *
 */
static void mod_destroy(void)
{
	if (dname)
		free(dname);	// dname was strdup'ed
	if (bname)
		free(bname);	// bname was strdup'ed

	destroy_mod_Core();
	destroy_mod_Ranks();
	destroy_mod_Logger();
	destroy_mod_Router();
}

int apy_load_script(void)
{
	PyObject *sys_path, *pDir, *pModule, *pFunc, *pArgs;
	PyThreadState *mainThreadState;

	Py_Initialize();
	PyEval_InitThreads();
	mainThreadState = PyThreadState_Get();

	format_exc_obj = InitTracebackModule();

	if (format_exc_obj == NULL || !PyCallable_Check(format_exc_obj))
	{
		Py_XDECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	sys_path = PySys_GetObject("path");
	/* PySys_GetObject doesn't pass reference! No need to DEREF */
	if (sys_path == NULL) {
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"'module' object 'sys' has no attribute 'path'");
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	pDir = PyString_FromString(dname);
	if (pDir == NULL) {
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"PyString_FromString() has failed");
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	PyList_Insert(sys_path, 0, pDir);
	Py_DECREF(pDir);

	if (ap_init_modules() != 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_AttributeError, "init_modules() has failed");
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	if (python_msgobj_init() != 0) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_AttributeError,
					"python_msgobj_init() has failed");
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	pModule = PyImport_ImportModule(bname);
	if (pModule == NULL) {
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_ImportError, "No module named '%s'", bname);
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();

		return -1;
	}

	pFunc = PyObject_GetAttrString(pModule, mod_init_fname.s);
	Py_DECREF(pModule);

	/* pFunc is a new reference */

	if (pFunc == NULL) {
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"'module' object '%s' has no attribute '%s'",
					bname, mod_init_fname.s);
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		Py_XDECREF(pFunc);
		PyEval_ReleaseLock();
		return -1;
	}

	if (!PyCallable_Check(pFunc)) {
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"module object '%s' has is not callable attribute '%s'",
					bname, mod_init_fname.s);
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		Py_XDECREF(pFunc);
		PyEval_ReleaseLock();
		return -1;
	}


	pArgs = PyTuple_New(0);
	if (pArgs == NULL) {
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		Py_DECREF(pFunc);
		PyEval_ReleaseLock();
		return -1;
	}

	_sr_apy_handler_obj = PyObject_CallObject(pFunc, pArgs);

	Py_XDECREF(pFunc);
	Py_XDECREF(pArgs);

	if (_sr_apy_handler_obj == Py_None) {
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_TypeError,
					"Function '%s' of module '%s' has returned None."
					" Should be a class instance.", mod_init_fname.s, bname);
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	if (PyErr_Occurred()) {
		python_handle_exception("mod_init");
		Py_XDECREF(_sr_apy_handler_obj);
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	if (_sr_apy_handler_obj == NULL) {
		LM_ERR("PyObject_CallObject() returned NULL but no exception!\n");
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_TypeError,
					"Function '%s' of module '%s' has returned not returned"
					" object. Should be a class instance.",
					mod_init_fname.s, bname);
		python_handle_exception("mod_init");
		Py_DECREF(format_exc_obj);
		PyEval_ReleaseLock();
		return -1;
	}

	myThreadState = PyThreadState_New(mainThreadState->interp);
	PyEval_ReleaseLock();

	return 0;
}

int apy_init_script(int rank)
{
	PyObject *pFunc, *pArgs, *pValue, *pResult;
	int rval;
	char *classname;

	PyEval_AcquireLock();
	PyThreadState_Swap(myThreadState);

	// get instance class name
	classname = get_instance_class_name(_sr_apy_handler_obj);
	if (classname == NULL)
	{
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"'module' instance has no class name");
		python_handle_exception("child_init");
		Py_DECREF(format_exc_obj);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}

	pFunc = PyObject_GetAttrString(_sr_apy_handler_obj, child_init_mname.s);

	if (pFunc == NULL) {
		python_handle_exception("child_init");
		Py_XDECREF(pFunc);
		Py_DECREF(format_exc_obj);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}

	if (!PyCallable_Check(pFunc)) {
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_AttributeError,
					"class object '%s' has is not callable attribute '%s'",
					classname, mod_init_fname.s);
		python_handle_exception("child_init");
		Py_DECREF(format_exc_obj);
		Py_XDECREF(pFunc);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}

	pArgs = PyTuple_New(1);
	if (pArgs == NULL) {
		python_handle_exception("child_init");
		Py_DECREF(format_exc_obj);
		Py_DECREF(pFunc);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}

	pValue = PyInt_FromLong((long)rank);
	if (pValue == NULL) {
		python_handle_exception("child_init");
		Py_DECREF(format_exc_obj);
		Py_DECREF(pArgs);
		Py_DECREF(pFunc);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}
	PyTuple_SetItem(pArgs, 0, pValue);
	/* pValue has been stolen */

	pResult = PyObject_CallObject(pFunc, pArgs);
	Py_DECREF(pFunc);
	Py_DECREF(pArgs);

	if (PyErr_Occurred()) {
		python_handle_exception("child_init");
		Py_DECREF(format_exc_obj);
		Py_XDECREF(pResult);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}

	if (pResult == NULL) {
		LM_ERR("PyObject_CallObject() returned NULL but no exception!\n");
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}

	if (!PyInt_Check(pResult))
	{
		if (!PyErr_Occurred())
			PyErr_Format(PyExc_TypeError,
					"method '%s' of class '%s' should return 'int' type",
					child_init_mname.s, classname);
		python_handle_exception("child_init");
		Py_DECREF(format_exc_obj);
		Py_XDECREF(pResult);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		return -1;
	}

	rval = PyInt_AsLong(pResult);
	Py_DECREF(pResult);
	PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();

	return rval;
}
/**
 *
 */
static int ki_app_python_exec(sip_msg_t *msg, str *method)
{
	if(method==NULL || method->s==NULL || method->len<=0) {
		LM_ERR("invalid method name\n");
		return -1;
	}
	if(method->s[method->len]!='\0') {
		LM_ERR("invalid terminated method name\n");
		return -1;
	}
	return apy_exec(msg, method->s, NULL, 1);
}

/**
 *
 */
static int ki_app_python_exec_p1(sip_msg_t *msg, str *method, str *p1)
{
	if(method==NULL || method->s==NULL || method->len<=0) {
		LM_ERR("invalid method name\n");
		return -1;
	}
	if(method->s[method->len]!='\0') {
		LM_ERR("invalid terminated method name\n");
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

	return apy_exec(msg, method->s, p1->s, 1);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_app_python_exports[] = {
	{ str_init("app_python"), str_init("exec"),
		SR_KEMIP_INT, ki_app_python_exec,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_python"), str_init("exec_p1"),
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
