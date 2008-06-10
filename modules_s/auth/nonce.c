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
#include "../../globals.h"


int auth_checks_reg = 0;
int auth_checks_ood = 0;
int auth_checks_ind = 0;


/** Select extra check configuration based on request type.
 * This function determines which configuration variable for
 * extra authentication checks is to be used based on the
 * type of the request. It returns the value of auth_checks_reg
 * for REGISTER requests, the value auth_checks_ind for requests
 * that contain valid To tag and the value of auth_checks_ood
 * otherwise.
 */
int get_auth_checks(struct sip_msg* msg)
{
	str tag;

	if (msg == NULL) return 0;

	if (msg->REQ_METHOD == METHOD_REGISTER) {
		return auth_checks_reg;
	}
		
	if (!msg->to && parse_headers(msg, HDR_TO_F, 0) == -1) {
		DBG("auth: Error while parsing To header field\n");
		return auth_checks_ood;
	}
	if (msg->to) {
		tag = get_to(msg)->tag_value;
		if (tag.s && tag.len > 0) return auth_checks_ind;
	}
	return auth_checks_ood;
}


/** Convert integer to its HEX representation.
 * This function converts an integer number to
 * its hexadecimal representation. The destination
 * buffer must be at least 8 bytes long, the resulting
 * string is NOT zero terminated.
 * @param dst is the destination buffer, at least 8 characters long
 * @param src is the integer number to be converted
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


/** Calculates the nonce string for RFC2617 digest authentication.
 * This function creates the nonce string as it will be sent to the
 * user agent in digest challenge. The format of the nonce string
 * depends on the value of three module parameters, auth_checks_register,
 * auth_checks_no_dlg, and auth_checks_in_dlg. These module parameters
 * control the amount of information from the SIP requst that will be
 * stored in the nonce string for verification purposes.
 *
 * If all three parameters contain zero then the nonce string consists
 * of time in seconds since 1.1. 1970 and a secret phrase:
 * <expire_time> <valid_since> MD5(<expire_time>, <valid_since>, secret)
 * If any of the parameters is not zero (some optional checks are enabled
 * then the nonce string will also contain MD5 hash of selected parts
 * of the SIP request:
 * <expire_time> <valid_since> MD5(<expire_time>, <valid_since>, secret1) MD5(<extra_checks>, secret2)
 * @param nonce  Pointer to a buffer of *nonce_len. It must have enough
 *               space to hold the nonce. MAX_NONCE_LEN should be always 
 *               safe.
 * @param nonce_len A value/result parameter. Initially it contains the
 *                  nonce buffer length. If the length is too small, it 
 *                  will be set to the needed length and the function will 
 *                  return error immediately. After a succesfull call it will 
 *                  contain the size of nonce written into the buffer, 
 *                  without the terminating 0.
 * @param cfg This is the value of one of the tree module parameters that
 *            control which optional checks are enabled/disabled and which
 *            parts of the message will be included in the nonce string.
 * @param since Time when nonce was created, i.e. nonce is valid since <valid_since> up to <expires>
 * @param expires Time in seconds after which the nonce will be considered 
 *                stale.
 * @param secret1 A secret used for the nonce expires integrity check:
 *                MD5(<expire_time>, <valid_since>, secret1).
 * @param secret2 A secret used for integrity check of the message parts 
 *                selected by auth_extra_checks (if any):
 *                MD5(<msg_parts(auth_extra_checks)>, secret2).
 * @param msg The message for which the nonce is computed. If auth_extra_checks
 *            is set, the MD5 of some fields of the message will be included in 
 *            the  generated nonce.
 * @return 0 on success and -1 on error
 */
int calc_nonce(char* nonce, int *nonce_len, int cfg, int since, int expires, str* secret1,
			   str* secret2, struct sip_msg* msg)
{
	MD5_CTX ctx;
	unsigned char bin[16];
	str* s;
	int len;

	if (*nonce_len < MAX_NONCE_LEN) {
		if (cfg && msg) {
			*nonce_len = MAX_NONCE_LEN;
			return -1;
		} else if (*nonce_len < MIN_NONCE_LEN) {
			*nonce_len = MIN_NONCE_LEN;
			return -1;
		}
	}
	MD5Init(&ctx);
	
	integer2hex(nonce, expires);
	integer2hex(nonce + 8, since);
	MD5Update(&ctx, nonce, 8 + 8);
	MD5Update(&ctx, secret1->s, secret1->len);
	MD5Final(bin, &ctx);
	string2hex(bin, 16, nonce + 8 + 8);
	len = 8 + 8 + 32;
	
	if (cfg && msg) {
		MD5Init(&ctx);
		if (cfg & AUTH_CHECK_FULL_URI) {
			s = GET_RURI(msg);
			MD5Update(&ctx, s->s, s->len);
		}
		if ((cfg & AUTH_CHECK_CALLID) && 
			!(parse_headers(msg, HDR_CALLID_F, 0) < 0 || msg->callid == 0)) {
			MD5Update(&ctx, msg->callid->body.s, msg->callid->body.len);
		}
		if ((cfg & AUTH_CHECK_FROMTAG) &&
			!(parse_headers(msg, HDR_FROM_F, 0) < 0 || msg->from == 0)) {
			MD5Update(&ctx, get_from(msg)->tag_value.s, 
					  get_from(msg)->tag_value.len);
		}
		if (cfg & AUTH_CHECK_SRC_IP) {
			MD5Update(&ctx, msg->rcv.src_ip.u.addr, msg->rcv.src_ip.len);
		}
		MD5Update(&ctx, secret2->s, secret2->len);
		MD5Final(bin, &ctx);
		string2hex(bin, 16, nonce + len);
		len += 32;
	}
	*nonce_len = len;
	return 0;
}


/** Returns the expire time of the nonce string.
 * This function returns the absolute expire time
 * extracted from the nonce string in the parameter.
 * @param n is a valid nonce string.
 * @return Absolute time when the nonce string expires.
 */
time_t get_nonce_expires(str* n)
{
	return (time_t)hex2integer(n->s);
}

/** Returns the valid_since time of the nonce string.
 * This function returns the absolute time
 * extracted from the nonce string in the parameter.
 * @param n is a valid nonce string.
 * @return Absolute time when the nonce string was created.
 */
time_t get_nonce_since(str* n)
{
	return (time_t)hex2integer(n->s + 8);
}


/** Check whether the nonce returned by UA is valid.
 * This function checks whether the nonce string returned by UA
 * in digest response is valid. The function checks if the nonce
 * string hasn't expired, it verifies the secret stored in the nonce
 * string with the secret configured on the server. If any of the
 * optional extra integrity checks are enabled then it also verifies
 * whether the corresponding parts in the new SIP requests are same.
 * @param nonce A nonce string to be verified.
 * @param secret1 A secret used for the nonce expires integrity check:
 *                MD5(<expire_time>,, secret1).
 * @param secret2 A secret used for integrity check of the message parts 
 *                selected by auth_extra_checks (if any):
 *                MD5(<msg_parts(auth_extra_checks)>, secret2).
 * @param msg The message which contains the nonce being verified. 
 * @return 0 - success (the nonce was not tampered with and if 
 *             auth_extra_checks are enabled - the selected message fields
 *             have not changes from the time the nonce was generated)
 *         -1 - invalid nonce
 *          1 - nonce length mismatch
 *          2 - no match
 *          3 - nonce expires ok, but the auth_extra checks failed
 */
int check_nonce(str* nonce, str* secret1, str* secret2, struct sip_msg* msg)
{
	int since, expires, non_len, cfg;
	char non[MAX_NONCE_LEN];

	cfg = get_auth_checks(msg);

	if (nonce->s == 0) {
		return -1;  /* Invalid nonce */
	}
	non_len = sizeof(non);

	if (get_nonce_len(cfg) != nonce->len) {
		return 1; /* Lengths must be equal */
	}

	since = get_nonce_since(nonce);
	expires = get_nonce_expires(nonce);
	if (calc_nonce(non, &non_len, cfg, since, expires, secret1, secret2, msg) != 0) {
		if (since < up_since) {
			/* if valid_since time is time pointing before ser was started then we consider nonce as stalled. 
			   It may be the nonce generated by previous ser instance having different secret keys.
			   Therefore we force credentials to be rebuilt by UAC without prompting for password */
			return 3;
		}
		ERR("auth: check_nonce: calc_nonce failed (len %zd, needed %d)\n",
			sizeof(non), non_len);
		return -1;
	}

	DBG("auth:check_nonce: comparing [%.*s] and [%.*s]\n",
	    nonce->len, ZSW(nonce->s), non_len, non);
	
	if (!memcmp(non, nonce->s, MIN_NONCE_LEN)) {
		if (cfg) {
			if (non_len != nonce->len)
				return 2; /* someone truncated our nonce? */
			if (memcmp(non + MIN_NONCE_LEN, nonce->s + MIN_NONCE_LEN, 
					   non_len - MIN_NONCE_LEN) != 0)
				return 3; /* auth_extra_checks failed */
		}
		return 0;
	}
	
	return 2;
}


/** Checks if nonce is stale.
 * This function checks if a nonce given to it in the parameter is stale. 
 * A nonce is stale if the expire time stored in the nonce is in the past.
 * @param n a nonce to be checked.
 * @return 1 the nonce is stale, 0 the nonce is not stale.
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
