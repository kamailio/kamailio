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
#include "../../core/str.h"
#include "../../core/sr_module.h"

// local includes
#include "python_exec.h"
#include "app_python3_mod.h"
#include "python_iface.h"
#include "python_msgobj.h"
#include "python_support.h"

#include "mod_Router.h"
#include "mod_Ranks.h"

PyObject *_sr_apy_ranks_module = NULL;

PyMethodDef RanksMethods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef Router_Ranks_moduledef = {
        PyModuleDef_HEAD_INIT,
        "Router.Ranks",
        NULL,
        -1,
        RanksMethods,
        NULL,
        NULL,
        NULL,
        NULL
};

PyObject* get_ranks_module(void)
{
	_sr_apy_ranks_module = PyModule_Create(&Router_Ranks_moduledef);

	PyModule_AddObject(_sr_apy_ranks_module, "PROC_MAIN",
			PyLong_FromLong((long)PROC_MAIN));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_TIMER",
			PyLong_FromLong((long)PROC_TIMER));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_RPC",
			PyLong_FromLong((long)PROC_RPC));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_FIFO",
			PyLong_FromLong((long)PROC_FIFO));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_TCP_MAIN",
			PyLong_FromLong((long)PROC_TCP_MAIN));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_UNIXSOCK",
			PyLong_FromLong((long)PROC_UNIXSOCK));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_ATTENDANT",
			PyLong_FromLong((long)PROC_ATTENDANT));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_INIT",
			PyLong_FromLong((long)PROC_INIT));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_NOCHLDINIT",
			PyLong_FromLong((long)PROC_NOCHLDINIT));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_SIPINIT",
			PyLong_FromLong((long)PROC_SIPINIT));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_SIPRPC",
			PyLong_FromLong((long)PROC_SIPRPC));
	PyModule_AddObject(_sr_apy_ranks_module, "PROC_MIN",
			PyLong_FromLong((long)PROC_MIN));

	Py_INCREF(_sr_apy_ranks_module);

#ifdef WITH_EXTRA_DEBUG
	LM_ERR("Module 'Router.Ranks' has been initialized\n");
#endif

	return _sr_apy_ranks_module;
}

void destroy_mod_Ranks(void)
{
	Py_XDECREF(_sr_apy_ranks_module);

#ifdef WITH_EXTRA_DEBUG
	LM_ERR("Module 'Router.Ranks' has been destroyed\n");
#endif

}

