/**
 * $Id$
 *
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
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

#include <stdio.h>
#include <string.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"

#include "json_funcs.h"

MODULE_VERSION

static int fixup_get_field(void** param, int param_no);
static int fixup_get_field_free(void** param, int param_no);

/* Exported functions */
static cmd_export_t cmds[]={
	{"json_get_field", (cmd_function)json_get_field, 3, 
		fixup_get_field, fixup_get_field_free, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
		"json",
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* Exported functions */
		0,		 /* Exported parameters */
		0,		 /* exported statistics */
		0,	             /* exported MI functions */
		0,				 /* exported pseudo-variables */
		0,				 /* extra processes */
		0,        /* module initialization function */
		0,				 /* response function*/
		0,	 /* destroy function */
		0       /* per-child init function */
};


static int fixup_get_field(void** param, int param_no)
{
  if (param_no == 1 || param_no == 2) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 3) {
		if (fixup_pvar_null(param, 1) != 0) {
		    LM_ERR("failed to fixup result pvar\n");
		    return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
		    LM_ERR("result pvar is not writeble\n");
		    return -1;
		}
		return 0;
	}
	
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_get_field_free(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		LM_WARN("free function has not been defined for spve\n");
		return 0;
	}

	if (param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}
