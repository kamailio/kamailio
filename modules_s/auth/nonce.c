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
/*
 * History:
 * --------
 *            ...
 * 2007-10-19 auth extra checks: longer nonces that include selected message
 *            parts to protect against various reply attacks without keeping
 *            state (andrei)
 */


#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "../../md5global.h"
#include "../../md5.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../ip_addr.h"
#include "nonce.h"


int   auth_extra_checks = 0;      /* by default don't do any extra checks */


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
 * Nonce value depends on the auth_extra_checks flags:
 *  if auth_extra_checks==0 (no extra check set)
 *    consists of time in seconds since 1.1 1970 and secret phrase:
 *     <expire_time> MD5(<expire_time>, secret)
 *  else
 *    like above but with an extra MD5 on some fields selected by the
 *     auth_extra_checks flags:
 *      <expire_time> MD5(<expire_time>, secret1) MD5(<extra_checks>, secret2)
 *
 * Params:
 *   nonce     - pointer to a buffer of *nonce_len. It must have enough
 *               space to hold the nonce. MAX_NONCE_LEN should be always safe.
 *   nonce_len - value - result parameter. Initially it contains the
 *               nonce buffer length. If the length is too small, it will be
 *               set to the needed length and the function will return 
 *               error immediately.
 *               After a succesfull call it will contain the size of nonce
 *               written into the buffer, without the terminating 0.
 *   expires   - time in seconds after which the nonce will be considered 
 *               stale
 *   secret1    - secret used for  the nonce expires integrity  check:
 *                 MD5(<expire_time>,, secret1).
 *   secret2    - secret used for integrity check of the message parts
 *                 selected by auth_extra_checks (if any):
 *                 MD5(<msg_parts(auth_extra_checks)>, secret2).
 *   msg       - message for which the nonce is computed. If auth_extra_checks
 *               is set, the MD5 of some fields of the message will be included
 *               in the  generated nonce
 * Returns:  0 on success and -1 on error
 *
 * WARNING: older versions used to 0-terminate the nonce. This is not the
 *          case anymore.
 *
 */
int calc_nonce(char* nonce, int *nonce_len, int expires, str* secret1,
				str* secret2, struct sip_msg* msg)
{
	MD5_CTX ctx;
	unsigned char bin[16];
	str* s;
	int len;


	if (*nonce_len < MAX_NONCE_LEN){
		if (auth_extra_checks && msg){
			*nonce_len=MAX_NONCE_LEN;
			return -1;
		}else if (*nonce_len < MIN_NONCE_LEN){
			*nonce_len=MIN_NONCE_LEN;
			return -1;
		}
	}
	MD5Init(&ctx);
	
	integer2hex(nonce, expires);
	MD5Update(&ctx, nonce, 8);
	MD5Update(&ctx, secret1->s, secret1->len);
	MD5Final(bin, &ctx);
	string2hex(bin, 16, nonce + 8);
	len=8 + 32;
	
	if (auth_extra_checks && msg){
		MD5Init(&ctx);
		if (auth_extra_checks & AUTH_CHECK_FULL_URI){
			s=GET_RURI(msg);
			MD5Update(&ctx, s->s, s->len);
		}
		if ((auth_extra_checks & AUTH_CHECK_CALLID) && 
				! (parse_headers(msg, HDR_CALLID_F, 0)<0 || msg->callid==0)){
			MD5Update(&ctx, msg->callid->body.s, msg->callid->body.len);
		}
		if ((auth_extra_checks & AUTH_CHECK_FROMTAG) &&
				! (parse_headers(msg, HDR_FROM_F, 0)<0 || msg->from==0)){
			MD5Update(&ctx, get_from(msg)->tag_value.s, 
							get_from(msg)->tag_value.len);
		}
		if (auth_extra_checks & AUTH_CHECK_SRC_IP){
			MD5Update(&ctx, msg->rcv.src_ip.u.addr, msg->rcv.src_ip.len);
		}
		MD5Update(&ctx, secret2->s, secret2->len);
		MD5Final(bin, &ctx);
		string2hex(bin, 16, nonce + len);
		len+=32;
	}
	*nonce_len=len;
	return 0;
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
 * Returns:
 *         0 - success (the nonce was not tampered with and if 
 *             auth_extra_checks are enabled - the selected message fields
 *             have not changes from the time the nonce was generated)
 *         -1 - invalid nonce
 *          1 - nonce length mismatch
 *          2 - no match
 *          3 - nonce expires ok, but the auth_extra checks failed
 */
int check_nonce(str* nonce, str* secret1, str* secret2, struct sip_msg* msg)
{
	int expires;
	char non[MAX_NONCE_LEN];
	int non_len;

	if (nonce->s == 0) {
		return -1;  /* Invalid nonce */
	}
	non_len=sizeof(non);

	if (get_cfg_nonce_len() != nonce->len) {
		return 1; /* Lengths must be equal */
	}

	expires = get_nonce_expires(nonce);
	if (calc_nonce(non, &non_len, expires, secret1, secret2, msg)!=0){
		ERR("auth: check_nonce: calc_nonce failed (len %d, needed %d)\n",
				sizeof(non), non_len);
		return -1;
	}

	DBG("auth:check_nonce: comparing [%.*s] and [%.*s]\n",
	    nonce->len, ZSW(nonce->s), non_len, non);
	
	if (!memcmp(non, nonce->s, MIN_NONCE_LEN)) {
		if (auth_extra_checks){
			if (non_len!=nonce->len)
				return 2; /* someone truncated our nounce? */
			if (memcmp(non+MIN_NONCE_LEN, nonce->s+MIN_NONCE_LEN, 
							non_len-MIN_NONCE_LEN)!=0)
				return 3; /* auth_extra_checks failed */
		}
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
