/* 
 * Radius based peering module
 *
 * Copyright (C) 2008 Juha Heinanen
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
 */

/*! \file
 * \ingroup peering
 * \brief Peering:: Core
 *
 * - Module: \ref peering
 */

/*! \defgroup peering Peering :: Radius based peering verification module
 *
 * The Peering module allows SIP providers (operators or
 *  organizations) to verify from a broker if source or destination
 *  of a SIP request is a trusted peer.
 *
 */


#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../config.h"
#include "../../lib/kcore/radius.h"
#include "verify.h"

MODULE_VERSION

struct attr attrs[A_MAX];
struct val vals[V_MAX];
void *rh;

static int mod_init(void);         /* Module initialization function */


/*
 * Module parameter variables
 */
static char* radius_config = DEFAULT_RADIUSCLIENT_CONF;
int verify_destination_service_type = -1;
int verify_source_service_type = -1;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"verify_destination", (cmd_function)verify_destination, 0, 0, 0,
     REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
    {"verify_source", (cmd_function)verify_source, 0, 0, 0,
     REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"radius_config", PARAM_STRING, &radius_config},
    {"verify_destination_service_type", INT_PARAM,
     &verify_destination_service_type},
    {"verify_source_service_type", INT_PARAM,
     &verify_source_service_type},
    {0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
    "peering", 
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


/*
 * Module initialization function
 */
static int mod_init(void)
{
    memset(attrs, 0, sizeof(attrs));
    memset(vals, 0, sizeof(vals));
    attrs[A_USER_NAME].n = "User-Name";
    attrs[A_SIP_URI_USER].n = "SIP-URI-User";
    attrs[A_SIP_FROM_TAG].n = "SIP-From-Tag";
    attrs[A_SIP_CALL_ID].n = "SIP-Call-Id";
    attrs[A_SIP_REQUEST_HASH].n = "SIP-Request-Hash";
    attrs[A_SIP_AVP].n = "SIP-AVP";
    attrs[A_SERVICE_TYPE].n = "Service-Type";
    vals[V_SIP_VERIFY_DESTINATION].n = "Sip-Verify-Destination";
    vals[V_SIP_VERIFY_SOURCE].n = "Sip-Verify-Source";

    if ((rh = rc_read_config(radius_config)) == NULL) {
        LM_ERR("error opening configuration file\n");
	return -1;
    }

    if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
	LM_ERR("error opening dictionary file\n");
	return -2;
    }

    INIT_AV(rh, attrs, A_MAX, vals, V_MAX, "peering", -3, -4);

    if (verify_destination_service_type != -1) {
	vals[V_SIP_VERIFY_DESTINATION].v = 
		verify_destination_service_type;
    }

    if (verify_source_service_type != -1) {
	vals[V_SIP_VERIFY_SOURCE].v = verify_source_service_type;
    }

    return 0;
}
