/*
 * $Id$
 *
 * Various URI checks
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
 * 2003-02-26: Created by janakj
 */

#include <string.h>
#include "../../str.h"
#include "../../dprint.h"               /* Debugging */
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../ut.h"                   /* Handy utilities */
#include "../../db/db.h"                /* Database API */
#include "uri_mod.h"
#include "checks.h"

/*
 * If defined, username part of digest credentials can contain also
 * domain, some user agents use this approach because realm parameter
 * is not protected by the digest. If you define
 * USER_CAN_CONTAIN_DOMAIN then the functions will extract the domain
 * part before processing the username.
 */
#define USER_CAN_CONTAIN_DOMAIN


/*
 * Check if the username matches the username in credentials
 */
int is_user(struct sip_msg* _m, char* _user, char* _str2)
{
	str* s;
	struct hdr_field* h;
	auth_body_t* c;

	s = (str*)_user;

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

	if (!c->digest.username.len) {
		DBG("is_user(): Username not found in credentials\n");
		return -1;
	}

	if (s->len != c->digest.username.len) {
		DBG("is_user(): Username length does not match\n");
		return -1;
	}

	if (!memcmp(s->s, c->digest.username.s, s->len)) {
		DBG("is_user(): Username matches\n");
		return 1;
	} else {
		DBG("is_user(): Username differs\n");
		return -1;
	}
}


/*
 * Check if a header field contains the same username
 * as digest credentials
 */
static inline int check_username(struct sip_msg* _m, str* _uri)
{
	struct hdr_field* h;
	auth_body_t* c;
	struct sip_uri puri;

	int len;

	if (!_uri) {
		LOG(L_ERR, "check_username(): Bad parameter\n");
		return -1;
	}

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "check_username(): No authorized credentials found (error in scripts)\n");
			LOG(L_ERR, "check_username(): Call {www,proxy}_authorize before calling check_* function !\n");
			return -2;
		}
	}

	c = (auth_body_t*)(h->parsed);

	if (parse_uri(_uri->s, _uri->len, &puri) < 0) {
		LOG(L_ERR, "check_username(): Error while parsing URI\n");
		return -3;
	}

	if (!puri.user.len) return -4;

	len = c->digest.username.len;

	if (puri.user.len == len) {
		if (!strncasecmp(puri.user.s, c->digest.username.s, len)) {
			DBG("check_username(): Username is same\n");
			return 1;
		}
	}
	
	DBG("check_username(): Username is different\n");
	return -5;
}


/*
 * Check username part in To header field
 */
int check_to(struct sip_msg* _m, char* _s1, char* _s2)
{
	if (!_m->to && ((parse_headers(_m, HDR_TO, 0) == -1) || (!_m->to))) {
		LOG(L_ERR, "check_to(): Error while parsing To header field\n");
		return -1;
	}
	return check_username(_m, &get_to(_m)->uri);
}


/*
 * Check username part in From header field
 */
int check_from(struct sip_msg* _m, char* _s1, char* _s2)
{
	if (!_m->from && ((parse_headers(_m, HDR_FROM, 0) == -1) || (!_m->from))) {
		LOG(L_ERR, "check_from(): Error while parsing From header field\n");
		return -1;
	}
	return check_username(_m, &get_from(_m)->uri);
}


/*
 * Check if uri belongs to a local user
 */
int does_uri_exist(struct sip_msg* _msg, char* _s1, char* _s2)
{
	db_key_t keys[2];
	db_val_t vals[2];
	db_key_t cols[1];
	db_res_t* res;

	if (parse_sip_msg_uri(_msg) < 0) {
		LOG(L_ERR, "does_uri_exist(): Error while parsing URI\n");
		return -1;
	}

	if (use_uri_table) {
		if (db_use_table(db_handle, uri_table) < 0) {
			LOG(L_ERR, "does_uri_exist(): Error while trying to use uri table\n");
		}
		keys[0] = uri_domain_column;
		keys[1] = uri_uriuser_column;
		cols[0] = uri_uriuser_column;
	} else {
		if (db_use_table(db_handle, subscriber_table) < 0) {
			LOG(L_ERR, "does_uri_exist(): Error while trying to use subscriber table\n");
		}
		keys[0] = subscriber_domain_column;
		keys[1] = subscriber_user_column;
		cols[0] = subscriber_user_column;
	}

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;
	VAL_STR(vals) = _msg->parsed_uri.host;
	VAL_STR(vals + 1) = _msg->parsed_uri.user;

	if (db_query(db_handle, keys, 0, vals, cols, 2, 1, 0, &res) < 0) {
		LOG(L_ERR, "does_uri_existl(): Error while querying database\n");
		return -1;
	}
	
	if (RES_ROW_N(res) == 0) {
		DBG("does_uri_exit(): User in request uri does not exist\n");
		db_free_query(db_handle, res);
		return -1;
	} else {
		DBG("does_uri_exit(): User in request uri does exist\n");
		db_free_query(db_handle, res);
		return 1;
	}
}
