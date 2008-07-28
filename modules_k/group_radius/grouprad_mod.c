/* 
 * $Id$ 
 *
 * Group membership - module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-02-25 - created by janakj
 *  2003-03-16 - flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free
 */

#include <string.h>
#include <stdlib.h>
#include "../../error.h"
#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../config.h"
#include "../../radius.h"
#include "../../mem/mem.h"
#include "grouprad_mod.h"
#include "group.h"

MODULE_VERSION

void *rh;
struct attr attrs[A_MAX];
struct val vals[V_MAX];

static int mod_init(void); /* Module initialization function */
static int hf_fixup(void** param, int param_no); /* Header field fixup */


/*
 * Module parameter variables
 */
static char* radius_config = DEFAULT_RADIUSCLIENT_CONF;
int use_domain = 0;  /* By default we use domain */


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"radius_is_user_in", (cmd_function)radius_is_user_in, 2, hf_fixup,
			0, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{0, 0, 0, 0, 0, 0}
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
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0           /* child initialization function */
};


static int mod_init(void)
{
	LM_DBG("group_radius - initializing\n");

	memset(attrs, 0, sizeof(attrs));
	memset(vals, 0, sizeof(vals));
	attrs[A_SERVICE_TYPE].n	= "Service-Type";
	attrs[A_USER_NAME].n	= "User-Name";
	attrs[A_SIP_GROUP].n    = "Sip-Group";
	vals[V_GROUP_CHECK].n	= "Group-Check";

	if ((rh = rc_read_config(radius_config)) == NULL) {
		LM_ERR("failed to open configuration file \n");
		return -1;
	}
    
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
		LM_ERR("failed to open dictionary file \n");
		return -2;
	}

	INIT_AV(rh, attrs, A_MAX, vals, V_MAX, "group_radius", -3, -4);

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
			LM_ERR("unsupported Header Field identifier\n");
			return E_UNSPEC;
		}

		pkg_free(ptr);
	} else if (param_no == 2) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LM_ERR("no pkg memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}

