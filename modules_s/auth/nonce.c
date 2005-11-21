/*
 * $Id$
 *
 * Nonce related functions
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


#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "../../md5global.h"
#include "../../md5.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "nonce.h"


/*
 * Convert an integer to its hex representation,
 * destination array must be at least 8 bytes long,
 * this string is NOT zero terminated
 */
static inline void integer2hex(char* dst, int src)
{
	int i;
	unsigned char j;
	char* s;

	src = htonl(src);
	s = (char*)&src;
    
	for (i = 0; i < 4; i++) {
		
		j = (s[i] >> 4) & 0xf;
		if (j <= 9) {
			dst[i * 2] = (j + '0');
		} else { 
			dst[i * 2] = (j + 'a' - 10);
		}

		j = s[i] & 0xf;
		if (j <= 9) {
			dst[i * 2 + 1] = (j + '0');
		} else {
		       dst[i * 2 + 1] = (j + 'a' - 10);
		}
	}
}


/*
 * Convert hex string to integer
 */
static inline int hex2integer(char* src)
{
	unsigned int i, res = 0;

	for(i = 0; i < 8; i++) {
		res *= 16;
		if ((src[i] >= '0') && (src[i] <= '9')) {
			res += src[i] - '0';
		} else if ((src[i] >= 'a') && (src[i] <= 'f')) {
			res += src[i] - 'a' + 10;
		} else if ((src[i] >= 'A') && (src[i] <= 'F')) {
			res += src[i] - 'A' + 10;
		} else return 0;
	}

	return res;
}


/*
 * Calculate nonce value
 * Nonce value consists of time in seconds since 1.1 1970 and
 * secret phrase
 */
void calc_nonce(char* nonce, int expires, str* secret, struct sip_msg* msg)
{
	MD5_CTX ctx;
	unsigned char bin[16];

	MD5Init(&ctx);
	
	integer2hex(nonce, expires);
	MD5Update(&ctx, nonce, 8);

	MD5Update(&ctx, secret->s, secret->len);
	MD5Final(bin, &ctx);
	string2hex(bin, 16, nonce + 8);
	nonce[8 + 32] = '\0';
}


/*
 * Get expiry time from nonce string
 */
time_t get_nonce_expires(str* n)
{
	return (time_t)hex2integer(n->s);
}


/*
 * Check, if the nonce received from client is
 * correct
 */
int check_nonce(str* nonce, str* secret, struct sip_msg* msg)
{
	int expires;
	char non[NONCE_LEN + 1];

	if (nonce->s == 0) {
		return -1;  /* Invalid nonce */
	}

	if (NONCE_LEN != nonce->len) {
		return 1; /* Lengths must be equal */
	}

	expires = get_nonce_expires(nonce);
	calc_nonce(non, expires, secret, msg);

	DBG("auth:check_nonce: comparing [%.*s] and [%.*s]\n",
	    nonce->len, ZSW(nonce->s), NONCE_LEN, non);
	
	if (!memcmp(non, nonce->s, nonce->len)) {
		return 0;
	}

	return 2;
}


/*
 * Check if a nonce is stale
 */
int is_nonce_stale(str* n) 
{
	if (!n->s) return 0;

	if (get_nonce_expires(n) < time(0)) {
		return 1;
	} else {
		return 0;
	}
}
