/* 
 * $Id$ 
 *
 * Group membership - module interface
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
 * --------
 * 2003-02-25 - created by janakj
 * 2003-03-16 - flags export parameter added (janakj)
 */

#include <string.h>
#include <stdlib.h>
#include <radiusclient.h>
#include "../../error.h"
#include "../../dprint.h"
#include "../../sr_module.h"
#include "grouprad_mod.h"
#include "group.h"


static int mod_init(void); /* Module initialization function */
static int hf_fixup(void** param, int param_no); /* Header field fixup */


/*
 * Module parameter variables
 */
char* radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";
int use_domain = 1;  /* By default we use domain */


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"radius_is_user_in", radius_is_user_in, 2, hf_fixup, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"radius_config", STR_PARAM, &radius_config},
	{"use_domain",    INT_PARAM, &use_domain   },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"group_radius", 
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0,          /* oncancel function */
	0           /* child initialization function */
};


static int mod_init(void)
{
	DBG("group_radius - initializing\n");
	
	if (rc_read_config(radius_config) != 0) {
		LOG(L_ERR, "group_radius: Error opening configuration file \n");
		return -1;
	}
    
	if (rc_read_dictionary(rc_conf_str("dictionary")) != 0) {
		LOG(L_ERR, "group_radius: Error opening dictionary file \n");
		return -2;
	}

	return 0;
}


/*
 * Convert HF description string to hdr_field pointer
 *
 * Supported strings: 
 * "Request-URI", "To", "From", "Credentials"
 */
static int hf_fixup(void** param, int param_no)
{
	void* ptr;
	str* s;

	if (param_no == 1) {
		ptr = *param;
		
		if (!strcasecmp((char*)*param, "Request-URI")) {
			*param = (void*)1;
		} else if (!strcasecmp((char*)*param, "To")) {
			*param = (void*)2;
		} else if (!strcasecmp((char*)*param, "From")) {
			*param = (void*)3;
		} else if (!strcasecmp((char*)*param, "Credentials")) {
			*param = (void*)4;
		} else {
			LOG(L_ERR, "hf_fixup(): Unsupported Header Field identifier\n");
			return E_UNSPEC;
		}

		free(ptr);
	} else if (param_no == 2) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "hf_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}

