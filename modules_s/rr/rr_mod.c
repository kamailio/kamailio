/*
 * Route & Record-Route module
 *
 * $Id$
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
 */
/* History:
 * --------
 *  2003-03-11  updated to the new module interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-01  Added record_route with ip address parameter (janakj)
 *  2003-04-14  enable_full_lr parameter introduced (janakj)
 */


#include <stdio.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "loose.h"
#include "record.h"

int append_fromtag = 1;
int enable_double_rr = 1; /* Enable using of 2 RR by default */
int enable_full_lr = 0;   /* Disabled by default */

static int mod_init(void);
static int str_fixup(void** param, int param_no);


/*
 * Exported functions
 */
/*
 * I do not want people to use strict routing so it is disabled by default,
 * you should always use loose routing, if you really need strict routing then
 * you can replace the last zeroes with REQUEST_ROUTE to enable strict_route and
 * record_route_strict. Don't do that unless you know what you are really doing !
 * Oh, BTW, have I mentioned already that you shouldn't use strict routing ?
 */
static cmd_export_t cmds[] = {
	{"loose_route",          loose_route,         0, 0,         REQUEST_ROUTE},
	{"record_route",         record_route,        0, 0,         REQUEST_ROUTE},
	{"record_route_preset",  record_route_preset, 1, str_fixup, REQUEST_ROUTE},
	{"record_route_strict" , record_route_strict, 0, 0,         0            },
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] ={ 
	{"append_fromtag",   INT_PARAM, &append_fromtag  },
	{"enable_double_rr", INT_PARAM, &enable_double_rr},
	{"enable_full_lr",   INT_PARAM, &enable_full_lr  },
	{0, 0, 0 }
};


struct module_exports exports = {
	"rr",
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	mod_init,  /* initialize module */
	0,         /* response function*/
	0,         /* destroy function */
	0,         /* oncancel function */
	0          /* per-child init function */
};


static int mod_init(void)
{
	DBG("rr - initializing\n");
	return 0;
}


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
