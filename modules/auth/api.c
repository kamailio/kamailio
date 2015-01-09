/*
 * Digest Authentication Module
 * 
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <string.h>
#include "api.h"
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "auth_mod.h"
#include "nonce.h"

static int auth_check_hdr_md5(struct sip_msg* msg, auth_body_t* auth_body,
		auth_result_t* auth_res);

/*
 * Purpose of this function is to find credentials with given realm,
 * do sanity check, validate credential correctness and determine if
 * we should really authenticate (there must be no authentication for
 * ACK and CANCEL
 * @param hdr output param where the Authorize headerfield will be returned.
 * @param check_hdr  pointer to the function checking Authorization header field
 */
auth_result_t pre_auth(struct sip_msg* msg, str* realm, hdr_types_t hftype,
						struct hdr_field**  hdr,
						check_auth_hdr_t check_auth_hdr)
{
	int ret;
	auth_body_t* c;
	check_auth_hdr_t check_hf;
	auth_result_t    auth_rv;

	     /* ACK and CANCEL must be always authenticated, there is
	      * no way how to challenge ACK and CANCEL cannot be
	      * challenged because it must have the same CSeq as
	      * the request to be canceled.
	      * PRACK is also not authenticated
	      */

	if (msg->REQ_METHOD & (METHOD_ACK|METHOD_CANCEL|METHOD_PRACK))
		return AUTHENTICATED;

	     /* Try to find credentials with corresponding realm
	      * in the message, parse them and return pointer to
	      * parsed structure
	      */
	strip_realm(realm);
	ret = find_credentials(msg, realm, hftype, hdr);
	if (ret < 0) {
		LOG(L_ERR, "auth:pre_auth: Error while looking for credentials\n");
		return ERROR;
	} else if (ret > 0) {
		DBG("auth:pre_auth: Credentials with realm '%.*s' not found\n",
				realm->len, ZSW(realm->s));
		return NO_CREDENTIALS;
	}

	     /* Pointer to the parsed credentials */
	c = (auth_body_t*)((*hdr)->parsed);

	    /* digest headers are in c->digest */
	DBG("auth: digest-algo: %.*s parsed value: %d\n",
			c->digest.alg.alg_str.len, c->digest.alg.alg_str.s,
			c->digest.alg.alg_parsed);

	if (mark_authorized_cred(msg, *hdr) < 0) {
		LOG(L_ERR, "auth:pre_auth: Error while marking parsed credentials\n");
		return ERROR;
	}

	    /* check authorization header field's validity */
	if (check_auth_hdr == NULL) {
		check_hf = auth_check_hdr_md5;
	} else {	/* use check function of external authentication module */
		check_hf = check_auth_hdr;
	}
	/* use the right function */
	if (!check_hf(msg, c, &auth_rv)) {
		return auth_rv;
	}
	
	return DO_AUTHENTICATION;
}

/**
 * TODO move it to rfc2617.c 
 * 
 * @param auth_res return value of authentication. Maybe the it will be not affected.
 * @result if authentication should continue (1) or not (0)
 * 
 */
static int auth_check_hdr_md5(struct sip_msg* msg, auth_body_t* auth,
		auth_result_t* auth_res)
{
	int ret;
	
	    /* Check credentials correctness here */
	if (check_dig_cred(&auth->digest) != E_DIG_OK) {
		LOG(L_ERR, "auth:pre_auth: Credentials are not filled properly\n");
		*auth_res = BAD_CREDENTIALS;
		return 0;
	}

	ret = check_nonce(auth, &secret1, &secret2, msg);
	if (ret!=0){
		if (ret==3 || ret==4){
			/* failed auth_extra_checks or stale */
			auth->stale=1; /* we mark the nonce as stale 
							(hack that makes our life much easier) */
			*auth_res = STALE_NONCE;
			return 0;
		} else if (ret==6) {
			*auth_res = NONCE_REUSED;
			return 0;
		} else {
			DBG("auth:pre_auth: Invalid nonce value received (ret %d)\n", ret);
			*auth_res = NOT_AUTHENTICATED;
			return 0;
		}
	}
	return 1;
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

	if (c->stale ) {
		if ((msg->REQ_METHOD == METHOD_ACK) || 
		    (msg->REQ_METHOD == METHOD_CANCEL)) {
			     /* Method is ACK or CANCEL, we must accept stale
			      * nonces because there is no way how to challenge
			      * with new nonce (ACK has no response associated 
			      * and CANCEL must have the same CSeq as the request 
			      * to be canceled)
			      */
		} else {
			c->stale = 1;
			res = NOT_AUTHENTICATED;
		}
	}

	return res;
}

/*
 * Calculate the response and compare with the given response string
 * Authorization is successful if this two strings are same
 */
int auth_check_response(dig_cred_t* cred, str* method, char* ha1)
{
	HASHHEX resp, hent;

	/*
	 * First, we have to verify that the response received has
	 * the same length as responses created by us
	 */
	if (cred->response.len != 32) {
		DBG("check_response: Receive response len != 32\n");
		return BAD_CREDENTIALS;
	}

	/*
	 * Now, calculate our response from parameters received
	 * from the user agent
	 */
	calc_response(ha1, &(cred->nonce),
				  &(cred->nc), &(cred->cnonce),
				  &(cred->qop.qop_str), cred->qop.qop_parsed == QOP_AUTHINT,
				  method, &(cred->uri), hent, resp);

	DBG("check_response: Our result = \'%s\'\n", resp);

	/*
	 * And simply compare the strings, the user is
	 * authorized if they match
	 */
	if (!memcmp(resp, cred->response.s, 32)) {
		DBG("check_response: Authorization is OK\n");
		return AUTHENTICATED;
	} else {
		DBG("check_response: Authorization failed\n");
		return NOT_AUTHENTICATED;
	}
}


int bind_auth_s(auth_api_s_t* api)
{
	if (!api) {
		LOG(L_ERR, "bind_auth: Invalid parameter value\n");
		return -1;
	}

	api->pre_auth = pre_auth;
	api->post_auth = post_auth;
	api->build_challenge = build_challenge_hf;
	api->qop = &auth_qop;
	api->calc_HA1 = calc_HA1;
	api->calc_response = calc_response;
	api->check_response = auth_check_response;
	api->auth_challenge = auth_challenge;
	api->pv_authenticate = pv_authenticate;
	api->consume_credentials = consume_credentials;
	return 0;
}
