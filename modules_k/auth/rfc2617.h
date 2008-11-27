/*
 * $Id: rfc2617.h 2 2005-06-13 16:47:24Z bogdan_iancu $
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
 * \brief Digest response calculation as per RFC2617
 * \ingroup auth
 * - Module: \ref auth
 */

#ifndef RFC2617_H
#define RFC2617_H

#include "../../str.h"


#define HASHLEN 16
typedef char HASH[HASHLEN];


#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];


/*! Type of algorithm used */
typedef enum {
	HA_MD5,      /*!< Plain MD5 */
	HA_MD5_SESS, /*!< MD5-Session */
} ha_alg_t;


/*!
 * \brief Convert to hex form
 * \param _b hash value
 * \param _h hex value
 */
void cvt_hex(HASH _b, HASHHEX _h);


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
void calc_HA1(ha_alg_t _alg, str* _username, str* _realm,
		str* _password, str* _nonce, str* _cnonce,
		HASHHEX _sess_key);


/*!
 * \brief Calculate request-digest/response-digest as per HTTP Digest spec
 * \param _ha1 H(A1)
 * \param _nonce nonce from server
 * \param _nc 8 hex digits
 * \param _qop qop-value: "", "auth", "auth-int
 * \param _auth_int  1 if auth-int is used
 * \param _method method from the request
 * \param _uri requested URL/ URI
 * \param _hentity  H(entity body) if qop="auth-int"
 * \param _response request-digest or response-digest
 */
void calc_response(HASHHEX _ha1, str* _nonce, str* _nc, str* _cnonce,
		str* _qop, int _auth_int, str* _method, str* _uri,
		HASHHEX _hentity, HASHHEX _response);


#endif
