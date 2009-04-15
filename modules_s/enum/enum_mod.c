/*
 * $Id$
 *
 * Enum module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-12-15: added suffix parameter to enum_query (jh)
 */


#include "enum_mod.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "enum.h"

MODULE_VERSION

str domain_suffix = STR_STATIC_INIT("e164.arpa.");
str tel_uri_params = STR_STATIC_INIT("");
str default_service = STR_NULL;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"enum_query", enum_query, 0, 0,                REQUEST_ROUTE},
	{"enum_query", enum_query, 1, fixup_var_str_1,  REQUEST_ROUTE},
	{"enum_query", enum_query, 2, fixup_var_str_12, REQUEST_ROUTE},
	{"is_e164",    is_e164,    1, fixup_var_str_1,  REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
        {"domain_suffix",   PARAM_STR, &domain_suffix },
        {"tel_uri_params",  PARAM_STR, &tel_uri_params},
	{"default_service", PARAM_STR, &default_service},
	{0, 0, 0}
};


/*
 * Module parameter variables
 */
struct module_exports exports = {
	"enum",
	cmds,     /* Exported functions */
	0,        /* RPC method */
	params,   /* Exported parameters */
	0,        /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};
