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
 /*
  * History:
  * --------
  *  2004-06-07  updated to the new DB api, moved reload_table here, created 
  *               domain_db_{init.bind,ver,close} (andrei)
  */

#include "domain_mod.h"
#include "hash.h"
#include "../../db/db.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"

static db_con_t* db_handle=0;
static db_func_t domain_dbf;

/* helper db functions*/

int domain_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &domain_dbf )) {
		LOG(L_CRIT, "ERROR: domain_db_bind: cannot bind to database module! "
		"Did you forget to load a database module ?\n");
		return -1;
	}
	return 0;
}



int domain_db_init(char* db_url)
{
	if (domain_dbf.init==0){
		LOG(L_CRIT, "BUG: domain_db_init: unbound database module\n");
		goto error;
	}
	db_handle=domain_dbf.init(db_url);
	if (db_handle==0){
		LOG(L_CRIT, "ERROR:domain_db_init: cannot initialize database "
							"connection\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


void domain_db_close()
{
	if (db_handle && domain_dbf.close){
		domain_dbf.close(db_handle);
		db_handle=0;
	}
}



int domain_db_ver(str* name)
{
	int ver;

	if (db_handle==0){
		LOG(L_CRIT, "BUG:domain_db_ver: null database handler\n");
		return -1;
	}
	ver=table_version(&domain_dbf, db_handle, name);
	return ver;
}



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

		keys[0]=domain_col.s;
		cols[0]=domain_col.s;
		
		if (domain_dbf.use_table(db_handle, domain_table.s) < 0) {
			LOG(L_ERR, "is_local(): Error while trying to use domain table\n");
			return -1;
		}

		VAL_TYPE(vals) = DB_STR;
		VAL_NULL(vals) = 0;
		
		VAL_STR(vals).s = _host->s;
		VAL_STR(vals).len = _host->len;

		if (domain_dbf.query(db_handle, keys, 0, vals, cols, 1, 1, 0, &res) < 0
				) {
			LOG(L_ERR, "is_local(): Error while querying database\n");
			return -1;
		}

		if (RES_ROW_N(res) == 0) {
			DBG("is_local(): Realm '%.*s' is not local\n", 
			    _host->len, ZSW(_host->s));
			domain_dbf.free_query(db_handle, res);
			return -1;
		} else {
			DBG("is_local(): Realm '%.*s' is local\n", 
			    _host->len, ZSW(_host->s));
			domain_dbf.free_query(db_handle, res);
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



/*
 * Reload domain table to new hash table and when done, make new hash table
 * current one.
 */
int reload_domain_table ( void )
{
/*	db_key_t keys[] = {domain_col}; */
	db_val_t vals[1];
	db_key_t cols[1];
	db_res_t* res;
	db_row_t* row;
	db_val_t* val;

	struct domain_list **new_hash_table;
	int i;

	cols[0] = domain_col.s;

	if (domain_dbf.use_table(db_handle, domain_table.s) < 0) {
		LOG(L_ERR, "reload_domain_table(): Error while trying to use domain table\n");
		return -1;
	}

	VAL_TYPE(vals) = DB_STR;
	VAL_NULL(vals) = 0;
    
	if (domain_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 1, 0, &res) < 0) {
		LOG(L_ERR, "reload_domain_table(): Error while querying database\n");
		return -1;
	}

	/* Choose new hash table and free its old contents */
	if (*hash_table == hash_table_1) {
		hash_table_free(hash_table_2);
		new_hash_table = hash_table_2;
	} else {
		hash_table_free(hash_table_1);
		new_hash_table = hash_table_1;
	}

	row = RES_ROWS(res);

	DBG("Number of rows in domain table: %d\n", RES_ROW_N(res));
		
	for (i = 0; i < RES_ROW_N(res); i++) {
		val = ROW_VALUES(row + i);
		if ((ROW_N(row) == 1) && (VAL_TYPE(val) == DB_STRING)) {
			
			DBG("Value: %s inserted into domain hash table\n", VAL_STRING(val));

			if (hash_table_install(new_hash_table, (char *)(VAL_STRING(val))) == -1) {
				LOG(L_ERR, "domain_reload(): Hash table problem\n");
				domain_dbf.free_query(db_handle, res);
				return -1;
			}
		} else {
			LOG(L_ERR, "domain_reload(): Database problem\n");
			domain_dbf.free_query(db_handle, res);
			return -1;
		}
	}
	domain_dbf.free_query(db_handle, res);

	*hash_table = new_hash_table;
	
	return 1;
}

