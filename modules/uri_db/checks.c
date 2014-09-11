/*
 * $Id$
 *
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
 * History:
 * --------
 * 2003-02-26: Created by janakj
 * 2004-03-20: has_totag introduced (jiri)
 * 2004-06-07  updated to the new DB api, added uridb_db_{bind,init,close,ver}
 *              (andrei)
 */

#include <string.h>
#include "../../str.h"
#include "../../dprint.h"               /* Debugging */
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../ut.h"                   /* Handy utilities */
#include "../../lib/srdb1/db.h"                /* Database API */
#include "uridb_mod.h"
#include "checks.h"

static db1_con_t* db_handle = 0;   /* Database connection handle */
static db_func_t uridb_dbf;


/*
 * Check if a header field contains the same username
 * as digest credentials
 */
static inline int check_username(struct sip_msg* _m, struct sip_uri *_uri)
{
	struct hdr_field* h;
	auth_body_t* c;
	db_key_t keys[3];
	db_val_t vals[3];
	db_key_t cols[1];
	db1_res_t* res = NULL;

	if (!_uri) {
		LM_ERR("Bad parameter\n");
		return -1;
	}

	/* Get authorized digest credentials */
	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LM_ERR("No authorized credentials found (error in scripts)\n");
			LM_ERR("Call {www,proxy}_authorize before calling check_* functions!\n");
			return -2;
		}
	}

	c = (auth_body_t*)(h->parsed);

	/* Parse To/From URI */
	/* Make sure that the URI contains username */
	if (!_uri->user.len) {
		LM_ERR("Username not found in URI\n");
		return -4;
	}

	/* If use_uri_table is set, use URI table to determine if Digest username
	 * and To/From username match. URI table is a table enumerating all allowed
	 * usernames for a single, thus a user can have several different usernames
	 * (which are different from digest username and it will still match)
	 */
	if (use_uri_table) {
		if (uridb_dbf.use_table(db_handle, &db_table) < 0) {
			LM_ERR("Error while trying to use uri table\n");
			return -7;
		}

		keys[0] = &uridb_user_col;
		keys[1] = &uridb_domain_col;
		keys[2] = &uridb_uriuser_col;
		cols[0] = &uridb_user_col;

		VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = DB1_STR;
		VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = 0;

		VAL_STR(vals) = c->digest.username.user;
		VAL_STR(vals + 1) = *GET_REALM(&c->digest);
		VAL_STR(vals + 2) = _uri->user;

		if (uridb_dbf.query(db_handle, keys, 0, vals, cols, 3, 1, 0, &res) < 0)
		{
			LM_ERR("Error while querying database\n");
			return -8;
		}

		/* If the previous function returns at least one row, it means
		 * there is an entry for given digest username and URI username
		 * and thus this combination is allowed and the function will match
		 */
		if (RES_ROW_N(res) == 0) {
			LM_DBG("From/To user '%.*s' is spoofed\n",
				   _uri->user.len, ZSW(_uri->user.s));
			uridb_dbf.free_result(db_handle, res);
			return -9;
		} else {
			LM_DBG("From/To user '%.*s' and auth user match\n",
				   _uri->user.len, ZSW(_uri->user.s));
			uridb_dbf.free_result(db_handle, res);
			return 1;
		}
	} else {
		/* URI table not used, simply compare digest username and From/To
		 * username, the comparison is case insensitive
		 */
		if (_uri->user.len == c->digest.username.user.len) {
			if (!strncasecmp(_uri->user.s, c->digest.username.user.s, 
			_uri->user.len)) {
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
int check_to(struct sip_msg* _m, char* _s1, char* _s2)
{
	if (!_m->to && ((parse_headers(_m, HDR_TO_F, 0) == -1) || (!_m->to))) {
		LM_ERR("Error while parsing To header field\n");
		return -1;
	}
	if(parse_to_uri(_m)==NULL) {
		LM_ERR("Error while parsing To header URI\n");
		return -1;
	}

	return check_username(_m, &get_to(_m)->parsed_uri);
}


/*
 * Check username part in From header field
 */
int check_from(struct sip_msg* _m, char* _s1, char* _s2)
{
	if (parse_from_header(_m) < 0) {
		LM_ERR("Error while parsing From header field\n");
		return -1;
	}
	if(parse_from_uri(_m)==NULL) {
		LM_ERR("Error while parsing From header URI\n");
		return -1;
	}

	return check_username(_m, &get_from(_m)->parsed_uri);
}


/*
 * Check if uri belongs to a local user
 */
int does_uri_exist(struct sip_msg* _msg, char* _s1, char* _s2)
{
	db_key_t keys[2];
	db_val_t vals[2];
	db_key_t cols[1];
	db1_res_t* res = NULL;

	if (parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("Error while parsing URI\n");
		return -1;
	}

	if (use_uri_table) {
		if (uridb_dbf.use_table(db_handle, &db_table) < 0) {
			LM_ERR("Error while trying to use uri table\n");
			return -2;
		}
		keys[0] = &uridb_uriuser_col;
		keys[1] = &uridb_domain_col;
		cols[0] = &uridb_uriuser_col;
	} else {
		if (uridb_dbf.use_table(db_handle, &db_table) < 0) {
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

	if (uridb_dbf.query(db_handle, keys, 0, vals, cols, (use_domain ? 2 : 1),
				1, 0, &res) < 0) {
		LM_ERR("Error while querying database\n");
		return -4;
	}
	
	if (RES_ROW_N(res) == 0) {
		LM_DBG("User in request uri does not exist\n");
		uridb_dbf.free_result(db_handle, res);
		return -5;
	} else {
		LM_DBG("User in request uri does exist\n");
		uridb_dbf.free_result(db_handle, res);
		return 1;
	}
}



int uridb_db_init(const str* db_url)
{
	if (uridb_dbf.init==0){
		LM_CRIT("BUG: null dbf\n");
		return -1; 
	}
	
	db_handle=uridb_dbf.init(db_url);
	if (db_handle==0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}
	return 0;
}



int uridb_db_bind(const str* db_url)
{
	if (db_bind_mod(db_url, &uridb_dbf)<0){
		LM_ERR("unable to bind to the database module\n");
		return -1;
	}

	if (!DB_CAPABILITY(uridb_dbf, DB_CAP_QUERY)) {
		LM_ERR("Database module does not implement the 'query' function\n");
		return -1;
	}

	return 0;
}


void uridb_db_close(void)
{
	if (db_handle && uridb_dbf.close){
		uridb_dbf.close(db_handle);
		db_handle=0;
	}
}


int uridb_db_ver(const str* db_url, str* name)
{
	db1_con_t* dbh;
	int ver;
	
	if (uridb_dbf.init==0){
		LM_CRIT("BUG: unbound database\n");
		return -1;
	}

	dbh=uridb_dbf.init(db_url);
	if (dbh==0){
		LM_ERR("unable to open database connection\n");
		return -1;
	}
	ver=db_table_version(&uridb_dbf, dbh, name);
	uridb_dbf.close(dbh);
	return ver;
}
