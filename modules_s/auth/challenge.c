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
 *
 * History:
 * --------
 * 2003-01-20 snprintf in build_auth_hf replaced with memcpy to avoid
 *            possible issues with too small buffer
 * 2003-01-26 consume_credentials no longer complains about ACK/CANCEL(jiri)
 */

#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "../../parser/digest/digest.h"
#include "auth_mod.h"
#include "common.h"
#include "challenge.h"
#include "nonce.h"


/*
 * proxy_challenge function sends this reply
 */
#define MESSAGE_407          "Proxy Authentication Required"
#define PROXY_AUTH_CHALLENGE "Proxy-Authenticate"


/*
 * www_challenge function send this reply
 */
#define MESSAGE_401        "Unauthorized"
#define WWW_AUTH_CHALLENGE "WWW-Authenticate"


#define QOP_PARAM	  ", qop=\"auth\""
#define QOP_PARAM_LEN	  (sizeof(QOP_PARAM)-1)
#define STALE_PARAM	  ", stale=true"
#define STALE_PARAM_LEN	  (sizeof(STALE_PARAM)-1)
#define DIGEST_REALM	  ": Digest realm=\""
#define DIGEST_REALM_LEN  (sizeof(DIGEST_REALM)-1)
#define DIGEST_NONCE	  "\", nonce=\""
#define DIGEST_NONCE_LEN  (sizeof(DIGEST_NONCE)-1)
#define DIGEST_MD5	  ", algorithm=MD5"
#define DIGEST_MD5_LEN	  (sizeof(DIGEST_MD5)-1)


/*
 * Create {WWW,Proxy}-Authenticate header field
 */
static inline char *build_auth_hf(int _retries, int _stale, str* _realm, 
				  int* _len, int _qop, char* _hf_name)
{
	
	int hf_name_len;
	char *hf, *p;

	     /* length calculation */
	*_len=hf_name_len=strlen(_hf_name);
	*_len+=DIGEST_REALM_LEN
		+_realm->len
		+DIGEST_NONCE_LEN
		+NONCE_LEN
		+1 /* '"' */
		+((_qop)? QOP_PARAM_LEN:0)
		+((_stale)? STALE_PARAM_LEN : 0)
#ifdef _PRINT_MD5
		+DIGEST_MD5_LEN
#endif
		+CRLF_LEN ;
	
	p=hf=pkg_malloc(*_len+1);
	if (!hf) {
		LOG(L_ERR, "ERROR: build_auth_hf: no memory\n");
		*_len=0;
		return 0;
	}

	memcpy(p, _hf_name, hf_name_len); p+=hf_name_len;
	memcpy(p, DIGEST_REALM, DIGEST_REALM_LEN);p+=DIGEST_REALM_LEN;
	memcpy(p, _realm->s, _realm->len);p+=_realm->len;
	memcpy(p, DIGEST_NONCE, DIGEST_NONCE_LEN);p+=DIGEST_NONCE_LEN;
	calc_nonce(p, time(0) + nonce_expire, &secret);
	p+=NONCE_LEN;
	*p='"';p++;
	if (_qop) {
		memcpy(p, QOP_PARAM, QOP_PARAM_LEN);
		p+=QOP_PARAM_LEN;
	}
	if (_stale) {
		memcpy(p, STALE_PARAM, STALE_PARAM_LEN);
		p+=STALE_PARAM_LEN;
	}
#ifdef _PRINT_MD5
	memcpy(p, DIGEST_MD5, DIGEST_MD5_LEN ); p+=DIGEST_MD5_LEN;
#endif
	memcpy(p, CRLF, CRLF_LEN ); p+=CRLF_LEN;
	*p=0; /* zero terminator, just in case */
	
	DBG("build_auth_hf(): '%s'\n", hf);
	return hf;
}


/*
 * Create and send a challenge
 */
static inline int challenge(struct sip_msg* _msg, str* _realm, int _qop, 
			    int _code, char* _message, char* _challenge_msg)
{
	int auth_hf_len;
	struct hdr_field* h;
	auth_body_t* cred = 0;
	char *auth_hf;
	int ret, hftype = 0; /* Makes gcc happy */
	struct sip_uri uri;

	switch(_code) {
	case 401: 
		get_authorized_cred(_msg->authorization, &h); 
		hftype = HDR_AUTHORIZATION;
		break;
	case 407: 
		get_authorized_cred(_msg->proxy_auth, &h);
		hftype = HDR_PROXYAUTH;
		break;
	}

	if (h) cred = (auth_body_t*)(h->parsed);

	if (_realm->len == 0) {
		if (get_realm(_msg, hftype, &uri) < 0) {
			LOG(L_ERR, "challenge(): Error while extracting URI\n");
			if (send_resp(_msg, 400, MESSAGE_400, 0, 0) == -1) {
				LOG(L_ERR, "challenge(): Error while sending response\n");
				return -1;
			}
			return 0;
		}

		_realm = &uri.host;
	}

	auth_hf = build_auth_hf(0, (cred ? cred->stale : 0), _realm, &auth_hf_len, _qop, _challenge_msg);
	if (!auth_hf) {
		LOG(L_ERR, "ERROR: challenge: no mem w/cred\n");
		return -1;
	}
	
	ret = send_resp(_msg, _code, _message, auth_hf, auth_hf_len);
	if (auth_hf) pkg_free(auth_hf);
	if (ret == -1) {
		LOG(L_ERR, "challenge(): Error while sending response\n");
		return -1;
	}
	
	return 0;
}


/*
 * Challenge a user to send credentials using WWW-Authorize header field
 */
int www_challenge(struct sip_msg* _msg, char* _realm, char* _qop)
{
	return challenge(_msg, (str*)_realm, (int)(long)_qop, 401, MESSAGE_401,
						WWW_AUTH_CHALLENGE);
}


/*
 * Challenge a user to send credentials using Proxy-Authorize header field
 */
int proxy_challenge(struct sip_msg* _msg, char* _realm, char* _qop)
{
	return challenge(_msg, (str*)_realm, (int)(long)_qop, 407, MESSAGE_407, 
						PROXY_AUTH_CHALLENGE);
}


/*
 * Remove used credentials from a SIP message header
 */
int consume_credentials(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct hdr_field* h;
	int len;

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) { 
			if (_m->REQ_METHOD!=METHOD_ACK 
					&& _m->REQ_METHOD==METHOD_CANCEL) {
				LOG(L_ERR, "consume_credentials(): No authorized "
					"credentials found (error in scripts)\n");
			}
			return -1;
		}
	}

	len=h->len;

	if (del_lump(_m, h->name.s - _m->buf, len, 0) == 0) {
		LOG(L_ERR, "consume_credentials(): Can't remove credentials\n");
		return -1;
	}

	return 1;
}
