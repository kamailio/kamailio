/* 
 * $Id$
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
 * Check if domain is local
 */
int is_domain_local(str* _host)
{
	if (db_mode == 0) {
		db_key_t keys[1];
		db_val_t vals[1];
		db_key_t cols[1]; 
		db_res_t* res;

		keys[0]=domain_col;
		cols[0]=domain_col;
		
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
		return hash_table_lookup (_host);
	}
			
}

/*
 * Check if host in From uri is local
 */
int is_from_local(struct sip_msg* _msg, char* _s1, char* _s2)
{
	str uri;
	struct sip_uri puri;

	if (parse_from_header(_msg) < 0) {
		LOG(L_ERR, "is_from_local(): Error while parsing From header\n");
		return -2;
	}

	uri = get_from(_msg)->uri;

	if (parse_uri(uri.s, uri.len, &puri) < 0) {
		LOG(L_ERR, "is_from_local(): Error while parsing URI\n");
		return -3;
	}

	return is_domain_local(&(puri.host));

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
