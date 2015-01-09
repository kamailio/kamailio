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


#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "rfc2617.h"
#include "../../md5.h"
#include "../../dprint.h"


inline void cvt_hex(HASH _b, HASHHEX _h)
{
	unsigned short i;
	unsigned char j;
	
	for (i = 0; i < HASHLEN; i++) {
		j = (_b[i] >> 4) & 0xf;
		if (j <= 9) {
			_h[i * 2] = (j + '0');
		} else {
			_h[i * 2] = (j + 'a' - 10);
		}

		j = _b[i] & 0xf;

		if (j <= 9) {
			_h[i * 2 + 1] = (j + '0');
		} else {
			_h[i * 2 + 1] = (j + 'a' - 10);
		}
	};

	_h[HASHHEXLEN] = '\0';
}


/* 
 * calculate H(A1) as per spec 
 */
void calc_HA1(ha_alg_t _alg, str* _username, str* _realm, str* _password,
	      str* _nonce, str* _cnonce, HASHHEX _sess_key)
{
	MD5_CTX Md5Ctx;
	HASH HA1;
	
	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, _username->s, _username->len);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, _realm->s, _realm->len);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, _password->s, _password->len);
	MD5Final(HA1, &Md5Ctx);

	if (_alg == HA_MD5_SESS) {
		MD5Init(&Md5Ctx);
		MD5Update(&Md5Ctx, HA1, HASHLEN);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, _nonce->s, _nonce->len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, _cnonce->s, _cnonce->len);
		MD5Final(HA1, &Md5Ctx);
	};

	cvt_hex(HA1, _sess_key);
}


/* 
 * calculate request-digest/response-digest as per HTTP Digest spec 
 */
void calc_response(HASHHEX _ha1,      /* H(A1) */
		   str* _nonce,       /* nonce from server */
		   str* _nc,          /* 8 hex digits */
		   str* _cnonce,      /* client nonce */
		   str* _qop,         /* qop-value: "", "auth", "auth-int" */
		   int _auth_int,     /* 1 if auth-int is used */
		   str* _method,      /* method from the request */
		   str* _uri,         /* requested URL */
		   HASHHEX _hentity,  /* H(entity body) if qop="auth-int" */
		   HASHHEX _response) /* request-digest or response-digest */
{
	MD5_CTX Md5Ctx;
	HASH HA2;
	HASH RespHash;
	HASHHEX HA2Hex;
	
	     /* calculate H(A2) */
	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, _method->s, _method->len);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, _uri->s, _uri->len);

	if (_auth_int) {
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, _hentity, HASHHEXLEN);
	};

	MD5Final(HA2, &Md5Ctx);
	cvt_hex(HA2, HA2Hex);

	     /* calculate response */
	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, _ha1, HASHHEXLEN);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, _nonce->s, _nonce->len);
	MD5Update(&Md5Ctx, ":", 1);

	if (_qop->len) {
		MD5Update(&Md5Ctx, _nc->s, _nc->len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, _cnonce->s, _cnonce->len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, _qop->s, _qop->len);
		MD5Update(&Md5Ctx, ":", 1);
	};
	MD5Update(&Md5Ctx, HA2Hex, HASHHEXLEN);
	MD5Final(RespHash, &Md5Ctx);
	cvt_hex(RespHash, _response);
}
