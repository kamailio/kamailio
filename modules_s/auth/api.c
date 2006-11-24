/*
 * $Id$
 *
 * Digest Authentication Module
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include "api.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "auth_mod.h"
#include "nonce.h"

/*
 * Purpose of this function is to find credentials with given realm,
 * do sanity check, validate credential correctness and determine if
 * we should really authenticate (there must be no authentication for
 * ACK and CANCEL
 */
auth_result_t pre_auth(struct sip_msg* msg, str* realm, hdr_types_t hftype,
						struct hdr_field**  hdr)
{
	int ret;
	auth_body_t* c;
	static str prack = STR_STATIC_INIT("PRACK");

	     /* ACK and CANCEL must be always authenticated, there is
	      * no way how to challenge ACK and CANCEL cannot be
	      * challenged because it must have the same CSeq as
	      * the request to be canceled
	      */

	if ((msg->REQ_METHOD == METHOD_ACK) ||  (msg->REQ_METHOD == METHOD_CANCEL)) return AUTHENTICATED;
	     /* PRACK is also not authenticated */
	if ((msg->REQ_METHOD == METHOD_OTHER)) {
		if (msg->first_line.u.request.method.len == prack.len &&
		    !memcmp(msg->first_line.u.request.method.s, prack.s, prack.len))
			return AUTHENTICATED;
	}

	     /* Try to find credentials with corresponding realm
	      * in the message, parse them and return pointer to
	      * parsed structure
	      */
	ret = find_credentials(msg, realm, hftype, hdr);
	if (ret < 0) {
		LOG(L_ERR, "auth:pre_auth: Error while looking for credentials\n");
		return ERROR;
	} else if (ret > 0) {
		DBG("auth:pre_auth: Credentials with realm '%.*s' not found\n", realm->len, ZSW(realm->s));
		return NOT_AUTHENTICATED;
	}

	     /* Pointer to the parsed credentials */
	c = (auth_body_t*)((*hdr)->parsed);

	     /* Check credentials correctness here */
	if (check_dig_cred(&(c->digest)) != E_DIG_OK) {
		LOG(L_ERR, "auth:pre_auth: Credentials are not filled properly\n");
		return BAD_CREDENTIALS;
	}

	if (check_nonce(&c->digest.nonce, &secret, msg) != 0) {
		DBG("auth:pre_auth: Invalid nonce value received\n");
		return NOT_AUTHENTICATED;
	}

	return DO_AUTHENTICATION;
}


/*
 * Purpose of this function is to do post authentication steps like
 * marking authorized credentials and so on.
 */
auth_result_t post_auth(struct sip_msg* msg, struct hdr_field* hdr)
{
	int res = AUTHENTICATED;
	auth_body_t* c;

	c = (auth_body_t*)((hdr)->parsed);

	if (is_nonce_stale(&c->digest.nonce)) {
		if ((msg->REQ_METHOD == METHOD_ACK) || 
		    (msg->REQ_METHOD == METHOD_CANCEL)) {
			     /* Method is ACK or CANCEL, we must accept stale
			      * nonces because there is no way how to challenge
			      * with new nonce (ACK has no response associated 
			      * and CANCEL must have the same CSeq as the request 
			      * to be canceled)
			      */
		} else {
			DBG("auth:post_auth: Response is OK, but nonce is stale\n");
			c->stale = 1;
			res = NOT_AUTHENTICATED;
		}
	}

	if (mark_authorized_cred(msg, hdr) < 0) {
		LOG(L_ERR, "auth:post_auth: Error while marking parsed credentials\n");
		res = ERROR;
	}

	return res;
}


int bind_auth(auth_api_t* api)
{
	if (!api) {
		LOG(L_ERR, "bind_auth: Invalid parameter value\n");
		return -1;
	}

	api->pre_auth = pre_auth;
	api->post_auth = post_auth;
	api->build_challenge = build_challenge_hf;
	api->qop = &qop;
	return 0;
}
