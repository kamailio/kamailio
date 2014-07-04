/*
 * $Id$
 *
 * Copyright (C) 2004 Juha Heinanen <jh@tutpro.com>
 * Copyright (C) 2004 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef RADIUSCLIENT_NG_4
#  include <radiusclient.h>
# else
#  include <radiusclient-ng.h>
#endif

#include "../../rad_dict.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../parser/digest/digest_parser.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../domain/domain.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "../../config.h"


MODULE_VERSION


static int mod_init(void);
static int radius_load_attrs(struct sip_msg*, char*, char*);
static int attrs_fixup(void**, int);

static char *radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";

static void *rh;
struct attr attrs[A_MAX];
struct val vals[V_MAX];

static domain_get_did_t dm_get_did = NULL;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"radius_load_attrs", radius_load_attrs, 2, attrs_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
    {0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"radius_config", PARAM_STRING, &radius_config },
    {0, 0, 0}
};


struct module_exports exports = {
    "avp_radius",
    cmds,      /* Exported commands */
    0,         /* RPC methods */
    params,    /* Exported parameters */
    mod_init,  /* module initialization function */
    0,         /* response function*/
    0,         /* destroy function */
    0,         /* oncancel function */
    0          /* per-child init function */
};


static int mod_init(void)
{
    DICT_VENDOR *vend;
    memset(attrs, 0, sizeof(attrs));
    memset(vals, 0, sizeof(vals));
    
    attrs[A_USER_NAME].n        = "User-Name";
    attrs[A_SER_SERVICE_TYPE].n = "SER-Service-Type";
    attrs[A_SER_ATTR].n	        = "SER-Attr";
    attrs[A_SER_DID].n          = "SER-DID";
    attrs[A_SER_URI_SCHEME].n   = "SER-Uri-Scheme";
    
    vals[V_GET_URI_ATTRS].n  = "Get-URI-Attrs";
    vals[V_GET_USER_ATTRS].n = "Get-User-Attrs";

	 /* open log */
    rc_openlog("ser");
    
	 /* read config */
    if ((rh = rc_read_config(radius_config)) == NULL) {
	LOG(L_ERR, "avp_radius: Error opening radius config file: %s\n",
	    radius_config);
	return -1;
    }
    
	 /* read dictionary */
    if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
	LOG(L_ERR, "avp_radius: Error reading radius dictionary\n");
	return -1;
    }
    
    vend = rc_dict_findvend(rh, "iptelorg");
    if (vend == NULL) {
	ERR("RADIUS dictionary is missing required vendor 'iptelorg'\n");
	return -1;
    }

    INIT_AV(rh, attrs, vals, "avp", -1, -1);
    return 0;
}


static void attr_name_value(str* name, str* value, VALUE_PAIR* vp)
{
    int i;
    
    for (i = 0; i < vp->lvalue; i++) {
	if (vp->strvalue[i] == ':' || vp->strvalue[i] == '=') {
	    name->s = vp->strvalue;
	    name->len = i;
	    
	    if (i == (vp->lvalue - 1)) {
		value->s = (char*)0;
		value->len = 0;
	    } else {
		value->s = vp->strvalue + i + 1;
		value->len = vp->lvalue - i - 1;
	    }
	    return;
	}
    }

    name->len = value->len = 0;
    name->s = value->s = (char*)0;
}


/*
 * Generate AVPs from the database result
 */
static int generate_avps(unsigned int flags, VALUE_PAIR* received)
{
    int_str name, val;
    VALUE_PAIR *vp;
    
    vp = received;
    while ((vp = rc_avpair_get(vp, ATTRID(attrs[A_SER_ATTR].v), VENDOR(attrs[A_SER_ATTR].v)))) {
	attr_name_value(&name.s, &val.s, vp);
	if (name.s.len == 0) {
	    ERR("Missing attribute name\n");
	    return -1;
	}

	if (add_avp(flags | AVP_NAME_STR | AVP_VAL_STR, name, val) < 0) {
	    ERR("Unable to create a new SER attribute\n");
	    return -1;
	}
	vp = vp->next;
    }
    
    return 0;
}


static int load_user_attrs(struct sip_msg* msg, unsigned long flags, fparam_t* fp)
{
    static char rad_msg[4096];
    str uid;
    UINT4 service;
    VALUE_PAIR* send, *received;

    send = NULL;
    received = NULL;

    if (get_str_fparam(&uid, msg, (fparam_t*)fp) < 0) {
	ERR("Unable to get UID\n");
	return -1;
    }

    service = vals[V_GET_USER_ATTRS].v;

    if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_USER_NAME].v), 
		       uid.s, uid.len,
		       VENDOR(attrs[A_USER_NAME].v))) {
	ERR("Error while adding A_USER_NAME\n");
	goto error;
    }
    
    if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SER_SERVICE_TYPE].v), 
		       &vals[V_GET_USER_ATTRS].v, -1, 
		       VENDOR(attrs[A_SER_SERVICE_TYPE].v))) {
	ERR("Error adding A_SERVICE_TYPE\n");
	goto error;
    }


    if (rc_auth(rh, 0, send, &received, rad_msg) != OK_RC) {
	DBG("load_user_attrs: Failure\n");
	goto error;
    }

    DBG("load_user_attrs: Success\n");
    rc_avpair_free(send);

    if (generate_avps(flags, received) < 0) {
	rc_avpair_free(received);
	goto error;
    }

    rc_avpair_free(received);
    return 1;

 error:
    if (send) rc_avpair_free(send);
    if (received) rc_avpair_free(send);
    return -1;
}


static int load_uri_attrs(struct sip_msg* msg, unsigned long flags, fparam_t* fp)
{
    static char rad_msg[4096];
    struct sip_uri puri;
    str uri, did, scheme;
    UINT4 service;
    VALUE_PAIR* send, *received;

    send = NULL;
    received = NULL;

    if (get_str_fparam(&uri, msg, (fparam_t*)fp) != 0) {
	ERR("Unable to get URI\n");
	return -1;
    }

    if (parse_uri(uri.s, uri.len, &puri) < 0) {
	ERR("Error while parsing URI '%.*s'\n", uri.len, uri.s);
	return -1;
    }

    if (puri.host.len) {
	/* domain name is present */
	if (dm_get_did(&did, &puri.host) < 0) {
		DBG("Cannot lookup DID for domain %.*s, using default value\n", puri.host.len, ZSW(puri.host.s));
		did.s = DEFAULT_DID;
		did.len = sizeof(DEFAULT_DID) - 1;
	}
    } else {
	/* domain name is missing -- can be caused by tel: URI */
	DBG("There is no domain name, using default value\n");
	did.s = DEFAULT_DID;
	did.len = sizeof(DEFAULT_DID) - 1;
    }

    uri_type_to_str(puri.type, &scheme);
    service = vals[V_GET_URI_ATTRS].v;

    if (scheme.len) {
	if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SER_URI_SCHEME].v), 
			   scheme.s, scheme.len,
			   VENDOR(attrs[A_SER_URI_SCHEME].v))) {
	    ERR("Error while adding A_SER_URI_SCHEME\n");
	    goto error;
	}
    }

    if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_USER_NAME].v), 
		       puri.user.s, puri.user.len,
		       VENDOR(attrs[A_USER_NAME].v))) {
	ERR("Error while adding A_USER_NAME\n");
	goto error;
    }

    if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SER_DID].v), 
		       did.s, did.len,
		       VENDOR(attrs[A_SER_DID].v))) {
	ERR("Error while adding A_SER_DID\n");
	goto error;
    }
    
    if (!rc_avpair_add(rh, &send, ATTRID(attrs[A_SER_SERVICE_TYPE].v), 
		       &vals[V_GET_URI_ATTRS].v, -1,
		       VENDOR(attrs[A_SER_SERVICE_TYPE].v))) {
	ERR("Error adding A_SERVICE_TYPE\n");
	goto error;
    }

    if (rc_auth(rh, 0, send, &received, rad_msg) != OK_RC) {
	DBG("load_uri_attrs: Failure\n");
	goto error;
    }

    DBG("load_uri_attrs: Success\n");
    rc_avpair_free(send);

    if (generate_avps(flags, received) < 0) {
	rc_avpair_free(received);
	goto error;
    }

    rc_avpair_free(received);
    return 1;

 error:
    if (send) rc_avpair_free(send);
    if (received) rc_avpair_free(send);
    return -1;
}


/*
 * Load user attributes
 */
static int radius_load_attrs(struct sip_msg* msg, char* fl, char* fp)
{
    unsigned long flags;
    
    flags = (unsigned long)fl;

    if (flags & AVP_CLASS_URI) {
       	return load_uri_attrs(msg, flags, (fparam_t*)fp);
    } else {
	return load_user_attrs(msg, flags, (fparam_t*)fp);
    }
}


static int attrs_fixup(void** param, int param_no)
{
    unsigned long flags;
    char* s;
    
    if (param_no == 1) {
	     /* Determine the track and class of attributes to be loaded */
	s = (char*)*param;
	flags = 0;
	if (*s != '$' || (strlen(s) != 3)) {
	    ERR("Invalid parameter value, $xy expected\n");
	    return -1;
	}
	switch((s[1] << 8) + s[2]) {
	case 0x4655: /* $fu */
	case 0x6675:
	case 0x4675:
	case 0x6655:
	    flags = AVP_TRACK_FROM | AVP_CLASS_USER;
	    break;
	    
	case 0x4652: /* $fr */
	case 0x6672:
	case 0x4672:
	case 0x6652:
	    flags = AVP_TRACK_FROM | AVP_CLASS_URI;
	    break;
	    
	case 0x5455: /* $tu */
	case 0x7475:
	case 0x5475:
	case 0x7455:
	    flags = AVP_TRACK_TO | AVP_CLASS_USER;
	    break;
	    
	case 0x5452: /* $tr */
	case 0x7472:
	case 0x5472:
	case 0x7452:
	    flags = AVP_TRACK_TO | AVP_CLASS_URI;
	    break;
	    
	default:
	    ERR("Invalid parameter value: '%s'\n", s);
	    return -1;
	}

	if ((flags & AVP_CLASS_URI) && !dm_get_did) {
	    dm_get_did = (domain_get_did_t)find_export("get_did", 0, 0);
	    if (!dm_get_did) {
		ERR("Domain module required but not found\n");
		return -1;
	    }
	}
	
	pkg_free(*param);
	*param = (void*)flags;
    } else if (param_no == 2) {
	return fixup_var_str_12(param, 2);
    }
    return 0;
}
