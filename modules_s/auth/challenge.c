/*
 * $Id$
 *
 * Challenge related functions
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


#include <time.h>
#include <stdio.h>
#include "../../dprint.h"
#include "../../parser/digest/digest.h" /* cred_body_t get_authorized_cred*/
#include "challenge.h"
#include "nonce.h"                      /* calc_nonce */
#include "common.h"                     /* send_resp */
#include "auth_mod.h"                   /* Module parameters */
#include "defs.h"                       /* PRINT_MD5 */


#define MESSAGE_407 "Proxy Authentication Required"
#define MESSAGE_401 "Unauthorized"
#define MESSAGE_403 "Forbidden"

#define WWW_AUTH_CHALLENGE   "WWW-Authenticate"
#define PROXY_AUTH_CHALLENGE "Proxy-Authenticate"

#define AUTH_HF_LEN 512

static char auth_hf[AUTH_HF_LEN];


/*
 * Create {WWW,Proxy}-Authenticate header field
 */
static inline void build_auth_hf(int _retries, int _stale, char* _realm, char* _buf, 
				 int* _len, int _qop, char* _hf_name)
{
	char nonce[NONCE_LEN + 1];
	
	calc_nonce(nonce, time(NULL) + nonce_expire, _retries, &secret);
	nonce[NONCE_LEN] = '\0';
	
	*_len = snprintf(_buf, AUTH_HF_LEN,
			 "%s: Digest realm=\"%s\", nonce=\"%s\"%s%s"
#ifdef PRINT_MD5
			 ", algorithm=MD5"
#endif
			 "\r\n", 
			 _hf_name, 
			 _realm, 
			 nonce,
			 (_qop) ? (", qop=\"auth\"") : (""),
			 (_stale) ? (", stale=true") : ("")
			 );		
	
	DBG("build_auth_hf(): %s\n", _buf);
}


/*
 * Create and send a challenge
 */
static inline int challenge(struct sip_msg* _msg, char* _realm, int _qop, 
			    int _code, char* _message, char* _challenge_msg)
{
	int auth_hf_len;
	struct hdr_field* h;
	auth_body_t* cred = 0;

	switch(_code) {
	case 401: get_authorized_cred(_msg->authorization, &h); break;
	case 407: get_authorized_cred(_msg->proxy_auth, &h);    break;
	}

	if (h) cred = (auth_body_t*)(h->parsed);

	if (cred != 0) {
		if (cred->nonce_retries > retry_count) {
			DBG("challenge(): Retry count exceeded, sending Forbidden\n");
			_code = 403;
			_message = MESSAGE_403;
			auth_hf_len = 0;
		} else {
			if (cred->stale == 0) {
				cred->nonce_retries++;
			} else {
				cred->nonce_retries = 0;
			}
			
			build_auth_hf(cred->nonce_retries, cred->stale, 
				      _realm, auth_hf, &auth_hf_len,
				      _qop, _challenge_msg);
		}
	} else {
		build_auth_hf(0, 0, _realm, auth_hf, &auth_hf_len, _qop, _challenge_msg);
	}
	
	if (send_resp(_msg, _code, _message, auth_hf, auth_hf_len) == -1) {
		LOG(L_ERR, "www_challenge(): Error while sending response\n");
		return -1;
	}
	return 0;
}


/*
 * Challenge a user to send credentials using WWW-Authorize header field
 */
int www_challenge(struct sip_msg* _msg, char* _realm, char* _qop)
{
	return challenge(_msg, _realm, (int)_qop, 401, MESSAGE_401, WWW_AUTH_CHALLENGE);
}


/*
 * Challenge a user to send credentials using Proxy-Authorize header field
 */
int proxy_challenge(struct sip_msg* _msg, char* _realm, char* _qop)
{
	return challenge(_msg, _realm, (int)_qop, 407, MESSAGE_407, PROXY_AUTH_CHALLENGE);
}
