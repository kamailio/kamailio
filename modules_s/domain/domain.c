/* domain.c v 0.2 2002/12/27
 *
 * Domain table related functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
 */


#include "domain_mod.h"
#include "hash.h"
#include "../../db/db.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"

/*
 * Check that From header is properly parsed and if so,
 * return pointer to parsed From header.  Otherwise return NULL.
 */
inline struct to_body *get_parsed_from_body(struct sip_msg *_msg)
{
	if (!(_msg->from)) {
		LOG(L_ERR, "get_parsed_from(): Request does not have a From header\n");
		return NULL;
	}
	if (!(_msg->from->parsed) || ((struct to_body *)_msg->from->parsed)->error != PARSE_OK) {
		LOG(L_ERR, "get_parsed_from(): From header is not properly parsed\n");
		return NULL;
	}
	return (struct to_body *)(_msg->from->parsed);
}

/*
 * Check if domain is local
 */
int is_domain_local(str* _host)
{
	if (db_mode == 0) {
		db_key_t keys[] = {domain_domain_col};
		db_val_t vals[1];
		db_key_t cols[] = {domain_domain_col};
		db_res_t* res;

		if (db_use_table(db_handle, domain_table) < 0) {
			LOG(L_ERR, "is_local(): Error while trying to use domain table\n");
			return -1;
		}

		VAL_TYPE(vals) = DB_STR;
		VAL_NULL(vals) = 0;
		
		VAL_STR(vals).s = _host->s;
		VAL_STR(vals).len = _host->len;

		if (db_query(db_handle, keys, 0, vals, cols, 1, 1, 0, &res) < 0) {
			LOG(L_ERR, "is_local(): Error while querying database\n");
			return -1;
		}

		if (RES_ROW_N(res) == 0) {
			DBG("is_local(): Realm \'%.*s\' is not local\n", 
			    _host->len, _host->s);
			db_free_query(db_handle, res);
			return -1;
		} else {
			DBG("is_local(): Realm \'%.*s\' is local\n", 
			    _host->len, _host->s);
			db_free_query(db_handle, res);
			return 1;
		}
	} else {
		return hash_table_lookup (_host->s, _host->len);
	}
			
}

/*
 * Check if host in From uri is local
 */
int is_from_local(struct sip_msg* _msg, char* _s1, char* _s2)
{
	struct to_body* body;
	struct sip_uri uri;
	int ret;

	body = get_parsed_from_body(_msg);
	if (!body) return -1;

	if (parse_uri(body->uri.s, body->uri.len, &uri) < 0) {
		LOG(L_ERR, "is_from_local(): Error while parsing From uri\n");
		return -1;
	}

	ret = is_domain_local(&(uri.host));
	free_uri(&uri);
	return ret;
}

/*
 * Check if host in Request URI is local
 */
int is_uri_host_local(struct sip_msg* _msg, char* _s1, char* _s2)
{
	if (parse_sip_msg_uri(_msg) < 0) {
	    LOG(L_ERR, "is_uri_host_local(): Error while parsing URI\n");
	    return -1;
	}

	return is_domain_local(&(_msg->parsed_uri.host));
}
