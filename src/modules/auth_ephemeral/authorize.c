/*
 * $Id$
 *
 * Copyright (C) 2013 Crocodile RCS Ltd
 * Copyright (C) 2017 ng-voice GmbH
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

#include "../../core/basex.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/str.h"
#include "../../core/ut.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/hf.h"
#include "../../core/mod_fix.h"

#include "auth_ephemeral_mod.h"
#include "authorize.h"

static inline int get_pass(str *_username, str *_secret, str *_password)
{
	unsigned int hmac_len = SHA_DIGEST_LENGTH;
	unsigned char hmac_sha1[512];

	switch(autheph_sha_alg) {
		case AUTHEPH_SHA1:
			hmac_len = SHA_DIGEST_LENGTH;
			if (HMAC(EVP_sha1(), _secret->s, _secret->len,
					(unsigned char *) _username->s,
					_username->len, hmac_sha1, &hmac_len) == NULL)
			{
				LM_ERR("HMAC-SHA1 failed\n");
				return -1;
			}
			break;
		case AUTHEPH_SHA256:
			hmac_len = SHA256_DIGEST_LENGTH;
			if (HMAC(EVP_sha256(), _secret->s, _secret->len,
					(unsigned char *) _username->s,
					_username->len, hmac_sha1, &hmac_len) == NULL)
			{
				LM_ERR("HMAC-SHA256 failed\n");
				return -1;
			}
			break;
		case AUTHEPH_SHA384:
			hmac_len = SHA384_DIGEST_LENGTH;
			if (HMAC(EVP_sha384(), _secret->s, _secret->len,
					(unsigned char *) _username->s,
					_username->len, hmac_sha1, &hmac_len) == NULL)
			{
				LM_ERR("HMAC-SHA384 failed\n");
				return -1;
			}
			break;
		case AUTHEPH_SHA512:
			hmac_len = SHA512_DIGEST_LENGTH;
			if (HMAC(EVP_sha512(), _secret->s, _secret->len,
					(unsigned char *) _username->s,
					_username->len, hmac_sha1, &hmac_len) == NULL)
			{
				LM_ERR("HMAC-SHA512 failed\n");
				return -1;
			}
			break;
		default:
			LM_ERR("Invalid SHA Algorithm\n");
			return -1;

	}

	LM_DBG("HMAC-Len (%i)\n", hmac_len);


	_password->len = base64_enc(hmac_sha1, hmac_len,
					(unsigned char *) _password->s,
					base64_enc_len(hmac_len));
	LM_DBG("calculated password: %.*s (%i)\n", _password->len, _password->s, _password->len);

	return 0;
}

static inline int get_ha1(struct username *_username, str *_domain,
				str *_secret, char *_ha1)
{
	char password[base64_enc_len(SHA512_DIGEST_LENGTH)];
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
	auth_result_t ret;
	char ha1[512];
	auth_body_t *cred = (auth_body_t*) _h->parsed;

	LM_DBG("secret: %.*s (%i)\n", _secret->len, _secret->s, _secret->len);

	if (get_ha1(&cred->digest.username, _realm, _secret, ha1) < 0)
	{
		LM_ERR("calculating HA1\n");
		return AUTH_ERROR;
	}

	LM_DBG("HA1: %i\n", (int)strlen(ha1));
	
	ret = eph_auth_api.check_response(&cred->digest, _method, ha1);
	if (ret == AUTHENTICATED)
	{
		if (eph_auth_api.post_auth(_m, _h, ha1) != AUTHENTICATED) {
			return AUTH_ERROR;
		}
		return AUTH_OK;
	} else if (ret == NOT_AUTHENTICATED) {
		return AUTH_INVALID_PASSWORD;
	} else {
		return AUTH_ERROR;
	}
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
		return AUTH_USERNAME_EXPIRED;
	}

	return 0;
}

static inline int digest_authenticate(struct sip_msg *_m, str *_realm,
				hdr_types_t _hftype, str *_method)
{
	struct hdr_field* h;
	auth_cfg_result_t ret = AUTH_ERROR;
	auth_result_t rauth;
	struct secret *secret_struct;
	str username;

	LM_DBG("realm: %.*s\n", _realm->len, _realm->s);
	LM_DBG("method: %.*s\n", _method->len, _method->s);

	rauth = eph_auth_api.pre_auth(_m, _realm, _hftype, &h, NULL);
	switch(rauth)
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

	int res = autheph_verify_timestamp(&username);
	if (res < 0)
	{
		if (res == -1)
		{
			LM_ERR("invalid timestamp in username\n");
			return AUTH_ERROR;
		} else {
			return AUTH_USERNAME_EXPIRED;
		}
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

int ki_autheph_check(sip_msg_t *_m, str *srealm)
{

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

	if (srealm->len == 0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (_m->REQ_METHOD == METHOD_REGISTER)
	{
		return digest_authenticate(_m, srealm, HDR_AUTHORIZATION_T,
					&_m->first_line.u.request.method);
	}
	else
	{
		return digest_authenticate(_m, srealm, HDR_PROXYAUTH_T,
					&_m->first_line.u.request.method);
	}
}

int autheph_check(struct sip_msg *_m, char *_realm, char *_p2)
{
	str srealm;

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

	return ki_autheph_check(_m, &srealm);
}

int ki_autheph_www(sip_msg_t *_m, str *srealm)
{
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

	if (srealm->len == 0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, srealm, HDR_AUTHORIZATION_T,
					&_m->first_line.u.request.method);
}

int autheph_www(struct sip_msg *_m, char *_realm, char *_p2)
{
	str srealm;

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

	return ki_autheph_www(_m, &srealm);
}

int ki_autheph_www_method(sip_msg_t *_m, str *srealm, str *smethod)
{
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

	if (srealm->len == 0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (smethod->len == 0)
	{
		LM_ERR("invalid method value - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, srealm, HDR_AUTHORIZATION_T, smethod);
}

int autheph_www2(struct sip_msg *_m, char *_realm, char *_method)
{
	str srealm;
	str smethod;

	if(_m == NULL || _realm == NULL || _method == NULL)
	{
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, _m, (fparam_t*)_realm) < 0)
	{
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&smethod, _m, (fparam_t*)_method) < 0)
	{
		LM_ERR("failed to get method value\n");
		return AUTH_ERROR;
	}

	return ki_autheph_www_method(_m, &srealm, &smethod);
}

int ki_autheph_proxy(sip_msg_t *_m, str *srealm)
{
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

	if (srealm->len == 0)
	{
		LM_ERR("invalid realm parameter - empty value\n");
		return AUTH_ERROR;
	}

	return digest_authenticate(_m, srealm, HDR_PROXYAUTH_T,
					&_m->first_line.u.request.method);
}

int autheph_proxy(struct sip_msg *_m, char *_realm, char *_p2)
{
	str srealm;

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

	return ki_autheph_proxy(_m, &srealm);
}

int ki_autheph_authenticate(sip_msg_t *_m, str *susername, str *spassword)
{
	unsigned int hmac_len = SHA_DIGEST_LENGTH;
	switch(autheph_sha_alg) {
		case AUTHEPH_SHA1:
			hmac_len = SHA_DIGEST_LENGTH;
			break;
		case AUTHEPH_SHA256:
			hmac_len = SHA256_DIGEST_LENGTH;
			break;
		case AUTHEPH_SHA384:
			hmac_len = SHA384_DIGEST_LENGTH;
			break;
		case AUTHEPH_SHA512:
			hmac_len = SHA512_DIGEST_LENGTH;
			break;
		default:
			LM_ERR("Invalid SHA Algorithm\n");
			return AUTH_ERROR;
	}

	char generated_password[base64_enc_len(hmac_len)];
	str sgenerated_password;
	struct secret *secret_struct;

	if (susername->len == 0)
	{
		LM_ERR("invalid username parameter - empty value\n");
		return AUTH_ERROR;
	}

	if (spassword->len == 0)
	{
		LM_ERR("invalid password parameter - empty value\n");
		return AUTH_ERROR;
	}

	int res = autheph_verify_timestamp(susername);
	if (res < 0)
	{
		if (res == -1)
		{
			LM_ERR("invalid timestamp in username\n");
			return AUTH_ERROR;
		} else {
			return AUTH_USERNAME_EXPIRED;
		}
	}

	LM_DBG("username: %.*s\n", susername->len, susername->s);
	LM_DBG("password: %.*s\n", spassword->len, spassword->s);

	sgenerated_password.s = generated_password;
	SECRET_LOCK;
	secret_struct = secret_list;
	while (secret_struct != NULL)
	{
		LM_DBG("trying secret: %.*s (%i)\n",
			secret_struct->secret_key.len,
			secret_struct->secret_key.s,
			secret_struct->secret_key.len);
		if (get_pass(susername, &secret_struct->secret_key,
				&sgenerated_password) == 0)
		{
			LM_DBG("generated password: %.*s (%i)\n", 
				sgenerated_password.len,
				sgenerated_password.s,
				sgenerated_password.len);
			if (spassword->len == sgenerated_password.len
					&& strncmp(spassword->s, sgenerated_password.s,
						spassword->len) == 0)
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

int autheph_authenticate(struct sip_msg *_m, char *_username, char *_password)
{
	str susername, spassword;

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

	if (get_str_fparam(&spassword, _m, (fparam_t*)_password) < 0)
	{
		LM_ERR("failed to get password value\n");
		return AUTH_ERROR;
	}

	return ki_autheph_authenticate(_m, &susername, &spassword);
}
