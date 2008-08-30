/*
 * $Id$
 *
 * Copyright (C) 2004 Juha Heinanen <jh@tutpro.com>
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

MODULE_VERSION

static int mod_init(void);
static int radius_load_caller_avps(struct sip_msg*, char*, char*);
static int radius_load_callee_avps(struct sip_msg*, char*, char*);

static char *radius_config = DEFAULT_RADIUSCLIENT_CONF;
static int caller_service_type = -1;
static int callee_service_type = -1;

static void *rh;
struct attr attrs[A_MAX];
struct val vals[V_MAX];


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
	0,         /* destroy function */
	0          /* per-child init function */
};


static int mod_init(void)
{
	LM_INFO("initializing...\n");

	memset(attrs, 0, sizeof(attrs));
	memset(vals, 0, sizeof(vals));

	attrs[A_SERVICE_TYPE].n	  = "Service-Type";
	attrs[A_USER_NAME].n	  = "User-Name";
	attrs[A_SIP_AVP].n	  = "SIP-AVP";
	vals[V_SIP_CALLER_AVPS].n = "SIP-Caller-AVPs";
	vals[V_SIP_CALLEE_AVPS].n = "SIP-Callee-AVPs";

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

	INIT_AV(rh, attrs, A_MAX, vals, V_MAX, "avp", -1, -1);

	if (caller_service_type != -1) {
		vals[V_SIP_CALLER_AVPS].v = caller_service_type;
	}

	if (callee_service_type != -1) {
		vals[V_SIP_CALLEE_AVPS].v = callee_service_type;
	}

	return 0;
}


static inline int extract_avp(VALUE_PAIR* vp, unsigned short *flags,
			      int_str *name, int_str *value)
{
	static str names, values;
	unsigned int r;
	char *p;
	char *end;

	/* empty? */
	if (vp->lvalue==0 || vp->strvalue==0)
		goto error;

	p = vp->strvalue;
	end = vp->strvalue + vp->lvalue;

	/* get name */
	if (*p!='#') {
		/* name AVP */
		*flags |= AVP_NAME_STR;
		names.s = p;
	} else {
		names.s = ++p;
	}

	names.len = 0;
	while( p<end && *p!=':' && *p!='#')
		p++;
	if (names.s==p || p==end) {
		LM_ERR("empty AVP name\n");
		goto error;
	}
	names.len = p - names.s;

	/* get value */
	if (*p!='#') {
		/* string value */
		*flags |= AVP_VAL_STR;
	}
	values.s = ++p;
	values.len = end-values.s;
	if (values.len==0) {
		LM_ERR("empty AVP value\n");
		goto error;
	}

	if ( !((*flags)&AVP_NAME_STR) ) {
		/* convert name to id*/
		if (str2int(&names,&r)!=0 ) {
			LM_ERR("invalid AVP ID '%.*s'\n", names.len,names.s);
			goto error;
		}
		name->n = (int)r;
	} else {
		name->s = names;
	}

	if ( !((*flags)&AVP_VAL_STR) ) {
		/* convert value to integer */
		if (str2int(&values,&r)!=0 ) {
			LM_ERR("invalid AVP numrical value '%.*s'\n", values.len,values.s);
			goto error;
		}
		value->n = (int)r;
	} else {
		value->s = values;
	}

	return 0;
error:
	return -1;
}


/* Generate AVPs from Radius reply items */
static void generate_avps(VALUE_PAIR* received)
{
    int_str name, val;
    unsigned short flags;
    VALUE_PAIR *vp;

    vp = received;

    for( ; (vp=rc_avpair_get(vp,attrs[A_SIP_AVP].v,0)) ; vp=vp->next) {
	flags = 0;
	if (extract_avp( vp, &flags, &name, &val)!=0 )
	    continue;
	if (add_avp( flags, name, val) < 0) {
	    LM_ERR("unable to create a new AVP\n");
	} else {
	    LM_DBG("AVP '%.*s'/%d='%.*s'/%d has been added\n",
		   (flags&AVP_NAME_STR)?name.s.len:4,
		   (flags&AVP_NAME_STR)?name.s.s:"null",
		   (flags&AVP_NAME_STR)?0:name.n,
		   (flags&AVP_VAL_STR)?val.s.len:4,
		   (flags&AVP_VAL_STR)?val.s.s:"null",
		   (flags&AVP_VAL_STR)?0:val.n );
	}
    }
    
    return;
}


/*
 * Loads from Radius caller's AVPs based on pvar argument.
 * Returns 1 if Radius request succeeded and -1 otherwise.
 */
int radius_load_caller_avps(struct sip_msg* _m, char* _caller, char* _s2)
{
    str user;
    VALUE_PAIR *send, *received;
    UINT4 service;
    static char msg[4096];
    int res;

    if ((_caller == NULL) ||
	(fixup_get_svalue(_m, (gparam_p)_caller, &user) != 0)) {
	LM_ERR("invalid caller parameter");
	return -1;
    }

    send = received = 0;

    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user.s, user.len, 0)) {
	LM_ERR("error adding A_USER_NAME\n");
	return -1;
    }

    service = vals[V_SIP_CALLER_AVPS].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
	LM_ERR("error adding A_SERVICE_TYPE <%u>\n", service);
	rc_avpair_free(send);
	return -1;
    }
    if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	generate_avps(received);
	rc_avpair_free(received);
	return 1;
    } else {
	if (res == REJECT_RC) {
	    LM_DBG("rejected\n");
	} else {
	    LM_ERR("failure\n");
	}
	rc_avpair_free(send);
	rc_avpair_free(received);
	return -1;
    }
}


/*
 * Loads from Radius callee's AVPs based on pvar argument.
 * Returns 1 if Radius request succeeded and -1 otherwise.
 */
int radius_load_callee_avps(struct sip_msg* _m, char* _callee, char* _s2)
{
    str user;
    VALUE_PAIR *send, *received;
    UINT4 service;
    static char msg[4096];
    int res;

    send = received = 0;

    if ((_callee == NULL) ||
	(fixup_get_svalue(_m, (gparam_p)_callee, &user) != 0)) {
	LM_ERR("invalid callee parameter");
	return -1;
    }

    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user.s, user.len, 0)) {
	LM_ERR("error adding A_USER_NAME\n");
	return -1;
    }

    service = vals[V_SIP_CALLEE_AVPS].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
	LM_ERR("error adding A_SERVICE_TYPE <%u>\n", service);
	rc_avpair_free(send);
	return -1;
    }
    if ((res = rc_auth(rh, 0, send, &received, msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	generate_avps(received);
	rc_avpair_free(received);
	return 1;
    } else {
	if (res == REJECT_RC) {
	    LM_DBG("rejected\n");
	} else {
	    LM_ERR("failure\n");
	}
	rc_avpair_free(send);
	rc_avpair_free(received);
	return -1;
    }
}
