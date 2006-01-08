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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../../usr_avp.h"
#include "../../ut.h"


MODULE_VERSION

#define CALLER_PREFIX "caller_"
#define CALLER_PREFIX_LEN (sizeof(CALLER_PREFIX) - 1)

#define CALLEE_PREFIX "callee_"
#define CALLEE_PREFIX_LEN (sizeof(CALLEE_PREFIX) - 1)


typedef enum load_avp_param {
	LOAD_CALLER,       /* Use the caller's username and domain as the key */
	LOAD_CALLEE,       /* Use the callee's username and domain as the key */
	LOAD_DIGEST
} load_avp_param_t;


static int mod_init(void);
static int load_avp_radius(struct sip_msg*, char*, char*);
static int load_avp_fixup(void**, int);


static char *radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";
static int caller_service_type = -1;
static int callee_service_type = -1;
static str caller_prefix = STR_STATIC_INIT(CALLER_PREFIX);
static str callee_prefix = STR_STATIC_INIT(CALLEE_PREFIX);

static void *rh;
struct attr attrs[A_MAX];
struct val vals[V_MAX];



/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"avp_load_radius", load_avp_radius, 1, load_avp_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"radius_config",       PARAM_STRING, &radius_config      },
	{"caller_service_type", PARAM_INT,    &caller_service_type},
	{"callee_service_type", PARAM_INT,    &callee_service_type},
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
        DBG("avp_radius - Initializing\n");

	memset(attrs, 0, sizeof(attrs));
	memset(vals, 0, sizeof(vals));

	attrs[A_USER_NAME].n	  = "User-Name";
	attrs[A_SERVICE_TYPE].n	  = "Service-Type";

	attrs[A_SER_ATTRS].n	  = "SER-Attrs";
	vals[V_SER_CALLER_AVPS].n = "SER-Caller-AVPs";
	vals[V_SER_CALLEE_AVPS].n = "SER-Callee-AVPs";

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

	INIT_AV(rh, attrs, vals, "avp", -1, -1);

	if (caller_service_type != -1) {
		vals[V_SER_CALLER_AVPS].v = caller_service_type;
	}

	if (callee_service_type != -1) {
		vals[V_SER_CALLEE_AVPS].v = callee_service_type;
	}

	return 0;
}


static inline void attr_name_value(VALUE_PAIR* vp, str* name, str* value)
{
	int i;


	for (i = 0; i < vp->lvalue; i++) {
		if (vp->strvalue[i] == ':') {
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


static int load_avp_user(struct sip_msg* msg, str* prefix, load_avp_param_t type)
{
	static char rad_msg[4096];
	str user_domain, name_str;
	str* user, *domain, *uri;
	struct hdr_field* h;
	dig_cred_t* cred = 0;
	int_str name, val;

	VALUE_PAIR* send, *received, *vp;
	UINT4 service;
	struct sip_uri puri;

	send = received = 0;
	user_domain.s = 0;

	switch(type) {
	case LOAD_CALLER:
		     /* Use From header field */
		if (parse_from_header(msg) < 0) {
			LOG(L_ERR, "load_avp_user: Error while parsing From header field\n");
			return -1;
		}

		uri = &get_from(msg)->uri;
		if (parse_uri(uri->s, uri->len, &puri) == -1) {
			LOG(L_ERR, "load_avp_user: Error while parsing From URI\n");
			return -1;
		}

		user = &puri.user;
		domain = &puri.host;
		service = vals[V_SER_CALLER_AVPS].v;
		break;

	case LOAD_CALLEE:
		     /* Use the Request-URI */
		if (parse_sip_msg_uri(msg) < 0) {
			LOG(L_ERR, "load_avp_user: Error while parsing Request-URI\n");
			return -1;
		}

		if (msg->parsed_uri.user.len == 0) {
			LOG(L_ERR, "load_avp_user: Request-URI user is missing\n");
			return -1;
		}

		user = &msg->parsed_uri.user;
		domain = &msg->parsed_uri.host;
		service = vals[V_SER_CALLEE_AVPS].v;
		break;

	case LOAD_DIGEST:
		     /* Use digest credentials */
		get_authorized_cred(msg->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "load_avp_user: No authoried credentials\n");
			return -1;
		}

		cred = &((auth_body_t*)(h->parsed))->digest;
		user = &cred->username.user;
		domain = &cred->realm;
		service = vals[V_SER_CALLER_AVPS].v;
		break;

	default:
		LOG(L_ERR, "load_avp_user: Unknown type\n");
		return -1;

	}

	user_domain.len = user->len + 1 + domain->len;
	user_domain.s = (char*)pkg_malloc(user_domain.len);
	if (!user_domain.s) {
		LOG(L_ERR, "avp_load_user: No memory left\n");
		return -1;
	}

	memcpy(user_domain.s, user->s, user->len);
	user_domain.s[user->len] = '@';
	memcpy(user_domain.s + user->len + 1, domain->s, domain->len);

	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v,
			   user_domain.s, user_domain.len, 0)) {
		LOG(L_ERR, "avp_load_user: Error adding PW_USER_NAME\n");
		goto error;
	}

	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
		LOG(L_ERR, "avp_load_user: Error adding PW_SERVICE_TYPE\n");
		goto error;
	}

	if (rc_auth(rh, 0, send, &received, rad_msg) == OK_RC) {
		DBG("avp_load_user: Success\n");
		rc_avpair_free(send);
		pkg_free(user_domain.s);

		vp = received;
		while ((vp = rc_avpair_get(vp, attrs[A_SER_ATTRS].v, 0))) {
			attr_name_value(vp, &name_str, &val.s);
			if (name.s.len == 0) {
			    LOG(L_ERR, "avp_load_user: Missing attribute name\n");
			    return -1;
			}

			name.s.s = (char*)pkg_malloc(prefix->len + name_str.len);
			if (!name.s.s) {
				LOG(L_ERR, "avp_load_user: No memory left\n");
				return -1;
			}
			name.s.len = prefix->len + name_str.len;
			memcpy(name.s.s, prefix->s, prefix->len);
			memcpy(name.s.s + prefix->len, name_str.s, name_str.len);

			add_avp(AVP_TRACK_FROM | AVP_CLASS_USER | AVP_NAME_STR | AVP_VAL_STR, name, val);
			DBG("avp_load_user: AVP '%.*s'='%.*s' has been added\n",
			    name_str.len, ZSW(name_str.s),
			    val.s.len, ZSW(val.s.s));

			pkg_free(name.s.s);
			vp = vp->next;
		}

		rc_avpair_free(received);
		return 1;
	} else {
		DBG("avp_load_user: Failure\n");
	}

 error:
	if (send) rc_avpair_free(send);
	if (received) rc_avpair_free(received);
	if (user_domain.s) pkg_free(user_domain.s);
	return -1;
}


static int load_avp_radius(struct sip_msg* msg, char* attr, char* _dummy)
{
	switch((load_avp_param_t)attr) {
	case LOAD_CALLER:
		return load_avp_user(msg, &caller_prefix, LOAD_CALLER);
		break;

	case LOAD_CALLEE:
		return load_avp_user(msg, &callee_prefix, LOAD_CALLEE);
		break;

	case LOAD_DIGEST:
		return load_avp_user(msg, &caller_prefix, LOAD_DIGEST);

	default:
		LOG(L_ERR, "load_avp_radius: Unknown parameter value\n");
		return -1;
	}
}


static int load_avp_fixup(void** param, int param_no)
{
	long id = 0;

	if (param_no == 1) {
		if (!strcasecmp(*param, "caller")) {
			id = LOAD_CALLER;
		} else if (!strcasecmp(*param, "callee")) {
			id = LOAD_CALLEE;
		} else if (!strcasecmp(*param, "digest")) {
			id = LOAD_DIGEST;
		} else {
			LOG(L_ERR, "load_avp_fixup: Unknown parameter\n");
			return -1;
		}
	}

	pkg_free(*param);
	*param=(void*)id;
	return 0;
}
