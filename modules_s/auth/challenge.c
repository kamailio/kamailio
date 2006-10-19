/*
 * $Id$
 *
 * Challenge related functions
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
#include "../../usr_avp.h"
#include "../../ut.h"
#include "auth_mod.h"
#include "challenge.h"
#include "nonce.h"
#include "api.h"

#define QOP_PARAM_START   ", qop=\""
#define QOP_PARAM_START_LEN (sizeof(QOP_PARAM_START)-1)
#define QOP_PARAM_END     "\""
#define QOP_PARAM_END_LEN (sizeof(QOP_PARAM_END)-1)
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
 * The result is stored in an attribute
 * return -1 on error, 0 on success
 */
int build_challenge_hf(struct sip_msg* msg, int stale, str* realm, int hftype)
{
    char *p;
    str* hfn, hf;
    avp_value_t val;
    
    if (hftype == HDR_PROXYAUTH_T) {
	hfn = &proxy_challenge_header;
    } else {
	hfn = &www_challenge_header;
    }

    hf.len = hfn->len;
    hf.len += DIGEST_REALM_LEN
	+ realm->len
	+ DIGEST_NONCE_LEN
	+ NONCE_LEN
	+ 1 /* '"' */
	+ ((stale) ? STALE_PARAM_LEN : 0)
#ifdef _PRINT_MD5
	+DIGEST_MD5_LEN
#endif
	+CRLF_LEN;
    
    if (qop.qop_parsed != QOP_UNSPEC) {
	hf.len += QOP_PARAM_START_LEN + qop.qop_str.len + QOP_PARAM_END_LEN;
    }

    p = hf.s = pkg_malloc(hf.len);
    if (!hf.s) {
	ERR("build_challenge_hf: No memory left\n");
	return -1;
    }
    
    memcpy(p, hfn->s, hfn->len); p += hfn->len;
    memcpy(p, DIGEST_REALM, DIGEST_REALM_LEN); p += DIGEST_REALM_LEN;
    memcpy(p, realm->s, realm->len); p += realm->len;
    memcpy(p, DIGEST_NONCE, DIGEST_NONCE_LEN); p += DIGEST_NONCE_LEN;
    calc_nonce(p, time(0) + nonce_expire, &secret, msg);
    p += NONCE_LEN;
    *p = '"'; p++;
    if (qop.qop_parsed != QOP_UNSPEC) {
	memcpy(p, QOP_PARAM_START, QOP_PARAM_START_LEN);
	p += QOP_PARAM_START_LEN;
	memcpy(p, qop.qop_str.s, qop.qop_str.len);
	p += qop.qop_str.len;
	memcpy(p, QOP_PARAM_END, QOP_PARAM_END_LEN);
	p += QOP_PARAM_END_LEN;
    }
    if (stale) {
	memcpy(p, STALE_PARAM, STALE_PARAM_LEN);
	p += STALE_PARAM_LEN;
    }
#ifdef _PRINT_MD5
    memcpy(p, DIGEST_MD5, DIGEST_MD5_LEN ); p += DIGEST_MD5_LEN;
#endif
    memcpy(p, CRLF, CRLF_LEN); p += CRLF_LEN;
    
    DBG("auth:build_challenge_hf: '%.*s'\n", hf.len, ZSW(hf.s));

    val.s = hf;
    if (add_avp(challenge_avpid.flags | AVP_VAL_STR, challenge_avpid.name, val) < 0) {
	ERR("build_challenge_hf: Error while creating attribute\n");
	pkg_free(hf.s);
	return -1;
    }
	pkg_free(hf.s);

    return 0;
}
