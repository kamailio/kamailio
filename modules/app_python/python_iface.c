/* $Id$
 *
 * Copyright (C) 2009 Sippy Software, Inc., http://www.sippysoft.com
 *
 * This file is part of SIP-Router, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include <Python.h>

#include "../../action.h"
#include "../../dprint.h"
#include "../../route_struct.h"
#include "python_exec.h"


/*
 * Python method: LM_GEN1(self, int log_level, str msg)
 */
static PyObject *router_LM_GEN1(PyObject *self, PyObject *args)
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
static PyObject *router_LM_GEN2(PyObject *self, PyObject *args)
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
static PyObject *router_LM_ALERT(PyObject *self, PyObject *args)
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
static PyObject *router_LM_CRIT(PyObject *self, PyObject *args)
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
static PyObject *router_LM_WARN(PyObject *self, PyObject *args)
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
static PyObject *router_LM_NOTICE(PyObject *self, PyObject *args)
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
static PyObject *router_LM_ERR(PyObject *self, PyObject *args)
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
static PyObject *router_LM_INFO(PyObject *self, PyObject *args)
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
static PyObject *router_LM_DBG(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_DBG", &msg))
        return NULL;

    LM_DBG("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}


PyMethodDef RouterMethods[] = {
    {"LM_GEN1",		router_LM_GEN1,		METH_VARARGS, "Print GEN1 message."},
    {"LM_GEN2",		router_LM_GEN2,		METH_VARARGS, "Print GEN2 message."},
    {"LM_ALERT",	router_LM_ALERT,	METH_VARARGS, "Print alert message."},
    {"LM_CRIT",		router_LM_CRIT,		METH_VARARGS, "Print critical message."},
    {"LM_ERR",		router_LM_ERR,		METH_VARARGS, "Print error message."},
    {"LM_WARN",		router_LM_WARN,		METH_VARARGS, "Print warning message."},
    {"LM_NOTICE",	router_LM_NOTICE,	METH_VARARGS, "Print notice message."},
    {"LM_INFO",		router_LM_INFO,		METH_VARARGS, "Print info message."},
    {"LM_DBG",		router_LM_DBG,		METH_VARARGS, "Print debug message."},
    {NULL, 		NULL, 			0, 		NULL}
};

/*
 * Default module properties
 */
void RouterAddProperties(PyObject *m)
{
    /*
    * Log levels
    * Reference: dprint.h
    */
    PyModule_AddObject(m, "L_ALERT",  PyInt_FromLong((long)L_ALERT));
    PyModule_AddObject(m, "L_BUG",    PyInt_FromLong((long)L_BUG));
    PyModule_AddObject(m, "L_CRIT2",  PyInt_FromLong((long)L_CRIT2)); /* like L_CRIT, but adds prefix */
    PyModule_AddObject(m, "L_CRIT",   PyInt_FromLong((long)L_CRIT));  /* no prefix added */
    PyModule_AddObject(m, "L_ERR",    PyInt_FromLong((long)L_ERR));
    PyModule_AddObject(m, "L_WARN",   PyInt_FromLong((long)L_WARN));
    PyModule_AddObject(m, "L_NOTICE", PyInt_FromLong((long)L_NOTICE));
    PyModule_AddObject(m, "L_INFO",   PyInt_FromLong((long)L_INFO));
    PyModule_AddObject(m, "L_DBG",    PyInt_FromLong((long)L_DBG));

    /*
    * Facility
    * Reference: dprint.h
    */
    PyModule_AddObject(m, "DEFAULT_FACILITY", PyInt_FromLong((long)DEFAULT_FACILITY));
}

