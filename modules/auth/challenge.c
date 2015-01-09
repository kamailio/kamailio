/*
 * Digest Authentication Module
 * 
 * Copyright (C) 2001-2003 FhG Fokus
 * 
 * This file is part of Kamailio, a free SIP server.
 * 
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 * 
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
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
#include "nc.h"
#include "ot_nonce.h"

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
#define DIGEST_ALGORITHM     ", algorithm="
#define DIGEST_ALGORITHM_LEN (sizeof(DIGEST_ALGORITHM)-1)


extern str auth_realm_prefix;
/**
 * @brief Strip the beginning of a realm string
 *
 * Strip the beginning of a realm string, depending on the length of
 * the realm_prefix.
 * @param _realm realm string
 */
void strip_realm(str* _realm)
{
	/* no param defined -- return */
	if (!auth_realm_prefix.len) return;

	/* prefix longer than realm -- return */
	if (auth_realm_prefix.len > _realm->len) return;

	/* match ? -- if so, shorten realm -*/
	if (memcmp(auth_realm_prefix.s, _realm->s, auth_realm_prefix.len) == 0) {
		_realm->s += auth_realm_prefix.len;
		_realm->len -= auth_realm_prefix.len;
	}
	return;
}


/**
 * Create and return {WWW,Proxy}-Authenticate header field
 * @param nonce nonce value
 * @param algorithm algorithm value
 * @param qop qop value
 * @return -1 on error, 0 on success
 * 
 * The result is stored in param ahf.
 * If nonce is not null that it is used, instead of call calc_nonce.
 * If algorithm is not null that it is used irrespective of _PRINT_MD5
 * 
 * Major usage of nonce and algorithm params is AKA authentication. 
 */
int get_challenge_hf(struct sip_msg* msg, int stale, str* realm,
		str* nonce, str* algorithm, struct qp* qop, int hftype, str *ahf)
{
    char *p;
    str* hfn, hf;
    int nonce_len, l, cfg;
	int t;
#if defined USE_NC || defined USE_OT_NONCE
	unsigned int n_id;
	unsigned char pool;
	unsigned char pool_flags;
#endif

	if(!ahf)
	{
		LM_ERR("invalid output parameter\n");
		return -1;
	}

	strip_realm(realm);
    if (realm) {
        DEBUG("build_challenge_hf: realm='%.*s'\n", realm->len, realm->s);
    }
    if (nonce) {
        DEBUG("build_challenge_hf: nonce='%.*s'\n", nonce->len, nonce->s);
    }
    if (algorithm) {
        DEBUG("build_challenge_hf: algorithm='%.*s'\n", algorithm->len,
				algorithm->s);
    }
    if (qop && qop->qop_parsed != QOP_UNSPEC) {
        DEBUG("build_challenge_hf: qop='%.*s'\n", qop->qop_str.len,
				qop->qop_str.s);
    }

    if (hftype == HDR_PROXYAUTH_T) {
		hfn = &proxy_challenge_header;
    } else {
		hfn = &www_challenge_header;
    }
    
	cfg = get_auth_checks(msg);

    nonce_len = get_nonce_len(cfg, nc_enabled || otn_enabled);

    hf.len = hfn->len;
    if (realm) {
		hf.len += DIGEST_REALM_LEN
		+ realm->len; 
    }

    hf.len += DIGEST_NONCE_LEN;

    if (nonce) {
    	hf.len += nonce->len
    	          + 1; /* '"' */
    }
    else {
    	hf.len += nonce_len
    	          + 1; /* '"' */
    }
	hf.len += ((stale) ? STALE_PARAM_LEN : 0);
    if (algorithm) {
    	hf.len += DIGEST_ALGORITHM_LEN + algorithm->len;
    }
    else {
    	hf.len += 0
#ifdef _PRINT_MD5
	+DIGEST_MD5_LEN
#endif
		;
    }
    
    if (qop && qop->qop_parsed != QOP_UNSPEC) {
		hf.len += QOP_PARAM_START_LEN + qop->qop_str.len + QOP_PARAM_END_LEN;
    }
	hf.len += CRLF_LEN;
    p = hf.s = pkg_malloc(hf.len);
    if (!hf.s) {
		ERR("auth: No memory left (%d bytes)\n", hf.len);
		return -1;
    }

    memcpy(p, hfn->s, hfn->len); p += hfn->len;

    if(realm){
	memcpy(p, DIGEST_REALM, DIGEST_REALM_LEN); p += DIGEST_REALM_LEN;
	memcpy(p, realm->s, realm->len); p += realm->len;
    }

    memcpy(p, DIGEST_NONCE, DIGEST_NONCE_LEN); p += DIGEST_NONCE_LEN;
    if (nonce) {
        memcpy(p, nonce->s, nonce->len); p += nonce->len;
    }
    else {
        l=nonce_len;
		t=time(0);
#if defined USE_NC || defined USE_OT_NONCE
		if (nc_enabled || otn_enabled){
			pool=nid_get_pool();
			n_id=nid_inc(pool);
			pool_flags=0;
#ifdef USE_NC
			if (nc_enabled){
				nc_new(n_id, pool);
				pool_flags|=  NF_VALID_NC_ID;
			}
#endif
#ifdef USE_OT_NONCE
			if (otn_enabled){
				otn_new(n_id, pool);
				pool_flags|= NF_VALID_OT_ID;
			}
#endif
		}else{
			pool=0;
			pool_flags=0;
			n_id=0;
		}
		if (calc_nonce(p, &l, cfg, t, t + nonce_expire, n_id,
						pool | pool_flags,
						&secret1, &secret2, msg) != 0)
#else  /* USE_NC || USE_OT_NONCE*/
		if (calc_nonce(p, &l, cfg, t, t + nonce_expire, 
						&secret1, &secret2, msg) != 0) 
#endif /* USE_NC || USE_OT_NONCE */
		{
            ERR("auth: calc_nonce failed (len %d, needed %d)\n",
                 nonce_len, l);
            pkg_free(hf.s);
            return -1;
        }
        p += l;
    }
    *p = '"'; p++;

    if (qop && qop->qop_parsed != QOP_UNSPEC) {
		memcpy(p, QOP_PARAM_START, QOP_PARAM_START_LEN);
		p += QOP_PARAM_START_LEN;
		memcpy(p, qop->qop_str.s, qop->qop_str.len);
		p += qop->qop_str.len;
		memcpy(p, QOP_PARAM_END, QOP_PARAM_END_LEN);
		p += QOP_PARAM_END_LEN;
    }
    if (stale) {
		memcpy(p, STALE_PARAM, STALE_PARAM_LEN);
		p += STALE_PARAM_LEN;
    }
	if (algorithm) {
		memcpy(p, DIGEST_ALGORITHM, DIGEST_ALGORITHM_LEN);
		p += DIGEST_ALGORITHM_LEN;
		memcpy(p, algorithm->s, algorithm->len);
		p += algorithm->len;
	}
	else {
#ifdef _PRINT_MD5
    memcpy(p, DIGEST_MD5, DIGEST_MD5_LEN ); p += DIGEST_MD5_LEN;
#endif
	}
    memcpy(p, CRLF, CRLF_LEN); p += CRLF_LEN;
	hf.len=(int)(p-hf.s); /* fix len, it might be smaller due to a smaller
							 nonce */
    
    DBG("auth: '%.*s'\n", hf.len, ZSW(hf.s));
	*ahf = hf;
    return 0;
}

/**
 * Create {WWW,Proxy}-Authenticate header field
 * @param nonce nonce value
 * @param algorithm algorithm value
 * @return -1 on error, 0 on success
 *
 * The result is stored in an attribute.
 * If nonce is not null that it is used, instead of call calc_nonce.
 * If algorithm is not null that it is used irrespective of _PRINT_MD5
 * The value of 'qop' module parameter is used.
 *
 * Major usage of nonce and algorithm params is AKA authentication.
 */
int build_challenge_hf(struct sip_msg* msg, int stale, str* realm,
		str* nonce, str* algorithm, int hftype)
{
    str hf = {0, 0};
    avp_value_t val;
	int ret;

	ret = get_challenge_hf(msg, stale, realm, nonce, algorithm, &auth_qop,
				hftype, &hf);
	if(ret < 0)
		return ret;

	val.s = hf;
    if(add_avp(challenge_avpid.flags | AVP_VAL_STR, challenge_avpid.name, val)
			< 0) {
		ERR("auth: Error while creating attribute with challenge\n");
		pkg_free(hf.s);
		return -1;
    }
	pkg_free(hf.s);
	return 0;
}
