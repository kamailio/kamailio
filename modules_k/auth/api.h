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

#ifndef AUTH_API_H
#define AUTH_API_H


#include "../../parser/digest/digest.h"
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../str.h"
#include "../../usr_avp.h"
#include "rfc2617.h"


typedef enum auth_result {
	NONCE_REUSED = -6,  /*!< Returned if nonce is used more than once */
	AUTH_ERROR,         /*!< Error occurred, a reply has not been sent out */
	NO_CREDENTIALS,     /*!< Credentials missing */
	STALE_NONCE,        /*!< Stale nonce */
	INVALID_PASSWORD,   /*!< Invalid password */
	USER_UNKNOWN,       /*!< User non existant */
	ERROR,              /*!< Error occurred, a reply has been sent out,
	                        return 0 to the openser core */
	AUTHORIZED,         /*!< Authorized. If returned by pre_auth,
	                         no digest authorization necessary */
	DO_AUTHORIZATION,   /*!< Can only be returned by pre_auth. */
	                    /*!< Means to continue doing authorization */
} auth_result_t;


/*!
 * \brief Find credentials with given realm, check if we need to authenticate
 *
 * The purpose of this function is to find credentials with given realm,
 * do sanity check, validate credential correctness and determine if
 * we should really authenticate (there must be no authentication for
 * ACK and CANCEL
 * \param _m SIP message
 * \param _realm authentification realm
 * \param _hftype header field type
 * \param _h header field
 * \return authentification result
 */
typedef auth_result_t (*pre_auth_t)(struct sip_msg* _m, str* _realm,
		hdr_types_t _hftype, struct hdr_field** _h);


/*!
 * \brief Find credentials with given realm, check if we need to authenticate
 *
 * The purpose of this function is to find credentials with given realm,
 * do sanity check, validate credential correctness and determine if
 * we should really authenticate (there must be no authentication for
 * ACK and CANCEL
 * \param _m SIP message
 * \param _realm authentification realm
 * \param _hftype header field type
 * \param _h header field
 * \return authentification result
 */
auth_result_t pre_auth(struct sip_msg* _m, str* _realm,
		hdr_types_t _hftype, struct hdr_field** _h);


/*!
 * \brief Do post authentification steps
 *
 * The purpose of this function is to do post authentication steps like
 * marking authorized credentials and so on.
 * \param _m SIP message
 * \param _h header field
 * \return authentification result
 */
typedef auth_result_t (*post_auth_t)(struct sip_msg* _m, struct hdr_field* _h);


/*!
 * \brief Do post authentification steps
 *
 * The purpose of this function is to do post authentication steps like
 * marking authorized credentials and so on.
 * \param _m SIP message
 * \param _h header field
 * \return authentification result
 */
auth_result_t post_auth(struct sip_msg* _m, struct hdr_field* _h);


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
typedef int (*check_response_t)(dig_cred_t* _cred, str* _method, char* _ha1);


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
int check_response(dig_cred_t* _cred, str* _method, char* _ha1);


/*!
 * \brief Calculate H(A1) as per HTTP Digest spec
 * \param _alg type of hash algorithm
 * \param _username username
 * \param _realm authentification realm
 * \param _password password
 * \param _nonce nonce value
 * \param _cnonce cnonce value
 * \param _sess_key session key, result will be stored there
 */
typedef void (*calc_HA1_t)(ha_alg_t _alg, str* _username, str* _realm,
		str* _password, str* _nonce, str* _cnonce, HASHHEX _sess_key);


/*!
 * \brief Strip the beginning of a realm string
 *
 * Strip the beginning of a realm string, depending on the length of
 * the realm_prefix.
 * \param _realm realm string
 */
void strip_realm(str *_realm);


/*! Auth module API */
typedef struct auth_api_k {
	pre_auth_t  pre_auth;  /*!< The function to be called before auth */
	post_auth_t post_auth; /*!< The function to be called after auth */
	calc_HA1_t  calc_HA1;  /*!< calculate H(A1) as per spec */
	check_response_t check_response; /*!< check auth response */
} auth_api_k_t;


typedef int (*bind_auth_k_t)(auth_api_k_t* api);


/*!
 * \brief Bind function for the auth API
 * \param api binded API
 * \return 0 on success, -1 on failure
 */
int bind_auth_k(auth_api_k_t* api);


#endif
