/*
 * Verification functions
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA.
 *
 */

/*! \file
 * \ingroup peering
 * \brief Verification functions
 *
 * - Module: \ref peering
 */



#include "../../str.h"
#include "../../lib/kcore/radius.h"
#include "../../usr_avp.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../lib/kcore/radius.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../pvar.h"
#include "peering.h"

#include <stdlib.h>
#include <string.h>

/*
 * Extract name and value of an AVP from VALUE_PAIR
 */
static inline int extract_avp(VALUE_PAIR* vp, unsigned short *flags,
			      int_str *name, int_str *value)
{
	char *p, *q, *r;

	LM_DBG("vp->name '%.*s'\n",(int)strlen(vp->name),vp->name);
	LM_DBG("vp->attribute '%d'\n",vp->attribute);
	LM_DBG("vp->type '%d'\n",vp->type);
	LM_DBG("vp->lvalue '%d'\n",vp->lvalue);
	if (vp->type == PW_TYPE_STRING) 
		LM_DBG("vp->strvalue '%.*s'\n",(int)strlen(vp->strvalue),vp->strvalue);

	if ( vp->attribute == attrs[A_SIP_AVP].v && vp->type == PW_TYPE_STRING ) {
		errno = 0;
		p = strchr(vp->strvalue,'#');
		q = strchr(vp->strvalue,':');
		r = strrchr(vp->strvalue,'#');
		if ( p == vp->strvalue && q != NULL ) {
			// int name and str value
			*flags |= AVP_VAL_STR;
			p++;
			name->n = strtol(p,&q,10);
			q++;
			value->s.len = strlen(q);
			if (!(value->s.s = pkg_malloc(value->s.len + 1))) {
				*flags = 0;
				return 0;
			}
			strcpy (value->s.s,q);
		} else if ( p != NULL && p == r && p > vp->strvalue && q == NULL ) {
			// str name and int value
			*flags |= AVP_NAME_STR;
			name->s.len = p - vp->strvalue;
			if (!(name->s.s = pkg_malloc(name->s.len + 1))) {
				*flags = 0;
				return 0;
			}
			strncpy(name->s.s,vp->strvalue,name->s.len);
			name->s.s[name->s.len] = '\0';
			p++;
			value->n = strtol(p,&r,10);
		} else if ( p != NULL && p != r && q == NULL ) {
			// int name and int vale
			p++;
			name->n = strtol(p,&q,10);
			r++;
			value->n = strtol(r,&q,10);
		} else if ( (p == NULL || p > q) && q != NULL ) {
			// str name and str value
			*flags |= AVP_VAL_STR|AVP_NAME_STR;
			name->s.len = q - vp->strvalue;
			if (!(name->s.s = pkg_malloc(name->s.len + 1))) {
				*flags = 0;
				return 0;
			}
			strncpy(name->s.s,vp->strvalue,name->s.len);
			name->s.s[name->s.len] = '\0';
			q++;
			value->s.len = strlen(q);
			if (!(value->s.s = pkg_malloc(value->s.len + 1))) {
				pkg_free(name->s.s);
				*flags = 0;
				return 0;
			}
			strcpy (value->s.s,q);
		} else // error - unknown
			return 0;
	    if ( errno != 0 ) {
			perror("strtol");
			if (*flags&AVP_NAME_STR) 
				pkg_free(name->s.s);
			if (*flags&AVP_VAL_STR) 
				pkg_free(value->s.s);
			*flags = 0;
			return 0;
		}
	} else if (vp->type == PW_TYPE_STRING) {
			*flags |= AVP_VAL_STR|AVP_NAME_STR;
			if (!(p = strchr(vp->strvalue,'='))) {
				if (!(p = strchr(vp->strvalue,':'))) {
					if (!(p = strchr(vp->strvalue,'#'))) {
						p = vp->strvalue;
					} else p++;
				} else p++;
			} else p++;
			value->s.len = vp->lvalue - (p - vp->strvalue);
			if (!(value->s.s = pkg_malloc(value->s.len + 1))) {
				*flags = 0;
				return 0;
			}
			strcpy (value->s.s, p);
			name->s.len = strlen(vp->name); 
			if (!(name->s.s = pkg_malloc(name->s.len + 1))) {
				pkg_free(value->s.s);
				*flags = 0;
				return 0;
			}
			strcpy(name->s.s,vp->name);
	} else if ((vp->type == PW_TYPE_INTEGER) || (vp->type == PW_TYPE_IPADDR) || (vp->type == PW_TYPE_DATE)) {
			*flags |= AVP_NAME_STR;
			value->n = vp->lvalue;
			name->s.len = strlen(vp->name); 
			if (!(name->s.s = pkg_malloc(name->s.len+1))) {
				*flags = 0;
				return 0;
			}
			strcpy(name->s.s,vp->name);
	} else {
			LM_ERR("Unknown AVP type '%d'!\n",vp->type);
			return 0;
	}
	return 1;
}


/*
 * Generate AVPs from result of Radius query
 */
static int generate_avps(VALUE_PAIR* received)
{
	int_str name, val;
	unsigned short flags;
	VALUE_PAIR *vp;

	LM_DBG("getting AVPs from RADIUS Reply\n");
	for( vp = received; vp; vp=vp->next) {
		flags = 0;
		if (!extract_avp( vp, &flags, &name, &val)){
			LM_ERR("error while extracting AVP '%.*s'\n",(int)strlen(vp->name),vp->name);
			continue;
		}
		if (add_avp( flags, name, val) < 0) {
			LM_ERR("error while creating a new AVP\n");
		} else {
			if (flags&PV_VAL_STR) {
				LM_DBG("Added AVP name '%.*s'='%.*s'\n",(int)strlen(name.s.s),name.s.s,(int)strlen(val.s.s),val.s.s);
			} else  if (flags&PV_VAL_INT) {
				LM_DBG("Added AVP name '%.*s'=%d\n",(int)strlen(name.s.s),name.s.s,val.n);
			} else
				LM_DBG("AVP '%.*s'/%d='%.*s'/%d has been added\n",
					(flags&AVP_NAME_STR)?name.s.len:4,
					(flags&AVP_NAME_STR)?name.s.s:"null",
					(flags&AVP_NAME_STR)?0:name.n,
					(flags&AVP_VAL_STR)?val.s.len:4,
					(flags&AVP_VAL_STR)?val.s.s:"null",
					(flags&AVP_VAL_STR)?0:val.n );
		}
		if (flags&AVP_NAME_STR) 
			pkg_free(name.s.s);
		if (flags&AVP_VAL_STR) 
			pkg_free(val.s.s);
	}
	return 0;
}

/* 
 * Send Radius request to verify destination and generate AVPs from
 * reply items of positive response.
 */
int verify_destination(struct sip_msg* _msg, char* s1, char* s2)
{
    VALUE_PAIR* send, *received;
    uint32_t service;
    static char rad_msg[4096];
    int i;

    send = received = 0;

    /* Add Request-URI host A_USER_NAME and user as A_SIP_URI_USER */
    if (parse_sip_msg_uri(_msg) < 0) {
        LM_ERR("error while parsing Request-URI\n");
	return -1;
    }
    
    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v,
		       _msg->parsed_uri.host.s,
		       _msg->parsed_uri.host.len, 0)) {
	LM_ERR("error adding PW_USER_NAME\n");
	goto err;
    }
   
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_URI_USER].v,
		       _msg->parsed_uri.user.s,
		       _msg->parsed_uri.user.len, 0)) {
	LM_ERR("error adding PW_SIP_URI_USER\n");
	goto err;
    }

    /* Add From Tag */
    if (parse_from_header(_msg) < 0) {
	LM_ERR("error while parsing From header field\n");
	goto err;
    }

    if ((_msg->from==NULL) || (get_from(_msg) == NULL) ||
	(get_from(_msg)->tag_value.s == NULL) ||
	(get_from(_msg)->tag_value.len <= 0)) {
	LM_ERR("error while accessing From header tag\n");
	goto err;
    }
    
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_FROM_TAG].v,
		       get_from(_msg)->tag_value.s,
		       get_from(_msg)->tag_value.len, 0)) {
	LM_ERR("error adding PW_SIP_FROM_TAG\n");
	goto err;
    }

    /* Add Call-Id */
    if ((parse_headers(_msg, HDR_CALLID_F, 0) == -1) ||
	(_msg->callid == NULL) || (_msg->callid->body.s == NULL) ||
	(_msg->callid->body.len <= 0)) {
	LM_ERR("error while accessing Call-Id\n");
	goto err;
    }

    if (!rc_avpair_add(rh, &send, attrs[A_SIP_CALL_ID].v,
		       _msg->callid->body.s,
		       _msg->callid->body.len, 0)) {
	LM_ERR("error adding PW_SIP_CALL_ID\n");
	goto err;
    }
    
    /* Add Service-Type */
    service = vals[V_SIP_VERIFY_DESTINATION].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v,
		       &service, -1, 0)) {
	LM_ERR("error adding PW_SERVICE_TYPE\n");
	goto err;
    }
    
    /* Send Request and generate AVPs of positive reply */
    if ((i = rc_auth(rh, SIP_PORT, send, &received, rad_msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	generate_avps(received);
	rc_avpair_free(received);
	return 1;
    } else {
#ifdef REJECT_RC
	if (i == REJECT_RC) {
	    LM_DBG("rejected\n");
	} else {
	    LM_ERR("failure\n");
	}
	goto err;
#else
	LM_DBG("failure\n");
	goto err;
#endif
    }

err:
    if (send) rc_avpair_free(send);
    if (received) rc_avpair_free(received);
    return -1;
}


/* 
 * Send Radius request to verify source.
 */
int verify_source(struct sip_msg* _msg, char* s1, char* s2)
{
    VALUE_PAIR* send, *received;
    struct hdr_field *hf;
    uint32_t service;
    static char rad_msg[4096];
    int i;

    send = received = 0;
 
    /* Add Request-URI host A_USER_NAME and user as A_SIP_URI_USER */
    if (parse_sip_msg_uri(_msg) < 0) {
        LM_ERR("error while parsing Request-URI\n");
	return -1;
    }

    if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v,
		       _msg->parsed_uri.host.s,
		       _msg->parsed_uri.host.len, 0)) {
	LM_ERR("error adding PW_USER_NAME\n");
	goto err;
    }
   
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_URI_USER].v,
		       _msg->parsed_uri.user.s,
		       _msg->parsed_uri.user.len, 0)) {
	LM_ERR("error adding PW_SIP_URI_USER\n");
	goto err;
    }

    /* Add From Tag */
    if (parse_from_header(_msg) < 0) {
	LM_ERR("error while parsing From header field\n");
	goto err;
    }

    if ((_msg->from==NULL) || (get_from(_msg) == NULL) ||
	(get_from(_msg)->tag_value.s == NULL) ||
	(get_from(_msg)->tag_value.len <= 0)) {
	LM_ERR("error while accessing From header tag\n");
	goto err;
    }
    
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_FROM_TAG].v,
		       get_from(_msg)->tag_value.s,
		       get_from(_msg)->tag_value.len, 0)) {
	LM_ERR("error adding PW_SIP_FROM_TAG\n");
	goto err;
    }

    /* Add Call-Id */
    if ((parse_headers(_msg, HDR_CALLID_F, 0) == -1) ||
	(_msg->callid == NULL) || (_msg->callid->body.s == NULL) ||
	(_msg->callid->body.len <= 0)) {
	LM_ERR("error while accessing Call-Id\n");
	goto err;
    }

    if (!rc_avpair_add(rh, &send, attrs[A_SIP_CALL_ID].v,
		       _msg->callid->body.s,
		       _msg->callid->body.len, 0)) {
	LM_ERR("error adding PW_SIP_CALL_ID\n");
	goto err;
    }

    /* Add P-Request-Hash header body */
    parse_headers(_msg, HDR_EOH_F, 0);
    for (hf = _msg->headers; hf; hf = hf->next) {
		if(cmp_hdrname_strzn(&hf->name, "P-Request-Hash",
			sizeof("P-Request-Hash") - 1) == 0)
	    break;
    }
    if (!hf) {
	LM_ERR("no P-Request-Hash header field\n");
	goto err;
    }
    if ((hf->body.s == NULL) || (hf->body.len <= 0)) {
	LM_ERR("error while accessing P-Request-Hash body\n");
	goto err;
    }
    if (!rc_avpair_add(rh, &send, attrs[A_SIP_REQUEST_HASH].v,
		       hf->body.s, hf->body.len, 0)) {
	LM_ERR("error adding PW_SIP_REQUEST_HASH\n");
	goto err;
    }
    
    /* Add Service-Type */
    service = vals[V_SIP_VERIFY_SOURCE].v;
    if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v,
		       &service, -1, 0)) {
	LM_ERR("error adding PW_SERVICE_TYPE\n");
	goto err;
    }
    
    /* Send Request and generate AVPs of positive reply */
    if ((i = rc_auth(rh, SIP_PORT, send, &received, rad_msg)) == OK_RC) {
	LM_DBG("success\n");
	rc_avpair_free(send);
	rc_avpair_free(received);
	return 1;
    } else {
#ifdef REJECT_RC
	if (i == REJECT_RC) {
	    LM_DBG("rejected\n");
	} else {
	    LM_ERR("failure\n");
	}
	goto err;
#else
	LM_DBG("failure\n");
	goto err;
#endif
    }

err:
    if (send) rc_avpair_free(send);
    if (received) rc_avpair_free(received);
    return -1;
}
