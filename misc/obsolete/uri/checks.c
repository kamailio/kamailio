/*
 * $Id$
 *
 * Various URI checks and Request URI manipulation
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 * 2003-02-26: Created by janakj
 * 2004-03-20: has_totag introduced (jiri)
 * 2004-04-14: uri_param and add_uri_param introduced (jih)
 */

#include <string.h>
#include "../../str.h"
#include "../../dprint.h"               /* Debugging */
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_param.h"
#include "../../ut.h"                   /* Handy utilities */
#include "../../dset.h"
#include "uri_mod.h"
#include "checks.h"


/*
 * Checks if From includes a To-tag -- good to identify
 * if a request creates a new dialog
 */
int has_totag(struct sip_msg* _m, char* _foo, char* _bar)
{
	str tag;

	if (!_m->to && parse_headers(_m, HDR_TO_F,0)==-1) {
		LOG(L_ERR, "ERROR: has_totag: To parsing failed\n");
		return -1;
	}
	if (!_m->to) {
		LOG(L_ERR, "ERROR: has_totag: no To\n");
		return -1;
	}
	tag=get_to(_m)->tag_value;
	if (tag.s==0 || tag.len==0) {
		DBG("DEBUG: has_totag: no totag\n");
		return -1;
	}
	DBG("DEBUG: has_totag: totag found\n");
	return 1;
}


/*
 * Check if the username matches the username in credentials
 */
int is_user(struct sip_msg* _m, char* _user, char* _str2)
{
	str s;
	struct hdr_field* h;
	auth_body_t* c;

	if (get_str_fparam(&s, _m, (fparam_t *)_user) < 0) {
		ERR("is_user: failed to recover parameter.\n");
		return -1;
	}

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_user(): No authorized credentials found (error in scripts)\n");
			LOG(L_ERR, "is_user(): Call {www,proxy}_authorize before calling is_user function !\n");
			return -1;
		}
	}

	c = (auth_body_t*)(h->parsed);

	if (!c->digest.username.user.len) {
		DBG("is_user(): Username not found in credentials\n");
		return -1;
	}

	if (s.len != c->digest.username.user.len) {
		DBG("is_user(): Username length does not match\n");
		return -1;
	}

	if (!memcmp(s.s, c->digest.username.user.s, s.len)) {
		DBG("is_user(): Username matches\n");
		return 1;
	} else {
		DBG("is_user(): Username differs\n");
		return -1;
	}
}


/*
 * Find if Request URI has a given parameter with no value
 */
int uri_param_1(struct sip_msg* _msg, char* _param, char* _str2)
{
	return uri_param_2(_msg, _param, (char*)0);
}


/*
 * Find if Request URI has a given parameter with matching value
 */
int uri_param_2(struct sip_msg* _msg, char* _param, char* _value)
{
	str param, value, t;

	param_hooks_t hooks;
	param_t* params;


	if (get_str_fparam(&param, _msg, (fparam_t *)_param) < 0) {
		ERR("is_user: failed to recover 1st parameter.\n");
		return -1;
	}
	if (_value) {
		if (get_str_fparam(&value, _msg, (fparam_t *)_value) < 0) {
			ERR("is_user: failed to recover 1st parameter.\n");
			return -1;
		}
	} else {
	    value.s = 0;
	}

	if (parse_sip_msg_uri(_msg) < 0) {
	        LOG(L_ERR, "uri_param(): ruri parsing failed\n");
	        return -1;
	}

	t = _msg->parsed_uri.params;

	if (parse_params(&t, CLASS_ANY, &hooks, &params) < 0) {
	        LOG(L_ERR, "uri_param(): ruri parameter parsing failed\n");
	        return -1;
	}

	while (params) {
		if ((params->name.len == param.len) &&
		    (strncmp(params->name.s, param.s, param.len) == 0)) {
			if (value.s) {
				if ((value.len == params->body.len) &&
				    strncmp(value.s, params->body.s, value.len) == 0) {
					goto ok;
				} else {
					goto nok;
				}
			} else {
				if (params->body.len > 0) {
					goto nok;
				} else {
					goto ok;
				}
			}
		} else {
			params = params->next;
		}
	}
	
nok:
	free_params(params);
	return -1;

ok:
	free_params(params);
	return 1;
}



/*
 * Adds a new parameter to Request URI
 */
int add_uri_param(struct sip_msg* _msg, char* _param, char* _s2)
{
	str param, *cur_uri, new_uri;
	struct sip_uri *parsed_uri;
	char *at;

	if (get_str_fparam(&param, _msg, (fparam_t *)_param) < 0) {
	    ERR("add_uri_param: failed to recover parameter.\n");
	    return -1;
	}

	if (param.len == 0) {
		return 1;
	}

	if (parse_sip_msg_uri(_msg) < 0) {
	        LOG(L_ERR, "add_uri_param(): ruri parsing failed\n");
	        return -1;
	}

	parsed_uri = &(_msg->parsed_uri);

	/* if current ruri has no headers, pad param at the end */
	if (parsed_uri->headers.len == 0) {
		cur_uri =  GET_RURI(_msg);
		new_uri.len = cur_uri->len + param.len + 1;
		if (new_uri.len > MAX_URI_SIZE) {
			LOG(L_ERR, "add_uri_param(): new ruri too long\n");
			return -1;
		}
		new_uri.s = pkg_malloc(new_uri.len);
		if (new_uri.s == 0) {
			LOG(L_ERR, "add_uri_param(): Memory allocation failure\n");
			return -1;
		}
		memcpy(new_uri.s, cur_uri->s, cur_uri->len);
		*(new_uri.s + cur_uri->len) = ';';
		memcpy(new_uri.s + cur_uri->len + 1, param.s, param.len);
		if (rewrite_uri(_msg, &new_uri ) == 1) {
			goto ok;
		} else {
			goto nok;
		}
	}

	/* otherwise take the long path */
	new_uri.len = 4 +
		(parsed_uri->user.len ? parsed_uri->user.len + 1 : 0) +
		(parsed_uri->passwd.len ? parsed_uri->passwd.len + 1 : 0) +
		parsed_uri->host.len +
		(parsed_uri->port.len ? parsed_uri->port.len + 1 : 0) +
		parsed_uri->params.len + param.len + 1 +
		parsed_uri->headers.len + 1;
	if (new_uri.len > MAX_URI_SIZE) {
	        LOG(L_ERR, "add_uri_param(): new ruri too long\n");
		return -1;
	}

	new_uri.s = pkg_malloc(new_uri.len);
	if (new_uri.s == 0) {
		LOG(L_ERR, "add_uri_param(): Memory allocation failure\n");
		return -1;
	}

	at = new_uri.s;
	memcpy(at, "sip:", 4);
	at = at + 4;
	if (parsed_uri->user.len) {
		memcpy(at, parsed_uri->user.s, parsed_uri->user.len);
		if (parsed_uri->passwd.len) {
			*at = ':';
			at = at + 1;
			memcpy(at, parsed_uri->passwd.s, parsed_uri->passwd.len);
			at = at + parsed_uri->passwd.len;
		};
		*at = '@';
		at = at + 1;
	}
	memcpy(at, parsed_uri->host.s, parsed_uri->host.len);
	at = at + parsed_uri->host.len;
	if (parsed_uri->port.len) {
		*at = ':';
		at = at + 1;
		memcpy(at, parsed_uri->port.s, parsed_uri->port.len);
		at = at + parsed_uri->port.len;
	}
	memcpy(at, parsed_uri->params.s, parsed_uri->params.len);
	at = at + parsed_uri->params.len;
	*at = ';';
	at = at + 1;
	memcpy(at, param.s, param.len);
	at = at + param.len;
	*at = '?';
	at = at + 1;
	memcpy(at, parsed_uri->headers.s, parsed_uri->headers.len);

	if (rewrite_uri(_msg, &new_uri) == 1) {
		goto ok;
	}

nok:
	pkg_free(new_uri.s);
	return -1;

ok:
	pkg_free(new_uri.s);
	return 1;
}


/*
 * Converts Request-URI, if it is tel URI, to SIP URI.  Returns 1, if
 * conversion succeeded or if no conversion was needed, i.e., Request-URI
 * was not tel URI.  Returns -1, if conversion failed.
 */
int tel2sip(struct sip_msg* _msg, char* _s1, char* _s2)
{
	str *ruri, furi;
	struct sip_uri pfuri;
	str suri;
	char* at;

	ruri = GET_RURI(_msg);

	if (ruri->len < 4) return 1;

	if (strncmp(ruri->s, "tel:", 4) != 0) return 1;

	if (parse_from_header(_msg) < 0) {
		LOG(L_ERR, "tel2sip(): Error while parsing From header\n");
		return -1;
	}
	furi = get_from(_msg)->uri;
	if (parse_uri(furi.s, furi.len, &pfuri) < 0) {
		LOG(L_ERR, "tel2sip(): Error while parsing From URI\n");
		return -1;
	}

	suri.len = 4 + ruri->len - 4 + 1 + pfuri.host.len + 1 + 10;
	suri.s = pkg_malloc(suri.len);
	if (suri.s == 0) {
		LOG(L_ERR, "tel2sip(): Memory allocation failure\n");
		return -1;
	}
	at = suri.s;
	memcpy(at, "sip:", 4);
	at = at + 4;
	memcpy(at, ruri->s + 4, ruri->len - 4);
	at = at + ruri->len - 4;
	*at = '@';
	at = at + 1;
	memcpy(at, pfuri.host.s, pfuri.host.len);
	at = at + pfuri.host.len;
	*at = ';';
	at = at + 1;
	memcpy(at, "user=phone", 10);

	LOG(L_ERR, "tel2sip(): SIP URI is <%.*s>\n", suri.len, suri.s);

	if (rewrite_uri(_msg, &suri) == 1) {
		pkg_free(suri.s);
		return 1;
	} else {
		pkg_free(suri.s);
		return -1;
	}
}
