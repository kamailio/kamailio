/*
 * $Id$
 *
 * Group membership checking over Radius
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
 * --------
 * 2003-03-10 - created by janakj
 *
 */

#include <radiusclient.h>
#include <string.h>
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/hf.h"
#include "../../parser/digest/digest.h"
#include "group.h"
#include "../../modules/acc/dict.h"
#include "grouprad_mod.h"


/*
 * Get actual Request-URI
 */
static inline int get_request_uri(struct sip_msg* _m, str* _u)
{
	     /* Use new_uri if present */
	if (_m->new_uri.s) {
		_u->s = _m->new_uri.s;
		_u->len = _m->new_uri.len;
	} else {
		_u->s = _m->first_line.u.request.uri.s;
		_u->len = _m->first_line.u.request.uri.len;
	}

	return 0;
}


/*
 * Get To header field URI
 */
static inline int get_to_uri(struct sip_msg* _m, str* _u)
{
	     /* Double check that the header field is there
	      * and is parsed
	      */
	if (!_m->to && ((parse_headers(_m, HDR_TO, 0) == -1) || !_m->to)) {
		LOG(L_ERR, "get_to_uri(): Can't get To header field\n");
		return -1;
	}
	
	_u->s = ((struct to_body*)_m->to->parsed)->uri.s;
	_u->len = ((struct to_body*)_m->to->parsed)->uri.len;
	
	return 0;
}


/*
 * Get From header field URI
 */
static inline int get_from_uri(struct sip_msg* _m, str* _u)
{
	     /* Double check that the header field is there
	      * and is parsed
	      */
	if (parse_from_header(_m) < 0) {
		LOG(L_ERR, "get_from_uri(): Error while parsing From body\n");
		return -1;
	}
	
	_u->s = ((struct to_body*)_m->from->parsed)->uri.s;
	_u->len = ((struct to_body*)_m->from->parsed)->uri.len;

	return 0;
}


/*
 * Check from Radius if a user belongs to a group. User-Name is digest
 * username or digest username@realm, SIP-Group is group, and Service-Type
 * is Group-Check.  SIP-Group is SER specific attribute and Group-Check is
 * SER specific service type value.
 */
int radius_is_user_in(struct sip_msg* _m, char* _hf, char* _group)
{
	str *grp, user_name, user, domain, uri;
	dig_cred_t* cred = 0;
	int hf_type;
	UINT4 service;
	VALUE_PAIR *send, *received;
	static char msg[4096];
	struct hdr_field* h;
	struct sip_uri puri;

	grp = (str*)_group; /* via fixup */
	send = received = 0;

	hf_type = (int)_hf;

	switch(hf_type) {
	case 1: /* Request-URI */
		if (get_request_uri(_m, &uri) < 0) {
			LOG(L_ERR, "radius_is_user_in(): Error while extracting Request-URI\n");
			return -1;
		}
		break;

	case 2: /* To */
		if (get_to_uri(_m, &uri) < 0) {
			LOG(L_ERR, "radius_is_user_in(): Error while extracting To\n");
			return -2;
		}
		break;

	case 3: /* From */
		if (get_from_uri(_m, &uri) < 0) {
			LOG(L_ERR, "radius_is_user_in(): Error while extracting From\n");
			return -3;
		}
		break;

	case 4: /* Credentials */
		get_authorized_cred(_m->authorization, &h);
		if (!h) {
			get_authorized_cred(_m->proxy_auth, &h);
			if (!h) {
				LOG(L_ERR, "radius_is_user_in(): No authorized credentials found (error in scripts)\n");
				return -4;
			}
		}
		cred = &((auth_body_t*)(h->parsed))->digest;
		break;
	}

	if (hf_type != 4) {
		if (parse_uri(uri.s, uri.len, &puri) < 0) {
			LOG(L_ERR, "radius_is_user_in(): Error while parsing URI\n");
			return -5;
		}
		user = puri.user;
		domain = puri.host;
	} else {
		user = cred->username.user;
		domain = cred->realm;
	}
		

	if (use_domain) {
		user_name.len = user.len + domain.len + 1;
		user_name.s = (char*)pkg_malloc(user_name.len);
		if (!user_name.s) {
			LOG(L_ERR, "radius_is_user_in(): No memory left\n");
			return -6;
		}
		
		memcpy(user_name.s, user.s, user.len);
		user_name.s[user.len] = '@';
		memcpy(user_name.s + user.len + 1, domain.s, domain.len);
	} else {
		user_name = user;
	}

	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user_name.s, user_name.len, 0)) {
		LOG(L_ERR, "radius_is_user_in(): Error adding User-Name attribute\n");
		rc_avpair_free(send);
		if (use_domain) pkg_free(user_name.s);
		return -7;
	}

	if (use_domain) pkg_free(user_name.s);

	if (!rc_avpair_add(rh, &send, attrs[A_SIP_GROUP].v, grp->s, grp->len, 0)) {
		LOG(L_ERR, "radius_is_user_in(): Error adding Sip-Group attribute\n");
	 	return -8;  	
	}

	service = vals[V_GROUP_CHECK].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, 0, 0)) {
		LOG(L_ERR, "radius_is_user_in(): Error adding Service-Type attribute\n");
		rc_avpair_free(send);
	 	return -9;  	
	}

	if (rc_auth(rh, 0, send, &received, msg) == OK_RC) {
		DBG("radius_is_user_in(): Success\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return 1;
	} else {
		DBG("radius_is_user_in(): Failure\n");
		rc_avpair_free(send);
		rc_avpair_free(received);
		return -11;
	}
}
