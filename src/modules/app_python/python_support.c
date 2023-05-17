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
#include <stdio.h>
#include <stdarg.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"

#include "app_python_mod.h"
#include "python_support.h"

static char *make_message(const char *fmt, va_list ap);

void python_handle_exception(const char *fmt, ...)
{
	PyObject *pResult;
	const char *msg;
	char *buf;
	size_t buflen, msglen;
	PyObject *exception, *v, *tb, *args;
	PyObject *line;
	int i;
	char *srcbuf;
	int exc_exit = 0;
	va_list ap;

	// We don't want to generate traceback when no errors occurred
	if(!PyErr_Occurred())
		return;

	if(fmt == NULL)
		srcbuf = NULL;
	else {
		va_start(ap, fmt);
		srcbuf = make_message(fmt, ap);
		va_end(ap);
	}

	PyErr_Fetch(&exception, &v, &tb);
	PyErr_Clear();
	if(exception == NULL) {
		LM_ERR("Can't get traceback, PyErr_Fetch() has failed.\n");
		if(srcbuf)
			pkg_free(srcbuf);
		return;
	}

	PyErr_NormalizeException(&exception, &v, &tb);
	if(exception == NULL) {
		LM_ERR("Can't get traceback, PyErr_NormalizeException() has failed.\n");
		if(srcbuf)
			pkg_free(srcbuf);
		return;
	}

	exc_exit = PyErr_GivenExceptionMatches(exception, PyExc_SystemExit);
	args = PyTuple_Pack(3, exception, v, tb ? tb : Py_None);
	Py_XDECREF(exception);
	Py_XDECREF(v);
	Py_XDECREF(tb);
	if(args == NULL) {
		LM_ERR("Can't get traceback, PyTuple_Pack() has failed.\n");
		if(srcbuf)
			pkg_free(srcbuf);
		return;
	}

	pResult = PyObject_CallObject(format_exc_obj, args);
	Py_DECREF(args);
	if(pResult == NULL) {
		LM_ERR("Can't get traceback, traceback.format_exception() has "
			   "failed.\n");
		if(srcbuf)
			pkg_free(srcbuf);
		return;
	}

	buflen = 1;
	buf = (char *)pkg_realloc(NULL, buflen * sizeof(char));
	if(!buf) {
		PKG_MEM_ERROR;
		if(srcbuf)
			pkg_free(srcbuf);
		return;
	}
	memset(buf, 0, buflen * sizeof(char));

	for(i = 0; i < PySequence_Size(pResult); i++) {
		line = PySequence_GetItem(pResult, i);
		if(line == NULL) {
			LM_ERR("Can't get traceback, PySequence_GetItem() has failed.\n");
			Py_DECREF(pResult);
			if(buf)
				pkg_free(buf);
			if(srcbuf)
				pkg_free(srcbuf);
			return;
		}

		msg = PyString_AsString(line);

		if(msg == NULL) {
			LM_ERR("Can't get traceback, PyString_AsString() has failed.\n");
			Py_DECREF(line);
			Py_DECREF(pResult);
			if(buf)
				pkg_free(buf);
			if(srcbuf)
				pkg_free(srcbuf);
			return;
		}

		msglen = strlen(msg);
		buflen += ++msglen;

		buf = (char *)pkg_reallocxf(buf, buflen * sizeof(char));
		if(!buf) {
			PKG_MEM_ERROR;
			Py_DECREF(line);
			Py_DECREF(pResult);
			if(srcbuf)
				pkg_free(srcbuf);
			return;
		}

		strncat(buf, msg, msglen >= buflen ? buflen - 1 : msglen);

		Py_DECREF(line);
	}

	if(srcbuf == NULL) {
		if(exc_exit) {
			LM_DBG("Unhandled exception in the Python code:\n%s", buf);
		} else {
			LM_ERR("Unhandled exception in the Python code:\n%s", buf);
		}
	} else {
		if(exc_exit) {
			LM_DBG("%s: Unhandled exception in the Python code:\n%s", srcbuf,
					buf);
		} else {
			LM_ERR("%s: Unhandled exception in the Python code:\n%s", srcbuf,
					buf);
		}
	}

	if(buf)
		pkg_free(buf);

	if(srcbuf)
		pkg_free(srcbuf);

	Py_DECREF(pResult);
}


PyObject *InitTracebackModule()
{
	PyObject *pModule, *pTracebackObject;

	pModule = PyImport_ImportModule("traceback");
	if(pModule == NULL) {
		LM_ERR("Cannot import module 'traceback'.\n");
		return NULL;
	}

	pTracebackObject = PyObject_GetAttrString(pModule, "format_exception");
	Py_DECREF(pModule);
	if(pTracebackObject == NULL || !PyCallable_Check(pTracebackObject)) {
		LM_ERR("AttributeError: 'module' object 'traceback' has no attribute"
			   " 'format_exception'.\n");
		Py_XDECREF(pTracebackObject);
		return NULL;
	}

	return pTracebackObject;
}


static char *make_message(const char *fmt, va_list ap)
{
	int n;
	size_t size;
	char *p, *np;

	size = 100; /* Guess we need no more than 100 bytes. */
	p = (char *)pkg_realloc(NULL, size * sizeof(char));
	if(!p) {
		PKG_MEM_ERROR;
		return NULL;
	}
	memset(p, 0, size * sizeof(char));

	while(1) {
		n = vsnprintf(p, size, fmt, ap);

		if(n > -1 && n < size)
			return p;

		if(n > -1) /* glibc 2.1 */
			size = n + 1;
		else /* glibc 2.0 */
			size *= 2;

		np = (char *)pkg_realloc(p, size * sizeof(char));
		if(!np) {
			PKG_MEM_ERROR;
			if(p)
				pkg_free(p);
			return NULL;
		} else
			p = np;
	}

	return NULL; // shall not happened, but who knows ;)
}

char *get_class_name(PyObject *y)
{
	PyObject *p;
	char *name;

	p = PyObject_GetAttrString(y, "__name__");
	if(p == NULL || p == Py_None) {
		Py_XDECREF(p);
		return NULL;
	}

	name = PyString_AsString(p);
	Py_XDECREF(p);

	return name;
}


char *get_instance_class_name(PyObject *y)
{
	PyObject *p, *n;
	char *name;

	n = PyObject_GetAttrString(y, "__class__");
	if(n == NULL || n == Py_None) {
		Py_XDECREF(n);
		return NULL;
	}

	p = PyObject_GetAttrString(n, "__name__");
	if(p == NULL || p == Py_None) {
		Py_XDECREF(p);
		return NULL;
	}

	name = PyString_AsString(p);
	Py_XDECREF(p);
	Py_XDECREF(n);

	return name;
}
