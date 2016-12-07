/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */
 /*
  * History:
  * --------
  *  2004-06-06  updated to the new DB api (andrei)
  */

#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../str.h"
#include "cpl_db.h"

#define TABLE_VERSION 1

static db1_con_t* db_hdl=0;
static db_func_t cpl_dbf;

str cpl_username_col = str_init("username");
str cpl_domain_col = str_init("domain");
str cpl_xml_col  = str_init("cpl_xml");
str cpl_bin_col  = str_init("cpl_bin");


int cpl_db_bind(const str* db_url, const str *db_table)
{
	if (db_bind_mod(db_url, &cpl_dbf )) {
		LM_CRIT("cannot bind to database module! "
		    "Did you forget to load a database module ?\n");
		return -1;
	}
	
	/* CPL module uses all database functions */
	if (!DB_CAPABILITY(cpl_dbf, DB_CAP_ALL)) {
		LM_CRIT("Database modules does not "
		    "provide all functions needed by cpl-c module\n");
		return -1;
	}

	if ( cpl_db_init( db_url, db_table) )
		return -1;

	if(db_check_table_version(&cpl_dbf, db_hdl, db_table, TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		cpl_db_close();
		return -1;
	}

	cpl_db_close();
	return 0;
}



int cpl_db_init(const str* db_url, const str* db_table)
{
	if (cpl_dbf.init==0){
		LM_CRIT("BUG - unbound database module\n");
		return -1;
	}

	db_hdl=cpl_dbf.init(db_url);
	
	if (db_hdl==0){
		LM_CRIT("cannot initialize database connection\n");
		return -1;
	}
	
	if (cpl_dbf.use_table(db_hdl, db_table)<0) {
		LM_CRIT("cannot select table \"%.*s\"\n",db_table->len, db_table->s);
		cpl_db_close();
		return -1;
	}

	return 0;
}


void cpl_db_close(void)
{
	if (db_hdl && cpl_dbf.close){
		cpl_dbf.close(db_hdl);
		db_hdl=0;
	}
}


/* gets from database the cpl script in binary format; the returned script is
 * allocated in shared memory
 * Returns:  1 - success
 *          -1 - error
 */
int get_user_script(str *username, str *domain, str *script, str* key)
{
	db_key_t   keys_cmp[2];
	db_key_t   keys_ret[1];
	db_val_t   vals[2];
	db1_res_t   *res = NULL;
	int n;

	keys_cmp[0] = &cpl_username_col;
	keys_cmp[1] = &cpl_domain_col;
	keys_ret[0] = key;

	LM_DBG("fetching script for user <%.*s>\n",
		username->len,username->s);
	vals[0].type = DB1_STR;
	vals[0].nul  = 0;
	vals[0].val.str_val = *username;
	n = 1;
	if (domain) {
		vals[1].type = DB1_STR;
		vals[1].nul  = 0;
		vals[1].val.str_val = *domain;
		n++;
	}

	if (cpl_dbf.query(db_hdl, keys_cmp, 0, vals, keys_ret, n, 1, NULL, &res)
			< 0){
		LM_ERR("db_query failed\n");
		goto error;
	}

	if (res->n==0) {
		LM_DBG("user <%.*s> not found in db -> probably "
			"he has no script\n",username->len, username->s);
		script->s = 0;
		script->len = 0;
	} else {
		if (res->rows[0].values[0].nul) {
			LM_DBG("user <%.*s> has a NULL script\n",
				username->len, username->s);
			script->s = 0;
			script->len = 0;
		} else {
			LM_DBG("we got the script len=%d\n",
				res->rows[0].values[0].val.blob_val.len);
			script->len = res->rows[0].values[0].val.blob_val.len;
			script->s = shm_malloc( script->len );
			if (!script->s) {
				LM_ERR("no free sh_mem\n");
				goto error;
			}
			memcpy( script->s, res->rows[0].values[0].val.blob_val.s,
				script->len);
		}
	}

	cpl_dbf.free_result( db_hdl, res);
	return 1;
error:
	if (res)
		cpl_dbf.free_result( db_hdl, res);
	script->s = 0;
	script->len = 0;
	return -1;
}



/* inserts into database a cpl script in XML format(xml) along with its binary
 * format (bin)
 * Returns:  1 - success
 *          -1 - error
 */
int write_to_db(str *username, str *domain, str *xml, str *bin)
{
	db_key_t   keys[4];
	db_val_t   vals[4];
	db1_res_t   *res = NULL;
	int n;

	/* lets see if the user is already in database */
	keys[2] = &cpl_username_col;
	vals[2].type = DB1_STR;
	vals[2].nul  = 0;
	vals[2].val.str_val = *username;
	n = 1;
	if (domain) {
		keys[3] = &cpl_domain_col;
		vals[3].type = DB1_STR;
		vals[3].nul  = 0;
		vals[3].val.str_val = *domain;
		n++;
	}
	if (cpl_dbf.query(db_hdl, keys+2, 0, vals+2, keys+2, n, 1, NULL, &res)<0) {
		LM_ERR("db_query failed\n");
		goto error;
	}
	if (res->n>1) {
		LM_ERR("Inconsistent CPL database:"
			" %d records for user %.*s\n",res->n,username->len,username->s);
		goto error;
	}

	/* cpl text */
	keys[0] = &cpl_xml_col;
	vals[0].type = DB1_BLOB;
	vals[0].nul  = 0;
	vals[0].val.blob_val.s = xml->s;
	vals[0].val.blob_val.len = xml->len;
	n++;
	/* cpl bin */
	keys[1] = &cpl_bin_col;
	vals[1].type = DB1_BLOB;
	vals[1].nul  = 0;
	vals[1].val.blob_val.s = bin->s;
	vals[1].val.blob_val.len = bin->len;
	n++;
	/* insert or update ? */
	if (res->n==0) {
		LM_DBG("no user %.*s in CPL database->insert\n",
			username->len,username->s);
		if (cpl_dbf.insert(db_hdl, keys, vals, n) < 0) {
			LM_ERR("insert failed !\n");
			goto error;
		}
	} else {
		LM_DBG("user %.*s already in CPL database ->"
			" update\n",username->len,username->s);
		if (cpl_dbf.update(db_hdl, keys+2, 0, vals+2, keys, vals, n-2, 2) < 0) {
			LM_ERR("update failed !\n");
			goto error;
		}
	}

	return 1;
error:
	return -1;
}



/* delete from database the entity record for a given user - if a user has no
 * script, he will be removed completely from db; users without script are not
 * allowed into db ;-)
 * Returns:  1 - success
 *          -1 - error
 */
int rmv_from_db(str *username, str *domain)
{
	db_key_t   keys[2];
	db_val_t   vals[2];
	int n;

	/* username */
	keys[0] = &cpl_username_col;
	vals[0].type = DB1_STR;
	vals[0].nul  = 0;
	vals[0].val.str_val = *username;
	n = 1;
	if (domain) {
		keys[1] = &cpl_domain_col;
		vals[1].type = DB1_STR;
		vals[1].nul  = 0;
		vals[1].val.str_val = *domain;
		n++;
	}

	if (cpl_dbf.delete(db_hdl, keys, NULL, vals, n) < 0) {
		LM_ERR("failed to delete script for "
			"user \"%.*s\"\n",username->len,username->s);
		return -1;
	}

	return 1;
}

