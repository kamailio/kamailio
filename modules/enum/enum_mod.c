/*
 * $Id$
 *
 * Enum module
 *
 * Copyright (C) 2002-2008 Juha Heinanen
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
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-12-15: added suffix parameter to enum_query (jh)
 */

/*!
 * \file
 * \brief SIP-router enum :: Enum and E164 related functions 
 * \ingroup enum
 * Module: \ref enum
 */


#include "enum_mod.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "../../mod_fix.h"
#include "enum.h"

MODULE_VERSION

/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Module parameter variables
 */
char* domain_suffix = "e164.arpa.";
char* tel_uri_params = "";

char* branchlabel = "i";
char* i_enum_suffix = "e164.arpa.";
char* bl_algorithm = "cc";


/*
 * Internal module variables
 */
str suffix;
str param;
str service;

str i_suffix;
str i_branchlabel;
str i_bl_alg;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"enum_query", (cmd_function)enum_query_0, 0, 0, 0, REQUEST_ROUTE},
	{"enum_query", (cmd_function)enum_query_1, 1, fixup_spve_null, 0,
	 REQUEST_ROUTE},
	{"enum_query", (cmd_function)enum_query_2, 2, fixup_spve_str, 0,
	 REQUEST_ROUTE},
	{"enum_pv_query", (cmd_function)enum_pv_query_1, 1, fixup_pvar_null,
	 fixup_free_pvar_null, REQUEST_ROUTE},
	{"enum_pv_query", (cmd_function)enum_pv_query_2, 2, fixup_pvar_str,
	 fixup_free_pvar_str, REQUEST_ROUTE},
	{"enum_pv_query", (cmd_function)enum_pv_query_3, 3,
	 fixup_pvar_str_str, fixup_free_pvar_str_str, REQUEST_ROUTE},
	{"is_from_user_enum", (cmd_function)is_from_user_enum_0, 0, 0, 0,
	 REQUEST_ROUTE},
	{"is_from_user_enum", (cmd_function)is_from_user_enum_1, 1,
	 fixup_str_null, fixup_free_str_null, REQUEST_ROUTE},
	{"is_from_user_enum", (cmd_function)is_from_user_enum_2, 2,
	 fixup_str_str, fixup_free_str_str, REQUEST_ROUTE},
	{"i_enum_query", (cmd_function)i_enum_query_0, 0, 0, 0, REQUEST_ROUTE},
	{"i_enum_query", (cmd_function)i_enum_query_1, 1, fixup_str_null, 0,
	 REQUEST_ROUTE},
	{"i_enum_query", (cmd_function)i_enum_query_2, 2, fixup_str_str, 0,
	 REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"domain_suffix", STR_PARAM, &domain_suffix},
	{"tel_uri_params", STR_PARAM, &tel_uri_params},
	{"branchlabel", STR_PARAM, &branchlabel},
	{"i_enum_suffix", STR_PARAM, &i_enum_suffix},
	{"bl_algorithm", STR_PARAM, &bl_algorithm},
	{0, 0, 0}
};


/*
 * Module parameter variables
 */
struct module_exports exports = {
	"enum", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	0,        /* exported statistics */
	0,        /* exported MI functions */
	0,        /* exported pseudo-variables */
	0,        /* extra processes */
	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	suffix.s = domain_suffix;
	suffix.len = strlen(suffix.s);

	param.s = tel_uri_params;
	param.len = strlen(param.s);

	service.len = 0;

	i_suffix.s = i_enum_suffix;
	i_suffix.len = strlen(i_enum_suffix);

	i_branchlabel.s = branchlabel;
	i_branchlabel.len = strlen(branchlabel);

	i_bl_alg.s = bl_algorithm;
	i_bl_alg.len = strlen(bl_algorithm);

	return 0;
}

