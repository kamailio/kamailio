/*
 * Digest Authentication - Radius support
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/error.h"
#include "../../core/dprint.h"
#include "../../core/config.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../misc_radius/radius.h"
#include "../../core/mem/mem.h"
#include "auth_radius.h"
#include "authorize.h"
#include "extra.h"

MODULE_VERSION

struct attr attrs[A_MAX + MAX_EXTRA];
struct val vals[V_MAX + MAX_EXTRA];
void *rh;

auth_api_s_t auth_api;

static int mod_init(void); /* Module initialization function */

/*
 * Module parameter variables
 */
static char *radius_config = DEFAULT_RADIUSCLIENT_CONF;
static int service_type = -1;

int use_ruri_flag = -1;
int ar_radius_avps_mode = 0;
int append_realm_to_username = 1;

static char *auth_extra_str = 0;
struct extra_attr *auth_extra = 0;

/*
 * Exported functions
 */
/* clang-format off */
static cmd_export_t cmds[] = {
	{"radius_www_authorize", (cmd_function)radius_www_authorize_1,   1,
			fixup_spve_null, fixup_free_spve_null, REQUEST_ROUTE},
	{"radius_www_authorize", (cmd_function)radius_www_authorize_2,   2,
			fixup_spve_spve, fixup_free_spve_spve, REQUEST_ROUTE},
	{"radius_proxy_authorize", (cmd_function)radius_proxy_authorize_1, 1,
			fixup_spve_null, fixup_free_spve_null, REQUEST_ROUTE},
	{"radius_proxy_authorize", (cmd_function)radius_proxy_authorize_2, 2,
			fixup_spve_spve, fixup_free_spve_spve, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"radius_config",    PARAM_STRING, &radius_config	},
	{"service_type",     INT_PARAM, &service_type	},
	{"use_ruri_flag",    INT_PARAM, &use_ruri_flag	},
	{"auth_extra",       PARAM_STRING, &auth_extra_str	},
	{"radius_avps_mode",	 INT_PARAM, &ar_radius_avps_mode	},
	{"append_realm_to_username", INT_PARAM, &append_realm_to_username  },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"auth_radius",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported RPC method */
	0,          /* exported pseudo-variables */
	0,          /* response function */
	mod_init,   /* module initialization function */
	0,          /* child initialization function */
	0           /* destroy function */
};
/* clang-format off */


/*
 * Module initialization function
 */
static int mod_init(void)
{
	DICT_VENDOR *vend;
	bind_auth_s_t bind_auth;
	int n;

	if ((rh = rc_read_config(radius_config)) == NULL) {
		LM_ERR("failed to open configuration file: %s\n",
				(radius_config)?radius_config:"none");
		return -1;
	}

	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
		LM_ERR("failed to open dictionary file \n");
		return -2;
	}

	bind_auth = (bind_auth_s_t)find_export("bind_auth_s", 0, 0);
	if (!bind_auth) {
		LM_ERR("unable to find bind_auth function. Check if you load the auth module.\n");
		return -1;
	}

	if (bind_auth(&auth_api) < 0) {
		LM_ERR("cannot bind to auth module\n");
		return -4;
	}

	/* init the extra engine */
	init_extra_engine();

	/* parse extra attributes (if any) */
	if (auth_extra_str &&
	    (auth_extra=parse_extra_str(auth_extra_str)) == 0 ) {
	    LM_ERR("failed to parse auth_extra parameter\n");
	    return -1;
	}

	memset(attrs, 0, sizeof(attrs));
	attrs[A_SERVICE_TYPE].n			= "Service-Type";
	attrs[A_SIP_URI_USER].n			= "Sip-URI-User";
	attrs[A_DIGEST_RESPONSE].n		= "Digest-Response";
	attrs[A_DIGEST_ALGORITHM].n		= "Digest-Algorithm";
	attrs[A_DIGEST_BODY_DIGEST].n		= "Digest-Body-Digest";
	attrs[A_DIGEST_CNONCE].n		= "Digest-CNonce";
	attrs[A_DIGEST_NONCE_COUNT].n		= "Digest-Nonce-Count";
	attrs[A_DIGEST_QOP].n			= "Digest-QOP";
	attrs[A_DIGEST_METHOD].n		= "Digest-Method";
	attrs[A_DIGEST_URI].n			= "Digest-URI";
	attrs[A_DIGEST_NONCE].n			= "Digest-Nonce";
	attrs[A_DIGEST_REALM].n			= "Digest-Realm";
	attrs[A_DIGEST_USER_NAME].n		= "Digest-User-Name";
	attrs[A_USER_NAME].n			= "User-Name";
	attrs[A_SIP_AVP].n			= "SIP-AVP";
	vend = rc_dict_findvend(rh, "Cisco");
	if (vend == NULL) {
	    LM_DBG("no `Cisco' vendor in Radius dictionary\n");
	} else {
	    attrs[A_CISCO_AVPAIR].n		= "Cisco-AVPair";
	}
	n = A_MAX;
	n += extra2attrs(auth_extra, attrs, n);
	memset(vals, 0, sizeof(vals));
	vals[V_SIP_SESSION].n			= "Sip-Session";
	INIT_AV(rh, attrs, n, vals, V_MAX, "auth_radius", -5, -6);

	if (service_type != -1) {
		vals[V_SIP_SESSION].v = service_type;
	}

	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_auth_radius_exports[] = {
	{ str_init("auth_radius"), str_init("proxy_authorize"),
		SR_KEMIP_INT, ki_radius_proxy_authorize,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_radius"), str_init("proxy_authorize_user"),
		SR_KEMIP_INT, ki_radius_proxy_authorize_user,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_radius"), str_init("www_authorize"),
		SR_KEMIP_INT, ki_radius_www_authorize,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("auth_radius"), str_init("www_authorize_user"),
		SR_KEMIP_INT, ki_radius_www_authorize_user,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_auth_radius_exports);
	return 0;
}
