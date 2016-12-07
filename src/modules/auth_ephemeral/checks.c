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
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../str.h"
#include "../../ut.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../mod_fix.h"

#include "autheph_mod.h"
#include "authorize.h"
#include "checks.h"

static inline int check_username(str *_username, struct sip_uri *_uri)
{
	str uname, domain = {0, 0};
	int pos = 0;

	if (_username == NULL || _username->len == 0)
	{
		LM_ERR("invalid username\n");
		return CHECK_ERROR;
	}

	while (pos < _username->len && _username->s[pos] != ':')
		pos++;

	if (pos < _username->len - 1)
	{
		if (autheph_username_format == AUTHEPH_USERNAME_NON_IETF)
		{
			uname.s = _username->s;
			uname.len = pos;
		}
		else
		{
			uname.s = _username->s + pos + 1;
			uname.len = _username->len - pos - 1;
		}
	}
	else
	{
		return CHECK_NO_USER;
	}

	pos = 0;
	while (pos < uname.len && uname.s[pos] != '@')
		pos++;

	if (pos < uname.len - 1)
	{
		domain.s = uname.s + pos + 1;
		domain.len = uname.len - pos - 1;
		uname.len = pos;
	}

	if (uname.len == _uri->user.len
		&& strncmp(uname.s, _uri->user.s, uname.len) == 0)
	{
		if (domain.len == 0)
		{
			return CHECK_OK;
		}
		else if (domain.len == _uri->host.len
			&& strncmp(domain.s, _uri->host.s, domain.len) == 0)
		{
			return CHECK_OK;
		}
	}

	return CHECK_ERROR;
}

static inline int check_from(struct sip_msg *_m, str *_username)
{
	if (parse_from_header(_m) < 0)
	{
		LM_ERR("parsing From: header\n");
		return CHECK_ERROR;
	}

	if (parse_from_uri(_m) == NULL)
	{
		LM_ERR("parsing From: URI\n");
		return CHECK_ERROR;
	}

	return check_username(_username, &get_from(_m)->parsed_uri);
}

static inline int get_cred(struct sip_msg *_m, str *_username)
{
	struct hdr_field *h;

	get_authorized_cred(_m->authorization, &h);
	if (!h)
	{
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h)
		{
			LM_ERR("No authorized credentials found\n");
			return -1;
		}
	}

	*_username = ((auth_body_t *) h->parsed)->digest.username.whole;
	return 0;
}

int autheph_check_from0(struct sip_msg *_m)
{
	str username = {0, 0};

	if (eph_auth_api.pre_auth == NULL)
	{
		LM_ERR("autheph_check_from() with no username parameter "
			"cannot be used without the auth module\n");
		return CHECK_ERROR;
	}

	if (_m == NULL)
	{
		LM_ERR("invalid parameters\n");
		return CHECK_ERROR;
	}

	if (get_cred(_m, &username) < 0)
	{
		LM_ERR("call autheph_(check|proxy|www) before calling "
			" check_from() with no username parameter\n");
		return CHECK_ERROR;
	}

	return check_from(_m, &username);
}

int autheph_check_from1(struct sip_msg *_m, char *_username)
{
	str susername;

	if (_m == NULL || _username == NULL)
	{
		LM_ERR("invalid parameters\n");
		return CHECK_ERROR;
	}

	if (get_str_fparam(&susername, _m, (fparam_t*)_username) < 0)
	{
		LM_ERR("failed to get username value\n");
		return CHECK_ERROR;
	}

	if (susername.len == 0)
	{
		LM_ERR("invalid username parameter - empty value\n");
		return CHECK_ERROR;
	}


	return check_from(_m, &susername);
}

static inline int check_to(struct sip_msg *_m, str *_username)
{
	if (!_m->to && ((parse_headers(_m, HDR_TO_F, 0) == -1) || (!_m->to)))
	{
		LM_ERR("parsing To: header\n");
		return CHECK_ERROR;
	}

	if (parse_to_uri(_m) == NULL)
	{
		LM_ERR("parsing To: URI\n");
		return CHECK_ERROR;
	}

	return check_username(_username, &get_to(_m)->parsed_uri);
}

int autheph_check_to0(struct sip_msg *_m)
{
	str username = {0, 0};

	if (eph_auth_api.pre_auth == NULL)
	{
		LM_ERR("autheph_check_to() with no username parameter "
			"cannot be used without the auth module\n");
		return CHECK_ERROR;
	}

	if (_m == NULL)
	{
		LM_ERR("invalid parameters\n");
		return CHECK_ERROR;
	}

	if (get_cred(_m, &username) < 0)
	{
		LM_ERR("call autheph_(check|proxy|www) before calling "
			" check_to() with no username parameter\n");
		return CHECK_ERROR;
	}

	return check_to(_m, &username);
}

int autheph_check_to1(struct sip_msg *_m, char *_username)
{
	str susername;

	if (_m == NULL || _username == NULL)
	{
		LM_ERR("invalid parameters\n");
		return CHECK_ERROR;
	}

	if (get_str_fparam(&susername, _m, (fparam_t*)_username) < 0)
	{
		LM_ERR("failed to get username value\n");
		return CHECK_ERROR;
	}

	if (susername.len == 0)
	{
		LM_ERR("invalid username parameter - empty value\n");
		return CHECK_ERROR;
	}

	return check_to(_m, &susername);
}

int autheph_check_timestamp(struct sip_msg *_m, char *_username)
{
	str susername;

	if (_m == NULL || _username == NULL)
	{
		LM_ERR("invalid parameters\n");
		return CHECK_ERROR;
	}

	if (get_str_fparam(&susername, _m, (fparam_t*)_username) < 0)
	{
		LM_ERR("failed to get username value\n");
		return CHECK_ERROR;
	}

	if (susername.len == 0)
	{
		LM_ERR("invalid username parameter - empty value\n");
		return CHECK_ERROR;
	}

	if (autheph_verify_timestamp(&susername) < 0)
	{
		return CHECK_ERROR;
	}

	return CHECK_OK;
}
