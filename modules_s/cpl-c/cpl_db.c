/*
 * $Id$
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
 */

#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "cpl_db.h"




/* gets from database the cpl script in binary format; the returned script is
 * allocated in shared memory
 * Returns:  1 - success
 *          -1 - error
 */
int get_user_script( db_con_t *db_hdl, str *user, str *script, char* key)
{
	db_key_t   keys_cmp[] = {"user"};
	db_key_t   keys_ret[] = { key };
	db_val_t   vals[1];
	db_res_t   *res = 0 ;
	char       tmp;

	/* it shouldn't be aproblem overwriting a \0 at the end of user name; the
	 * user name is part of the sip_msg struct received -  that is allocated in
	 * process private mem - no sync problems; also there is all the time some
	 * extra characters after the user name - at least the EOH */
	tmp = user->s[user->len];
	user->s[user->len] = 0;
	DBG("DEBUG:get_user_script: fetching script for user <%s>\n",user->s);
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = user->s;
	if (db_query(db_hdl, keys_cmp, 0, vals, keys_ret, 1, 1, NULL, &res) < 0) {
		LOG(L_ERR,"ERROR:cpl-c:get_user_script: db_query failed\n");
		goto error;
	}
	user->s[user->len] = tmp;

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

	db_free_query( db_hdl, res);
	return 1;
error:
	if (res)
		db_free_query( db_hdl, res);
	script->s = 0;
	script->len = 0;
	return -1;
}



/* inserts into database a cpl script in XML format(xml) along with its binary
 * format (bin)
 * Returns:  1 - success
 *          -1 - error
 */
int write_to_db(db_con_t *db_con, char *usr, str *xml, str *bin)
{
	db_key_t   keys[] = {"user","cpl_xml","cpl_bin"};
	db_val_t   vals[3];
	db_res_t   *res;

	/* lets see if the user is already in database */
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = usr;
	if (db_query(db_con, keys, 0, vals, keys, 1, 1, NULL, &res) < 0) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: db_query failed\n");
		goto error;
	}
	if (res->n>1) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: Incosistent CPL database:"
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
		DBG("DEBUG:cpl:write_to_db:No user %s in CPL databse->insert\n",usr);
		if (db_insert(db_con, keys, vals, 3) < 0) {
			LOG(L_ERR,"ERROR:cpl:write_to_db: insert failed !\n");
			goto error;
		}
	} else {
		DBG("DEBUG:cpl:write_to_db:User %s already in CPL database ->"
			" update\n",usr);
		if (db_update(db_con, keys, 0, vals, keys+1, vals+1, 1, 2) < 0) {
			LOG(L_ERR,"ERROR:cpl:write_to_db: updare failed !\n");
			goto error;
		}
	}

	return 1;
error:
	return -1;
}



/* delete from database the entiry record for a given user - if a user has no
 * script, he will be removed complitly from db; users without script are not
 * allowed into db ;-)
 * Returns:  1 - success
 *          -1 - error
 */
int rmv_from_db(db_con_t *db_con, char *usr)
{
	db_key_t   keys[] = {"user"};
	db_val_t   vals[1];

	/* username */
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = usr;

	if (db_delete(db_con, keys, NULL, vals, 1) < 0) {
		LOG(L_ERR,"ERROR:cpl-c:rmv_from_db: error when deleting script for "
			"user \"%s\"\n",usr);
		return -1;
	}

	return 1;
}

