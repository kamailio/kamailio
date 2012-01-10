/* 
 * $Id$
 *
 * Domain table related functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2004-06-07  updated to the new DB api, moved reload_table here, created 
 *               domain_db_{init.bind,ver,close} (andrei)
 *  2004-09-06  is_uri_host_local() can now be called also from
 *              failure route (juhe)
 */

#include "domain_mod.h"
#include "hash.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../route.h"
#include "../../pvar.h"
#include "../../str.h"

static db1_con_t* db_handle=0;
static db_func_t domain_dbf;

/* helper db functions*/

int domain_db_bind(const str* db_url)
{
	if (db_bind_mod(db_url, &domain_dbf )) {
	        LM_ERR("Cannot bind to database module!");
		return -1;
	}
	return 0;
}



int domain_db_init(const str* db_url)
{	
	if (domain_dbf.init==0){
		LM_ERR("Unbound database module\n");
		goto error;
	}
	if (db_handle!=0)
		return 0;

	db_handle=domain_dbf.init(db_url);
	if (db_handle==0){
		LM_ERR("Cannot initialize database connection\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


void domain_db_close(void)
{
	if (db_handle && domain_dbf.close){
		domain_dbf.close(db_handle);
		db_handle=0;
	}
}



int domain_db_ver(str* name, int version)
{
	if (db_handle==0){
		LM_ERR("null database handler\n");
		return -1;
	}
	return db_check_table_version(&domain_dbf, db_handle, name, version);
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
		db1_res_t* res = NULL;

		keys[0] = &domain_col;
		cols[0] = &domain_col;
		
		if (domain_dbf.use_table(db_handle, &domain_table) < 0) {
			LM_ERR("Error while trying to use domain table\n");
			return -1;
		}

		VAL_TYPE(vals) = DB1_STR;
		VAL_NULL(vals) = 0;
		
		VAL_STR(vals).s = _host->s;
		VAL_STR(vals).len = _host->len;

		if (domain_dbf.query(db_handle, keys, 0, vals, cols, 1, 1, 0, &res) < 0
				) {
			LM_ERR("Error while querying database\n");
			return -1;
		}

		if (RES_ROW_N(res) == 0) {
			LM_DBG("Realm '%.*s' is not local\n", 
			       _host->len, ZSW(_host->s));
			domain_dbf.free_result(db_handle, res);
			return -1;
		} else {
			LM_DBG("Realm '%.*s' is local\n", 
			       _host->len, ZSW(_host->s));
			domain_dbf.free_result(db_handle, res);
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
	struct sip_uri *puri;

	if ((puri=parse_from_uri(_msg))==NULL) {
		LM_ERR("Error while parsing From header\n");
		return -2;
	}

	return is_domain_local(&(puri->host));

}

/*
 * Check if host in Request URI is local
 */
int is_uri_host_local(struct sip_msg* _msg, char* _s1, char* _s2)
{
	str branch;
	qvalue_t q;
	struct sip_uri puri;

	if ( is_route_type(REQUEST_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE) ) {
		if (parse_sip_msg_uri(_msg) < 0) {
			LM_ERR("Error while parsing R-URI\n");
			return -1;
		}
		return is_domain_local(&(_msg->parsed_uri.host));
	} else if (is_route_type(FAILURE_ROUTE)) {
			branch.s = get_branch(0, &branch.len, &q, 0, 0, 0, 0);
			if (branch.s) {
				if (parse_uri(branch.s, branch.len, &puri) < 0) {
					LM_ERR("Error while parsing branch URI\n");
					return -1;
				}
				return is_domain_local(&(puri.host));
			} else {
				LM_ERR("Branch is missing, error in script\n");
				return -1;
			}
	} else {
		LM_ERR("Unsupported route type\n");
		return -1;
	}
}


/*
 * Check if domain given as value of pseudo variable parameter is local
 */
int w_is_domain_local(struct sip_msg* _msg, char* _sp, char* _s2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;

    sp = (pv_spec_t *)_sp;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
		LM_DBG("Missing domain name\n");
		return -1;
	    }
	    return is_domain_local(&(pv_val.rs));
	} else {
	   LM_DBG("Pseudo variable value is not string\n");
	   return -1;
	}
    } else {
	LM_DBG("Cannot get pseudo variable value\n");
	return -1;
    }
}

int domain_check_self(str* host, unsigned short port, unsigned short proto)
{
	if(is_domain_local(host)>0)
		return 1;
	return 0;
}

/*
 * Reload domain table to new hash table and when done, make new hash table
 * current one.
 */
int reload_domain_table ( void )
{
	db_val_t vals[1];
	db_key_t cols[1];
	db1_res_t* res = NULL;
	db_row_t* row;
	db_val_t* val;

	struct domain_list **new_hash_table;
	int i;

	cols[0] = &domain_col;

	if (domain_dbf.use_table(db_handle, &domain_table) < 0) {
		LM_ERR("Error while trying to use domain table\n");
		return -1;
	}

	VAL_TYPE(vals) = DB1_STR;
	VAL_NULL(vals) = 0;

	if (domain_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 1, 0, &res) < 0) {
		LM_ERR("Error while querying database\n");
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

	LM_DBG("Number of rows in domain table: %d\n", RES_ROW_N(res));
		
	for (i = 0; i < RES_ROW_N(res); i++) {
		val = ROW_VALUES(row + i);
		if ((ROW_N(row) == 1) && (VAL_TYPE(val) == DB1_STRING)) {
			
			LM_DBG("Value: %s inserted into domain hash table\n",VAL_STRING(val));

			if (hash_table_install(new_hash_table,(char*)VAL_STRING(val))==-1){
				LM_ERR("Hash table problem\n");
				domain_dbf.free_result(db_handle, res);
				return -1;
			}
		} else {
			LM_ERR("Database problem\n");
			domain_dbf.free_result(db_handle, res);
			return -1;
		}
	}
	domain_dbf.free_result(db_handle, res);

	*hash_table = new_hash_table;
	
	return 1;
}

