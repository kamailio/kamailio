/**
 * Copyright (C) 2012 Konstantin Mosesov
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

// Python includes
#include <Python.h>
#include "structmember.h"

// Other/system includes
#include <libgen.h>

// router includes
#include "../../str.h"
#include "../../sr_module.h"

// local includes
#include "python_exec.h"
#include "python_mod.h"
#include "python_iface.h"
#include "python_msgobj.h"
#include "python_support.h"

#include "mod_Router.h"
#include "mod_Logger.h"

/*
 * Python method: LM_GEN1(self, int log_level, str msg)
 */
static PyObject *logger_LM_GEN1(PyObject *self, PyObject *args)
{
    int log_level;
    char *msg;

    if (!PyArg_ParseTuple(args, "is:LM_GEN1", &log_level, &msg))
        return NULL;

    LM_GEN1(log_level, "%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Python method: LM_GEN2(self, int log_facility, int log_level, str msg)
 */
static PyObject *logger_LM_GEN2(PyObject *self, PyObject *args)
{
    int log_facility;
    int log_level;
    char *msg;

    if(!PyArg_ParseTuple(args, "iis:LM_GEN2", &log_facility, &log_level, &msg))
        return NULL;

    LM_GEN2(log_facility, log_level, "%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Python method: LM_ALERT(self, str msg)
 */
static PyObject *logger_LM_ALERT(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_ALERT", &msg))
        return NULL;

    LM_ALERT("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * Python method: LM_CRIT(self, str msg)
 */
static PyObject *logger_LM_CRIT(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_CRIT", &msg))
        return NULL;

    LM_CRIT("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Python method: LM_WARN(self, str msg)
 */
static PyObject *logger_LM_WARN(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_WARN", &msg))
        return NULL;

    LM_WARN("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Python method: LM_NOTICE(self, str msg)
 */
static PyObject *logger_LM_NOTICE(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_NOTICE", &msg))
        return NULL;

    LM_NOTICE("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Python method: LM_ERR(self, str msg)
 */
static PyObject *logger_LM_ERR(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_ERR", &msg))
        return NULL;

    LM_ERR("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Python method: LM_INFO(self, str msg)
 */
static PyObject *logger_LM_INFO(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_INFO", &msg))
        return NULL;

    LM_INFO("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Python method: LM_DBG(self, str msg)
 */
static PyObject *logger_LM_DBG(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_DBG", &msg))
        return NULL;

    LM_DBG("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

PyMethodDef LoggerMethods[] = {
    {"LM_GEN1",		(PyCFunction)logger_LM_GEN1,		METH_VARARGS, "Print GEN1 message."},
    {"LM_GEN2",		(PyCFunction)logger_LM_GEN2,		METH_VARARGS, "Print GEN2 message."},
    {"LM_ALERT",	(PyCFunction)logger_LM_ALERT,		METH_VARARGS, "Print alert message."},
    {"LM_CRIT",		(PyCFunction)logger_LM_CRIT,		METH_VARARGS, "Print critical message."},
    {"LM_ERR",		(PyCFunction)logger_LM_ERR,		METH_VARARGS, "Print error message."},
    {"LM_WARN",		(PyCFunction)logger_LM_WARN,		METH_VARARGS, "Print warning message."},
    {"LM_NOTICE",	(PyCFunction)logger_LM_NOTICE,		METH_VARARGS, "Print notice message."},
    {"LM_INFO",		(PyCFunction)logger_LM_INFO,		METH_VARARGS, "Print info message."},
    {"LM_DBG",		(PyCFunction)logger_LM_DBG,		METH_VARARGS, "Print debug message."},
    {NULL, 		NULL, 			0, 		NULL}
};

void init_mod_Logger(void)
{
    logger_module = Py_InitModule("Router.Logger", LoggerMethods);
    PyDict_SetItemString(main_module_dict, "Logger", logger_module);

    /*
    * Log levels
    * Reference: dprint.h
    */
    PyModule_AddObject(logger_module, "L_ALERT",  PyInt_FromLong((long)L_ALERT));
    PyModule_AddObject(logger_module, "L_BUG",    PyInt_FromLong((long)L_BUG));
    PyModule_AddObject(logger_module, "L_CRIT2",  PyInt_FromLong((long)L_CRIT2)); /* like L_CRIT, but adds prefix */
    PyModule_AddObject(logger_module, "L_CRIT",   PyInt_FromLong((long)L_CRIT));  /* no prefix added */
    PyModule_AddObject(logger_module, "L_ERR",    PyInt_FromLong((long)L_ERR));
    PyModule_AddObject(logger_module, "L_WARN",   PyInt_FromLong((long)L_WARN));
    PyModule_AddObject(logger_module, "L_NOTICE", PyInt_FromLong((long)L_NOTICE));
    PyModule_AddObject(logger_module, "L_INFO",   PyInt_FromLong((long)L_INFO));
    PyModule_AddObject(logger_module, "L_DBG",    PyInt_FromLong((long)L_DBG));

    /*
    * Facility
    * Reference: dprint.h
    */
    PyModule_AddObject(logger_module, "DEFAULT_FACILITY", PyInt_FromLong((long)DEFAULT_FACILITY));

    Py_INCREF(logger_module);

#ifdef WITH_EXTRA_DEBUG
    LM_ERR("Module 'Router.Logger' has been initialized\n");
#endif

}

void destroy_mod_Logger(void)
{
    Py_XDECREF(logger_module);

#ifdef WITH_EXTRA_DEBUG
    LM_ERR("Module 'Router.Logger' has been destroyed\n");
#endif

}

