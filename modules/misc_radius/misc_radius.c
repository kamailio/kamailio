/*
 * misc_radius.c  -- various radius based functions
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../lib/kcore/radius.h"
#include "../../mod_fix.h"
#include "misc_radius.h"
#include "functions.h"
#include "extra.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

/* Module parameter variables */
static char *radius_config = DEFAULT_RADIUSCLIENT_CONF;
static int caller_service_type = -1;
static int callee_service_type = -1;
static int group_service_type = -1;
static int uri_service_type = -1;
int common_response = 0;
int use_sip_uri_host = 0;

void *rh;
static char *caller_extra_str = 0;
struct extra_attr *caller_extra = 0;
static char *callee_extra_str = 0;
struct extra_attr *callee_extra = 0;
static char *group_extra_str = 0;
struct extra_attr *group_extra = 0;
static char *uri_extra_str = 0;
struct extra_attr *uri_extra = 0;

struct attr caller_attrs[SA_STATIC_MAX+MAX_EXTRA];
struct attr callee_attrs[SA_STATIC_MAX+MAX_EXTRA];
struct attr group_attrs[SA_STATIC_MAX+MAX_EXTRA];
struct attr uri_attrs[SA_STATIC_MAX+MAX_EXTRA];
struct val caller_vals[RV_STATIC_MAX];
struct val callee_vals[EV_STATIC_MAX];
struct val group_vals[GV_STATIC_MAX];
struct val uri_vals[UV_STATIC_MAX];


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"radius_load_caller_avps", (cmd_function)radius_load_caller_avps, 1,
     fixup_spve_null, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"radius_load_callee_avps", (cmd_function)radius_load_callee_avps, 1,
     fixup_spve_null, 0, REQUEST_ROUTE | FAILURE_ROUTE},
    {"radius_is_user_in", (cmd_function)radius_is_user_in, 2,
     fixup_spve_str, 0,
     REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
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
    {"radius_config",       PARAM_STRING, &radius_config      },
    {"caller_service_type", INT_PARAM, &caller_service_type},
    {"callee_service_type", INT_PARAM, &callee_service_type},
    {"group_service_type",  INT_PARAM, &group_service_type },
    {"uri_service_type",    INT_PARAM, &uri_service_type   },
    {"caller_extra",        PARAM_STRING, &caller_extra_str   },
    {"callee_extra",        PARAM_STRING, &callee_extra_str   },
    {"group_extra",         PARAM_STRING, &group_extra_str    },
    {"uri_extra",           PARAM_STRING, &uri_extra_str      },
    {"use_sip_uri_host",    INT_PARAM, &use_sip_uri_host   },
    {"common_response",     INT_PARAM, &common_response    },
    {0, 0, 0}
};	


struct module_exports exports = {
    "misc_radius", 
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


/* Macro to set static attribute names */
#define SET_STATIC(_attrs)		                        \
    do {								                \
	memset((_attrs), 0, sizeof((_attrs)));				\
	(_attrs)[SA_SERVICE_TYPE].n	   = "Service-Type";	\
	(_attrs)[SA_USER_NAME].n	   = "User-Name";		\
	(_attrs)[SA_SIP_AVP].n	       = "SIP-AVP";			\
	(_attrs)[SA_SIP_GROUP].n	   = "SIP-Group";		\
	if (use_sip_uri_host) {						        \
	    (_attrs)[SA_SIP_URI_HOST].n  = "SIP-URI-Host";  \
	} else {							                \
	    (_attrs)[SA_SIP_URI_HOST].n  = "User-Name";     \
	}								                    \
	n = SA_STATIC_MAX;						            \
    }while(0)


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
    if (group_extra_str &&
	(group_extra=parse_extra_str(group_extra_str)) == 0 ) {
	LM_ERR("failed to parse group_extra parameter\n");
	return -1;
    }
    if (uri_extra_str &&
	(uri_extra=parse_extra_str(uri_extra_str)) == 0 ) {
	LM_ERR("failed to parse uri_extra parameter\n");
	return -1;
    }

    SET_STATIC(caller_attrs);
    n += extra2attrs(caller_extra, caller_attrs, n);
    memset(caller_vals, 0, sizeof(caller_vals));
    caller_vals[RV_SIP_CALLER_AVPS].n     = "SIP-Caller-AVPs";
    INIT_AV(rh, caller_attrs, n, caller_vals, RV_STATIC_MAX, "misc_radius",
	    -1, -1);
    if (caller_service_type != -1) {
	caller_vals[RV_SIP_CALLER_AVPS].v = caller_service_type;
    }

    SET_STATIC(callee_attrs);
    n += extra2attrs(callee_extra, callee_attrs, n);
    memset(callee_vals, 0, sizeof(callee_vals));
    callee_vals[EV_SIP_CALLEE_AVPS].n     = "SIP-Callee-AVPs";
    INIT_AV(rh, callee_attrs, n, callee_vals, EV_STATIC_MAX, "misc_radius",
	    -1, -1);
    if (callee_service_type != -1) {
	callee_vals[EV_SIP_CALLEE_AVPS].v = callee_service_type;
    }

    SET_STATIC(group_attrs);
    n += extra2attrs(group_extra, group_attrs, n);
    memset(group_vals, 0, sizeof(group_vals));
    group_vals[GV_GROUP_CHECK].n     = "Group-Check";
    INIT_AV(rh, group_attrs, n, group_vals, RV_STATIC_MAX, "misc_radius",
	    -1, -1);
    if (group_service_type != -1) {
	group_vals[GV_GROUP_CHECK].v = group_service_type;
    }
    SET_STATIC(uri_attrs);
    n += extra2attrs(uri_extra, uri_attrs, n);
    memset(uri_vals, 0, sizeof(uri_vals));
    uri_vals[UV_CALL_CHECK].n     = "Call-Check";
    INIT_AV(rh, uri_attrs, n, uri_vals, UV_STATIC_MAX, "misc_radius",
	    -1, -1);
    if (uri_service_type != -1) {
	uri_vals[UV_CALL_CHECK].v = uri_service_type;
    }

    return 0;
}


static void destroy(void)
{
    if (caller_extra) destroy_extras(caller_extra);
    if (callee_extra) destroy_extras(callee_extra);
    if (group_extra) destroy_extras(group_extra);
    if (uri_extra) destroy_extras(group_extra);
}
