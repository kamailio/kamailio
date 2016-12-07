/*
 * Digest Authentication Module
 * Digest response calculation as per RFC2617
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


#ifndef RFC2617_SHA256_H
#define RFC2617_SHA256_H

#include "../../core/str.h"
#include "rfc2617.h"


#define HASHLEN_SHA256 32
typedef unsigned char HASH_SHA256[HASHLEN_SHA256];


#define HASHHEXLEN_SHA256 64
typedef char HASHHEX_SHA256[HASHHEXLEN_SHA256+1];


/*
 * Convert to hex form
 */
void cvt_hex_sha256(HASH_SHA256 Bin, HASHHEX_SHA256 Hex);


/*
 * calculate H(A1) as per HTTP Digest spec
 */
void calc_HA1_sha256(ha_alg_t _alg,      /* Type of algorithm */
		str* _username,     /* username */
		str* _realm,        /* realm */
		str* _password,     /* password */
		str* _nonce,        /* nonce string */
		str* _cnonce,       /* cnonce */
		HASHHEX_SHA256 _sess_key); /* Result will be stored here */


/* calculate request-digest/response-digest as per HTTP Digest spec */
void calc_response_sha256(HASHHEX_SHA256 _ha1,       /* H(A1) */
		str* _nonce,        /* nonce from server */
		str* _nc,           /* 8 hex digits */
		str* _cnonce,       /* client nonce */
		str* _qop,          /* qop-value: "", "auth", "auth-int" */
		int _auth_int,      /* 1 if auth-int is used */
		str* _method,       /* method from the request */
		str* _uri,          /* requested URL */
		HASHHEX_SHA256 _hentity,   /* H(entity body) if qop="auth-int" */
		HASHHEX_SHA256 _response); /* request-digest or response-digest */

#endif /* RFC2617_SHA256_H */
