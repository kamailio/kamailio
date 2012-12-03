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

/* Return the number of arguments of the application command line */
static PyObject*
router_LM_ERR(PyObject *self, PyObject *args)
{
    char *msg;

    if(!PyArg_ParseTuple(args, "s:LM_ERR", &msg))
        return NULL;

    LM_ERR("%s", msg);

    Py_INCREF(Py_None);
    return Py_None;
}

PyMethodDef RouterMethods[] = {
    {"LM_ERR", router_LM_ERR, METH_VARARGS,
     "Pring error message."},
    {NULL, NULL, 0, NULL}
};
