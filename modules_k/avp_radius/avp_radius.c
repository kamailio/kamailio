/*
 * $Id$
 *
 * Copyright (C) 2004-2008 Juha Heinanen <jh@tutpro.com>
 * Copyright (C) 2004 FhG Fokus
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
 * -------
 * 2005-07-08: Radius AVP may contain any kind of Kamailio AVP - ID/name or
 *             int/str value (bogdan)
 * 2008-09-03  New implementation where avp_load_radius() function is replaced
 *             by radius_load_caller_avps() and radius_load_callee_avps(callee) 
 *             functions that take caller and callee as string parameter that
 *             may contain pseudo variables.  Support for adding function
 *             specific extra attributes defined by module parameters.
 */

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../parser/digest/digest_parser.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "../../config.h"
#include "../../radius.h"
#include "../../mod_fix.h"
#include "avp_radius.h"
#include "functions.h"
#include "extra.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

static char *radius_config = DEFAULT_RADIUSCLIENT_CONF;
static int caller_service_type = -1;
static int callee_service_type = -1;

void *rh;
static char *caller_extra_str = 0;
struct extra_attr *caller_extra = 0;
static char *callee_extra_str = 0;
struct extra_attr *callee_extra = 0;

/* Caller and callee AVP attributes and values */
struct attr caller_attrs[SA_STATIC_MAX+MAX_EXTRA];
struct attr callee_attrs[SA_STATIC_MAX+MAX_EXTRA];
struct val caller_vals[RV_STATIC_MAX];
struct val callee_vals[EV_STATIC_MAX];


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"radius_load_caller_avps", (cmd_function)radius_load_caller_avps, 1,
     fixup_spve_null, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"radius_load_callee_avps", (cmd_function)radius_load_callee_avps, 1,
     fixup_spve_null, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"radius_config",       STR_PARAM, &radius_config      },
    {"caller_service_type", INT_PARAM, &caller_service_type},
    {"callee_service_type", INT_PARAM, &callee_service_type},
    {"caller_extra",        STR_PARAM, &caller_extra_str   },
    {"callee_extra",        STR_PARAM, &callee_extra_str   },
    {0, 0, 0}
};	


struct module_exports exports = {
    "avp_radius", 
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,      /* Exported commands */
    params,    /* Exported parameters */
    0,         /* exported statistics */
    0,         /* exported MI functions */
    0,         /* exported pseudo-variables */
    0,         /* extra processes */
    mod_init,  /* module initialization function */
    0,         /* response function*/
    destroy,   /* destroy function */
    0          /* per-child init function */
};


static int mod_init(void)
{
    int n;

    LM_INFO("initializing...\n");
    
    /* read config */
    if ((rh = rc_read_config(radius_config)) == NULL) {
	LM_ERR("failed to open radius config file: %s\n", radius_config);
	return -1;
    }

    /* read dictionary */
    if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
	LM_ERR("failed to read radius dictionary\n");
	return -1;
    }

    /* init the extra engine */
    init_extra_engine();

    /* parse extra attributes (if any) */
    if (caller_extra_str &&
	(caller_extra=parse_extra_str(caller_extra_str)) == 0 ) {
	LM_ERR("failed to parse caller_extra parameter\n");
	return -1;
    }
    if (callee_extra_str &&
	(callee_extra=parse_extra_str(callee_extra_str)) == 0 ) {
	LM_ERR("failed to parse callee_extra parameter\n");
	return -1;
    }

    memset(caller_attrs, 0, sizeof(caller_attrs));
    memset(caller_vals, 0, sizeof(caller_vals));
    caller_attrs[SA_SERVICE_TYPE].n	   = "Service-Type";
    caller_attrs[SA_USER_NAME].n	   = "User-Name";
    caller_attrs[SA_SIP_AVP].n	           = "SIP-AVP";
    n = SA_STATIC_MAX;
    n += extra2attrs(caller_extra, caller_attrs, n);
    caller_vals[RV_SIP_CALLER_AVPS].n     = "SIP-Caller-AVPs";
    INIT_AV(rh, caller_attrs, n, caller_vals, RV_STATIC_MAX, "avp_radius",
	    -1, -1);
    if (caller_service_type != -1) {
	caller_vals[RV_SIP_CALLER_AVPS].v = caller_service_type;
    }

    memset(callee_attrs, 0, sizeof(callee_attrs));
    memset(callee_vals, 0, sizeof(callee_vals));
    callee_attrs[SA_SERVICE_TYPE].n	   = "Service-Type";
    callee_attrs[SA_USER_NAME].n	   = "User-Name";
    callee_attrs[SA_SIP_AVP].n	           = "SIP-AVP";
    n = SA_STATIC_MAX;
    n += extra2attrs(callee_extra, callee_attrs, n);
    callee_vals[EV_SIP_CALLEE_AVPS].n     = "SIP-Callee-AVPs";
    INIT_AV(rh, callee_attrs, n, callee_vals, EV_STATIC_MAX, "avp_radius",
	    -1, -1);
    if (callee_service_type != -1) {
	callee_vals[EV_SIP_CALLEE_AVPS].v = callee_service_type;
    }

    return 0;
}


static void destroy(void)
{
    if (caller_extra) destroy_extras(caller_extra);
    if (callee_extra) destroy_extras(callee_extra);
}
