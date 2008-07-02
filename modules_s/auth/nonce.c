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
 * 2008-07-01 switched to base64 for nonces; check staleness in check_nonce
 *            (andrei)
 */


#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "../../compiler_opt.h"
#include "../../md5global.h"
#include "../../md5.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../ip_addr.h"
#include "nonce.h"
#include "../../globals.h"
#include <assert.h>


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



/* like calc_nonce (below), but calculate the binary nonce (don't convert to
 * ascii)  and return the binary nonce len (cannot return error)
 * See calc_nonce below for more details.*/
inline static int calc_bin_nonce(union bin_nonce* b_nonce, int cfg,
					int since, int expires, str* secret1, str* secret2,
					struct sip_msg* msg)
{
	MD5_CTX ctx;
	
	str* s;
	int len;

	MD5Init(&ctx);
	
	b_nonce->n.expire=htonl(expires);
	b_nonce->n.since=htonl(since);
	MD5Update(&ctx, &b_nonce->raw[0], 4 + 4);
	MD5Update(&ctx, secret1->s, secret1->len);
	MD5Final(&b_nonce->n.md5_1[0], &ctx);
	len = 4 + 4 + 16;
	
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
		MD5Final(&b_nonce->n.md5_2[0], &ctx);
		len += 16;
	}
	return len;
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
int calc_nonce(char* nonce, int *nonce_len, int cfg, int since, 
					int expires, str* secret1, str* secret2,
					struct sip_msg* msg)
{
	union bin_nonce b_nonce;
	int len;
	if (unlikely(*nonce_len < MAX_NONCE_LEN)) {
		if (unlikely(cfg && msg)) {
			*nonce_len = MAX_NONCE_LEN;
			return -1;
		} else if (*nonce_len < MIN_NONCE_LEN) {
			*nonce_len = MIN_NONCE_LEN;
			return -1;
		}
	}
	len=calc_bin_nonce(&b_nonce, cfg, since, expires, secret1, secret2, msg);
	*nonce_len=base64_enc(&b_nonce.raw[0], len, 
							(unsigned char*)nonce, *nonce_len);
	assert(*nonce_len>=0); /*FIXME*/
	return 0;
}



/** Returns the expire time of the nonce string.
 * This function returns the absolute expire time
 * extracted from the nonce string in the parameter.
 * @param bn is a valid pointer to a union bin_nonce (decoded nonce)
 * @return Absolute time when the nonce string expires.
 */

#define get_bin_nonce_expire(bn) ((time_t)ntohl((bn)->n.expire))

/** Returns the valid_since time of the nonce string.
 * This function returns the absolute time
 * extracted from the nonce string in the parameter.
 * @param bn is a valid pointer to a union bin_nonce (decoded nonce)
 * @return Absolute time when the nonce string was created.
 */
#define get_bin_nonce_since(bn) ((time_t)ntohl((bn)->n.since))


/** Checks if nonce is stale.
 * This function checks if a nonce given to it in the parameter is stale. 
 * A nonce is stale if the expire time stored in the nonce is in the past.
 * @param b_nonce a pointer to a union bin_nonce to be checked.
 * @return 1 the nonce is stale, 0 the nonce is not stale.
 */
#define is_bin_nonce_stale(b_nonce) (get_bin_nonce_expire(b_nonce) < time(0))



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
 *          1 - nonce length too small
 *          2 - no match
 *          3 - nonce expires ok, but the auth_extra checks failed
 *          4 - stale
 */
int check_nonce(str* nonce, str* secret1, str* secret2, struct sip_msg* msg)
{
	int since, expires, b_nonce2_len, b_nonce_len, cfg;
	union bin_nonce b_nonce;
	union bin_nonce b_nonce2;

	cfg = get_auth_checks(msg);

	if (unlikely(nonce->s == 0)) {
		return -1;  /* Invalid nonce */
	}
	
	if (unlikely(nonce->len<MIN_NONCE_LEN)){
		return 1; /* length musth be >= then minimum length */
	}
	
	/* decode nonce */
	b_nonce_len=base64_dec((unsigned char*)nonce->s, nonce->len,
							&b_nonce.raw[0], sizeof(b_nonce));
	if (unlikely(b_nonce_len <=MIN_BIN_NONCE_LEN)){
		DBG("auth: check_nonce: base64_dec failed\n");
		return -1; /* error decoding the nonce (invalid nonce since we checked
		              the len of the base64 enc. nonce above)*/
	}
	
	since = get_bin_nonce_since(&b_nonce);
	expires = get_bin_nonce_expire(&b_nonce);
	if (unlikely(since < up_since)) {
		/* if valid_since time is time pointing before ser was started 
		 * then we consider nonce as stalled. 
		   It may be the nonce generated by previous ser instance having
		   different length (for example because of different auth.
		   checks)..  Therefore we force credentials to be rebuilt by UAC
		   without prompting for password */
		return 3;
	}
	
	b_nonce2_len=calc_bin_nonce(&b_nonce2, cfg, since, expires, secret1,
									secret2, msg);
	if (!memcmp(&b_nonce, &b_nonce2, MIN_BIN_NONCE_LEN)) {
		if (cfg) {
			if (unlikely(b_nonce_len != b_nonce2_len))
				return 2; /* someone truncated our nonce? */
			if (memcmp(&b_nonce.n.md5_2[0], &b_nonce2.n.md5_2[0], 16))
				return 3; /* auth_extra_checks failed */
		}
		if (unlikely(is_bin_nonce_stale(&b_nonce)))
			return 4;
		return 0;
	}
	
	return 2;
}

