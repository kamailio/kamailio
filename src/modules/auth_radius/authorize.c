/*
 * Digest Authentication - Radius support
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2010 Juha Heinanen
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


#include <string.h>
#include <stdlib.h>
#include "../../core/mem/mem.h"
#include "../../core/str.h"
#include "../../core/parser/hf.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../modules/auth/api.h"
#include "authorize.h"
#include "sterman.h"
#include "auth_radius.h"


/* 
 * Extract URI depending on the request from To or From header 
 */
static inline int get_uri_user(struct sip_msg *_m, str **_uri_user)
{
	struct sip_uri *puri;

	if((REQ_LINE(_m).method.len == 8)
			&& (memcmp(REQ_LINE(_m).method.s, "REGISTER", 8) == 0)) {
		if((puri = parse_to_uri(_m)) == NULL) {
			LM_ERR("failed to parse To header\n");
			return -1;
		}
	} else {
		if((puri = parse_from_uri(_m)) == NULL) {
			LM_ERR("parsing From header\n");
			return -1;
		}
	}

	*_uri_user = &(puri->user);

	return 0;
}


/*
 * Authorize digest credentials
 */
static int ki_authorize(
		sip_msg_t *_msg, str *srealm, str *suser, hdr_types_t _hftype)
{
	int res;
	auth_cfg_result_t ret;
	struct hdr_field *h;
	auth_body_t *cred;
	str *uri_user;
	str user = STR_NULL, domain;

	cred = 0;
	ret = -1;
	user.s = 0;

	/* get pre_auth domain from _realm pvar (if exists) */
	if(srealm) {
		domain = *srealm;
	} else {
		domain.len = 0;
		domain.s = 0;
	}

	switch(auth_api.pre_auth(_msg, &domain, _hftype, &h, NULL)) {
		default:
			BUG("unexpected reply '%d'.\n",
					auth_api.pre_auth(_msg, &domain, _hftype, &h, NULL));
#ifdef EXTRA_DEBUG
			abort();
#endif
			ret = -7;
			goto end;

		case NONCE_REUSED:
			ret = AUTH_NONCE_REUSED;
			goto end;

		case STALE_NONCE:
			ret = AUTH_STALE_NONCE;
			goto end;

		case ERROR:
		case BAD_CREDENTIALS:
		case NOT_AUTHENTICATED:
			ret = AUTH_ERROR;
			goto end;

		case NO_CREDENTIALS:
			ret = AUTH_NO_CREDENTIALS;
			goto end;

		case DO_AUTHENTICATION:
			break;

		case AUTHENTICATED:
			ret = AUTH_OK;
			goto end;
	}

	cred = (auth_body_t *)h->parsed;

	/* get uri_user from _uri_user pvap (if exists) or
       from To/From URI */
	if(suser != NULL && suser->len > 0) {
		res = radius_authorize_sterman(
				_msg, &cred->digest, &_msg->first_line.u.request.method, suser);
	} else {
		if(get_uri_user(_msg, &uri_user) < 0) {
			LM_ERR("To/From URI not found\n");
			ret = AUTH_ERROR;
			;
			goto end;
		}
		user.s = (char *)pkg_malloc(uri_user->len);
		if(user.s == NULL) {
			LM_ERR("no pkg memory left for user\n");
			ret = -7;
			goto end;
		}
		un_escape(uri_user, &user);
		res = radius_authorize_sterman(
				_msg, &cred->digest, &_msg->first_line.u.request.method, &user);
	}

	if(res == 1) {
		switch(auth_api.post_auth(_msg, h, NULL)) {
			default:
				BUG("unexpected reply '%d'.\n",
						auth_api.pre_auth(_msg, &domain, _hftype, &h, NULL));
#ifdef EXTRA_DEBUG
				abort();
#endif
				ret = -7;
				break;
			case ERROR:
			case NOT_AUTHENTICATED:
				ret = AUTH_ERROR;
				break;
			case AUTHENTICATED:
				ret = AUTH_OK;
				break;
		}
	} else {
		ret = AUTH_INVALID_PASSWORD;
	}

end:
	if(user.s)
		pkg_free(user.s);
	if(ret < 0) {
		if(auth_api.build_challenge(
				   _msg, (cred ? cred->stale : 0), &domain, NULL, NULL, _hftype)
				< 0) {
			LM_ERR("while creating challenge\n");
			ret = -7;
		}
	}
	return ret;
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg *_msg, gparam_t *_realm,
		gparam_t *_uri_user, hdr_types_t _hftype)
{
	str srealm = STR_NULL;
	str suser = STR_NULL;

	/* get pre_auth domain from _realm param (if exists) */
	if(_realm) {
		if(fixup_get_svalue(_msg, _realm, &srealm) < 0) {
			LM_ERR("failed to get realm value\n");
			return -5;
		}
	}
	if(_uri_user) {
		if(fixup_get_svalue(_msg, _uri_user, &suser) < 0) {
			LM_ERR("cannot get uri user value\n");
			return AUTH_ERROR;
		}
	}
	return ki_authorize(_msg, &srealm, &suser, _hftype);
}

/**
 *
 */
int ki_radius_proxy_authorize(sip_msg_t *msg, str *srealm)
{
	return ki_authorize(msg, srealm, NULL, HDR_PROXYAUTH_T);
}

/**
 *
 */
int ki_radius_proxy_authorize_user(sip_msg_t *msg, str *srealm, str *suser)
{
	return ki_authorize(msg, srealm, suser, HDR_PROXYAUTH_T);
}

/**
 *
 */
int ki_radius_www_authorize(sip_msg_t *msg, str *srealm)
{
	return ki_authorize(msg, srealm, NULL, HDR_AUTHORIZATION_T);
}

/**
 *
 */
int ki_radius_www_authorize_user(sip_msg_t *msg, str *srealm, str *suser)
{
	return ki_authorize(msg, srealm, suser, HDR_AUTHORIZATION_T);
}

/*
 * Authorize using Proxy-Authorize header field (no URI user parameter given)
 */
int radius_proxy_authorize_1(struct sip_msg *_msg, char *_realm, char *_s2)
{
	/* realm parameter is converted in fixup */
	return authorize(_msg, (gparam_t *)_realm, (gparam_t *)0, HDR_PROXYAUTH_T);
}


/*
 * Authorize using Proxy-Authorize header field (URI user parameter given)
 */
int radius_proxy_authorize_2(
		struct sip_msg *_msg, char *_realm, char *_uri_user)
{
	return authorize(
			_msg, (gparam_t *)_realm, (gparam_t *)_uri_user, HDR_PROXYAUTH_T);
}


/*
 * Authorize using WWW-Authorize header field (no URI user parameter given)
 */
int radius_www_authorize_1(struct sip_msg *_msg, char *_realm, char *_s2)
{
	return authorize(
			_msg, (gparam_t *)_realm, (gparam_t *)0, HDR_AUTHORIZATION_T);
}


/*
 * Authorize using WWW-Authorize header field (URI user parameter given)
 */
int radius_www_authorize_2(struct sip_msg *_msg, char *_realm, char *_uri_user)
{
	return authorize(_msg, (gparam_t *)_realm, (gparam_t *)_uri_user,
			HDR_AUTHORIZATION_T);
}
