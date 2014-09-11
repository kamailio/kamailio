/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

void calc_H(str *ent, HASHHEX hash)
{
	MD5_CTX Md5Ctx;
	HASH HA1;
	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, ent->s, ent->len);
	MD5Final(HA1, &Md5Ctx);
	cvt_hex(HA1, hash);
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
	LM_DBG("calc_response(_ha1=%.*s, _nonce=%.*s, _nc=%.*s,_cnonce=%.*s, _qop=%.*s, _auth_int=%d,_method=%.*s,_uri=%.*s,_hentity=%.*s)\n",
			HASHHEXLEN, _ha1,
			_nonce->len, _nonce->s,
			_nc->len, _nc->s,
			_cnonce->len, _cnonce->s,
			_qop->len, _qop->s,
			_auth_int,
			_method ? _method->len : 4, _method ? _method->s : "null",
			_uri->len, _uri->s,
			_auth_int ? HASHHEXLEN : 0, _hentity);

	MD5_CTX Md5Ctx;
	HASH HA2;
	HASH RespHash;
	HASHHEX HA2Hex;
	
	     /* calculate H(A2) */
	MD5Init(&Md5Ctx);
	if (_method) { /* _method is NULL when calculating H(A2) for rspauth in Authentication-Info */
		MD5Update(&Md5Ctx, _method->s, _method->len);
	}
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
	LM_DBG("H(A1) = %.*s, H(A2) = %.*s, rspauth = %.*s\n",
			HASHHEXLEN, _ha1, HASHHEXLEN, HA2Hex, HASHHEXLEN, _response);
}
