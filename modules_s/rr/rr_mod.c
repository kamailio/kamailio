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


#include <stdio.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../error.h"
#include "strict.h"
#include "loose.h"
#include "common.h"


int use_fast_cmp = 0;
int rr_append_fromtag = 1;


static int mod_init(void);
static int child_init(int rank);
static int int_fixup(void** param, int param_no);


struct module_exports exports = {
	"rr",
	(char*[]) {
		"loose_route",
		"strict_route",
		"record_route"
	},
	(cmd_function[]) {
		loose_route,
		strict_route,
		record_route
	},
	(int[]) {
		0,
		0,
		1
	},
	(fixup_function[]) {
		0,
		0,
		int_fixup
	},
	3, /* number of functions*/

	(char*[]) { /* Module parameter names */
		"use_fast_cmp",
		"append_fromtag"
	},
	(modparam_t[]) {   /* Module parameter types */
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&use_fast_cmp,
		&rr_append_fromtag
	},
	2,         /* Number of module paramers */

	mod_init,  /* initialize module */
	0,         /* response function*/
	0,         /* destroy function */
	0,         /* oncancel function */
	child_init /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "Record Route - initializing\n");
	return 0;
}


static int child_init(int rank)
{
	     /* Different children may be listening on
	      * different IPs or ports and therefore we
	      * must generate hash in every child
	      */
	generate_hash();

	if (generate_rr_suffix()) {
		LOG(L_ERR, "child_init: Error while generating RR suffix\n");
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
			free(*param);
			*param=(void*)qop;
		} else {
			LOG(L_ERR, "int_fixup(): Bad number <%s>\n",
			    (char*)(*param));
			return E_UNSPEC;
		}
	}

	return 0;
}
