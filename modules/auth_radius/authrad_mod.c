/* 
 * $Id$ 
 *
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
 * History:
 * -------
 *  2003-03-09: Based on auth_mod.c from radius_auth (janakj)
 *  2003-03-11: New module interface (janakj)
 *  2003-03-16: flags export parameter added (janakj)
 *  2003-03-19: all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2006-03-01: pseudo variables support for domain name (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../pvar.h"
#include "../../lib/kcore/radius.h"
#include "../../mem/mem.h"
#include "authrad_mod.h"
#include "authorize.h"
#include "extra.h"

MODULE_VERSION

struct attr attrs[A_MAX+MAX_EXTRA];
struct val vals[V_MAX+MAX_EXTRA];
void *rh;

auth_api_s_t auth_api;

static int mod_init(void);         /* Module initialization function */
static int auth_fixup(void** param, int param_no); /* char* -> str* */


/*
 * Module parameter variables
 */
static char* radius_config = DEFAULT_RADIUSCLIENT_CONF;
static int service_type = -1;

int use_ruri_flag = -1;
int ar_radius_avps_mode = 0;

static char *auth_extra_str = 0;
struct extra_attr *auth_extra = 0;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"radius_www_authorize", (cmd_function)radius_www_authorize_1,   1, auth_fixup,
			0, REQUEST_ROUTE},
	{"radius_www_authorize", (cmd_function)radius_www_authorize_2,   2, auth_fixup,
			0, REQUEST_ROUTE},
	{"radius_proxy_authorize", (cmd_function)radius_proxy_authorize_1, 1, auth_fixup,
			0, REQUEST_ROUTE},
	{"radius_proxy_authorize", (cmd_function)radius_proxy_authorize_2, 2, auth_fixup,
			0, REQUEST_ROUTE},
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
	DICT_VENDOR *vend;
	bind_auth_s_t bind_auth;
	int n;

	if ((rh = rc_read_config(radius_config)) == NULL) {
		LM_ERR("failed to open configuration file \n");
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


/*
 * Convert char* parameter to pv_elem_t* parameter
 */
static int auth_fixup(void** param, int param_no)
{
	pv_elem_t *model;
	str s;
	pv_spec_t *sp;

	if (param_no == 1) { /* realm (string that may contain pvars) */
		s.s = (char*)*param;
		if (s.s==0 || s.s[0]==0) {
			model = 0;
		} else {
			s.len = strlen(s.s);
			if (pv_parse_format(&s,&model)<0) {
				LM_ERR("pv_parse_format failed\n");
				return E_OUT_OF_MEM;
			}
		}
		*param = (void*)model;
	}

	if (param_no == 2) { /* URI user (a pvar) */
		sp = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if (sp == 0) {
			LM_ERR("no pkg memory left\n");
			return -1;
		}
		s.s = (char*)*param;
		s.len = strlen(s.s);
		if (pv_parse_spec(&s, sp) == 0) {
			LM_ERR("parsing of pseudo variable %s failed!\n", (char*)*param);
			pkg_free(sp);
			return -1;
		}
		if (sp->type == PVT_NULL) {
			LM_ERR("bad pseudo variable\n");
			pkg_free(sp);
			return -1;
		}
		*param = (void*)sp;
	}	

	return 0;
}
