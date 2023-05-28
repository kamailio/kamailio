/*
 * Various URI checks
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
#include "../../core/str.h"
#include "../../core/dprint.h"				 /* Debugging */
#include "../../core/parser/digest/digest.h" /* get_authorized_cred */
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/ut.h"		/* Handy utilities */
#include "../../lib/srdb1/db.h" /* Database API */
#include "../../core/mod_fix.h"
#include "uri_db.h"
#include "checks.h"

static db1_con_t *db_handle = 0; /* Database connection handle */
static db_func_t uridb_dbf;


/*
 * Check if a header field contains the same username
 * as digest credentials
 */
static inline int check_username(
		struct sip_msg *_m, struct sip_uri *_uri, str *_username, str *_realm)
{
	str username;
	str realm;

	db_key_t keys[3];
	db_val_t vals[3];
	db_key_t cols[1];
	db1_res_t *res = NULL;

	if(!_uri) {
		LM_ERR("Bad parameter\n");
		return -1;
	}

	/* Parse To/From URI */
	/* Make sure that the URI contains username */
	if(!_uri->user.len) {
		LM_ERR("Username not found in URI\n");
		return -4;
	}

	/* use digest credentials if no other credentials are supplied */
	if(!_username || !_realm) {
		struct hdr_field *h;
		auth_body_t *c;

		/* Get authorized digest credentials */
		get_authorized_cred(_m->authorization, &h);
		if(!h) {
			get_authorized_cred(_m->proxy_auth, &h);
			if(!h) {
				LM_ERR("No authorized credentials found (error in scripts)\n");
				LM_ERR("Call {www,proxy}_authorize before calling check_* "
					   "functions!\n");
				return -2;
			}
		}

		c = (auth_body_t *)(h->parsed);

		username = c->digest.username.user;
		realm = *GET_REALM(&c->digest);
	} else {
		username = *_username;
		realm = *_realm;
	}

	/* If use_uri_table is set, use URI table to determine if Digest username
	 * and To/From username match. URI table is a table enumerating all allowed
	 * usernames for a single, thus a user can have several different usernames
	 * (which are different from digest username and it will still match)
	 */
	if(use_uri_table) {
		if(uridb_dbf.use_table(db_handle, &db_table) < 0) {
			LM_ERR("Error while trying to use uri table\n");
			return -7;
		}

		keys[0] = &uridb_user_col;
		keys[1] = &uridb_domain_col;
		keys[2] = &uridb_uriuser_col;
		cols[0] = &uridb_user_col;

		VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = DB1_STR;
		VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = 0;

		VAL_STR(vals) = username;
		VAL_STR(vals + 1) = realm;
		VAL_STR(vals + 2) = _uri->user;

		if(uridb_dbf.query(db_handle, keys, 0, vals, cols, 3, 1, 0, &res) < 0) {
			LM_ERR("Error while querying database\n");
			return -8;
		}

		/* If the previous function returns at least one row, it means
		 * there is an entry for given digest username and URI username
		 * and thus this combination is allowed and the function will match
		 */
		if(RES_ROW_N(res) == 0) {
			LM_DBG("From/To user '%.*s' is spoofed\n", _uri->user.len,
					ZSW(_uri->user.s));
			uridb_dbf.free_result(db_handle, res);
			return -9;
		} else {
			LM_DBG("From/To user '%.*s' and auth user match\n", _uri->user.len,
					ZSW(_uri->user.s));
			uridb_dbf.free_result(db_handle, res);
			return 1;
		}
	} else {
		/* URI table not used, simply compare digest username and From/To
		 * username, the comparison is case insensitive
		 */
		if(_uri->user.len == username.len) {
			if(!strncasecmp(_uri->user.s, username.s, _uri->user.len)) {
				LM_DBG("Digest username and URI username match\n");
				return 1;
			}
		}

		LM_DBG("Digest username and URI username do NOT match\n");
		return -10;
	}
}


/*
 * Check username part in To header field
 */
int ki_check_to(struct sip_msg *_m)
{
	if(!_m->to && ((parse_headers(_m, HDR_TO_F, 0) == -1) || (!_m->to))) {
		LM_ERR("Error while parsing To header field\n");
		return -1;
	}
	if(parse_to_uri(_m) == NULL) {
		LM_ERR("Error while parsing To header URI\n");
		return -1;
	}

	return check_username(_m, &get_to(_m)->parsed_uri, NULL, NULL);
}


/*
 * Check username part in To header field
 */
int check_to(struct sip_msg *_m, char *_s1, char *_s2)
{
	return ki_check_to(_m);
}

/*
 * Check username part in From header field
 */
int ki_check_from(struct sip_msg *_m)
{
	if(parse_from_header(_m) < 0) {
		LM_ERR("Error while parsing From header field\n");
		return -1;
	}
	if(parse_from_uri(_m) == NULL) {
		LM_ERR("Error while parsing From header URI\n");
		return -1;
	}

	return check_username(_m, &get_from(_m)->parsed_uri, NULL, NULL);
}


/*
 * Check username part in From header field
 */
int check_from(struct sip_msg *_m, char *_s1, char *_s2)
{
	return ki_check_from(_m);
}

/*
 * Checks username part of the supplied sip URI.
 * Optinal with supplied credentials.
 */
int ki_check_uri_realm(
		struct sip_msg *msg, str *suri, str *susername, str *srealm)
{
	struct sip_uri parsed_uri;

	if(suri == NULL || suri->s == NULL || suri->len <= 0) {
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	if(parse_uri(suri->s, suri->len, &parsed_uri) != 0) {
		LM_ERR("Error while parsing URI: %.*s\n", suri->len, suri->s);
		return -1;
	}

	if(susername == NULL || susername->len <= 0 || srealm == NULL
			|| srealm->len <= 0) {
		return check_username(msg, &parsed_uri, NULL, NULL);
	}

	return check_username(msg, &parsed_uri, susername, srealm);
}

int ki_check_uri(struct sip_msg *msg, str *suri)
{
	return ki_check_uri_realm(msg, suri, NULL, NULL);
}

int check_uri(struct sip_msg *msg, char *uri, char *username, char *realm)
{
	str suri;
	str susername;
	str srealm;

	if(get_str_fparam(&suri, msg, (fparam_t *)uri) != 0) {
		LM_ERR("Error while getting URI value\n");
		return -1;
	}

	if(!username || !realm) {
		return ki_check_uri_realm(msg, &suri, NULL, NULL);
	}

	if(get_str_fparam(&susername, msg, (fparam_t *)username) != 0) {
		LM_ERR("Error while getting username value\n");
		return -1;
	}

	if(get_str_fparam(&srealm, msg, (fparam_t *)realm) != 0) {
		LM_ERR("Error while getting realm value\n");
		return -1;
	}
	return ki_check_uri_realm(msg, &suri, &susername, &srealm);
}

/*
 * Check if uri belongs to a local user
 */
int ki_does_uri_exist(struct sip_msg *_msg)
{
	db_key_t keys[2];
	db_val_t vals[2];
	db_key_t cols[1];
	db1_res_t *res = NULL;

	if(db_handle == NULL) {
		LM_ERR("database connection does not exist\n");
		return -1;
	}

	if(parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("Error while parsing URI\n");
		return -1;
	}

	if(use_uri_table) {
		if(uridb_dbf.use_table(db_handle, &db_table) < 0) {
			LM_ERR("Error while trying to use uri table\n");
			return -2;
		}
		keys[0] = &uridb_uriuser_col;
		keys[1] = &uridb_domain_col;
		cols[0] = &uridb_uriuser_col;
	} else {
		if(uridb_dbf.use_table(db_handle, &db_table) < 0) {
			LM_ERR("Error while trying to use subscriber table\n");
			return -3;
		}
		keys[0] = &uridb_user_col;
		keys[1] = &uridb_domain_col;
		cols[0] = &uridb_user_col;
	}

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB1_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;
	VAL_STR(vals) = _msg->parsed_uri.user;
	VAL_STR(vals + 1) = _msg->parsed_uri.host;

	if(uridb_dbf.query(
			   db_handle, keys, 0, vals, cols, (use_domain ? 2 : 1), 1, 0, &res)
			< 0) {
		LM_ERR("Error while querying database\n");
		return -4;
	}

	if(RES_ROW_N(res) == 0) {
		LM_DBG("User in request uri does not exist\n");
		uridb_dbf.free_result(db_handle, res);
		return -5;
	} else {
		LM_DBG("User in request uri does exist\n");
		uridb_dbf.free_result(db_handle, res);
		return 1;
	}
}


/*
 * Check if uri belongs to a local user
 */
int does_uri_exist(struct sip_msg *_msg, char *_s1, char *_s2)
{
	return ki_does_uri_exist(_msg);
}


int uridb_db_init(const str *db_url)
{
	if(uridb_dbf.init == 0) {
		LM_BUG("null dbf\n");
		return -1;
	}

	db_handle = uridb_dbf.init(db_url);
	if(db_handle == 0) {
		LM_ERR("unable to connect to the database\n");
		return -1;
	}
	return 0;
}


int uridb_db_bind(const str *db_url)
{
	if(db_bind_mod(db_url, &uridb_dbf) < 0) {
		LM_ERR("unable to bind to the database module\n");
		return -1;
	}

	if(!DB_CAPABILITY(uridb_dbf, DB_CAP_QUERY)) {
		LM_ERR("Database module does not implement the 'query' function\n");
		return -1;
	}

	return 0;
}


void uridb_db_close(void)
{
	if(db_handle && uridb_dbf.close) {
		uridb_dbf.close(db_handle);
		db_handle = 0;
	}
}


int uridb_db_ver(const str *db_url)
{
	db1_con_t *dbh;
	int ver;

	if(use_uri_table) {
		ver = URI_TABLE_VERSION;
	} else {
		ver = SUBSCRIBER_TABLE_VERSION;
	}

	if(uridb_dbf.init == 0) {
		LM_BUG("unbound database\n");
		return -1;
	}

	dbh = uridb_dbf.init(db_url);
	if(dbh == 0) {
		LM_ERR("unable to open database connection\n");
		return -1;
	}
	if(db_check_table_version(&uridb_dbf, dbh, &db_table, ver) < 0) {
		DB_TABLE_VERSION_ERROR(db_table);
		uridb_dbf.close(dbh);
		dbh = 0;
		return -1;
	}
	uridb_dbf.close(dbh);
	dbh = 0;

	return 0;
}
