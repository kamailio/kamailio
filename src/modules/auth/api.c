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
#include "../../core/dprint.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/sr_module.h"
#include "../../core/ut.h"
#include "../../core/data_lump_rpl.h"
#include "auth_mod.h"
#include "nonce.h"
#include "ot_nonce.h"
#include "rfc2617_sha256.h"
#include "challenge.h"

static int auth_check_hdr_md5_default(struct sip_msg* msg, auth_body_t* auth_body,
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
		LM_ERR("Error while looking for credentials\n");
		return ERROR;
	} else if (ret > 0) {
		LM_DBG("Credentials with realm '%.*s' not found\n",
				realm->len, ZSW(realm->s));
		return NO_CREDENTIALS;
	}

	/* Pointer to the parsed credentials */
	c = (auth_body_t*)((*hdr)->parsed);

	/* digest headers are in c->digest */
	LM_DBG("digest-algo: %.*s parsed value: %d\n",
			c->digest.alg.alg_str.len, c->digest.alg.alg_str.s,
			c->digest.alg.alg_parsed);

	if (mark_authorized_cred(msg, *hdr) < 0) {
		LM_ERR("Error while marking parsed credentials\n");
		return ERROR;
	}

	/* check authorization header field's validity */
	if (check_auth_hdr == NULL) {
		check_hf = auth_check_hdr_md5_default;
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
int auth_check_hdr_md5(struct sip_msg* msg, auth_body_t* auth,
		auth_result_t* auth_res, int update_nonce)
{
	int ret;

	/* Check credentials correctness here */
	if (check_dig_cred(&auth->digest) != E_DIG_OK) {
		LM_ERR("Credentials are not filled properly\n");
		*auth_res = BAD_CREDENTIALS;
		return 0;
	}

	ret = check_nonce(auth, &secret1, &secret2, msg, update_nonce);
	if (ret!=0){
		if (ret==3 || ret==4){
			/* failed auth_extra_checks or stale */
			auth->stale=1; /* we mark the nonce as stale
							* (hack that makes our life much easier) */
			*auth_res = STALE_NONCE;
			return 0;
		} else if (ret==6) {
			*auth_res = NONCE_REUSED;
			return 0;
		} else {
			LM_DBG("Invalid nonce value received (ret %d)\n", ret);
			*auth_res = NOT_AUTHENTICATED;
			return 0;
		}
	}
	return 1;
}

static int auth_check_hdr_md5_default(struct sip_msg* msg, auth_body_t* auth,
		auth_result_t* auth_res)
{
	return auth_check_hdr_md5(msg, auth, auth_res, 1);
}

/**
 * Adds the Authentication-Info header, based on the credentials sent by a successful REGISTER.
 * @param msg - SIP message to add the header to
 * @returns 1 on success, 0 on error
 */
static int add_authinfo_resp_hdr(struct sip_msg *msg, char* next_nonce, int nonce_len, str qop, char* rspauth, str cnonce, str nc)
{
	str authinfo_hdr;
	static const char authinfo_fmt[] = "Authentication-Info: "
		"nextnonce=\"%.*s\", "
		"qop=%.*s, "
		"rspauth=\"%.*s\", "
		"cnonce=\"%.*s\", "
		"nc=%.*s\r\n";

	authinfo_hdr.len = sizeof (authinfo_fmt) + nonce_len + qop.len + hash_hex_len + cnonce.len + nc.len - 20 /* format string parameters */ - 1 /* trailing \0 */;
	authinfo_hdr.s = pkg_malloc(authinfo_hdr.len + 1);

	if (!authinfo_hdr.s) {
		LM_ERR("Error allocating %d bytes\n", authinfo_hdr.len);
		goto error;
	}
	snprintf(authinfo_hdr.s, authinfo_hdr.len + 1, authinfo_fmt,
			nonce_len, next_nonce,
			qop.len, qop.s,
			hash_hex_len, rspauth,
			cnonce.len, cnonce.s,
			nc.len, nc.s);
	LM_DBG("authinfo hdr built: %.*s", authinfo_hdr.len, authinfo_hdr.s);
	if (add_lump_rpl(msg, authinfo_hdr.s, authinfo_hdr.len, LUMP_RPL_HDR)!=0) {
		LM_DBG("authinfo hdr added");
		pkg_free(authinfo_hdr.s);
		return 1;
	}
error:
	if (authinfo_hdr.s) pkg_free(authinfo_hdr.s);
	return 0;
}

/*
 * Purpose of this function is to do post authentication steps like
 * marking authorized credentials and so on.
 */
auth_result_t post_auth(struct sip_msg* msg, struct hdr_field* hdr, char* ha1)
{
	int res = AUTHENTICATED;
	auth_body_t* c;
	dig_cred_t* d;
	HASHHEX_SHA256 rspauth;
#ifdef USE_OT_NONCE
	char next_nonce[MAX_NONCE_LEN];
	int nonce_len;
	int cfg;
#endif

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
	else if (add_authinfo_hdr) {
		if (unlikely(!ha1)) {
			LM_ERR("add_authinfo_hdr is configured but the auth_* module "
				"you are using does not provide the ha1 value to post_auth\n");
		}
		else {
			d = &c->digest;

			/* calculate rspauth */
			calc_response(ha1,
					&d->nonce,
					&d->nc,
					&d->cnonce,
					&d->qop.qop_str,
					d->qop.qop_parsed == QOP_AUTHINT,
					0, /* method is empty for rspauth */
					&d->uri,
					NULL, /* TODO should be H(entity-body) if auth-int should be supported */
					rspauth);

			/* calculate new next nonce if otn is enabled */
#ifdef USE_OT_NONCE
			if (otn_enabled) {
				cfg = get_auth_checks(msg);
				nonce_len = sizeof(next_nonce);
				if (unlikely(calc_new_nonce(next_nonce, &nonce_len,
								cfg, msg) != 0)) {
					LM_ERR("calc nonce failed (len %d, needed %d)."
							" authinfo hdr is not added.\n",
							(int) sizeof(next_nonce), nonce_len);
				}
				else {
					add_authinfo_resp_hdr(msg, next_nonce, nonce_len,
							d->qop.qop_str, rspauth, d->cnonce, d->nc);
				}
			}
			else
#endif
				/* use current nonce as next nonce */
				add_authinfo_resp_hdr(msg, d->nonce.s, d->nonce.len,
						d->qop.qop_str, rspauth, d->cnonce, d->nc);
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
	HASHHEX_SHA256 resp, hent;

	/*
	 * First, we have to verify that the response received has
	 * the same length as responses created by us
	 */
	if (cred->response.len != hash_hex_len) {
		LM_DBG("Receive response len != %d\n", hash_hex_len);
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

	LM_DBG("Our result = \'%s\'\n", resp);

	/*
	 * And simply compare the strings, the user is
	 * authorized if they match
	 */
	if (!memcmp(resp, cred->response.s, hash_hex_len)) {
		LM_DBG("Authorization is OK\n");
		return AUTHENTICATED;
	} else {
		LM_DBG("Authorization failed\n");
		return NOT_AUTHENTICATED;
	}
}


int bind_auth_s(auth_api_s_t* api)
{
	if (!api) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	api->pre_auth = pre_auth;
	api->post_auth = post_auth;
	api->build_challenge = build_challenge_hf;
	api->qop = &auth_qop;
	api->calc_HA1 = calc_HA1;
	api->calc_response = calc_response;
	api->check_response = auth_check_response;
	api->auth_challenge_hftype = auth_challenge_hftype;
	api->pv_authenticate = pv_authenticate;
	api->consume_credentials = consume_credentials;
	return 0;
}
