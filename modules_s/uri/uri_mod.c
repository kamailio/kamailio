/* 
 * $Id$ 
 *
 * Various URI related functions
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *  2003-03-11: New module interface (janakj)
 *  2003-03-16: flags export parameter added (janakj)
 *  2003-03-19  replaces all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-05: default_uri #define used (jiri)
 *  2004-03-20: has_totag introduced (jiri)
 *  2004-04-14: uri_param and add_uri_param introduced (jih)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "uri_mod.h"
#include "checks.h"

MODULE_VERSION


static int str_fixup(void** param, int param_no);
static int uri_fixup(void** param, int param_no);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_user",        is_user,        1, str_fixup, REQUEST_ROUTE},
	{"has_totag", 	   has_totag,      0, 0,         REQUEST_ROUTE},
	{"uri_param",      uri_param_1,    1, str_fixup, REQUEST_ROUTE},
	{"uri_param",      uri_param_2,    2, uri_fixup, REQUEST_ROUTE},
	{"add_uri_param",  add_uri_param,  1, str_fixup, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri", 
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	0,         /* module initialization function */
	0,         /* response function */
        0,         /* destroy function */
	0,         /* oncancel function */
	0          /* child initialization function */
};


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;
	
	if (param_no == 1) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "str_fixup(): No memory left\n");
			return E_UNSPEC;
		}
		
		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}
	
	return 0;
}


/*
 * Convert both uri_param parameters to str* representation
 */
static int uri_fixup(void** param, int param_no)
{
       if (param_no == 1) {
               return str_fixup(param, 1);
       } else if (param_no == 2) {
               return str_fixup(param, 1);
       }
       return 0;
}
