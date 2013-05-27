/*
 * $Id$
 *
 * Copyright (C) 2013 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <openssl/hmac.h>

#include "../../basex.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../str.h"
#include "../../ut.h"
#include "../../parser/digest/digest.h"
#include "../../parser/hf.h"
#include "../../mod_fix.h"

#include "autheph_mod.h"
#include "authorize.h"

#if !defined(SHA_DIGEST_LENGTH)
#define SHA_DIGEST_LENGTH (20)
#endif

static inline int get_ha1(struct username* _username, str* _domain,
				str* _secret, char* _ha1)
{
	unsigned int hmac_len = SHA_DIGEST_LENGTH;
	unsigned char hmac_sha1[hmac_len];
	unsigned char password[base64_enc_len(hmac_len)];
	str spassword;

	if (HMAC(EVP_sha1(), _secret->s, _secret->len,
			(unsigned char *) _username->whole.s,
			_username->whole.len, hmac_sha1, &hmac_len) == NULL) {
		LM_ERR("HMAC-SHA1 failed\n");
		return -1;
	}

	spassword.len = base64_enc(hmac_sha1, hmac_len, password,
					base64_enc_len(hmac_len));
	spassword.s = (char *) password;
	LM_DBG("calculated password: %.*s\n", spassword.len, spassword.s);

	eph_auth_api.calc_HA1(HA_MD5, &_username->whole, _domain, &spassword,
				0, 0, _ha1);
	LM_DBG("calculated HA1: %s\n", _ha1);

	return 0;
}

static int do_auth(struct sip_msg* msg, struct hdr_field *h, str *realm,
			str *method, str* secret)
{
	int ret;
	char ha1[256];
	auth_body_t *cred = (auth_body_t*) h->parsed;

	LM_DBG("secret: %.*s\n", secret->len, secret->s);

	ret = get_ha1(&cred->digest.username, realm, secret, ha1);
	if (ret < 0)
	{
		return AUTH_ERROR;
	}

	ret = eph_auth_api.check_response(&(cred->digest), method, ha1);
	if (ret == AUTHENTICATED)
	{
		if (eph_auth_api.post_auth(msg, h) != AUTHENTICATED)
		{
			return AUTH_ERROR;
		}
	}
	else if (ret == NOT_AUTHENTICATED)
	{
		return AUTH_INVALID_PASSWORD;
	}
	else
	{
		ret = AUTH_ERROR;
	}

	return AUTH_OK;
}

static int verify_timestamp(str* username)
{
	int pos = 0, cur_time = (int) time(NULL);
	unsigned int expires;
	str time_str = {0, 0};

	while (pos < username->len && username->s[pos] != ':')
		pos++;

	if (pos < username->len - 1)
	{
		time_str.s = username->s + pos + 1;
		time_str.len = username->len - pos - 1;
	}
	else
	{
		time_str.s = username->s;
		time_str.len = username->len;
	}

	LM_DBG("username timestamp: %.*s\n", time_str.len, time_str.s);
	if (str2int(&time_str, &expires) < 0)
	{
		LM_ERR("unable to convert timestamp to int\n");
		return -1;
	}

	LM_DBG("current time: %d\n", cur_time);
	if (cur_time > expires)
	{
		LM_WARN("username has expired\n");
		return -1;
	}

	return 0;
}

static int digest_authenticate(struct sip_msg* msg, str *realm,
				hdr_types_t hftype, str *method)
{
	struct hdr_field* h;
	int ret;
	struct secret *secret_struct = secret_list;
	str username;

	LM_DBG("realm: %.*s\n", realm->len, realm->s);
	LM_DBG("method: %.*s\n", method->len, method->s);

	ret = eph_auth_api.pre_auth(msg, realm, hftype, &h, NULL);
	switch(ret) {
		case NONCE_REUSED:
			LM_DBG("nonce reused\n");
			return AUTH_NONCE_REUSED;
		case STALE_NONCE:
			LM_DBG("stale nonce\n");
			return AUTH_STALE_NONCE;
		case NO_CREDENTIALS:
			LM_DBG("no credentials\n");
			return AUTH_NO_CREDENTIALS;
		case ERROR:
		case BAD_CREDENTIALS:
			LM_DBG("error or bad credentials\n");
			return AUTH_ERROR;
		case CREATE_CHALLENGE:
			LM_ERR("CREATE_CHALLENGE is not a valid state\n");
			return AUTH_ERROR;
		case DO_RESYNCHRONIZATION:
			LM_ERR("DO_RESYNCHRONIZATION is not a valid state\n");
			return AUTH_ERROR;
		case NOT_AUTHENTICATED:
			LM_DBG("not authenticated\n");
			return AUTH_ERROR;
		case DO_AUTHENTICATION:
			break;
		case AUTHENTICATED:
			return AUTH_OK;
	}

	username = ((auth_body_t *) h->parsed)->digest.username.whole;
	LM_DBG("username: %.*s\n", username.len, username.s);

	if (verify_timestamp(&username) < 0)
	{
		LM_ERR("invalid timestamp in username\n");
		return AUTH_ERROR;
	}

	while (secret_struct != NULL)
	{
		ret = do_auth(msg, h, realm, method,
				&secret_struct->secret_key);
		if (ret == AUTH_OK)
		{
			break;
		}
		secret_struct = secret_struct->next;
	}

	return ret;
}

int autheph_check(struct sip_msg* _m, char* _realm)
{
	str srealm;

	if ((_m->REQ_METHOD == METHOD_ACK) || (_m->REQ_METHOD == METHOD_CANCEL))
	{
		return AUTH_OK;
	}

	if(_m==NULL || _realm==NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len==0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	if(_m->REQ_METHOD==METHOD_REGISTER)
		return digest_authenticate(_m, &srealm, HDR_AUTHORIZATION_T,
					&_m->first_line.u.request.method);
	else
		return digest_authenticate(_m, &srealm, HDR_PROXYAUTH_T,
					&_m->first_line.u.request.method);
}

int autheph_www(struct sip_msg* _m, char* _realm)
{
	str srealm;

	if ((_m->REQ_METHOD == METHOD_ACK) || (_m->REQ_METHOD == METHOD_CANCEL))
	{
		return AUTH_OK;
	}

	if(_m==NULL || _realm==NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len==0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, &srealm, HDR_AUTHORIZATION_T,
					&_m->first_line.u.request.method);
}

int autheph_www2(struct sip_msg* _m, char* _realm, char *_method)
{
	str srealm;
	str smethod;

	if ((_m->REQ_METHOD == METHOD_ACK) || (_m->REQ_METHOD == METHOD_CANCEL))
	{
		return AUTH_OK;
	}

	if(_m==NULL || _realm==NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len==0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&smethod, _m, (fparam_t*)_method) < 0)
	{
		LM_ERR("failed to get method value\n");
		return AUTH_ERROR;
	}

	if (smethod.len == 0)
	{
		LM_ERR("invalid method value - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, &srealm, HDR_AUTHORIZATION_T, &smethod);
}

int autheph_proxy(struct sip_msg* _m, char* _realm)
{
	str srealm;

	if ((_m->REQ_METHOD == METHOD_ACK) || (_m->REQ_METHOD == METHOD_CANCEL))
	{
		return AUTH_OK;
	}

	if(_m==NULL || _realm==NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len==0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, &srealm, HDR_PROXYAUTH_T,
					&_m->first_line.u.request.method);
}
