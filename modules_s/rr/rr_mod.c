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
 */


#include <stdio.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "strict.h"
#include "loose.h"
#include "common.h"


int use_fast_cmp = 0;
int append_fromtag = 1;


static int mod_init(void);
static int child_init(int rank);
static int int_fixup(void** param, int param_no);


/*
 * Exported functions
 */
static cmd_export_t cmds[]={
	{"loose_route",  loose_route,   0, 0, REQUEST_ROUTE},
	{"strict_route", strict_route,  0, 0, REQUEST_ROUTE},
	{"record_route", record_route,  0, 0, REQUEST_ROUTE},
	{0,0,0,0,0}
};


/*
 * Exported parameters
 */
static param_export_t params[]={
	{"use_fast_cmp",   INT_PARAM, &use_fast_cmp  },
	{"append_fromtag", INT_PARAM, &append_fromtag},
	{0,0,0}
};


struct module_exports exports = {
	"rr",
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	mod_init,  /* initialize module */
	0,         /* response function*/
	0,         /* destroy function */
	0,         /* oncancel function */
	child_init /* per-child init function */
};


static int mod_init(void)
{
	DBG("rr - initializing\n");
	return 0;
}


static int child_init(int rank)
{
	     /* Different children may be listening on
	      * different IPs or ports and therefore we
	      * must generate hash in each child
	      */
	generate_hash();

	if (generate_rr_suffix()) {
		LOG(L_ERR, "rr:child_init(): Error while generating RR suffix\n");
		return -1;
	}

	return 0;
}


static int int_fixup(void** param, int param_no)
{
	unsigned int qop;
	int err;
	
	if (param_no == 1) {
		qop = str2s(*param, strlen(*param), &err);

		if (err == 0) {
			pkg_free(*param);
			*param=(void*)qop;
		} else {
			LOG(L_ERR, "int_fixup(): Bad number <%s>\n",
			    (char*)(*param));
			return E_UNSPEC;
		}
	}

	return 0;
}
