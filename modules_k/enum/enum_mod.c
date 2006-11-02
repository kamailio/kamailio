/*
 * $Id$
 *
 * Enum module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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

/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Fixup functions
 */
static int str_fixup(void** param, int param_no);
static int enum_fixup(void** param, int param_no);


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
	{"enum_query",        enum_query_0,     0, 0,            REQUEST_ROUTE},
	{"enum_query",        enum_query_1,     1, str_fixup,    REQUEST_ROUTE},
	{"enum_query",        enum_query_2,     2, enum_fixup,   REQUEST_ROUTE},
	{"enum_fquery",       enum_fquery_0,    0, 0,            REQUEST_ROUTE},
	{"enum_fquery",       enum_fquery_1,    1, str_fixup,    REQUEST_ROUTE},
	{"enum_fquery",       enum_fquery_2,    2, enum_fixup,   REQUEST_ROUTE},
	{"is_from_user_enum", is_from_user_enum_0, 0, 0,         REQUEST_ROUTE},
	{"is_from_user_enum", is_from_user_enum_1, 1, str_fixup, REQUEST_ROUTE},
	{"is_from_user_enum", is_from_user_enum_2, 2, enum_fixup,REQUEST_ROUTE},
	{"i_enum_query",      i_enum_query_0,   0, 0,            REQUEST_ROUTE},
	{"i_enum_query",      i_enum_query_1,   1, str_fixup,    REQUEST_ROUTE},
	{"i_enum_query",      i_enum_query_2,   2, enum_fixup,   REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
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
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	0,        /* exported statistics */
	0,        /* exported MI functions */
	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	DBG("enum module - initializing\n");
	
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


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;

	if (param_no == 1) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "authorize_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}

/*
 * Convert both enum_query parameters to str* representation
 */
static int enum_fixup(void** param, int param_no)
{
       if (param_no == 1) {
               return str_fixup(param, 1);
       } else if (param_no == 2) {
               return str_fixup(param, 1);
       }
       return 0;
}
