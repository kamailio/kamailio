/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file
 * \brief Kamailio uac :: Authentication
 * \ingroup uac
 * Module: \ref uac
 */


#include "../../core/crypto/md5.h"
#include "../../core/crypto/sha256.h"
#include "../../core/dprint.h"

#include "auth_alg.h"


static inline void cvt_hex(HASH bin, HASHHEX hex)
{
	unsigned short i;
	unsigned char j;

	for(i = 0; i < HASHLEN; i++) {
		j = (bin[i] >> 4) & 0xf;
		if(j <= 9) {
			hex[i * 2] = (j + '0');
		} else {
			hex[i * 2] = (j + 'a' - 10);
		}

		j = bin[i] & 0xf;

		if(j <= 9) {
			hex[i * 2 + 1] = (j + '0');
		} else {
			hex[i * 2 + 1] = (j + 'a' - 10);
		}
	};

	hex[HASHHEXLEN] = '\0';
}


static inline void cvt_hex_sha256(unsigned char bin[HASHLEN_SHA256], char *hex)
{
	unsigned short i;
	unsigned char j;

	for(i = 0; i < HASHLEN_SHA256; i++) {
		j = (bin[i] >> 4) & 0xf;
		hex[i * 2] = (j <= 9) ? (j + '0') : (j + 'a' - 10);

		j = bin[i] & 0xf;
		hex[i * 2 + 1] = (j <= 9) ? (j + '0') : (j + 'a' - 10);
	}

	hex[HASHHEXLEN_SHA256] = '\0';
}


static inline void cvt_bin(HASHHEX hex, HASH bin)
{
	unsigned short i;
	unsigned char j;
	for(i = 0; i < HASHLEN; i++) {
		if(hex[2 * i] >= '0' && hex[2 * i] <= '9')
			j = (hex[2 * i] - '0') << 4;
		else if(hex[2 * i] >= 'a' && hex[2 * i] <= 'f')
			j = (hex[2 * i] - 'a' + 10) << 4;
		else if(hex[2 * i] >= 'A' && hex[2 * i] <= 'F')
			j = (hex[2 * i] - 'A' + 10) << 4;
		else
			j = 0;

		if(hex[2 * i + 1] >= '0' && hex[2 * i + 1] <= '9')
			j += hex[2 * i + 1] - '0';
		else if(hex[2 * i + 1] >= 'a' && hex[2 * i + 1] <= 'f')
			j += hex[2 * i + 1] - 'a' + 10;
		else if(hex[2 * i + 1] >= 'A' && hex[2 * i + 1] <= 'F')
			j += hex[2 * i + 1] - 'A' + 10;

		bin[i] = j;
	}
}


static inline void cvt_bin_sha256(char *hex, unsigned char bin[HASHLEN_SHA256])
{
	unsigned short i;
	unsigned char j;

	for(i = 0; i < HASHLEN_SHA256; i++) {
		if(hex[2 * i] >= '0' && hex[2 * i] <= '9')
			j = (hex[2 * i] - '0') << 4;
		else if(hex[2 * i] >= 'a' && hex[2 * i] <= 'f')
			j = (hex[2 * i] - 'a' + 10) << 4;
		else if(hex[2 * i] >= 'A' && hex[2 * i] <= 'F')
			j = (hex[2 * i] - 'A' + 10) << 4;
		else
			j = 0;

		if(hex[2 * i + 1] >= '0' && hex[2 * i + 1] <= '9')
			j += hex[2 * i + 1] - '0';
		else if(hex[2 * i + 1] >= 'a' && hex[2 * i + 1] <= 'f')
			j += hex[2 * i + 1] - 'a' + 10;
		else if(hex[2 * i + 1] >= 'A' && hex[2 * i + 1] <= 'F')
			j += hex[2 * i + 1] - 'A' + 10;

		bin[i] = j;
	}
}

/*
 * calculate H(A1)
 */
void uac_calc_HA1(struct uac_credential *crd, struct authenticate_body *auth,
		str *cnonce, HASHHEX sess_key)
{
	MD5_CTX Md5Ctx;
	HASH HA1;
	SHA256_CTX Sha256Ctx;
	unsigned char HA1Sha256[HASHLEN_SHA256];

	if(auth->flags & AUTHENTICATE_SHA256) {
		if(crd->aflags & UAC_FLCRED_HA1) {
			if(crd->passwd.len < HASHHEXLEN_SHA256) {
				LM_ERR("invalid SHA-256 HA1 length %d\n", crd->passwd.len);
				sess_key[0] = '\0';
				return;
			}
			memcpy(sess_key, crd->passwd.s, HASHHEXLEN_SHA256);
			sess_key[HASHHEXLEN_SHA256] = '\0';
			cvt_bin_sha256(sess_key, HA1Sha256);
		} else {
			sr_SHA256_Init(&Sha256Ctx);
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)crd->user.s, crd->user.len);
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
			sr_SHA256_Update(
					&Sha256Ctx, (uint8_t *)crd->realm.s, crd->realm.len);
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
			sr_SHA256_Update(
					&Sha256Ctx, (uint8_t *)crd->passwd.s, crd->passwd.len);
			sr_SHA256_Final(HA1Sha256, &Sha256Ctx);
		}

		cvt_hex_sha256(HA1Sha256, sess_key);
		return;
	}

	if(crd->aflags & UAC_FLCRED_HA1) {
		if(crd->passwd.len < HASHHEXLEN) {
			LM_ERR("invalid MD5 HA1 length %d\n", crd->passwd.len);
			sess_key[0] = '\0';
			return;
		}
		memcpy(sess_key, crd->passwd.s, HASHHEXLEN);
		sess_key[HASHHEXLEN] = '\0';
		if(auth->flags & AUTHENTICATE_MD5SESS) {
			cvt_bin(sess_key, HA1);
		} else {
			return;
		}
	} else {
		MD5Init(&Md5Ctx);
		MD5Update(&Md5Ctx, crd->user.s, crd->user.len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, crd->realm.s, crd->realm.len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, crd->passwd.s, crd->passwd.len);
		MD5Final(HA1, &Md5Ctx);
	}

	if(auth->flags & AUTHENTICATE_MD5SESS) {
		MD5Init(&Md5Ctx);
		MD5Update(&Md5Ctx, HA1, HASHLEN);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, auth->nonce.s, auth->nonce.len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, cnonce->s, cnonce->len);
		MD5Final(HA1, &Md5Ctx);
	}

	cvt_hex(HA1, sess_key);
}


/*
 * calculate H(A2)
 */
void uac_calc_HA2(str *method, str *uri, struct authenticate_body *auth,
		HASHHEX hentity, HASHHEX HA2Hex)
{
	MD5_CTX Md5Ctx;
	HASH HA2;
	SHA256_CTX Sha256Ctx;
	unsigned char HA2Sha256[HASHLEN_SHA256];

	if(auth->flags & AUTHENTICATE_SHA256) {
		sr_SHA256_Init(&Sha256Ctx);
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)method->s, method->len);
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)uri->s, uri->len);

		if(auth->flags & QOP_AUTH_INT) {
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)hentity, HASHHEXLEN_SHA256);
		}

		sr_SHA256_Final(HA2Sha256, &Sha256Ctx);
		cvt_hex_sha256(HA2Sha256, HA2Hex);
		return;
	}

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, method->s, method->len);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, uri->s, uri->len);

	if(auth->flags & QOP_AUTH_INT) {
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, hentity, HASHHEXLEN);
	};

	MD5Final(HA2, &Md5Ctx);
	cvt_hex(HA2, HA2Hex);
}


/*
 * calculate request-digest/response-digest as per HTTP Digest spec
 */
void uac_calc_response(HASHHEX ha1, HASHHEX ha2, struct authenticate_body *auth,
		str *nc, str *cnonce, HASHHEX response)
{
	MD5_CTX Md5Ctx;
	HASH RespHash;
	SHA256_CTX Sha256Ctx;
	unsigned char RespHashSha256[HASHLEN_SHA256];
	char *p;

	if(auth->flags & AUTHENTICATE_SHA256) {
		sr_SHA256_Init(&Sha256Ctx);
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)ha1, HASHHEXLEN_SHA256);
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)auth->nonce.s, auth->nonce.len);
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);

		if(auth->qop.len) {
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)nc->s, nc->len);
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)cnonce->s, cnonce->len);
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
			p = memchr(auth->qop.s, ',', auth->qop.len);
			if(p) {
				sr_SHA256_Update(&Sha256Ctx, (uint8_t *)auth->qop.s,
						(size_t)(p - auth->qop.s));
			} else {
				sr_SHA256_Update(
						&Sha256Ctx, (uint8_t *)auth->qop.s, auth->qop.len);
			}
			sr_SHA256_Update(&Sha256Ctx, (uint8_t *)":", 1);
		}
		sr_SHA256_Update(&Sha256Ctx, (uint8_t *)ha2, HASHHEXLEN_SHA256);
		sr_SHA256_Final(RespHashSha256, &Sha256Ctx);
		cvt_hex_sha256(RespHashSha256, response);
		return;
	}

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, ha1, HASHHEXLEN);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, auth->nonce.s, auth->nonce.len);
	MD5Update(&Md5Ctx, ":", 1);

	if(auth->qop.len) {
		MD5Update(&Md5Ctx, nc->s, nc->len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, cnonce->s, cnonce->len);
		MD5Update(&Md5Ctx, ":", 1);
		p = memchr(auth->qop.s, ',', auth->qop.len);
		if(p) {
			MD5Update(&Md5Ctx, auth->qop.s, (size_t)(p - auth->qop.s));
		} else {
			MD5Update(&Md5Ctx, auth->qop.s, auth->qop.len);
		}
		MD5Update(&Md5Ctx, ":", 1);
	};
	MD5Update(&Md5Ctx, ha2, HASHHEXLEN);
	MD5Final(RespHash, &Md5Ctx);
	cvt_hex(RespHash, response);
}
