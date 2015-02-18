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
#include "mod_Ranks.h"

PyMethodDef RanksMethods[] = {
    {NULL, NULL, 0, NULL}
};

void init_mod_Ranks(void)
{
    ranks_module = Py_InitModule("Router.Ranks", RanksMethods);
    PyDict_SetItemString(main_module_dict, "Ranks", ranks_module);

    PyModule_AddObject(ranks_module, "PROC_MAIN",		PyInt_FromLong((long)PROC_MAIN));		
    PyModule_AddObject(ranks_module, "PROC_TIMER",		PyInt_FromLong((long)PROC_TIMER));
    PyModule_AddObject(ranks_module, "PROC_RPC",		PyInt_FromLong((long)PROC_RPC));
    PyModule_AddObject(ranks_module, "PROC_FIFO",		PyInt_FromLong((long)PROC_FIFO));
    PyModule_AddObject(ranks_module, "PROC_TCP_MAIN",		PyInt_FromLong((long)PROC_TCP_MAIN));
    PyModule_AddObject(ranks_module, "PROC_UNIXSOCK",		PyInt_FromLong((long)PROC_UNIXSOCK));
    PyModule_AddObject(ranks_module, "PROC_ATTENDANT",		PyInt_FromLong((long)PROC_ATTENDANT));
    PyModule_AddObject(ranks_module, "PROC_INIT",		PyInt_FromLong((long)PROC_INIT));
    PyModule_AddObject(ranks_module, "PROC_NOCHLDINIT",		PyInt_FromLong((long)PROC_NOCHLDINIT));
    PyModule_AddObject(ranks_module, "PROC_SIPINIT",		PyInt_FromLong((long)PROC_SIPINIT));
    PyModule_AddObject(ranks_module, "PROC_SIPRPC",		PyInt_FromLong((long)PROC_SIPRPC));
    PyModule_AddObject(ranks_module, "PROC_MIN",		PyInt_FromLong((long)PROC_MIN));

    Py_INCREF(ranks_module);

#ifdef WITH_EXTRA_DEBUG
    LM_ERR("Module 'Router.Ranks' has been initialized\n");
#endif

}

void destroy_mod_Ranks(void)
{
    Py_XDECREF(ranks_module);

#ifdef WITH_EXTRA_DEBUG
    LM_ERR("Module 'Router.Ranks' has been destroyed\n");
#endif

}

