/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "../../md5global.h"
#include "../../md5.h"


void CvtHex(HASH Bin, HASHHEX Hex)
{
	unsigned short i;
	unsigned char j;
    
	for (i = 0; i < HASHLEN; i++) {
		j = (Bin[i] >> 4) & 0xf;
		if (j <= 9)
			Hex[i*2] = (j + '0');
		else
			Hex[i*2] = (j + 'a' - 10);
		j = Bin[i] & 0xf;
		if (j <= 9)
			Hex[i*2+1] = (j + '0');
		else
			Hex[i*2+1] = (j + 'a' - 10);
	};
	Hex[HASHHEXLEN] = '\0';
}


/* 
 * calculate H(A1) as per spec 
 */
void DigestCalcHA1(const char * pszAlg, const char * pszUserName,
		   const char * pszRealm, const char * pszPassword,
		   const char * pszNonce, const char * pszCNonce,
		   HASHHEX SessionKey)
{
	MD5_CTX Md5Ctx;
	HASH HA1;
	
	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, pszUserName, strlen(pszUserName));
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, pszRealm, strlen(pszRealm));
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, pszPassword, strlen(pszPassword));
	MD5Final(HA1, &Md5Ctx);
	if (strcmp(pszAlg, "md5-sess") == 0) {
		MD5Init(&Md5Ctx);
		MD5Update(&Md5Ctx, HA1, HASHLEN);
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, pszNonce, strlen(pszNonce));
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, pszCNonce, strlen(pszCNonce));
		MD5Final(HA1, &Md5Ctx);
	};
	CvtHex(HA1, SessionKey);
}


/* 
 * calculate request-digest/response-digest as per HTTP Digest spec 
 */
void DigestCalcResponse(
			HASHHEX HA1,                 /* H(A1) */
			const char * pszNonce,       /* nonce from server */
			const char * pszNonceCount,  /* 8 hex digits */
			const char * pszCNonce,      /* client nonce */
			const char * pszQop,         /* qop-value: "", "auth", "auth-int" */
			const char * pszMethod,      /* method from the request */
			const char * pszDigestUri,   /* requested URL */
			HASHHEX HEntity,             /* H(entity body) if qop="auth-int" */
			HASHHEX Response             /* request-digest or response-digest */)
{
	MD5_CTX Md5Ctx;
	HASH HA2;
	HASH RespHash;
	HASHHEX HA2Hex;
	
	     /* calculate H(A2) */
	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, pszMethod, strlen(pszMethod));
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, pszDigestUri, strlen(pszDigestUri));
	if (strcmp(pszQop, "auth-int") == 0) {
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, HEntity, HASHHEXLEN);
	};
	MD5Final(HA2, &Md5Ctx);
	CvtHex(HA2, HA2Hex);
	
	     /* calculate response */
	MD5Init(&Md5Ctx);
	MD5Update(&Md5Ctx, HA1, HASHHEXLEN);
	MD5Update(&Md5Ctx, ":", 1);
	MD5Update(&Md5Ctx, pszNonce, strlen(pszNonce));
	MD5Update(&Md5Ctx, ":", 1);
	if (*pszQop) {
		MD5Update(&Md5Ctx, pszNonceCount, strlen(pszNonceCount));
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, pszCNonce, strlen(pszCNonce));
		MD5Update(&Md5Ctx, ":", 1);
		MD5Update(&Md5Ctx, pszQop, strlen(pszQop));
		MD5Update(&Md5Ctx, ":", 1);
	};
	MD5Update(&Md5Ctx, HA2Hex, HASHHEXLEN);
	MD5Final(RespHash, &Md5Ctx);
	CvtHex(RespHash, Response);
}

