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

#ifndef API_H
#define API_H


#include "../../parser/msg_parser.h"
#include "../../parser/digest/digest.h"
#include "../../sr_module.h"
#include "../../usr_avp.h"
#include "../../parser/hf.h"
#include "../../str.h"
#include "challenge.h"
#include "rfc2617.h"

/**
 * return codes to config by auth functions
 */
typedef enum auth_cfg_result {
	AUTH_USER_MISMATCH = -8,    /*!< Auth user != From/To user */
	AUTH_NONCE_REUSED = -6,     /*!< Returned if nonce is used more than once */
	AUTH_NO_CREDENTIALS = -5,   /*!< Credentials missing */
	AUTH_STALE_NONCE = -4,      /*!< Stale nonce */
	AUTH_USER_UNKNOWN = -3,     /*!< User not found */
	AUTH_INVALID_PASSWORD = -2, /*!< Invalid password */
	AUTH_ERROR = -1,            /*!< Error occurred */
	AUTH_DROP = 0,              /*!< Error, stop config execution */
	AUTH_OK = 1                 /*!< Success */
} auth_cfg_result_t;


/**
 * flags for checks in auth functions
 */
#define AUTH_CHECK_ID_F 1<<0
#define AUTH_CHECK_SKIPFWD_F 1<<1

/**
 * return codes to auth API functions
 */
typedef enum auth_result {
	NONCE_REUSED = -5,  /* Returned if nonce is used more than once */
	NO_CREDENTIALS,     /* Credentials missing */
	STALE_NONCE,        /* Stale nonce */
	ERROR,              /* Error occurred, a reply has been sent out -> return 0 to the ser core */
	NOT_AUTHENTICATED,  /* Don't perform authentication, credentials missing */
	DO_AUTHENTICATION,  /* Perform digest authentication */
	AUTHENTICATED,      /* Authenticated by default, no digest authentication necessary */
	BAD_CREDENTIALS,    /* Digest credentials are malformed */
	CREATE_CHALLENGE,   /* when AKAv1-MD5 is used first request does not contain credentials,
	                     * only usename, realm and algorithm. Server should get Authentication
	                     * Vector from AuC/HSS, create challenge and send it to the UE. */
	DO_RESYNCHRONIZATION   /* When AUTS is received we need do resynchronization
	                        * of sequnce numbers with mobile station. */
} auth_result_t;


typedef int (*check_auth_hdr_t)(struct sip_msg* msg, auth_body_t* auth_body,
		auth_result_t* auth_res);
int check_auth_hdr(struct sip_msg* msg, auth_body_t* auth_body,
		auth_result_t* auth_res);

/*
 * Purpose of this function is to find credentials with given realm,
 * do sanity check, validate credential correctness and determine if
 * we should really authenticate (there must be no authentication for
 * ACK and CANCEL
 */
typedef auth_result_t (*pre_auth_t)(struct sip_msg* msg, str* realm,
				    hdr_types_t hftype, struct hdr_field** hdr,
					check_auth_hdr_t check_auth_hdr);
auth_result_t pre_auth(struct sip_msg* msg, str* realm, hdr_types_t hftype,
		       struct hdr_field** hdr, check_auth_hdr_t check_auth_hdr);


/*
 * Purpose of this function is to do post authentication steps like
 * marking authorized credentials and so on.
 */
typedef auth_result_t (*post_auth_t)(struct sip_msg* msg,
		struct hdr_field* hdr);
auth_result_t post_auth(struct sip_msg* msg, struct hdr_field* hdr);

typedef int (*check_response_t)(dig_cred_t* cred, str* method, char* ha1);
int auth_check_response(dig_cred_t* cred, str* method, char* ha1);

typedef int (*auth_challenge_f)(struct sip_msg *msg, str *realm, int flags,
		int hftype);
int auth_challenge(struct sip_msg *msg, str *realm, int flags,
		int hftype);

typedef int (*pv_authenticate_f)(struct sip_msg *msg, str *realm, str *passwd,
		int flags, int hftype, str *method);
int pv_authenticate(struct sip_msg *msg, str *realm, str *passwd,
		int flags, int hftype, str *method);

typedef int (*consume_credentials_f)(struct sip_msg* msg);
int consume_credentials(struct sip_msg* msg);

/*
 * Auth module API
 */
typedef struct auth_api_s {
    pre_auth_t pre_auth;                  /* The function to be called before authentication */
    post_auth_t post_auth;                /* The function to be called after authentication */
    build_challenge_hf_t build_challenge; /* Function to build digest challenge header */
    struct qp* qop;                       /* qop module parameter */
	calc_HA1_t         calc_HA1;
	calc_response_t    calc_response;
	check_response_t   check_response;
	auth_challenge_f   auth_challenge;
	pv_authenticate_f  pv_authenticate;
	consume_credentials_f consume_credentials;
} auth_api_s_t;

typedef int (*bind_auth_s_t)(auth_api_s_t* api);
int bind_auth_s(auth_api_s_t* api);

/**
 * load AUTH module API
 */
static inline int auth_load_api(auth_api_s_t* api)
{
	bind_auth_s_t bind_auth;

	/* bind to auth module and import the API */
	bind_auth = (bind_auth_s_t)find_export("bind_auth_s", 0, 0);
	if (!bind_auth) {
		LM_ERR("unable to find bind_auth function. Check if you load"
				" the auth module.\n");
		return -1;
	}

	if (bind_auth(api) < 0) {
		LM_ERR("unable to bind auth module\n");
		return -1;
	}
	return 0;
}

#endif /* API_H */
