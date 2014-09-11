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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */
#include <openssl/hmac.h>
#include <openssl/sha.h>

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

static inline int get_pass(str *_username, str *_secret, str *_password)
{
	unsigned int hmac_len = SHA_DIGEST_LENGTH;
	unsigned char hmac_sha1[hmac_len];

	if (HMAC(EVP_sha1(), _secret->s, _secret->len,
			(unsigned char *) _username->s,
			_username->len, hmac_sha1, &hmac_len) == NULL)
	{
		LM_ERR("HMAC-SHA1 failed\n");
		return -1;
	}

	_password->len = base64_enc(hmac_sha1, hmac_len,
					(unsigned char *) _password->s,
					base64_enc_len(hmac_len));
	LM_DBG("calculated password: %.*s\n", _password->len, _password->s);

	return 0;
}

static inline int get_ha1(struct username *_username, str *_domain,
				str *_secret, char *_ha1)
{
	char password[base64_enc_len(SHA_DIGEST_LENGTH)];
	str spassword;

	spassword.s = (char *) password;
	spassword.len = 0;

	if (get_pass(&_username->whole, _secret, &spassword) < 0)
	{
		LM_ERR("calculating password\n");
		return -1;
	}

	eph_auth_api.calc_HA1(HA_MD5, &_username->whole, _domain, &spassword,
				0, 0, _ha1);
	LM_DBG("calculated HA1: %s\n", _ha1);

	return 0;
}

static inline int do_auth(struct sip_msg *_m, struct hdr_field *_h, str *_realm,
			str *_method, str *_secret)
{
	int ret;
	char ha1[256];
	auth_body_t *cred = (auth_body_t*) _h->parsed;

	LM_DBG("secret: %.*s\n", _secret->len, _secret->s);

	if (get_ha1(&cred->digest.username, _realm, _secret, ha1) < 0)
	{
		LM_ERR("calculating HA1\n");
		return AUTH_ERROR;
	}

	ret = eph_auth_api.check_response(&cred->digest, _method, ha1);
	if (ret == AUTHENTICATED)
	{
		if (eph_auth_api.post_auth(_m, _h) != AUTHENTICATED)
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

int autheph_verify_timestamp(str *_username)
{
	int pos = 0, cur_time = (int) time(NULL);
	unsigned int expires;
	str time_str = {0, 0};

	while (pos < _username->len && _username->s[pos] != ':')
		pos++;

	if (autheph_username_format == AUTHEPH_USERNAME_NON_IETF)
	{
		if (pos < _username->len - 1)
		{
			time_str.s = _username->s + pos + 1;
			time_str.len = _username->len - pos - 1;
		}
		else
		{
			time_str.s = _username->s;
			time_str.len = _username->len;
		}
	}
	else
	{
		time_str.s = _username->s;
		if (pos < _username->len - 1)
		{
			time_str.len = pos;
		}
		else
		{
			time_str.len = _username->len;
		}
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

static inline int digest_authenticate(struct sip_msg *_m, str *_realm,
				hdr_types_t _hftype, str *_method)
{
	struct hdr_field* h;
	int ret;
	struct secret *secret_struct;
	str username;

	LM_DBG("realm: %.*s\n", _realm->len, _realm->s);
	LM_DBG("method: %.*s\n", _method->len, _method->s);

	ret = eph_auth_api.pre_auth(_m, _realm, _hftype, &h, NULL);
	switch(ret)
	{
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

	if (autheph_verify_timestamp(&username) < 0)
	{
		LM_ERR("invalid timestamp in username\n");
		return AUTH_ERROR;
	}

	SECRET_LOCK;
	secret_struct = secret_list;
	while (secret_struct != NULL)
	{
		ret = do_auth(_m, h, _realm, _method,
				&secret_struct->secret_key);
		if (ret == AUTH_OK)
		{
			break;
		}
		secret_struct = secret_struct->next;
	}
	SECRET_UNLOCK;

	return ret;
}

int autheph_check(struct sip_msg *_m, char *_realm)
{
	str srealm;

	if (eph_auth_api.pre_auth == NULL)
	{
		LM_ERR("autheph_check() cannot be used without the auth "
			"module\n");
		return AUTH_ERROR;
	}

	if (_m->REQ_METHOD == METHOD_ACK || _m->REQ_METHOD == METHOD_CANCEL)
	{
		return AUTH_OK;
	}

	if(_m == NULL || _realm == NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len == 0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (_m->REQ_METHOD == METHOD_REGISTER)
	{
		return digest_authenticate(_m, &srealm, HDR_AUTHORIZATION_T,
					&_m->first_line.u.request.method);
	}
	else
	{
		return digest_authenticate(_m, &srealm, HDR_PROXYAUTH_T,
					&_m->first_line.u.request.method);
	}
}

int autheph_www(struct sip_msg *_m, char *_realm)
{
	str srealm;

	if (eph_auth_api.pre_auth == NULL)
	{
		LM_ERR("autheph_www() cannot be used without the auth "
			"module\n");
		return AUTH_ERROR;
	}

	if (_m->REQ_METHOD == METHOD_ACK || _m->REQ_METHOD == METHOD_CANCEL)
	{
		return AUTH_OK;
	}

	if(_m == NULL || _realm == NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len == 0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, &srealm, HDR_AUTHORIZATION_T,
					&_m->first_line.u.request.method);
}

int autheph_www2(struct sip_msg *_m, char *_realm, char *_method)
{
	str srealm;
	str smethod;

	if (eph_auth_api.pre_auth == NULL)
	{
		LM_ERR("autheph_www() cannot be used without the auth "
			"module\n");
		return AUTH_ERROR;
	}

	if (_m->REQ_METHOD == METHOD_ACK || _m->REQ_METHOD == METHOD_CANCEL)
	{
		return AUTH_OK;
	}

	if(_m == NULL || _realm == NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len == 0)
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

int autheph_proxy(struct sip_msg *_m, char *_realm)
{
	str srealm;

	if (eph_auth_api.pre_auth == NULL)
	{
		LM_ERR("autheph_proxy() cannot be used without the auth "
			"module\n");
		return AUTH_ERROR;
	}

	if (_m->REQ_METHOD == METHOD_ACK || _m->REQ_METHOD == METHOD_CANCEL)
	{
		return AUTH_OK;
	}

	if(_m == NULL || _realm == NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (srealm.len == 0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, &srealm, HDR_PROXYAUTH_T,
					&_m->first_line.u.request.method);
}

int autheph_authenticate(struct sip_msg *_m, char *_username, char *_password)
{
	str susername, spassword;
	char generated_password[base64_enc_len(SHA_DIGEST_LENGTH)];
	str sgenerated_password;
	struct secret *secret_struct;

	if (_m == NULL || _username == NULL || _password == NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&susername, _m, (fparam_t*)_username) < 0)
	{
		LM_ERR("failed to get username value\n");
		return AUTH_ERROR;
	}

	if (susername.len == 0)
	{
		LM_ERR("invalid username parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&spassword, _m, (fparam_t*)_password) < 0)
	{
		LM_ERR("failed to get password value\n");
		return AUTH_ERROR;
	}

	if (spassword.len == 0)
	{
		LM_ERR("invalid password parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (autheph_verify_timestamp(&susername) < 0)
	{
		LM_ERR("invalid timestamp in username\n");
		return AUTH_ERROR;
	}

	LM_DBG("username: %.*s\n", susername.len, susername.s);
	LM_DBG("password: %.*s\n", spassword.len, spassword.s);

	sgenerated_password.s = generated_password;
	SECRET_LOCK;
	secret_struct = secret_list;
	while (secret_struct != NULL)
	{
		LM_DBG("trying secret: %.*s\n",
			secret_struct->secret_key.len,
			secret_struct->secret_key.s);
		if (get_pass(&susername, &secret_struct->secret_key,
				&sgenerated_password) == 0)
		{
			LM_DBG("generated password: %.*s\n",
				sgenerated_password.len, sgenerated_password.s);
			if (strncmp(spassword.s, sgenerated_password.s,
					spassword.len) == 0)
			{
				SECRET_UNLOCK;
				return AUTH_OK;
			}
		}
		secret_struct = secret_struct->next;
	}
	SECRET_UNLOCK;

	return AUTH_ERROR;
}
