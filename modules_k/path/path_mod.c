/*
 * $Id$ 
 *
 * Path handling for intermediate proxies
 *
 * Copyright (C) 2006 Inode GmbH (Andreas Granig <andreas.granig@inode.info>)
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mod_fix.h"
#include "../rr/api.h"

#include "path.h"
#include "path_mod.h"

MODULE_VERSION


/* If received-param of current Route uri should be used
 * as dst-uri. */
int use_received = 0;

/*
 * Module destroy function prototype
 */
static void destroy(void);

/*
 * Module child-init function prototype
 */
static int child_init(int rank);

/*
 * Module initialization function prototype
 */
static int mod_init(void);

/*
 * rr callback API
 */
struct rr_binds path_rrb;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{ "add_path",          (cmd_function)add_path,              0,
			0,              0,  REQUEST_ROUTE },
	{ "add_path",          (cmd_function)add_path_usr,          1,
			fixup_str_null, 0, REQUEST_ROUTE },
	{ "add_path_received", (cmd_function)add_path_received,     0,
			0,              0, REQUEST_ROUTE },
	{ "add_path_received", (cmd_function)add_path_received_usr, 1,
			fixup_str_null, 0, REQUEST_ROUTE },
	{ 0, 0, 0, 0, 0, 0 }
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"use_received", INT_PARAM, &use_received },
	{ 0, 0, 0 }
};


/*
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
	destroy,    /* destroy function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	return 0;
}


static int mod_init(void)
{
	LM_INFO("initializing...\n");

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
	
	return 0;
}


static void destroy(void)
{
}


