/*
 * $Id$
 *
 * Various URI checks
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
#include "../../db/db.h"                /* Database API */
#include "uridb_mod.h"
#include "checks.h"

static db_con_t* db_handle = 0;   /* Database connection handle */
static db_func_t uridb_dbf;


/*
 * Check if a header field contains the same username
 * as digest credentials
 */
static inline int check_username(struct sip_msg* _m, str* _uri)
{
	struct hdr_field* h;
	auth_body_t* c;
	struct sip_uri puri;
	db_key_t keys[3];
	db_val_t vals[3];
	db_key_t cols[1];
	db_res_t* res;

	if (!_uri) {
		LOG(L_ERR, "check_username(): Bad parameter\n");
		return -1;
	}

	     /* Get authorized digest credentials */
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

	     /* Parse To/From URI */
	if (parse_uri(_uri->s, _uri->len, &puri) < 0) {
		LOG(L_ERR, "check_username(): Error while parsing URI\n");
		return -3;
	}
	
	     /* Make sure that the URI contains username */
 	if (!puri.user.len) {
		LOG(L_ERR, "check_username(): Username not found in URI\n");
		return -4;
	}

	     /* If use_uri_table is set, use URI table to determine if Digest username
	      * and To/From username match. URI table is a table enumerating all allowed
	      * usernames for a single, thus a user can have several different usernames
	      * (which are different from digest username and it will still match)
	      */
	if (use_uri_table) {
		     /* Make sure that From/To URI domain and digest realm are equal
		      * FIXME: Should we move this outside this condition and make it general ?
		      */
		if (puri.host.len != c->digest.realm.len) {
			LOG(L_ERR, "check_username(): Digest realm and URI domain do not match\n");
			return -5;
		}

		if (strncasecmp(puri.host.s, c->digest.realm.s, puri.host.len) != 0) {
			DBG("check_username(): Digest realm and URI domain do not match\n");
			return -6;
		}

		if (uridb_dbf.use_table(db_handle, uri_table.s) < 0) {
			LOG(L_ERR, "ERROR: check_username(): "
					"Error while trying to use uri table\n");
			return -7;
		}

		keys[0] = uri_user_col.s;
		keys[1] = uri_domain_col.s;
		keys[2] = uri_uriuser_col.s;
		cols[0] = uri_user_col.s;

		VAL_TYPE(vals) = VAL_TYPE(vals + 1) = VAL_TYPE(vals + 2) = DB_STR;
		VAL_NULL(vals) = VAL_NULL(vals + 1) = VAL_NULL(vals + 2) = 0;
    
		VAL_STR(vals) = c->digest.username.user;
    		VAL_STR(vals + 1) = c->digest.realm;
		VAL_STR(vals + 2) = puri.user;

		if (uridb_dbf.query(db_handle, keys, 0, vals, cols, 3, 1, 0, &res) < 0)
		{
			LOG(L_ERR, "ERROR: check_username():"
					" Error while querying database\n");
			return -8;
		}

		     /* If the previous function returns at least one row, it means
		      * there is an entry for given digest username and URI username
		      * and thus this combination is allowed and the function will match
		      */
		if (RES_ROW_N(res) == 0) {
			DBG("check_username(): From/To user '%.*s' is spoofed\n", 
			    puri.user.len, ZSW(puri.user.s));
			uridb_dbf.free_query(db_handle, res);
			return -9;
		} else {
			DBG("check_username(): From/To user '%.*s' and auth user match\n", 
			    puri.user.len, ZSW(puri.user.s));
			uridb_dbf.free_query(db_handle, res);
			return 1;
		}
	} else {
		     /* URI table not used, simply compare digest username and From/To
		      * username, the comparison is case insensitive
		      */
		if (puri.user.len == c->digest.username.user.len) {
			if (!strncasecmp(puri.user.s, c->digest.username.user.s, puri.user.len)) {
				DBG("check_username(): Digest username and URI username match\n");
				return 1;
			}
		}
	
		DBG("check_username(): Digest username and URI username do NOT match\n");
		return -10;
	}
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
	if (parse_from_header(_m) < 0) {
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
		if (uridb_dbf.use_table(db_handle, uri_table.s) < 0) {
			LOG(L_ERR, "ERROR: does_uri_exist(): "
					"Error while trying to use uri table\n");
			return -2;
		}
		keys[0] = uri_uriuser_col.s;
		keys[1] = uri_domain_col.s;
		cols[0] = uri_uriuser_col.s;
	} else {
		if (uridb_dbf.use_table(db_handle, subscriber_table.s) < 0) {
			LOG(L_ERR, "ERROR: does_uri_exist():"
					" Error while trying to use subscriber table\n");
			return -3;
		}
		keys[0] = subscriber_user_col.s;
		keys[1] = subscriber_domain_col.s;
		cols[0] = subscriber_user_col.s;
	}

	VAL_TYPE(vals) = VAL_TYPE(vals + 1) = DB_STR;
	VAL_NULL(vals) = VAL_NULL(vals + 1) = 0;
	VAL_STR(vals) = _msg->parsed_uri.user;
	VAL_STR(vals + 1) = _msg->parsed_uri.host;

	if (uridb_dbf.query(db_handle, keys, 0, vals, cols, (use_domain ? 2 : 1),
				1, 0, &res) < 0) {
		LOG(L_ERR, "does_uri_exist(): Error while querying database\n");
		return -4;
	}
	
	if (RES_ROW_N(res) == 0) {
		DBG("does_uri_exit(): User in request uri does not exist\n");
		uridb_dbf.free_query(db_handle, res);
		return -5;
	} else {
		DBG("does_uri_exit(): User in request uri does exist\n");
		uridb_dbf.free_query(db_handle, res);
		return 1;
	}
}



int uridb_db_init(char* db_url)
{
	if (uridb_dbf.init==0){
		LOG(L_CRIT, "BUG: uridb_db_bind: null dbf\n");
		goto error;
	}
	db_handle=uridb_dbf.init(db_url);
	if (db_handle==0){
		LOG(L_ERR, "ERROR: uridb_db_bind: unable to connect to the database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}



int uridb_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &uridb_dbf)<0){
		LOG(L_ERR, "ERROR: uridb_db_bind: unable to bind to the database"
				" module\n");
		return -1;
	}
	return 0;
}


void uridb_db_close()
{
	if (db_handle && uridb_dbf.close){
		uridb_dbf.close(db_handle);
		db_handle=0;
	}
}


int uridb_db_ver(char* db_url, str* name)
{
	db_con_t* dbh;
	int ver;

	if (uridb_dbf.init==0){
		LOG(L_CRIT, "BUG: uridb_db_ver: unbound database\n");
		return -1;
	}
	dbh=uridb_dbf.init(db_url);
	if (dbh==0){
		LOG(L_ERR, "ERROR: uridb_db_ver: unable to open database connection\n");
		return -1;
	}
	ver=table_version(&uridb_dbf, dbh, name);
	uridb_dbf.close(dbh);
	return ver;
}
