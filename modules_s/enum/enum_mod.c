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
 */


#include "enum_mod.h"
#include <stdio.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "enum.h"
/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Fixup functions
 */
static int str_fixup(void** param, int param_no);


/*
 * Module parameter variables
 */
char* domain_suffix = "e164.arpa.";


/*
 * Internal module variables
 */
str suffix;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"enum_query",        enum_query,        1, str_fixup, REQUEST_ROUTE},
	{"is_from_user_e164", is_from_user_e164, 0, 0,         REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
        {"domain_suffix", STR_PARAM, &domain_suffix},
	{0, 0, 0}
};


/*
 * Module parameter variables
 */
struct module_exports exports = {
	"enum", 
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	DBG("enum module - initializing\n");
	
	suffix.s = domain_suffix;
	suffix.len = strlen(suffix.s);

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
