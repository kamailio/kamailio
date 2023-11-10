/*
 * Various URI checks and URI manipulation
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * Copyright (C) 2004-2010 Juha Heinanen
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

/*!
 * \file
 * \brief SIP-utils :: Various URI checks and Request URI manipulation
 * \ingroup siputils
 * - Module; \ref siputils
 */


#include <string.h>
#include "../../core/str.h"
#include "../../core/dprint.h" /* Debugging */
#include "../../core/mem/mem.h"
#include "../../core/parser/digest/digest.h" /* get_authorized_cred */
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_param.h"
#include "../../core/ut.h"		/* Handy utilities */
#include "../../lib/srdb1/db.h" /* Database API */
#include "../../core/dset.h"
#include "../../core/pvar.h"
#include "../../core/lvalue.h"
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "checks.h"

extern int e164_max_len;

/**
 * return 1 (true) if the SIP message type is request
 */
int w_is_request(struct sip_msg *msg, char *foo, char *bar)
{
	if(msg == NULL)
		return -1;

	if(msg->first_line.type == SIP_REQUEST)
		return 1;

	return -1;
}

int is_request(struct sip_msg *msg)
{
	if(msg == NULL)
		return -1;

	if(msg->first_line.type == SIP_REQUEST)
		return 1;

	return -1;
}

/**
 * return 1 (true) if the SIP message type is reply
 */
int w_is_reply(struct sip_msg *msg, char *foo, char *bar)
{
	if(msg == NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
		return 1;

	return -1;
}

int is_reply(struct sip_msg *msg)
{
	if(msg == NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)
		return 1;

	return -1;
}


/*
 * Checks if From includes a To-tag -- good to identify
 * if a request creates a new dialog
 */
int has_totag(struct sip_msg *_m)
{
	str tag;

	if(!_m->to && parse_headers(_m, HDR_TO_F, 0) == -1) {
		LM_ERR("To parsing failed\n");
		return -1;
	}
	if(!_m->to) {
		LM_ERR("no To\n");
		return -1;
	}
	tag = get_to(_m)->tag_value;
	if(tag.s == 0 || tag.len == 0) {
		LM_DBG("no totag\n");
		return -1;
	}
	LM_DBG("totag found\n");
	return 1;
}

int w_has_totag(struct sip_msg *_m, char *_foo, char *_bar)
{
	return has_totag(_m);
}

/*
 * Check if pseudo variable contains a valid uri
 */
int is_uri(struct sip_msg *_m, char *_sp, char *_s2)
{
	sip_uri_t turi;
	str uval;

	if(fixup_get_svalue(_m, (gparam_t *)_sp, &uval) != 0) {
		LM_ERR("cannot get parameter value\n");
		return -1;
	}
	if(parse_uri(uval.s, uval.len, &turi) != 0) {
		return -1;
	}
	return 1;
}

/*
 * Check if the username matches the username in credentials
 */
int ki_is_user(sip_msg_t *_m, str *suser)
{
	struct hdr_field *h;
	auth_body_t *c;

	get_authorized_cred(_m->authorization, &h);
	if(!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if(!h) {
			LM_ERR("no authorized credentials found (error in scripts)\n");
			LM_ERR("Call {www,proxy}_authorize before calling is_user function "
				   "!\n");
			return -1;
		}
	}

	c = (auth_body_t *)(h->parsed);

	if(!c->digest.username.user.len) {
		LM_DBG("username not found in credentials\n");
		return -1;
	}

	if(suser->len != c->digest.username.user.len) {
		LM_DBG("username length does not match\n");
		return -1;
	}

	if(!memcmp(suser->s, c->digest.username.user.s, suser->len)) {
		LM_DBG("username matches\n");
		return 1;
	} else {
		LM_DBG("username differs\n");
		return -1;
	}
}

int is_user(struct sip_msg *_m, char *_user, char *_str2)
{
	str suser;

	if(fixup_get_svalue(_m, (gparam_t *)_user, &suser) < 0) {
		LM_ERR("failed to get user param\n");
		return -1;
	}

	return ki_is_user(_m, &suser);
}

/*
 * Find if Request URI has a given parameter with no value
 */
int uri_param_1(struct sip_msg *_msg, char *_param, char *_str2)
{
	str sparam;
	if(fixup_get_svalue(_msg, (gparam_t *)_param, &sparam) < 0) {
		LM_ERR("failed to get parameter\n");
		return -1;
	}
	return ki_uri_param_value(_msg, &sparam, NULL);
}

/*
 * Find if Request URI has a given parameter with matching value
 */
int uri_param_2(struct sip_msg *_msg, char *_param, char *_value)
{
	str sparam;
	str svalue;
	if(fixup_get_svalue(_msg, (gparam_t *)_param, &sparam) < 0) {
		LM_ERR("failed to get parameter\n");
		return -1;
	}
	if(fixup_get_svalue(_msg, (gparam_t *)_value, &svalue) < 0) {
		LM_ERR("failed to get value\n");
		return -1;
	}
	return ki_uri_param_value(_msg, &sparam, &svalue);
}

/*
 * Find if Request URI has a given parameter with matching value
 */
int ki_uri_param_value(sip_msg_t *_msg, str *sparam, str *svalue)
{
	str t;

	param_hooks_t hooks;
	param_t *params, *pit;

	if(parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("ruri parsing failed\n");
		return -1;
	}

	t = _msg->parsed_uri.params;

	if(parse_params(&t, CLASS_ANY, &hooks, &params) < 0) {
		LM_ERR("ruri parameter parsing failed\n");
		return -1;
	}

	for(pit = params; pit; pit = pit->next) {
		if((pit->name.len == sparam->len)
				&& (strncmp(pit->name.s, sparam->s, sparam->len) == 0)) {
			if(svalue) {
				if((svalue->len == pit->body.len)
						&& strncmp(svalue->s, pit->body.s, svalue->len) == 0) {
					goto ok;
				} else {
					goto nok;
				}
			} else {
				if(pit->body.len > 0) {
					goto nok;
				} else {
					goto ok;
				}
			}
		}
	}

nok:
	free_params(params);
	return -1;

ok:
	free_params(params);
	return 1;
}


/**
 *
 */
int ki_uri_param(sip_msg_t *_msg, str *sparam)
{
	return ki_uri_param_value(_msg, sparam, NULL);
}

/*
 * Check if param with or without value exists in Request URI
 */
int ki_uri_param_any(sip_msg_t *msg, str *sparam)
{
	str t;
	str ouri;
	sip_uri_t puri;
	param_hooks_t hooks;
	param_t *params, *pit;

	if((msg->new_uri.s == NULL) || (msg->new_uri.len == 0)) {
		ouri = msg->first_line.u.request.uri;
		if(ouri.s == NULL) {
			LM_ERR("r-uri not found\n");
			return -1;
		}
	} else {
		ouri = msg->new_uri;
	}

	if(parse_uri(ouri.s, ouri.len, &puri) < 0) {
		LM_ERR("failed to parse r-uri [%.*s]\n", ouri.len, ouri.s);
		return -1;
	}
	if(puri.sip_params.len > 0) {
		t = puri.sip_params;
	} else if(puri.params.len > 0) {
		t = puri.params;
	} else {
		LM_DBG("no uri params [%.*s]\n", ouri.len, ouri.s);
		return -1;
	}

	if(parse_params(&t, CLASS_ANY, &hooks, &params) < 0) {
		LM_ERR("ruri parameter parsing failed\n");
		return -1;
	}

	for(pit = params; pit; pit = pit->next) {
		if((pit->name.len == sparam->len)
				&& (strncasecmp(pit->name.s, sparam->s, sparam->len) == 0)) {
			break;
		}
	}
	if(pit == NULL) {
		free_params(params);
		return -1;
	}

	free_params(params);
	return 1;
}

/*
 * Find if Request URI has a given parameter with or without value
 */
int w_uri_param_any(struct sip_msg *_msg, char *_param, char *_str2)
{
	str sparam;
	if(fixup_get_svalue(_msg, (gparam_t *)_param, &sparam) < 0) {
		LM_ERR("failed to get parameter\n");
		return -1;
	}
	return ki_uri_param_any(_msg, &sparam);
}

/*
 * Adds a new parameter to Request URI - kemi export
 */
int ki_add_uri_param(struct sip_msg *_msg, str *param)
{
	str *cur_uri, new_uri;
	struct sip_uri *parsed_uri;
	char *at;

	if(param == NULL || param->len == 0) {
		return 1;
	}

	if(parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("ruri parsing failed\n");
		return -1;
	}

	parsed_uri = &(_msg->parsed_uri);

	/* if current ruri has no headers, pad param at the end */
	if(parsed_uri->headers.len == 0) {
		cur_uri = GET_RURI(_msg);
		new_uri.len = cur_uri->len + param->len + 1;
		if(new_uri.len > MAX_URI_SIZE) {
			LM_ERR("new ruri too long\n");
			return -1;
		}
		new_uri.s = pkg_malloc(new_uri.len);
		if(new_uri.s == 0) {
			PKG_MEM_ERROR;
			return -1;
		}
		memcpy(new_uri.s, cur_uri->s, cur_uri->len);
		*(new_uri.s + cur_uri->len) = ';';
		memcpy(new_uri.s + cur_uri->len + 1, param->s, param->len);
		if(rewrite_uri(_msg, &new_uri) == 1) {
			goto ok;
		} else {
			goto nok;
		}
	}

	/* otherwise take the long path */
	new_uri.len = 4 + (parsed_uri->user.len ? parsed_uri->user.len + 1 : 0)
				  + (parsed_uri->passwd.len ? parsed_uri->passwd.len + 1 : 0)
				  + parsed_uri->host.len
				  + (parsed_uri->port.len ? parsed_uri->port.len + 1 : 0)
				  + parsed_uri->params.len + param->len + 1
				  + parsed_uri->headers.len + 1;
	if(new_uri.len > MAX_URI_SIZE) {
		LM_ERR("new ruri too long\n");
		return -1;
	}

	new_uri.s = pkg_malloc(new_uri.len);
	if(new_uri.s == 0) {
		PKG_MEM_ERROR;
		return -1;
	}

	at = new_uri.s;
	memcpy(at, "sip:", 4);
	at = at + 4;
	if(parsed_uri->user.len) {
		memcpy(at, parsed_uri->user.s, parsed_uri->user.len);
		if(parsed_uri->passwd.len) {
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
	if(parsed_uri->port.len) {
		*at = ':';
		at = at + 1;
		memcpy(at, parsed_uri->port.s, parsed_uri->port.len);
		at = at + parsed_uri->port.len;
	}
	memcpy(at, parsed_uri->params.s, parsed_uri->params.len);
	at = at + parsed_uri->params.len;
	*at = ';';
	at = at + 1;
	memcpy(at, param->s, param->len);
	at = at + param->len;
	*at = '?';
	at = at + 1;
	memcpy(at, parsed_uri->headers.s, parsed_uri->headers.len);

	if(rewrite_uri(_msg, &new_uri) == 1) {
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
 * Adds a new parameter to Request URI - cfg export
 */
int add_uri_param(struct sip_msg *_msg, char *_param, char *_s2)
{
	return ki_add_uri_param(_msg, (str *)_param);
}

/*
 * Remove param from Request URI
 */
int ki_uri_param_rm(sip_msg_t *msg, str *sparam)
{
	str t;
	char buri[MAX_URI_SIZE];
	str ouri;
	str nuri;
	sip_uri_t puri;
	param_hooks_t hooks;
	param_t *params, *pit;

	if((msg->new_uri.s == NULL) || (msg->new_uri.len == 0)) {
		ouri = msg->first_line.u.request.uri;
		if(ouri.s == NULL) {
			LM_ERR("r-uri not found\n");
			return -1;
		}
	} else {
		ouri = msg->new_uri;
	}
	if(ouri.len >= MAX_URI_SIZE) {
		LM_ERR("r-uri is too long (%d)\n", ouri.len);
		return -1;
	}

	if(parse_uri(ouri.s, ouri.len, &puri) < 0) {
		LM_ERR("failed to parse r-uri [%.*s]\n", ouri.len, ouri.s);
		return -1;
	}
	if(puri.sip_params.len > 0) {
		t = puri.sip_params;
	} else if(puri.params.len > 0) {
		t = puri.params;
	} else {
		LM_DBG("no uri params [%.*s]\n", ouri.len, ouri.s);
		return 1;
	}

	if(parse_params(&t, CLASS_ANY, &hooks, &params) < 0) {
		LM_ERR("ruri parameter parsing failed\n");
		return -1;
	}

	for(pit = params; pit; pit = pit->next) {
		if((pit->name.len == sparam->len)
				&& (strncasecmp(pit->name.s, sparam->s, sparam->len) == 0)) {
			break;
		}
	}
	if(pit == NULL) {
		free_params(params);
		return 1;
	}

	nuri.s = pit->name.s;
	while(nuri.s > ouri.s && *nuri.s != ';') {
		nuri.s--;
	}
	memcpy(buri, ouri.s, nuri.s - ouri.s);
	nuri.len = nuri.s - ouri.s;
	nuri.s = buri;
	if(pit->body.len > 0) {
		if(pit->body.s + pit->body.len < ouri.s + ouri.len) {
			memcpy(nuri.s + nuri.len, pit->body.s + pit->body.len,
					ouri.s + ouri.len - pit->body.s - pit->body.len);
			nuri.len += ouri.s + ouri.len - pit->body.s - pit->body.len;
		}
	} else {
		if(pit->name.s + pit->name.len < ouri.s + ouri.len) {
			memcpy(nuri.s + nuri.len, pit->name.s + pit->name.len,
					ouri.s + ouri.len - pit->name.s - pit->name.len);
			nuri.len += ouri.s + ouri.len - pit->name.s - pit->name.len;
		}
	}

	free_params(params);
	if(rewrite_uri(msg, &nuri) < 0) {
		return -1;
	}
	return 1;
}

/*
 * Remove param from Request URI
 */
int w_uri_param_rm(struct sip_msg *_msg, char *_param, char *_str2)
{
	str sparam;
	if(fixup_get_svalue(_msg, (gparam_t *)_param, &sparam) < 0) {
		LM_ERR("failed to get parameter\n");
		return -1;
	}
	return ki_uri_param_rm(_msg, &sparam);
}

/*
 * Converts URI, if it is tel URI, to SIP URI.  Returns 1, if
 * conversion succeeded and 2 if no conversion was needed, i.e., URI was not
 * tel URI.  Returns -1, if conversion failed.  Takes SIP URI hostpart from
 * second parameter and (if needed) writes the result to third parameter.
 */
int tel2sip(struct sip_msg *_msg, char *_uri, char *_hostpart, char *_res)
{
	str uri, hostpart, tel_uri, sip_uri;
	char *at;
	int i, j, in_tel_parameters = 0;
	pv_spec_t *res;
	pv_value_t res_val;

	/* get parameters */
	if(get_str_fparam(&uri, _msg, (fparam_t *)_uri) < 0) {
		LM_ERR("failed to get uri value\n");
		return -1;
	}
	if(get_str_fparam(&hostpart, _msg, (fparam_t *)_hostpart) < 0) {
		LM_ERR("failed to get hostpart value\n");
		return -1;
	}
	res = (pv_spec_t *)_res;

	/* check if anything needs to be done */
	if(uri.len < 4)
		return 2;
	if(strncasecmp(uri.s, "tel:", 4) != 0)
		return 2;

	/* reserve memory for clean tel uri */
	tel_uri.s = pkg_malloc(uri.len + 1);
	if(tel_uri.s == 0) {
		PKG_MEM_ERROR;
		return -1;
	}

	/* Remove visual separators before converting to SIP URI. Don't remove
	 * visual separators in TEL URI parameters (after the first ";") */
	for(i = 0, j = 0; i < uri.len; i++) {
		if(in_tel_parameters == 0) {
			if(uri.s[i] == ';')
				in_tel_parameters = 1;
		}
		if(in_tel_parameters == 0) {
			if((uri.s[i] != '-') && (uri.s[i] != '.') && (uri.s[i] != '(')
					&& (uri.s[i] != ')'))
				tel_uri.s[j++] = tolower(uri.s[i]);
		} else {
			tel_uri.s[j++] = tolower(uri.s[i]);
		}
	}
	tel_uri.s[j] = '\0';
	tel_uri.len = strlen(tel_uri.s);

	/* reserve memory for resulting sip uri */
	sip_uri.len = 4 + tel_uri.len - 4 + 1 + hostpart.len + 1 + 10;
	sip_uri.s = pkg_malloc(sip_uri.len + 1);
	if(sip_uri.s == 0) {
		PKG_MEM_ERROR;
		pkg_free(tel_uri.s);
		return -1;
	}

	/* create resulting sip uri */
	at = sip_uri.s;
	append_str(at, "sip:", 4);
	append_str(at, tel_uri.s + 4, tel_uri.len - 4);
	append_chr(at, '@');
	append_str(at, hostpart.s, hostpart.len);
	append_chr(at, ';');
	append_str(at, "user=phone", 10);

	/* tel_uri is not needed anymore */
	pkg_free(tel_uri.s);

	/* set result pv value and write sip uri to result pv */
	res_val.rs = sip_uri;
	res_val.flags = PV_VAL_STR;
	if(res->setf(_msg, &res->pvp, (int)EQ_T, &res_val) != 0) {
		LM_ERR("failed to set result pvar\n");
		pkg_free(sip_uri.s);
		return -1;
	}

	/* free allocated pkg memory and return */
	pkg_free(sip_uri.s);
	return 1;
}


/*
 * Check if parameter is an e164 number.
 */
int siputils_e164_check(str *_user)
{
	int i;
	char c;

	if((_user->len > 2) && (_user->len <= e164_max_len)
			&& ((_user->s)[0] == '+')) {
		for(i = 1; i < _user->len; i++) {
			c = (_user->s)[i];
			if(c < '0' || c > '9')
				return -1;
		}
		return 1;
	}
	return -1;
}


/*
 * Check if user part of URI in pseudo variable is an e164 number
 */
int w_is_e164(struct sip_msg *_m, char *_sp, char *_s2)
{
	pv_spec_t *sp;
	pv_value_t pv_val;

	sp = (pv_spec_t *)_sp;

	if(sp && (pv_get_spec_value(_m, sp, &pv_val) == 0)) {
		if(pv_val.flags & PV_VAL_STR) {
			if(pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
				LM_DBG("missing argument\n");
				return -1;
			}
			return siputils_e164_check(&(pv_val.rs));
		} else {
			LM_ERR("pseudo variable value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("failed to get pseudo variable value\n");
		return -1;
	}
}


/*
 * Check if user part of URI in pseudo variable is an e164 number
 */
int w_is_uri_user_e164(struct sip_msg *_m, char *_sp, char *_s2)
{
	pv_spec_t *sp;
	pv_value_t pv_val;

	sp = (pv_spec_t *)_sp;

	if(sp && (pv_get_spec_value(_m, sp, &pv_val) == 0)) {
		if(pv_val.flags & PV_VAL_STR) {
			if(pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
				LM_DBG("missing uri\n");
				return -1;
			}
			return is_uri_user_e164(&pv_val.rs);
		} else {
			LM_ERR("pseudo variable value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("failed to get pseudo variable value\n");
		return -1;
	}
}


int is_uri_user_e164(str *uri)
{
	char *chr;
	str user;

	chr = memchr(uri->s, ':', uri->len);
	if(chr == NULL) {
		LM_ERR("parsing URI failed\n");
		return -1;
	};
	user.s = chr + 1;
	chr = memchr(user.s, '@', uri->len - (user.s - uri->s));
	if(chr == NULL)
		return -1;
	user.len = chr - user.s;

	return siputils_e164_check(&user);
}

/*
 * Set userpart of URI
 */
int set_uri_user(struct sip_msg *_m, char *_uri, char *_value)
{
	pv_spec_t *uri_pv, *value_pv;
	pv_value_t uri_val, value_val, res_val;
	str uri, value;
	char *at, *colon, *c;
	char new_uri[MAX_URI_SIZE + 1];

	uri_pv = (pv_spec_t *)_uri;
	if(uri_pv && (pv_get_spec_value(_m, uri_pv, &uri_val) == 0)) {
		if(uri_val.flags & PV_VAL_STR) {
			if(uri_val.rs.len == 0 || uri_val.rs.s == NULL) {
				LM_ERR("missing uri value\n");
				return -1;
			}
		} else {
			LM_ERR("uri value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("failed to get uri value\n");
		return -1;
	}
	uri = uri_val.rs;

	value_pv = (pv_spec_t *)_value;
	if(value_pv && (pv_get_spec_value(_m, value_pv, &value_val) == 0)) {
		if(value_val.flags & PV_VAL_STR) {
			if(value_val.rs.s == NULL) {
				LM_ERR("missing uriuser value\n");
				return -1;
			}
		} else {
			LM_ERR("uriuser value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("failed to get uriuser value\n");
		return -1;
	}
	value = value_val.rs;

	colon = strchr(uri.s, ':');
	if(colon == NULL) {
		LM_ERR("uri does not contain ':' character\n");
		return -1;
	}
	at = strchr(uri.s, '@');
	c = &(new_uri[0]);
	if(at == NULL) {
		if(value.len == 0)
			return 1;
		if(uri.len + value.len > MAX_URI_SIZE) {
			LM_ERR("resulting uri would be too large\n");
			return -1;
		}
		append_str(c, uri.s, colon - uri.s + 1);
		append_str(c, value.s, value.len);
		append_chr(c, '@');
		append_str(c, colon + 1, uri.len - (colon - uri.s + 1));
		res_val.rs.len = uri.len + value.len + 1;
	} else {
		if(value.len == 0) {
			append_str(c, uri.s, colon - uri.s + 1);
			append_str(c, at + 1, uri.len - (at - uri.s + 1));
			res_val.rs.len = uri.len - (at - colon);
		} else {
			if(uri.len + value.len - (at - colon - 1) > MAX_URI_SIZE) {
				LM_ERR("resulting uri would be too large\n");
				return -1;
			}
			append_str(c, uri.s, colon - uri.s + 1);
			append_str(c, value.s, value.len);
			append_str(c, at, uri.len - (at - uri.s));
			res_val.rs.len = uri.len + value.len - (at - colon - 1);
		}
	}

	res_val.rs.s = &(new_uri[0]);
	LM_DBG("resulting uri: %.*s\n", res_val.rs.len, res_val.rs.s);
	res_val.flags = PV_VAL_STR;
	uri_pv->setf(_m, &uri_pv->pvp, (int)EQ_T, &res_val);

	return 1;
}

/*
 * Set hostpart of URI
 */
int set_uri_host(struct sip_msg *_m, char *_uri, char *_value)
{
	pv_spec_t *uri_pv, *value_pv;
	pv_value_t uri_val, value_val, res_val;
	str uri, value;
	char *at, *colon, *c, *next;
	unsigned int host_len;
	char new_uri[MAX_URI_SIZE + 1];

	uri_pv = (pv_spec_t *)_uri;
	if(uri_pv && (pv_get_spec_value(_m, uri_pv, &uri_val) == 0)) {
		if(uri_val.flags & PV_VAL_STR) {
			if(uri_val.rs.len == 0 || uri_val.rs.s == NULL) {
				LM_ERR("missing uri value\n");
				return -1;
			}
		} else {
			LM_ERR("uri value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("failed to get uri value\n");
		return -1;
	}
	uri = uri_val.rs;

	value_pv = (pv_spec_t *)_value;
	if(value_pv && (pv_get_spec_value(_m, value_pv, &value_val) == 0)) {
		if(value_val.flags & PV_VAL_STR) {
			if(value_val.rs.s == NULL) {
				LM_ERR("missing uri value\n");
				return -1;
			}
		} else {
			LM_ERR("uri value is not string\n");
			return -1;
		}
	} else {
		LM_ERR("failed to get uri value\n");
		return -1;
	}
	value = value_val.rs;

	if(value.len == 0) {
		LM_ERR("hostpart of uri cannot be empty\n");
		return -1;
	}
	if(uri.len + value.len > MAX_URI_SIZE) {
		LM_ERR("resulting uri would be too large\n");
		return -1;
	}

	colon = strchr(uri.s, ':');
	if(colon == NULL) {
		LM_ERR("uri does not contain ':' character\n");
		return -1;
	}
	c = &(new_uri[0]);
	at = strchr(colon + 1, '@');
	if(at == NULL) {
		next = colon + 1;
	} else {
		next = at + 1;
	}
	append_str(c, uri.s, next - uri.s);
	host_len = strcspn(next, ":;?");
	append_str(c, value.s, value.len);
	strcpy(c, next + host_len);
	res_val.rs.len = uri.len + value.len - host_len;
	res_val.rs.s = &(new_uri[0]);

	LM_DBG("resulting uri: %.*s\n", res_val.rs.len, res_val.rs.s);
	res_val.flags = PV_VAL_STR;
	uri_pv->setf(_m, &uri_pv->pvp, (int)EQ_T, &res_val);

	return 1;
}

/**
 * Find if Request URI has a given parameter and returns the value.
 */
int get_uri_param(struct sip_msg *_msg, char *_param, char *_value)
{
	str *param, t;
	pv_spec_t *dst;
	pv_value_t val;

	param_hooks_t hooks;
	param_t *params;

	param = (str *)_param;
	dst = (pv_spec_t *)_value;

	if(parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("ruri parsing failed\n");
		return -1;
	}

	t = _msg->parsed_uri.params;

	if(parse_params(&t, CLASS_ANY, &hooks, &params) < 0) {
		LM_ERR("ruri parameter parsing failed\n");
		return -1;
	}

	while(params) {
		if((params->name.len == param->len)
				&& (strncmp(params->name.s, param->s, param->len) == 0)) {
			memset(&val, 0, sizeof(pv_value_t));
			val.rs.s = params->body.s;
			val.rs.len = params->body.len;
			val.flags = PV_VAL_STR;
			dst->setf(_msg, &dst->pvp, (int)EQ_T, &val);
			goto found;
		} else {
			params = params->next;
		}
	}

	free_params(params);
	return -1;

found:
	free_params(params);
	return 1;
}


/*
 * Check if the parameter is a valid telephone number
 * - optional leading + followed by digits only
 */
int ki_is_tel_number(sip_msg_t *msg, str *tval)
{
	int i;

	if(tval == NULL || tval->len < 1)
		return -2;

	i = 0;
	if(tval->s[0] == '+') {
		if(tval->len < 2)
			return -2;
		if(tval->s[1] < '1' || tval->s[1] > '9')
			return -2;
		i = 2;
	}

	for(; i < tval->len; i++) {
		if(tval->s[i] < '0' || tval->s[i] > '9')
			return -2;
	}

	return 1;
}


/*
 * Check if the parameter is a valid telephone number
 * - optional leading + followed by digits only
 */
int is_tel_number(sip_msg_t *msg, char *_sp, char *_s2)
{
	str tval = {0, 0};

	if(fixup_get_svalue(msg, (gparam_t *)_sp, &tval) != 0) {
		LM_ERR("cannot get parameter value\n");
		return -1;
	}

	return ki_is_tel_number(msg, &tval);
}


/*
 * Check if the parameter contains decimal digits only
 */
int ki_is_numeric(sip_msg_t *msg, str *tval)
{
	int i;

	if(tval == NULL || tval->len <= 0)
		return -2;

	i = 0;
	for(; i < tval->len; i++) {
		if(tval->s[i] < '0' || tval->s[i] > '9')
			return -2;
	}

	return 1;
}


/*
 * Check if the parameter contains decimal digits only
 */
int is_numeric(sip_msg_t *msg, char *_sp, char *_s2)
{
	str tval = {0, 0};

	if(fixup_get_svalue(msg, (gparam_t *)_sp, &tval) != 0) {
		LM_ERR("cannot get parameter value\n");
		return -1;
	}

	return ki_is_numeric(msg, &tval);
}


/*
 * Check if the parameter contains alphanumeric characters
 */
int ki_is_alphanum(sip_msg_t *msg, str *tval)
{
	int i;

	if(tval == NULL || tval->len <= 0)
		return -2;

	i = 0;
	for(; i < tval->len; i++) {
		if(!((tval->s[i] >= '0' && tval->s[i] <= '9')
				   || (tval->s[i] >= 'A' && tval->s[i] <= 'Z')
				   || (tval->s[i] >= 'a' && tval->s[i] <= 'z')))
			return -3;
	}

	return 1;
}

/*
 * Check if the parameter contains alphanumeric characters
 */
int ksr_is_alphanum(sip_msg_t *msg, char *_sp, char *_s2)
{
	str tval = {0, 0};

	if(fixup_get_svalue(msg, (gparam_t *)_sp, &tval) != 0) {
		LM_ERR("cannot get parameter value\n");
		return -1;
	}

	return ki_is_alphanum(msg, &tval);
}

/*
 * Check if the parameter contains alphanumeric characters or are part of
 * the second parameter
 */
int ki_is_alphanumex(sip_msg_t *msg, str *tval, str *eset)
{
	int i;
	int j;
	int found;

	if(tval == NULL || tval->len <= 0)
		return -2;

	i = 0;
	for(; i < tval->len; i++) {
		if(!((tval->s[i] >= '0' && tval->s[i] <= '9')
				   || (tval->s[i] >= 'A' && tval->s[i] <= 'Z')
				   || (tval->s[i] >= 'a' && tval->s[i] <= 'z'))) {
			if(eset == NULL || eset->len <= 0) {
				return -3;
			}
			found = 0;
			for(j = 0; j < eset->len; j++) {
				if(tval->s[i] == eset->s[j]) {
					found = 1;
					break;
				}
			}
			if(found == 0) {
				return -3;
			}
		}
	}

	return 1;
}

/*
 * Check if the parameter contains alphanumeric characters or are part of
 * the second parameter
 */
int ksr_is_alphanumex(sip_msg_t *msg, char *_sp, char *_se)
{
	str tval = {0, 0};
	str eset = {0, 0};

	if(fixup_get_svalue(msg, (gparam_t *)_sp, &tval) != 0) {
		LM_ERR("cannot get tval parameter value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)_se, &eset) != 0) {
		LM_ERR("cannot get eset parameter value\n");
		return -1;
	}

	return ki_is_alphanumex(msg, &tval, &eset);
}
