/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
  *  2004-06-06  updated to the new DB api (andrei)
  */

#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "cpl_db.h"

static db_con_t* db_hdl=0;
static db_func_t cpl_dbf;



int cpl_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &cpl_dbf )) {
		LOG(L_CRIT, "ERROR:cpl_db_bind: cannot bind to database module! "
		"Did you forget to load a database module ?\n");
		return -1;
	}
	return 0;
}



int cpl_db_init(char* db_url, char* db_table)
{
	if (cpl_dbf.init==0){
		LOG(L_CRIT, "BUG: cpl_db_init: unbound database module\n");
		return -1;
	}
	db_hdl=cpl_dbf.init(db_url);
	if (db_hdl==0){
		LOG(L_CRIT,"ERROR:cpl_db_init: cannot initialize database "
			"connection\n");
		goto error;
	}
	if (cpl_dbf.use_table(db_hdl, db_table)<0) {
		LOG(L_CRIT,"ERROR:cpl_db_init: cannot select table \"%s\"\n",db_table);
		goto error;
	}
	return 0;
error:
	if (db_hdl){
		cpl_dbf.close(db_hdl);
		db_hdl=0;
	}
	return -1;
}

void cpl_db_close()
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
int get_user_script(str *user, str *script, const char* key)
{
	db_key_t   keys_cmp[] = {"user"};
	db_key_t   keys_ret[] = { key };
	db_val_t   vals[1];
	db_res_t   *res = 0 ;

	DBG("DEBUG:get_user_script: fetching script for user <%s>\n",user->s);
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = user->s;
	if (cpl_dbf.query(db_hdl, keys_cmp, 0, vals, keys_ret, 1, 1, NULL, &res)
			< 0){
		LOG(L_ERR,"ERROR:cpl-c:get_user_script: db_query failed\n");
		goto error;
	}

	if (res->n==0) {
		DBG("DEBUG:get_user_script: user <%.*s> not found in db -> probably "
			"he has no script\n",user->len, user->s);
		script->s = 0;
		script->len = 0;
	} else {
		if (res->rows[0].values[0].nul) {
			DBG("DEBUG:get_user_script: user <%.*s> has a NULL script\n",
				user->len, user->s);
			script->s = 0;
			script->len = 0;
		} else {
			DBG("DEBUG:get_user_script: we got the script len=%d\n",
				res->rows[0].values[0].val.blob_val.len);
			script->len = res->rows[0].values[0].val.blob_val.len;
			script->s = shm_malloc( script->len );
			if (!script->s) {
				LOG(L_ERR,"ERROR:cpl-c:get_user_script: no free sh_mem\n");
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
int write_to_db(char *usr, str *xml, str *bin)
{
	db_key_t   keys[] = {"user","cpl_xml","cpl_bin"};
	db_val_t   vals[3];
	db_res_t   *res;

	/* lets see if the user is already in database */
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = usr;
	if (cpl_dbf.query(db_hdl, keys, 0, vals, keys, 1, 1, NULL, &res) < 0) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: db_query failed\n");
		goto error;
	}
	if (res->n>1) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: Inconsistent CPL database:"
			" %d records for user %s\n",res->n,usr);
		goto error;
	}

	/* username */
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = usr;
	/* cpl text */
	vals[1].type = DB_BLOB;
	vals[1].nul  = 0;
	vals[1].val.blob_val.s = xml->s;
	vals[1].val.blob_val.len = xml->len;
	/* cpl bin */
	vals[2].type = DB_BLOB;
	vals[2].nul  = 0;
	vals[2].val.blob_val.s = bin->s;
	vals[2].val.blob_val.len = bin->len;
	/* insert or update ? */
	if (res->n==0) {
		DBG("DEBUG:cpl:write_to_db:No user %s in CPL database->insert\n",usr);
		if (cpl_dbf.insert(db_hdl, keys, vals, 3) < 0) {
			LOG(L_ERR,"ERROR:cpl:write_to_db: insert failed !\n");
			goto error;
		}
	} else {
		DBG("DEBUG:cpl:write_to_db:User %s already in CPL database ->"
			" update\n",usr);
		if (cpl_dbf.update(db_hdl, keys, 0, vals, keys+1, vals+1, 1, 2) < 0) {
			LOG(L_ERR,"ERROR:cpl:write_to_db: update failed !\n");
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
int rmv_from_db(char *usr)
{
	db_key_t   keys[] = {"user"};
	db_val_t   vals[1];

	/* username */
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = usr;

	if (cpl_dbf.delete(db_hdl, keys, NULL, vals, 1) < 0) {
		LOG(L_ERR,"ERROR:cpl-c:rmv_from_db: error when deleting script for "
			"user \"%s\"\n",usr);
		return -1;
	}

	return 1;
}

