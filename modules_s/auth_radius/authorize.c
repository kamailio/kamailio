/*
 * $Id$
 *
 * Authorize related functions that use radius
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
 */


#include "authorize.h"
#include "../../parser/hf.h"            /* HDR_PROXYAUTH & HDR_AUTHORIZATION */
#include "defs.h"                       /* ACK_CANCEL_HACK */
#include "../../str.h"
#include <string.h>                     /* memcmp */
#include "nonce.h"
#include "../../parser/digest/digest.h" /* dig_cred_t */
#include "common.h"                     /* send_resp */
#include "auth_mod.h"
#include "../../db/db.h"
#include "../../mem/mem.h"
#include "rfc2617.h"
#include "digest.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"    /* get_uri */
#include "utils.h"
#include <stdlib.h>


#define MESSAGE_400 "Bad Request"

/* Extract URI depending on the request from To or From header */
static int get_uri(struct sip_msg* _m, str** _uri)
{
	if ((REQ_LINE(_m).method.len == 8) && (strncmp(REQ_LINE(_m).method.s, "REGISTER", 8) == 0)) {
		*_uri = &(get_to(_m)->uri);
	} else {
		if (!(_m->from->parsed)) {
			if (parse_from_header(_m) == -1) {
				LOG(L_ERR, "get_realms(): Error while parsing headers\n");
				return -1;
			}
		}
		*_uri = &(get_from(_m)->uri);
	}
	return 1;
}


static inline int find_credentials(struct sip_msg* _m, str* _realm, int _hftype, struct hdr_field** _h)
{
	struct hdr_field** hook;
	struct hdr_field* ptr, *prev;
	int res;
	str* r;

	switch(_hftype) {
	case HDR_AUTHORIZATION: hook = &(_m->authorization); break;
	case HDR_PROXYAUTH:     hook = &(_m->proxy_auth);    break;
	default:
		LOG(L_ERR, "find_credentials(): Invalid header field typ as parameter\n");
		return -1;
	}

	*_h = 0;
	
	if (!(*hook)) {
		     /* No credentials parsed yet */
		if (parse_headers(_m, _hftype, 0) == -1) {
			LOG(L_ERR, "find_credentials(): Error while parsing headers\n");
			return -2;
		}
	}

	ptr = *hook;

	while(ptr) {
		res = parse_credentials(ptr);
		if (res < 0) {
			LOG(L_ERR, "find_credentials(): Error while parsing credentials\n");
			if (send_resp(_m, 400, MESSAGE_400, 0, 0) == -1) {
				LOG(L_ERR, "find_credentials(): Error while sending 400 reply\n");
			}
			return -1;
		} else if (res == 0) {
			r = &(((auth_body_t*)(ptr->parsed))->digest.realm);
			if (r->len == _realm->len) {
				if (!strncasecmp(_realm->s, r->s, r->len)) {
					*_h = ptr;
					return 0;
				}
			}
#ifdef REALM_HACK
			if (_realm->len == 0) {
				*_h = ptr;
				return 0;
 			}
#endif			
		}

		prev = ptr;
		if (parse_headers(_m, _hftype, 1) == -1) {
			LOG(L_ERR, "find_credentials(): Error while parsing headers\n");
			return -3;
		} else {
			if (prev != _m->last_header) {
				if (_m->last_header->type == _hftype) ptr = _m->last_header;
				else ptr = 0;
			} else ptr = 0;
		}
	}
	return 0;
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg* _msg, str* _realm, int _hftype)
{
	int res;
	struct hdr_field* h;
	auth_body_t* cred;
	str* uri;
	struct sip_uri puri;
	str user;

#ifdef ACK_CANCEL_HACK
	/* ACK must be always authorized, there is
         * no way how to challenge ACK
	 */
	if ((_msg->REQ_METHOD == METHOD_ACK) || 
	    (_msg->REQ_METHOD == METHOD_CANCEL)) {
	        return 1;
	}
#endif

	/* Try to find credentials with corresponding realm
	 * in the message, parse them and return pointer to
	 * parsed structure
	 */
	if (find_credentials(_msg, _realm, _hftype, &h) < 0) {
		LOG(L_ERR, "authorize(): Error while looking for credentials\n");
		return -1;
	}

	/*
	 * No credentials with given realm found, don't authorize
	 */
	if (h == 0) {
		DBG("authorize(): Credentials with given realm not found\n");
		return -1;
	}

	cred = (auth_body_t*)(h->parsed);

	/* Check credentials correctness here 
	 * FIXME: 400s should be sent from routing scripts, but we will need
	 * variables for that
	 */
	if (check_dig_cred(&(cred->digest)) != E_DIG_OK) {
		LOG(L_ERR, "authorize(): Credentials received are not filled properly\n");

		if (send_resp(_msg, 400, MESSAGE_400, NULL, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
		}
		return 0;
	}

	/* Retrieve number of retries with the received nonce and
	 * save it
	 */
	cred->nonce_retries = get_nonce_retry(&(cred->digest.nonce));

	/* Retrieve URI from To (Register) or From (other requests) header
	 */
	if (get_uri(_msg, &uri) == -1) {
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

	/* Un-escape user */
	
	user.s = malloc(puri.user.len);
	un_escape(&(puri.user), &user);

	if (radius_authorize_sterman(&cred->digest, &_msg->first_line.u.request.method, &user) == 1) res = 1;
	else res = -1;

	free(user.s);

	if (res != 1) {  /* response was OK */
		DBG("authorize(): Recalculated response is different\n");
		return -1;
	}

	if (mark_authorized_cred(_msg, h) < 0) {
		LOG(L_ERR, "authorize(): Error while marking parsed credentials\n");
		return -1;
	}
	return 1;
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

