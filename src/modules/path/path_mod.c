/*
 * $Id$ 
 *
 * Path handling for intermediate proxies
 *
 * Copyright (C) 2006 Inode GmbH (Andreas Granig <andreas.granig@inode.info>)
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

/*! \file
 * \brief Path :: Core
 *
 * \ingroup path
 * - Module: path
 */

/*! \defgroup path Path:: Handling of "path" header for intermediate proxies
 * This module is designed to be used at intermediate sip proxies
 * like loadbalancers in front of registrars and proxies. It
 * provides functions for inserting a Path header including a
 * parameter for passing forward the received-URI of a
 * registration to the next hop. It also provides a mechanism for
 * evaluating this parameter in subsequent requests and to set the
 * destination URI according to it.
 *
 * - No developer API
 * - No MI functions
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mod_fix.h"
#include "../outbound/api.h"
#include "../rr/api.h"

#include "path.h"
#include "path_mod.h"

MODULE_VERSION


/*! \brief If received-param of current Route uri should be used
 * as dst-uri. */
int use_received = 0;

/*! \brief
 * Module initialization function prototype
 */
static int mod_init(void);

/*! \brief
 * rr callback API
 */
struct rr_binds path_rrb;

/*! \brief
 * outbound API
 */
ob_api_t path_obb;

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{ "add_path",          (cmd_function)add_path,              0,
			0,              0,  REQUEST_ROUTE },
	{ "add_path",          (cmd_function)add_path_usr,          1,
			fixup_spve_null, 0, REQUEST_ROUTE },
	{ "add_path",          (cmd_function)add_path_usr,          2,
			fixup_spve_spve, 0, REQUEST_ROUTE },
	{ "add_path_received", (cmd_function)add_path_received,     0,
			0,              0, REQUEST_ROUTE },
	{ "add_path_received", (cmd_function)add_path_received_usr, 1,
			fixup_spve_null, 0, REQUEST_ROUTE },
	{ "add_path_received", (cmd_function)add_path_received_usr, 2,
			fixup_spve_spve, 0, REQUEST_ROUTE },
	{ 0, 0, 0, 0, 0, 0 }
};


/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"use_received", INT_PARAM, &use_received },
	{ 0, 0, 0 }
};


/*! \brief
 * Module interface
 */
struct module_exports exports = {
	"path", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,    /* destroy function */
	0           /* child initialization function */
};


static int mod_init(void)
{
	if (use_received) {
		if (load_rr_api(&path_rrb) != 0) {
			LM_ERR("failed to load rr-API\n");
			return -1;
		}
		if (path_rrb.register_rrcb(path_rr_callback, 0) != 0) {
			LM_ERR("failed to register rr callback\n");
			return -1;
		}
	}

	if (ob_load_api(&path_obb) == 0)
		LM_DBG("Bound path module to outbound module\n");
	else {
		LM_INFO("outbound module not available\n");
		memset(&path_obb, 0, sizeof(ob_api_t));
	}
	
	return 0;
}
