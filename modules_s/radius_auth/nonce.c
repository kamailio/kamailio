/*
 * $Id$
 *
 * Nonce related functions
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


#include "nonce.h"
#include "rfc2617.h"
#include <time.h>
#include "../../md5global.h"
#include "../../md5.h"
#include <string.h>


/*
 * Convert an integer to its hex representation,
 * destination array must be at least 8 bytes long,
 * this string is NOT zero terminated
 */
static inline void int2hex(char* _d, int _s)
{
	int i;
	unsigned char j;
	char* s = (char*)&_s;
    
	for (i = 0; i < 4; i++) {
		
		j = (s[4 - i - 1] >> 4) & 0xf;
		if (j <= 9) {
			_d[i * 2] = (j + '0');
		} else { 
			_d[i * 2] = (j + 'a' - 10);
		}

		j = s[4 - i - 1] & 0xf;
		if (j <= 9) {
			_d[i * 2 + 1] = (j + '0');
		} else {
		       _d[i * 2 + 1] = (j + 'a' - 10);
		}
	}
}


/*
 * Convert hex string to integer
 */
static inline int hex2int(char* _s)
{
	unsigned int i, res = 0;

	for(i = 0; i < 8; i++) {
		res *= 16;
		if ((_s[i] >= '0') && (_s[i] <= '9')) {
			res += _s[i] - '0';
		} else if ((_s[i] >= 'a') && (_s[i] <= 'f')) {
			res += _s[i] - 'a' + 10;
		} else if ((_s[i] >= 'A') && (_s[i] <= 'F')) {
			res += _s[i] - 'A' + 10;
		} else return 0;
	}

	return res;
}


/*
 * Calculate nonce value
 * Nonce value consists of time in seconds since 1.1 1970 and
 * secret phrase
 */
inline void calc_nonce(char* _nonce, int _expires, int _retry, str* _secret)
{
	MD5_CTX ctx;
	char bin[16];

	MD5Init(&ctx);
	
	int2hex(_nonce, _expires);
	MD5Update(&ctx, _nonce, 8);

	int2hex(_nonce + 8, _retry);
	MD5Update(&ctx, _nonce + 8, 8);

	MD5Update(&ctx, _secret->s, _secret->len);
	MD5Final(bin, &ctx);
	cvt_hex(bin, _nonce + 16);
}


/*
 * Get expiry time from nonce string
 */
inline time_t get_nonce_expires(str* _n)
{
	return (time_t)hex2int(_n->s);
}


/*
 * Get retry counter from nonce string
 */
inline int get_nonce_retry(str* _n)
{
	if (!_n->s) return 0;
	return hex2int(_n->s + 8);
}


/*
 * Check, if the nonce received from client is
 * correct
 */
int check_nonce(str* _nonce, str* _secret)
{
	int expires, retry;
	char non[NONCE_LEN + 1];

	if (_nonce->s == 0) {
		return 0;  /* Invalid nonce */
	}

	expires = get_nonce_expires(_nonce);
	retry = get_nonce_retry(_nonce);

	calc_nonce(non, expires, retry, _secret);

	if (NONCE_LEN != _nonce->len) {
		return 0; /* Lengths must be equal */
	}

	if (!memcmp(non, _nonce->s, _nonce->len)) {
		return 1;
	}

	return 0;
}


/*
 * Returns 1 if nonce is stale
 * 0 otherwise
 */
int nonce_is_stale(str* _n) 
{
	if (!_n->s) return 0;

	if (get_nonce_expires(_n) < time(NULL)) {
		return 1;
	} else {
		return 0;
	}
}
