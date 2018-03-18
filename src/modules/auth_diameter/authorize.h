/*
 * Digest Authentication - Diameter support
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

#ifndef DIAMETER_AUTHORIZE_H
#define DIAMETER_AUTHORIZE_H

#include "diameter_msg.h"
#include "../../core/parser/digest/digest_parser.h"
#include "../../core/parser/hf.h"
#include "../../core/pvar.h"
#include "../../core/str.h"
#include "defs.h"

typedef enum auth_diam_result {
	NONCE_REUSED = -6,  /*!< Returned if nonce is used more than once */
	AUTH_ERROR,         /*!< Error occurred, a reply has not been sent out */
	NO_CREDENTIALS,     /*!< Credentials missing */
	STALE_NONCE,        /*!< Stale nonce */
	INVALID_PASSWORD,   /*!< Invalid password */
	USER_UNKNOWN,       /*!< User non existant */
	ERROR,              /*!< Error occurred, a reply has been sent out,
	                        return 0 to the kamailio core */
	AUTHORIZED,         /*!< Authorized. If returned by pre_auth,
	                         no digest authorization necessary */
	DO_AUTHORIZATION,   /*!< Can only be returned by pre_auth. */
	                    /*!< Means to continue doing authorization */
} auth_diam_result_t;



int get_uri(struct sip_msg* m, str** uri);

int get_realm(struct sip_msg* m, int hftype, struct sip_uri* u);

auth_diam_result_t diam_pre_auth(struct sip_msg* m, str* realm, int hftype, 
						struct hdr_field** h);

int authorize(struct sip_msg* msg, pv_elem_t* realm, int hftype);

int diameter_authorize(struct hdr_field* cred, str* p_method, 
					sip_uri_t *uri,	sip_uri_t *ruri,
					unsigned int m_id, rd_buf_t *response);

int srv_response(struct sip_msg* msg, rd_buf_t* rb, int hftype);

int send_resp(struct sip_msg* _m, int _code, str* _reason,
					char* _hdr, int _hdr_len);

#endif /* DIAMETER_AUTHORIZE_H */
 
