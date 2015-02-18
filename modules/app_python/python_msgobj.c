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

#include "../../action.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../dset.h"
#include "../../parser/msg_parser.h"

#include "structmember.h"

typedef struct {
    PyObject_HEAD
    struct sip_msg *msg;
} msgobject;

static PyTypeObject MSGtype;

#define is_msgobject(v)         ((v)->ob_type == &MSGtype)

msgobject *newmsgobject(struct sip_msg *msg)
{
    msgobject *msgp;

    msgp = PyObject_New(msgobject, &MSGtype);
    if (msgp == NULL)
        return NULL;

    msgp->msg = msg;
    return msgp;
}

void msg_invalidate(msgobject *self)
{
    self->msg = NULL;
}

static void msg_dealloc(msgobject *msgp)
{
    PyObject_Del(msgp);
}

static PyObject *msg_copy(msgobject *self)
{
    msgobject *msgp;

    if ((msgp = newmsgobject(self->msg)) == NULL)
        return NULL;

    return (PyObject *)msgp;
}

static PyObject *msg_rewrite_ruri(msgobject *self, PyObject *args)
{
    char *ruri;
    struct action act;
    struct run_act_ctx ra_ctx;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if ((self->msg->first_line).type != SIP_REQUEST) {
        PyErr_SetString(PyExc_RuntimeError, "Not a request message - rewrite is not possible.\n");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if(!PyArg_ParseTuple(args, "s:rewrite_ruri", &ruri))
        return NULL;

    memset(&act, '\0', sizeof(act));

    act.type = SET_URI_T;
    act.val[0].type = STRING_ST;
    act.val[0].u.str.s = ruri;
    act.val[0].u.str.len = strlen(ruri);

    init_run_actions_ctx(&ra_ctx);
    if (do_action(&ra_ctx, &act, self->msg) < 0) {
        LM_ERR("Error in do_action\n");
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *msg_set_dst_uri(msgobject *self, PyObject *args)
{
    str ruri;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if ((self->msg->first_line).type != SIP_REQUEST) {
        PyErr_SetString(PyExc_RuntimeError, "Not a request message - set destination is not possible.\n");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if(!PyArg_ParseTuple(args, "s:set_dst_uri", &ruri.s))
        return NULL;

    ruri.len = strlen(ruri.s);

    if (set_dst_uri(self->msg, &ruri) < 0) {
        LM_ERR("Error in set_dst_uri\n");
        PyErr_SetString(PyExc_RuntimeError, "Error in set_dst_uri\n");
    }
    /* dst_uri changes, so it makes sense to re-use the current uri for
       forking */
    ruri_mark_new(); /* re-use uri for serial forking */

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *msg_getHeader(msgobject *self, PyObject *args)
{
    struct hdr_field *hf;
    str hname, *hbody;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if(!PyArg_ParseTuple(args, "s:getHeader", &hname.s))
        return NULL;
    hname.len = strlen(hname.s);

    parse_headers(self->msg, ~0, 0);
    hbody = NULL;
    for (hf = self->msg->headers; hf != NULL; hf = hf->next) {
        if (hname.len == hf->name.len &&
          strncasecmp(hname.s, hf->name.s, hname.len) == 0) {
            hbody = &(hf->body);
            break;
        }
    }

    if (hbody == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    return PyString_FromStringAndSize(hbody->s, hbody->len);
}

static PyObject *msg_call_function(msgobject *self, PyObject *args)
{
    int i, rval;
    char *fname, *arg1, *arg2;
    sr31_cmd_export_t* fexport;
    struct action *act;
    struct run_act_ctx ra_ctx;
    unsigned mod_ver;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    i = PySequence_Size(args);
    if (i < 1 || i > 3) {
        PyErr_SetString(PyExc_RuntimeError, "call_function() should " \
          "have from 1 to 3 arguments");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if(!PyArg_ParseTuple(args, "s|ss:call_function", &fname, &arg1, &arg2))
        return NULL;

    fexport = find_export_record(fname, i - 1, 0, &mod_ver);
    if (fexport == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "no such function");
        Py_INCREF(Py_None);
        return Py_None;
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
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (fexport->fixup != NULL) {
        if (i >= 3) {
            rval = fexport->fixup(&(act->val[3].u.data), 2);
            if (rval < 0) {
                PyErr_SetString(PyExc_RuntimeError, "Error in fixup (2)");
                Py_INCREF(Py_None);
                return Py_None;
            }
            act->val[3].type = MODFIXUP_ST;
        }
        if (i >= 2) {
            rval = fexport->fixup(&(act->val[2].u.data), 1);
            if (rval < 0) {
                PyErr_SetString(PyExc_RuntimeError, "Error in fixup (1)");
                Py_INCREF(Py_None);
                return Py_None;
            }
            act->val[2].type = MODFIXUP_ST;
        }
        if (i == 1) {
            rval = fexport->fixup(0, 0);
            if (rval < 0) {
                PyErr_SetString(PyExc_RuntimeError, "Error in fixup (0)");
                Py_INCREF(Py_None);
                return Py_None;
            }
        }
    }

    init_run_actions_ctx(&ra_ctx);
    rval = do_action(&ra_ctx, act, self->msg);

    if ((act->val[3].type == MODFIXUP_ST) && (act->val[3].u.data)) {
       pkg_free(act->val[3].u.data);
    }

    if ((act->val[2].type == MODFIXUP_ST) && (act->val[2].u.data)) {
        pkg_free(act->val[2].u.data);
    }

    pkg_free(act);

    return PyInt_FromLong(rval);
}

PyDoc_STRVAR(copy_doc,
"copy() -> msg object\n\
\n\
Return a copy (``clone'') of the msg object.");

static PyMethodDef msg_methods[] = {
    {"copy",          (PyCFunction)msg_copy,          METH_NOARGS,		copy_doc},
    {"rewrite_ruri",  (PyCFunction)msg_rewrite_ruri,  METH_VARARGS,		"Rewrite Request-URI."},
    {"set_dst_uri",   (PyCFunction)msg_set_dst_uri,   METH_VARARGS,		"Set destination URI."},
    {"getHeader",     (PyCFunction)msg_getHeader,     METH_VARARGS,		"Get SIP header field by name."},
    {"call_function", (PyCFunction)msg_call_function, METH_VARARGS,		"Invoke function exported by the other module."},
    {NULL, NULL, 0, NULL} /* sentinel */
};

static PyObject *msg_getType(msgobject *self, PyObject *unused)
{
    const char *rval;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    switch ((self->msg->first_line).type)
    {
	case SIP_REQUEST:
	    rval = "SIP_REQUEST";
	    break;

	case SIP_REPLY:
	    rval = "SIP_REPLY";
	    break;

	default:
            rval = "SIP_INVALID";
            break;
    }

    return PyString_FromString(rval);
}

static PyObject *msg_getMethod(msgobject *self, PyObject *unused)
{
    str *rval;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if ((self->msg->first_line).type != SIP_REQUEST) {
        PyErr_SetString(PyExc_RuntimeError, "Not a request message - no method available.\n");
        Py_INCREF(Py_None);
        return Py_None;
    }
    rval = &((self->msg->first_line).u.request.method);
    return PyString_FromStringAndSize(rval->s, rval->len);
}

static PyObject *msg_getStatus(msgobject *self, PyObject *unused)
{
    str *rval;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if ((self->msg->first_line).type != SIP_REPLY) {
        PyErr_SetString(PyExc_RuntimeError, "Not a non-reply message - no status available.\n");
        Py_INCREF(Py_None);
        return Py_None;
    }

    rval = &((self->msg->first_line).u.reply.status);
    return PyString_FromStringAndSize(rval->s, rval->len);
}

static PyObject *msg_getRURI(msgobject *self, PyObject *unused)
{
    str *rval;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if ((self->msg->first_line).type != SIP_REQUEST) {
        PyErr_SetString(PyExc_RuntimeError, "Not a request message - RURI is not available.\n");
        Py_INCREF(Py_None);
        return Py_None;
    }

    rval = &((self->msg->first_line).u.request.uri);
    return PyString_FromStringAndSize(rval->s, rval->len);
}

static PyObject *msg_get_src_address(msgobject *self, PyObject *unused)
{
    PyObject *src_ip, *src_port, *pyRval;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    src_ip = PyString_FromString(ip_addr2a(&self->msg->rcv.src_ip));
    if (src_ip == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    src_port = PyInt_FromLong(self->msg->rcv.src_port);
    if (src_port == NULL) {
        Py_DECREF(src_ip);
        Py_INCREF(Py_None);
        return Py_None;
    }

    pyRval = PyTuple_Pack(2, src_ip, src_port);
    Py_DECREF(src_ip);
    Py_DECREF(src_port);
    if (pyRval == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    return pyRval;
}

static PyObject *msg_get_dst_address(msgobject *self, PyObject *unused)
{
    PyObject *dst_ip, *dst_port, *pyRval;

    if (self->msg == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "self->msg is NULL");
        Py_INCREF(Py_None);
        return Py_None;
    }

    dst_ip = PyString_FromString(ip_addr2a(&self->msg->rcv.dst_ip));
    if (dst_ip == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    dst_port = PyInt_FromLong(self->msg->rcv.dst_port);
    if (dst_port == NULL) {
        Py_DECREF(dst_ip);
        Py_INCREF(Py_None);
        return Py_None;
    }

    pyRval = PyTuple_Pack(2, dst_ip, dst_port);
    Py_DECREF(dst_ip);
    Py_DECREF(dst_port);
    if (pyRval == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    return pyRval;
}

static PyGetSetDef msg_getseters[] = {
    {"Type",		(getter)msg_getType, NULL, NULL,		"Get message type - \"SIP_REQUEST\" or \"SIP_REPLY\"."},
    {"Method",		(getter)msg_getMethod, NULL, NULL,		"Get SIP method name."},
    {"Status",		(getter)msg_getStatus, NULL, NULL,		"Get SIP status code string."},
    {"RURI",		(getter)msg_getRURI, NULL, NULL,		"Get SIP Request-URI."},
    {"src_address",	(getter)msg_get_src_address, NULL, NULL,	"Get (IP, port) tuple representing source address of the message."},
    {"dst_address",	(getter)msg_get_dst_address, NULL, NULL,	"Get (IP, port) tuple representing destination address of the message."},
    {NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

static PyTypeObject MSGtype = {
#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION >= 6
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,                        /*ob_size*/
#endif
    "Router.msg",             /*tp_name*/
    sizeof(msgobject),        /*tp_size*/
    0,                        /*tp_itemsize*/
    /* methods */
    (destructor)msg_dealloc,  /*tp_dealloc*/
    0,                        /*tp_print*/
    0,                        /*tp_getattr*/
    0,                        /*tp_setattr*/
    0,                        /*tp_compare*/
    0,                        /*tp_repr*/
    0,                        /*tp_as_number*/
    0,                        /*tp_as_sequence*/
    0,                        /*tp_as_mapping*/
    0,                        /*tp_hash*/
    0,                        /*tp_call*/
    0,                        /*tp_str*/
    0,                        /*tp_getattro*/
    0,                        /*tp_setattro*/
    0,                        /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,       /*tp_flags*/
    0,                        /*tp_doc*/
    0,                        /*tp_traverse*/
    0,                        /*tp_clear*/
    0,                        /*tp_richcompare*/
    0,                        /*tp_weaklistoffset*/
    0,                        /*tp_iter*/
    0,                        /*tp_iternext*/
    msg_methods,              /*tp_methods*/
    0,                        /*tp_members*/
    msg_getseters,            /*tp_getset*/
};

int python_msgobj_init(void)
{
    MSGtype.ob_type = &PyType_Type;
    if (PyType_Ready(&MSGtype) < 0)
        return -1;
    return 0;
}
