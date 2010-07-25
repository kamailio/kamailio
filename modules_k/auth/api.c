/*
 * $Id$
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief Digest Authentication Module, API exports
 * \ingroup auth
 * - Module: \ref auth
 */

#include <string.h>
#include "../../dprint.h"
#include "../../parser/digest/digest.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../ut.h"
#include "auth_mod.h"
#include "nonce.h"
#include "common.h"
#include "api.h"
#include "index.h"

static str auth_400_err = str_init(MESSAGE_400);
static str auth_500_err = str_init(MESSAGE_500);


/*!
 * \brief Strip the beginning of a realm string
 *
 * Strip the beginning of a realm string, depending on the length of
 * the realm_prefix.
 * \param _realm realm string
 */
void strip_realm(str* _realm)
{
	/* no param defined -- return */
	if (!realm_prefix.len) return;

	/* prefix longer than realm -- return */
	if (realm_prefix.len > _realm->len) return;

	/* match ? -- if so, shorten realm -*/
	if (memcmp(realm_prefix.s, _realm->s, realm_prefix.len) == 0) {
		_realm->s += realm_prefix.len;
		_realm->len -= realm_prefix.len;
	}
	return;
}


/*!
 * \brief Find credentials with given realm, check if we need to authenticate
 *
 * The purpose of this function is to find credentials with given realm,
 * do sanity check, validate credential correctness and determine if
 * we should really authenticate (there must be no authentication for
 * ACK and CANCEL.
 * \param _m SIP message
 * \param _realm authentification realm
 * \param _hftype header field type
 * \param _h header field
 * \return authentification result
 */
auth_result_t pre_auth(struct sip_msg* _m, str* _realm, hdr_types_t _hftype,
													struct hdr_field** _h)
{
	int ret;
	auth_body_t* c;
	struct sip_uri *uri;

	/* ACK and CANCEL must be always authorized, there is
	 * no way how to challenge ACK and CANCEL cannot be
	 * challenged because it must have the same CSeq as
	 * the request to be canceled
	 */

	if ((_m->REQ_METHOD == METHOD_ACK) ||  (_m->REQ_METHOD == METHOD_CANCEL))
		return AUTHORIZED;

	if (_realm->len == 0) {
		if (get_realm(_m, _hftype, &uri) < 0) {
			LM_ERR("failed to extract realm\n");
			if (send_resp(_m, 400, &auth_400_err, 0, 0) == -1) {
				LM_ERR("failed to send 400 reply\n");
			}
			return ERROR;
		}
		
		*_realm = uri->host;
		strip_realm(_realm);
	}

	/* Try to find credentials with corresponding realm
	 * in the message, parse them and return pointer to
	 * parsed structure
	 */
	ret = find_credentials(_m, _realm, _hftype, _h);
	if (ret < 0) {
		LM_ERR("failed to find credentials\n");
		if (send_resp(_m, (ret == -2) ? 500 : 400, 
			      (ret == -2) ? &auth_500_err : &auth_400_err, 0, 0) == -1) {
			LM_ERR("failed to send 400 reply\n");
		}
		return ERROR;
	} else if (ret > 0) {
		LM_DBG("credentials with given realm not found\n");
		return NO_CREDENTIALS;
	}

	/* Pointer to the parsed credentials */
	c = (auth_body_t*)((*_h)->parsed);

	/* Check credentials correctness here */
	if (check_dig_cred(&(c->digest)) != E_DIG_OK) {
		LM_ERR("received credentials are not filled properly\n");
		if (send_resp(_m, 400, &auth_400_err, 0, 0) == -1) {
			LM_ERR("failed to send 400 reply\n");
		}
		return ERROR;
	}

	if (mark_authorized_cred(_m, *_h) < 0) {
		LM_ERR("failed to mark parsed credentials\n");
		if (send_resp(_m, 500, &auth_400_err, 0, 0) == -1) {
			LM_ERR("failed to send 400 reply\n");
		}
		return ERROR;
	}

	if (check_nonce(&c->digest.nonce, &secret) != 0) {
		LM_DBG("invalid nonce value received\n");
		c->stale = 1;
		return STALE_NONCE;
	}

	return DO_AUTHORIZATION;
}


/*!
 * \brief Do post authentification steps
 *
 * The purpose of this function is to do post authentication steps like
 * marking authorized credentials and so on.
 * \param _m SIP message
 * \param _h header field
 * \return authentification result
 */
auth_result_t post_auth(struct sip_msg* _m, struct hdr_field* _h)
{
	auth_body_t* c;
	int index = 0;

	c = (auth_body_t*)((_h)->parsed);

	if ((_m->REQ_METHOD == METHOD_ACK) ||
		(_m->REQ_METHOD == METHOD_CANCEL))
		return AUTHORIZED;

	if (is_nonce_stale(&c->digest.nonce)) {
		LM_DBG("response is OK, but nonce is stale\n");
		c->stale = 1;
		return STALE_NONCE;
	} else {
		if(nonce_reuse==0)
		{
			/* Verify if it is the first time this nonce is received */
			index= get_nonce_index(&c->digest.nonce);
			if(index== -1)
			{
				LM_ERR("failed to extract nonce index\n");
				return ERROR;
			}
			LM_DBG("nonce index= %d\n", index);

			if(!is_nonce_index_valid(index))
			{
				LM_DBG("nonce index not valid\n");
				return NONCE_REUSED;
			}
		}
	}
	return AUTHORIZED;
}


/*!
 * \brief Calculate the response and compare with given response
 *
 * Calculate the response and compare with the given response string.
 * Authorization is successful if this two strings are same.
 * \param _cred digest credentials
 * \param _method method from the request
 * \param _ha1 HA1 value
 * \return 0 if comparison was ok, 1 when length not match, 2 when comparison not ok
 */
int check_response(dig_cred_t* _cred, str* _method, char* _ha1)
{
	HASHHEX resp, hent;

	/*
	 * First, we have to verify that the response received has
	 * the same length as responses created by us
	 */
	if (_cred->response.len != 32) {
		LM_DBG("receive response len != 32\n");
		return 1;
	}

	/*
	 * Now, calculate our response from parameters received
	 * from the user agent
	 */
	calc_response(_ha1, &(_cred->nonce),
		&(_cred->nc), &(_cred->cnonce),
		&(_cred->qop.qop_str), _cred->qop.qop_parsed == QOP_AUTHINT,
		_method, &(_cred->uri), hent, resp);
	
	LM_DBG("our result = \'%s\'\n", resp);
	
	/*
	 * And simply compare the strings, the user is
	 * authorized if they match
	 */
	if (!memcmp(resp, _cred->response.s, 32)) {
		LM_DBG("authorization is OK\n");
		return 0;
	} else {
		LM_DBG("authorization failed\n");
		return 2;
	}
}


/*!
 * \brief Bind function for the auth API
 * \param api binded API
 * \return 0 on success, -1 on failure
 */
int bind_auth_k(auth_api_k_t* api)
{
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	api->pre_auth = pre_auth;
	api->post_auth = post_auth;
	api->calc_HA1 = calc_HA1;
	api->check_response = check_response;

	return 0;
}
