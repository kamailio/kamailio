/**
 * $Id$
 *
 * Copyright (C) 2008 Daniel-Constantin Mierla (asipto.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../pvar.h"
#include "pv_branch.h"
#include "pv_core.h"
#include "pv_stats.h"


MODULE_VERSION

static pv_export_t mod_pvs[] = {
	{ {"branch", sizeof("branch")-1}, /* branch attributes */
		101, pv_get_branchx, pv_set_branchx,
		pv_parse_branchx_name, pv_parse_index, 0, 0 },
	{ {"mi", (sizeof("mi")-1)}, /* message id */
		PVT_MSGID, pv_get_msgid, 0,
		0, 0, 0, 0},
	{ {"stat", sizeof("stat")-1}, /* statistics */
		101, pv_get_stat, 0,
		pv_parse_stat_name, 0, 0, 0 },

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


/** module exports */
struct module_exports exports= {
	"pv",
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,
	0,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	0,          /* module initialization function */
	0,
	0,
	0           /* per-child init function */
};

