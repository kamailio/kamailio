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

#ifndef API_H
#define API_H


#include "../../parser/msg_parser.h"
#include "../../parser/digest/digest.h"
#include "../../usr_avp.h"
#include "../../parser/hf.h"
#include "../../str.h"
#include "challenge.h"
#include "rfc2617.h"


typedef enum auth_result {
	ERROR = -2 ,        /* Error occurred, a reply has been sent out -> return 0 to the ser core */
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

/*
 * Auth module API
 */
typedef struct auth_api_s {
    pre_auth_t pre_auth;                  /* The function to be called before authentication */
    post_auth_t post_auth;                /* The function to be called after authentication */
    build_challenge_hf_t build_challenge; /* Function to build digest challenge header */
    struct qp* qop;                       /* qop module parameter */
	calc_HA1_t      calc_HA1;
	calc_response_t calc_response;
	check_response_t check_response;
} auth_api_s_t;

typedef int (*bind_auth_s_t)(auth_api_s_t* api);
int bind_auth_s(auth_api_s_t* api);


#endif /* API_H */
