/*
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
#include "../../core/sr_module.h"
#include "../../core/error.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "enum.h"

MODULE_VERSION


/*
 * Module parameter variables
 */
str suffix = str_init("e164.arpa.");
str param = str_init("");
str i_branchlabel = str_init("i");
str i_suffix = str_init("e164.arpa.");
str i_bl_alg = str_init("cc");

str service = {0, 0};

/*
 * Exported functions
 */
/* clang-format off */
static cmd_export_t cmds[] = {
	{"enum_query", (cmd_function)enum_query_0, 0, 0, 0, REQUEST_ROUTE},
	{"enum_query", (cmd_function)enum_query_1, 1, fixup_spve_null, 0,
	 REQUEST_ROUTE},
	{"enum_query", (cmd_function)enum_query_2, 2, fixup_spve_spve, 0,
	 REQUEST_ROUTE},
	{"enum_pv_query", (cmd_function)enum_pv_query_1, 1, fixup_spve_null,
	 fixup_free_spve_null, REQUEST_ROUTE},
	{"enum_pv_query", (cmd_function)enum_pv_query_2, 2, fixup_spve_spve,
	 fixup_free_spve_spve, REQUEST_ROUTE},
	{"enum_pv_query", (cmd_function)enum_pv_query_3, 3,
	 fixup_spve_all, fixup_free_spve_all, REQUEST_ROUTE},
	{"is_from_user_enum", (cmd_function)is_from_user_enum_0, 0, 0, 0,
	 REQUEST_ROUTE},
	{"is_from_user_enum", (cmd_function)is_from_user_enum_1, 1,
	 fixup_spve_null, fixup_free_spve_null, REQUEST_ROUTE},
	{"is_from_user_enum", (cmd_function)is_from_user_enum_2, 2,
	 fixup_spve_spve, fixup_free_spve_spve, REQUEST_ROUTE},
	{"i_enum_query", (cmd_function)i_enum_query_0, 0, 0, 0, REQUEST_ROUTE},
	{"i_enum_query", (cmd_function)i_enum_query_1, 1, fixup_spve_null, 0,
	 REQUEST_ROUTE},
	{"i_enum_query", (cmd_function)i_enum_query_2, 2, fixup_spve_spve, 0,
	 REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};
/* clang-format on */

/*
 * Exported parameters
 */
/* clang-format off */
static param_export_t params[] = {
	{"domain_suffix", PARAM_STR, &suffix},
	{"tel_uri_params", PARAM_STR, &param},
	{"branchlabel", PARAM_STR, &i_branchlabel},
	{"i_enum_suffix", PARAM_STR, &i_suffix},
	{"bl_algorithm", PARAM_STR, &i_bl_alg},
	{0, 0, 0}
};
/* clang-format on */

/*
 * Module parameter variables
 */
/* clang-format off */
struct module_exports exports = {
	"enum",				/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	0,					/* module initialization function */
	0,					/* per-child init function */
	0					/* module destroy function */
};
/* clang-format on */

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_enum_exports[] = {
	{ str_init("enum"), str_init("enum_query"),
		SR_KEMIP_INT, ki_enum_query,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("enum_query_suffix"),
		SR_KEMIP_INT, ki_enum_query_suffix,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("enum_query_suffix_service"),
		SR_KEMIP_INT, ki_enum_query_suffix_service,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("enum_pv_query"),
		SR_KEMIP_INT, ki_enum_pv_query,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("enum_pv_query_suffix"),
		SR_KEMIP_INT, ki_enum_pv_query_suffix,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("enum_pv_query_suffix_service"),
		SR_KEMIP_INT, ki_enum_pv_query_suffix_service,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("is_from_user_enum"),
		SR_KEMIP_INT, ki_is_from_user_enum,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("is_from_user_enum_suffix"),
		SR_KEMIP_INT, ki_is_from_user_enum_suffix,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("is_from_user_enum_suffix_service"),
		SR_KEMIP_INT, ki_is_from_user_enum_suffix_service,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("i_enum_query"),
		SR_KEMIP_INT, ki_i_enum_query,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("enum_i_query_suffix"),
		SR_KEMIP_INT, ki_i_enum_query_suffix,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("enum"), str_init("i_enum_query_suffix_service"),
		SR_KEMIP_INT, ki_i_enum_query_suffix_service,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_enum_exports);
	return 0;
}