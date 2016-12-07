/*
 * $Id$
 *
 * Digest Authentication - Radius support
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History:
 * -------
 * 2003-03-09: Based on authorize.c from radius_auth (janakj)
 */


#include <string.h>
#include <stdlib.h>
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../sr_module.h"
#include "../../parser/hf.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../dprint.h"
#include "../../id.h"
#include "../../ut.h"
#include "../../modules/auth/api.h"
#include "authorize.h"
#include "sterman.h"
#include "authrad_mod.h"


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
static int generate_avps(VALUE_PAIR* received)
{
	int_str name, val;
	VALUE_PAIR *vp;
	
	vp = rc_avpair_get(received, ATTRID(attrs[A_SER_UID].v), VENDOR(attrs[A_SER_UID].v));
	if (vp == NULL) {
	    WARN("RADIUS server did not send SER-UID attribute in digest authentication reply\n");
	    return -1;
	}
	val.s.len = vp->lvalue;
	val.s.s = vp->strvalue;
	name.s.s = "uid";
	name.s.len = 3;

	if (add_avp(AVP_TRACK_FROM | AVP_CLASS_USER | AVP_NAME_STR | AVP_VAL_STR, name, val) < 0) {
	    ERR("Unable to create UID attribute\n");
	    return -1;
	}

	vp = received;
	while ((vp = rc_avpair_get(vp, ATTRID(attrs[A_SER_ATTR].v), VENDOR(attrs[A_SER_ATTR].v)))) {
		attr_name_value(&name.s, &val.s, vp);
		if (name.s.len == 0) {
		    ERR("Missing attribute name\n");
		    return -1;
		}
		
		if (add_avp(AVP_TRACK_FROM | AVP_CLASS_USER | AVP_NAME_STR | AVP_VAL_STR, name, val) < 0) {
			LOG(L_ERR, "generate_avps: Unable to create a new AVP\n");
			return -1;
		} else {
			DBG("generate_avps: AVP '%.*s'='%.*s' has been added\n",
			    name.s.len, ZSW(name.s.s), 
			    val.s.len, ZSW(val.s.s));
		}
		vp = vp->next;
	}
	
	return 0;
}




/* 
 * Extract URI depending on the request from To or From header 
 */
static inline int get_uri(struct sip_msg* _m, str** _uri)
{
	if ((REQ_LINE(_m).method.len == 8) && (memcmp(REQ_LINE(_m).method.s, "REGISTER", 8) == 0)) {
		if (!_m->to && ((parse_headers(_m, HDR_TO_F, 0) == -1) || !_m->to)) {
			LOG(L_ERR, "get_uri(): To header field not found or malformed\n");
			return -1;
		}
		*_uri = &(get_to(_m)->uri);
	} else {
		if (parse_from_header(_m) == -1) {
			LOG(L_ERR, "get_uri(): Error while parsing headers\n");
			return -2;
		}
		*_uri = &(get_from(_m)->uri);
	}
	return 0;
}


/*
 * Authorize digest credentials
 */
static inline int authenticate(struct sip_msg* msg, str* realm,
			       hdr_types_t hftype)
{
	int res;
	auth_result_t ret;
	struct hdr_field* h;
	auth_body_t* cred;
	str* uri;
	struct sip_uri puri;
	str user, did;
	VALUE_PAIR* received;

	cred = 0;
	ret = -1;
	user.s = 0;
	received = NULL;

	switch(auth_api.pre_auth(msg, realm, hftype, &h, NULL)) {
	default:
		BUG("unexpected reply '%d'.\n", auth_api.pre_auth(msg, realm, hftype,
				&h, NULL));
#ifdef EXTRA_DEBUG
		abort();
#endif
	case NONCE_REUSED:
		LM_DBG("nonce reused");
		ret = AUTH_NONCE_REUSED;
		goto end;
	case STALE_NONCE:
		LM_DBG("stale nonce\n");
		ret = AUTH_STALE_NONCE;
		goto end;
	case NO_CREDENTIALS:
		LM_DBG("no credentials\n");
		ret = AUTH_NO_CREDENTIALS;
	case ERROR:
	case BAD_CREDENTIALS:
	    ret = -3;
	    goto end;

	case NOT_AUTHENTICATED:
	    ret = -1;
	    goto end;

	case DO_AUTHENTICATION:
	    break;

	case AUTHENTICATED:
	    ret = 1;
	    goto end;
	}

	cred = (auth_body_t*)h->parsed;

	if (use_did) {
	    if (msg->REQ_METHOD == METHOD_REGISTER) {
			ret = get_to_did(&did, msg);
	    } else {
			ret = get_from_did(&did, msg);
	    }
	    if (ret == 0) {
			did.s = DEFAULT_DID;
			did.len = sizeof(DEFAULT_DID) - 1;
	    }
	} else {
	    did.len = 0;
	    did.s = 0;
	}

	if (get_uri(msg, &uri) < 0) {
		LOG(L_ERR, "authorize(): From/To URI not found\n");
		ret = -1;
		goto end;
	}
	
	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "authorize(): Error while parsing From/To URI\n");
		ret = -1;
		goto end;
	}

	user.s = (char *)pkg_malloc(puri.user.len);
	if (user.s == NULL) {
		LOG(L_ERR, "authorize: No memory left\n");
		ret = -1;
		goto end;
	}
	un_escape(&(puri.user), &user);

	res = radius_authorize_sterman(&received, msg, &cred->digest, &msg->first_line.u.request.method, &user);
	if (res == 1) {
	    switch(auth_api.post_auth(msg, h)) {
	    case ERROR:             
	    case BAD_CREDENTIALS:
		ret = -2;
		break;

	    case NOT_AUTHENTICATED:
		ret = -1;
		break;

	    case AUTHENTICATED:
		if (generate_avps(received) < 0) {
		    ret = -1;
		    break;
		}
		ret = 1;
		break;

	    default:
		ret = -1;
		break;
	    }
	} else {
	    ret = -1;
	}

 end:
	if (received) rc_avpair_free(received);
	if (user.s) pkg_free(user.s);
	if (ret < 0) {
	    if (auth_api.build_challenge(msg, (cred ? cred->stale : 0), realm, NULL, NULL, hftype) < 0) {
		ERR("Error while creating challenge\n");
		ret = -2;
	    }
	}
	return ret;
}


/*
 * Authorize using Proxy-Authorize header field
 */
int radius_proxy_authorize(struct sip_msg* _msg, char* p1, char* p2)
{
    str realm;

    if (get_str_fparam(&realm, _msg, (fparam_t*)p1) < 0) {
	ERR("Cannot obtain digest realm from parameter '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }
    
	 /* realm parameter is converted to str* in str_fixup */
    return authenticate(_msg, &realm, HDR_PROXYAUTH_T);
}


/*
 * Authorize using WWW-Authorize header field
 */
int radius_www_authorize(struct sip_msg* _msg, char* p1, char* p2)
{
    str realm;

    if (get_str_fparam(&realm, _msg, (fparam_t*)p1) < 0) {
	ERR("Cannot obtain digest realm from parameter '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }
    
    return authenticate(_msg, &realm, HDR_AUTHORIZATION_T);
}

