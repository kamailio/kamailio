/*
 * $Id$
 *
 * Digest Authentication Module
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

#include <string.h>
#include "api.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "auth_mod.h"
#include "nonce.h"
#include "common.h"


/*
 * Find credentials with given realm in a SIP message header
 */
static inline int find_credentials(struct sip_msg* _m, str* _realm, int _hftype, struct hdr_field** _h)
{
	struct hdr_field** hook, *ptr, *prev;
	int res;
	str* r;

	     /*
	      * Determine if we should use WWW-Authorization or
	      * Proxy-Authorization header fields, this parameter
	      * is set in www_authorize and proxy_authorize
	      */
	switch(_hftype) {
	case HDR_AUTHORIZATION: hook = &(_m->authorization); break;
	case HDR_PROXYAUTH:     hook = &(_m->proxy_auth);    break;
	default:                hook = &(_m->authorization); break;
	}

	     /*
	      * If the credentials haven't been parsed yet, do it now
	      */
	if (*hook == 0) {
		     /* No credentials parsed yet */
		if (parse_headers(_m, _hftype, 0) == -1) {
			LOG(L_ERR, "find_credentials(): Error while parsing headers\n");
			return -1;
		}
	}

	ptr = *hook;

	     /*
	      * Iterate through the credentials in the message and
	      * find credentials with given realm
	      */
	while(ptr) {
		res = parse_credentials(ptr);
		if (res < 0) {
			LOG(L_ERR, "find_credentials(): Error while parsing credentials\n");
			return (res == -1) ? -2 : -3;
		} else if (res == 0) {
			r = &(((auth_body_t*)(ptr->parsed))->digest.realm);

			if (r->len == _realm->len) {
				if (!strncasecmp(_realm->s, r->s, r->len)) {
					*_h = ptr;
					return 0;
				}
			}
		}

		prev = ptr;
		if (parse_headers(_m, _hftype, 1) == -1) {
			LOG(L_ERR, "find_credentials(): Error while parsing headers\n");
			return -4;
		} else {
			if (prev != _m->last_header) {
				if (_m->last_header->type == _hftype) ptr = _m->last_header;
				else break;
			} else break;
		}
	}
	
	     /*
	      * Credentials with given realm not found
	      */
	return 1;
}


/*
 * Purpose of this function is to find credentials with given realm,
 * do sanity check, validate credential correctness and determine if
 * we should really authenticate (there must be no authentication for
 * ACK and CANCEL
 */
auth_result_t pre_auth(struct sip_msg* _m, str** _realm, int _hftype, struct hdr_field** _h)
{
	int ret;
	auth_body_t* c;
	struct sip_uri uri;

	     /* ACK and CANCEL must be always authorized, there is
	      * no way how to challenge ACK and CANCEL cannot be
	      * challenged because it must have the same CSeq as
	      * the request to be cancelled
	      */

	if ((_m->REQ_METHOD == METHOD_ACK) ||  (_m->REQ_METHOD == METHOD_CANCEL)) return AUTHORIZED;

	if ((*_realm)->len == 0) {
		if (get_realm(_m, &uri) < 0) {
			LOG(L_ERR, "pre_auth(): Error while extracting realm\n");
			if (send_resp(_m, 400, MESSAGE_400, 0, 0) == -1) {
				LOG(L_ERR, "pre_auth(): Error while sending 400 reply\n");
			}
			return ERROR;
		}
		
		*_realm = &uri.host;
	}

	     /* Try to find credentials with corresponding realm
	      * in the message, parse them and return pointer to
	      * parsed structure
	      */
	ret = find_credentials(_m, *_realm, _hftype, _h);
	if (ret < 0) {
		LOG(L_ERR, "pre_auth(): Error while looking for credentials\n");
		if (send_resp(_m, (ret == -2) ? 500 : 400, 
			      (ret == -2) ? MESSAGE_500 : MESSAGE_400, 0, 0) == -1) {
			LOG(L_ERR, "pre_auth(): Error while sending 400 reply\n");
		}
		return ERROR;
	} else if (ret > 0) {
		DBG("pre_auth(): Credentials with given realm not found\n");
		return NOT_AUTHORIZED;
	}

	     /* Pointer to the parsed credentials */
	c = (auth_body_t*)((*_h)->parsed);

	     /* Check credentials correctness here */
	if (check_dig_cred(&(c->digest)) != E_DIG_OK) {
		LOG(L_ERR, "pre_auth(): Credentials received are not filled properly\n");
		if (send_resp(_m, 400, MESSAGE_400, 0, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
		}
		return ERROR;
	}

	if (check_nonce(&c->digest.nonce, &secret) != 0) {
		LOG(L_ALERT, "pre_auth(): Invalid nonce value received, very suspicious !\n");
		return NOT_AUTHORIZED;
	}

	return DO_AUTHORIZATION;
}


/*
 * Purpose of this function is to do post authentication steps like
 * marking authorized credentials and so on.
 */
auth_result_t post_auth(struct sip_msg* _m, struct hdr_field* _h)
{
	auth_body_t* c;

	c = (auth_body_t*)((_h)->parsed);

	if (is_nonce_stale(&c->digest.nonce)) {
		if ((_m->REQ_METHOD == METHOD_ACK) || 
		    (_m->REQ_METHOD == METHOD_CANCEL)) {
			     /* Method is ACK or CANCEL, we must accept stale
			      * nonces because there is no way how to challenge
			      * with new nonce (ACK has no response associated 
			      * and CANCEL must have the same CSeq as the request 
			      * to be cancelled)
			      */
			goto mark;
		} else {
			DBG("post_auth(): Response is OK, but nonce is stale\n");
			c->stale = 1;
			return NOT_AUTHORIZED;
		}
	}
 mark:
	if (mark_authorized_cred(_m, _h) < 0) {
		LOG(L_ERR, "post_auth(): Error while marking parsed credentials\n");
		if (send_resp(_m, 500, MESSAGE_500, 0, 0) == -1) {
			LOG(L_ERR, "authorize(): Error while sending 400 reply\n");
		}
		return ERROR;
	}

	return AUTHORIZED;
}
