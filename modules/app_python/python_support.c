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
#include <stdio.h>

#include "../../dprint.h"
#include "python_mod.h"

void
python_handle_exception(const char *fname)
{
    PyObject *pResult;
    const char *msg;
    PyObject *exception, *v, *tb, *args;
    PyObject *line;
    int i;

    LM_ERR("%s: Unhandled exception in the Python code:\n", fname);
    PyErr_Fetch(&exception, &v, &tb);
    PyErr_Clear();
    if (exception == NULL) {
        LM_ERR("can't get traceback, PyErr_Fetch() has failed\n");
        return;
    }
    PyErr_NormalizeException(&exception, &v, &tb);
    if (exception == NULL) {
        LM_ERR("can't get traceback, PyErr_NormalizeException() has failed\n");
        return;
    }
    args = PyTuple_Pack(3, exception, v, tb ? tb : Py_None);
    Py_XDECREF(exception);
    Py_XDECREF(v);
    Py_XDECREF(tb);
    if (args == NULL) {
        LM_ERR("can't get traceback, PyTuple_Pack() has failed\n");
        return;
    }
    pResult = PyObject_CallObject(format_exc_obj, args);
    Py_DECREF(args);
    if (pResult == NULL) {
        LM_ERR("can't get traceback, traceback.format_exception() has failed\n");
        return;
    }
    for (i = 0; i < PySequence_Size(pResult); i++) {
        line = PySequence_GetItem(pResult, i);
        if (line == NULL) {
            LM_ERR("can't get traceback, PySequence_GetItem() has failed\n");
            Py_DECREF(pResult);
            return;
        }
        msg = PyString_AsString(line);
        if (msg == NULL) {
            LM_ERR("can't get traceback, PyString_AsString() has failed\n");
            Py_DECREF(line);
            Py_DECREF(pResult);
            return;
        }
        LM_ERR("\t%s", msg);
        Py_DECREF(line);
    }
    Py_DECREF(pResult);
}
