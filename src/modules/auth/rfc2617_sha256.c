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

#include "rfc2617_sha256.h"
#include "../../core/crypto/sha256.h"
#include "../../core/dprint.h"


inline void cvt_hex_sha256(HASH_SHA256 _b, HASHHEX_SHA256 _h)
{
	unsigned short i;
	unsigned char j;

	for (i = 0; i < HASHLEN_SHA256; i++) {
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

	_h[HASHHEXLEN_SHA256] = '\0';
}

/* Cast to unsigned values and forward to sr_SHA256_Update */
static inline void SHA256_Update(SHA256_CTX* context, char *data, int len)
{
	sr_SHA256_Update(context, (unsigned char*)data, (unsigned int)len);
}

/*
 * calculate H(A1) as per spec
 */
void calc_HA1_sha256(ha_alg_t _alg, str* _username, str* _realm, str* _password,
		str* _nonce, str* _cnonce, HASHHEX_SHA256 _sess_key)
{
	SHA256_CTX Sha256Ctx;
	HASH_SHA256 HA1;

	sr_SHA256_Init(&Sha256Ctx);
	SHA256_Update(&Sha256Ctx, _username->s, _username->len);
	SHA256_Update(&Sha256Ctx, ":", 1);
	SHA256_Update(&Sha256Ctx, _realm->s, _realm->len);
	SHA256_Update(&Sha256Ctx, ":", 1);
	SHA256_Update(&Sha256Ctx, _password->s, _password->len);
	sr_SHA256_Final(HA1, &Sha256Ctx);

	if (_alg == HA_MD5_SESS) {
		sr_SHA256_Init(&Sha256Ctx);
		sr_SHA256_Update(&Sha256Ctx, HA1, HASHLEN_SHA256);
		SHA256_Update(&Sha256Ctx, ":", 1);
		SHA256_Update(&Sha256Ctx, _nonce->s, _nonce->len);
		SHA256_Update(&Sha256Ctx, ":", 1);
		SHA256_Update(&Sha256Ctx, _cnonce->s, _cnonce->len);
		sr_SHA256_Final(HA1, &Sha256Ctx);
	};

	cvt_hex_sha256(HA1, _sess_key);
}


/*
 * calculate request-digest/response-digest as per HTTP Digest spec
 */
void calc_response_sha256(HASHHEX_SHA256 _ha1,      /* H(A1) */
		str* _nonce,       /* nonce from server */
		str* _nc,          /* 8 hex digits */
		str* _cnonce,      /* client nonce */
		str* _qop,         /* qop-value: "", "auth", "auth-int" */
		int _auth_int,     /* 1 if auth-int is used */
		str* _method,      /* method from the request */
		str* _uri,         /* requested URL */
		HASHHEX_SHA256 _hentity,  /* H(entity body) if qop="auth-int" */
		HASHHEX_SHA256 _response) /* request-digest or response-digest */
{
	SHA256_CTX Sha256Ctx;
	HASH_SHA256 HA2;
	HASH_SHA256 RespHash;
	HASHHEX_SHA256 HA2Hex;

	/* calculate H(A2) */
	sr_SHA256_Init(&Sha256Ctx);
	if (_method) {
		SHA256_Update(&Sha256Ctx, _method->s, _method->len);
	}
	SHA256_Update(&Sha256Ctx, ":", 1);
	SHA256_Update(&Sha256Ctx, _uri->s, _uri->len);

	if (_auth_int) {
		SHA256_Update(&Sha256Ctx, ":", 1);
		SHA256_Update(&Sha256Ctx, _hentity, HASHHEXLEN_SHA256);
	};

	sr_SHA256_Final(HA2, &Sha256Ctx);
	cvt_hex_sha256(HA2, HA2Hex);

	/* calculate response */
	sr_SHA256_Init(&Sha256Ctx);
	SHA256_Update(&Sha256Ctx, _ha1, HASHHEXLEN_SHA256);
	SHA256_Update(&Sha256Ctx, ":", 1);
	SHA256_Update(&Sha256Ctx, _nonce->s, _nonce->len);
	SHA256_Update(&Sha256Ctx, ":", 1);

	if (_qop->len) {
		SHA256_Update(&Sha256Ctx, _nc->s, _nc->len);
		SHA256_Update(&Sha256Ctx, ":", 1);
		SHA256_Update(&Sha256Ctx, _cnonce->s, _cnonce->len);
		SHA256_Update(&Sha256Ctx, ":", 1);
		SHA256_Update(&Sha256Ctx, _qop->s, _qop->len);
		SHA256_Update(&Sha256Ctx, ":", 1);
	};
	SHA256_Update(&Sha256Ctx, HA2Hex, HASHHEXLEN_SHA256);
	sr_SHA256_Final(RespHash, &Sha256Ctx);
	cvt_hex_sha256(RespHash, _response);
}
