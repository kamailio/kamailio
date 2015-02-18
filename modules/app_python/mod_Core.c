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
#include "mod_Core.h"

PyMethodDef CoreMethods[] = {
    {NULL, NULL, 0, NULL}
};

void init_mod_Core(void)
{
    core_module = Py_InitModule("Router.Core", CoreMethods);
    PyDict_SetItemString(main_module_dict, "Core", core_module);

    Py_INCREF(core_module);

#ifdef WITH_EXTRA_DEBUG
    LM_ERR("Module 'Router.Core' has been initialized\n");
#endif

}

void destroy_mod_Core(void)
{
    Py_XDECREF(core_module);
#ifdef WITH_EXTRA_DEBUG
    LM_ERR("Module 'Router.Core' has been destroyed\n");
#endif

}
