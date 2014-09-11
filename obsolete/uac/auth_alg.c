/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * UAC SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2005-01-31  first version (ramona)
 */


#include "../../md5.h"

#include "auth_alg.h"


static inline void cvt_hex(HASH bin, HASHHEX hex)
{
	unsigned short i;
	unsigned char j;

	for (i = 0; i<HASHLEN; i++)
	{
		j = (bin[i] >> 4) & 0xf;
		if (j <= 9)
		{
			hex[i * 2] = (j + '0');
		} else {
			hex[i * 2] = (j + 'a' - 10);
		}

		j = bin[i] & 0xf;

		if (j <= 9)
		{
			hex[i * 2 + 1] = (j + '0');
		} else {
			hex[i * 2 + 1] = (j + 'a' - 10);
		}
	};

	hex[HASHHEXLEN] = '\0';
}



/* 
 * calculate H(A1)
 */
void uac_calc_HA1( struct uac_credential *crd,
		struct authenticate_body *auth,
		str* cnonce,
		HASHHEX sess_key)
{
	MD5_CTX Md5Ctx;
	HASH HA1;

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, crd->user.s, crd->user.len);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, crd->realm.s, crd->realm.len);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, crd->passwd.s, crd->passwd.len);
	MD5Final(HA1, &Md5Ctx);

	if ( auth->flags& AUTHENTICATE_MD5SESS )
	{
		MD5Init(&Md5Ctx);
		MD5Update(&Md5Ctx, HA1, HASHLEN);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, auth->nonce.s, auth->nonce.len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, cnonce->s, cnonce->len);
		MD5Final(HA1, &Md5Ctx);
	};

	cvt_hex(HA1, sess_key);
}



/* 
 * calculate H(A2)
 */
void uac_calc_HA2( str *method, str *uri,
		struct authenticate_body *auth,
		HASHHEX hentity,
		HASHHEX HA2Hex )
{
	MD5_CTX Md5Ctx;
	HASH HA2;

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, method->s, method->len);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, uri->s, uri->len);

	if ( auth->flags&QOP_AUTH_INT)
	{
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, hentity, HASHHEXLEN);
	};

	MD5Final(HA2, &Md5Ctx);
	cvt_hex(HA2, HA2Hex);
}



/* 
 * calculate request-digest/response-digest as per HTTP Digest spec 
 */
void uac_calc_response( HASHHEX ha1, HASHHEX ha2,
		struct authenticate_body *auth,
		str* nc, str* cnonce,
		HASHHEX response)
{
	MD5_CTX Md5Ctx;
	HASH RespHash;

	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, ha1, HASHHEXLEN);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, auth->nonce.s, auth->nonce.len);
	MD5Update(&Md5Ctx, ":", 1);

	if ( auth->qop.len)
	{
		MD5Update(&Md5Ctx, nc->s, nc->len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, cnonce->s, cnonce->len);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, auth->qop.s, auth->qop.len);
		MD5Update(&Md5Ctx, ":", 1);
	};
	MD5Update(&Md5Ctx, ha2, HASHHEXLEN);
	MD5Final(RespHash, &Md5Ctx);
	cvt_hex(RespHash, response);
}
