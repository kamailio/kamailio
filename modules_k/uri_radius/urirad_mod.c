/* 
 * $Id$ 
 *
 * URI checks using Radius
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
 * 2003-03-15 - created by janakj
 * 2003-03-16 - flags export parameter added (janakj)
 * 2008-04-17: Added functions that accept pvar arguments (juhe)
 */


#include "../../dprint.h"
#include "../../config.h"
#include "../../radius.h"
#include "../../sr_module.h"
#include "../../mod_fix.h"
#include "urirad_mod.h"
#include "checks.h"

MODULE_VERSION

struct attr attrs[A_MAX];
struct val vals[V_MAX];
void *rh;

static int mod_init(void); /* Module initialization function */


/*
 * Module parameter variables
 */
static char* radius_config = DEFAULT_RADIUSCLIENT_CONF;
static int service_type = -1;
int use_sip_uri_host = 0;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"radius_does_uri_exist", (cmd_function)radius_does_uri_exist_0,
	 0, 0, 0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"radius_does_uri_exist", (cmd_function)radius_does_uri_exist_1,
	 1, fixup_pvar_null, fixup_free_pvar_null, REQUEST_ROUTE|LOCAL_ROUTE},
	{"radius_does_uri_user_exist",
	 (cmd_function)radius_does_uri_user_exist_0,
	 0, 0, 0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"radius_does_uri_user_exist",
	 (cmd_function)radius_does_uri_user_exist_1,
	 1, fixup_pvar_null, fixup_free_pvar_null, REQUEST_ROUTE|LOCAL_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"radius_config", STR_PARAM, &radius_config},
	{"service_type", INT_PARAM, &service_type},
	{"use_sip_uri_host", INT_PARAM, &use_sip_uri_host},
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri_radius", 
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
	LM_DBG("initializing\n");

	memset(attrs, 0, sizeof(attrs));
	memset(vals, 0, sizeof(vals));
	attrs[A_SERVICE_TYPE].n	= "Service-Type";
	attrs[A_USER_NAME].n	= "User-Name";
	if (use_sip_uri_host) {
	    attrs[A_SIP_URI_HOST].n	= "SIP-URI-Host";
	}
	attrs[A_SIP_AVP].n	= "SIP-AVP";
	vals[V_CALL_CHECK].n	= "Call-Check";

	if ((rh = rc_read_config(radius_config)) == NULL) {
		LM_ERR("opening configuration file failed\n");
		return -1;
	}
    
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
		LM_ERR("opening dictionary file failed\n");
		return -2;
	}

	INIT_AV(rh, attrs, A_MAX, vals, V_MAX, "uri_radius", -3, -4);

	if (service_type != -1)
		vals[V_CALL_CHECK].v = service_type;

	return 0;
}
