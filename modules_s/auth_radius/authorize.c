/*
 * $Id$
 *
 * Digest Authentication - Radius support
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * -------
 * 2003-03-09: Based on authorize.c from radius_auth (janakj)
 */


#include <string.h>
#include <stdlib.h>
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../parser/hf.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../auth/api.h"
#include "authorize.h"
#include "sterman.h"
#include "authrad_mod.h"


/* 
 * Extract URI depending on the request from To or From header 
 */
static inline int get_uri(struct sip_msg* _m, str** _uri)
{
	if ((REQ_LINE(_m).method.len == 8) && (memcmp(REQ_LINE(_m).method.s, "REGISTER", 8) == 0)) {
		if (!_m->to && ((parse_headers(_m, HDR_TO, 0) == -1) || !_m->to)) {
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
static inline int authorize(struct sip_msg* _msg, str* _realm, int _hftype)
{
	int res;
	auth_result_t ret;
	struct hdr_field* h;
	auth_body_t* cred;
	str* uri;
	struct sip_uri puri;
	str user, domain;

	domain = *_realm;
	ret = pre_auth_func(_msg, &domain, _hftype, &h);
	
	switch(ret) {
	case ERROR:            return 0;
	case NOT_AUTHORIZED:   return -1;
	case DO_AUTHORIZATION: break;
	case AUTHORIZED:       return 1;
	}

	cred = (auth_body_t*)h->parsed;

	if (get_uri(_msg, &uri) < 0) {
		LOG(L_ERR, "authorize(): From/To URI not found\n");
		return -1;
	}
	
	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "authorize(): Error while parsing From/To URI\n");
		return -1;
	}

	if (puri.host.len != cred->digest.realm.len) {
		DBG("authorize(): Credentials realm and URI host do not match\n");   
		return -1;
	}
	if (strncasecmp(puri.host.s, cred->digest.realm.s, puri.host.len) != 0) {
		DBG("authorize(): Credentials realm and URI host do not match\n");
		return -1;
	}

	user.s = (char *)pkg_malloc(puri.user.len);
	un_escape(&(puri.user), &user);

	res = radius_authorize_sterman(&cred->digest, &_msg->first_line.u.request.method, &user);
	pkg_free(user.s);

	if (res == 1) {
		ret = post_auth_func(_msg, h);
		switch(ret) {
		case ERROR:          return 0;
		case NOT_AUTHORIZED: return -1;
		case AUTHORIZED:     return 1;
		default:             return -1;
		}
	}

	return -1;
}


/*
 * Authorize using Proxy-Authorize header field
 */
int radius_proxy_authorize(struct sip_msg* _msg, char* _realm, char* _s2)
{
	/* realm parameter is converted to str* in str_fixup */
	return authorize(_msg, (str*)_realm, HDR_PROXYAUTH);
}


/*
 * Authorize using WWW-Authorize header field
 */
int radius_www_authorize(struct sip_msg* _msg, char* _realm, char* _s2)
{
	return authorize(_msg, (str*)_realm, HDR_AUTHORIZATION);
}

