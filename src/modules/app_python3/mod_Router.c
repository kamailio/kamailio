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
#include "../../core/str.h"
#include "../../core/sr_module.h"

// local includes
#include "python_exec.h"
#include "app_python3_mod.h"
#include "python_iface.h"
#include "python_msgobj.h"
#include "python_support.h"

#include "mod_Router.h"
#include "mod_Core.h"
#include "mod_Logger.h"
#include "mod_Ranks.h"

PyObject *_sr_apy_main_module = NULL;

PyMethodDef RouterMethods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef Router_Core_moduledef = {
        PyModuleDef_HEAD_INIT,
        "Router",
        NULL,
        -1,
        RouterMethods,
        NULL,
        NULL,
        NULL,
        NULL
};

extern PyObject* get_core_module();
extern PyObject* get_logger_module();
extern PyObject* get_ranks_module();
static PyObject* init_Router(void)
{
	_sr_apy_main_module = PyModule_Create(&Router_Core_moduledef);
    PyModule_AddObject(_sr_apy_main_module, "Core", get_core_module());
    PyModule_AddObject(_sr_apy_main_module, "Logger", get_logger_module());
    PyModule_AddObject(_sr_apy_main_module, "Ranks", get_ranks_module());

	Py_INCREF(_sr_apy_main_module);

    return _sr_apy_main_module;

}

extern
void init_mod_Router(void)
{

    PyImport_AppendInittab("Router", &init_Router);

#ifdef WITH_EXTRA_DEBUG
	LM_ERR("Module 'Router' has been initialized\n");
#endif

}

void destroy_mod_Router(void)
{
	Py_XDECREF(_sr_apy_main_module);

#ifdef WITH_EXTRA_DEBUG
	LM_ERR("Module 'Router' has been destroyed\n");
#endif

}

