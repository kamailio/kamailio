/*
 * $Id$
 *
 * Group membership
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
 *
 * History:
 * --------
 * 2003-02-25 - created by janakj
 * 2004-06-07   updated to the new DB api, added group_db_{bind,init,close,ver}
 *               (andrei)
 *
 */


#include <string.h>
#include "../../dprint.h"               /* Logging */
#include "../../db/db.h"                /* Generic database API */
#include "../../ut.h"
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "../../parser/hf.h"            /* Header Field types */
#include "../../parser/parse_from.h"    /* From parser */
#include "../../parser/parse_uri.h"
#include "../../id.h"
#include "group.h"
#include "group_mod.h"                   /* Module parameters */


static db_con_t* db_handle = 0;   /* Database connection handle */
static db_func_t group_dbf;


/*
 * Check if username in specified header field is in a table
 */
int is_user_in(struct sip_msg* _msg, char* _id, char* _grp)
{
	db_key_t keys[3];
	db_val_t vals[3];
	db_key_t col[1];
	db_res_t* res;
	str uid;
	long id;

	keys[0] = uid_column.s;
	keys[1] = group_column.s;
	col[0] = group_column.s;
	
	id = (long)_id;

	switch(id) {
	case 1: /* $to.uid*/
		if (get_to_uid(&uid, _msg) < 0) {
			LOG(L_ERR, "group:is_user_in: Unable to get To UID\n");
			return -1;
		}
		break;
		
	case 2: /* $from.uid */
		if (get_from_uid(&uid, _msg) < 0) {
			LOG(L_ERR, "group:is_user_in: Unable to get From UID\n");
			return -1;
		}
		break;
	}

	vals[0].type = DB_STR;
	vals[0].nul = 0;
	vals[0].val.str_val = uid;

	vals[1].type = DB_STR;
	vals[1].nul = 0;
	vals[1].val.str_val = *(str*)_grp;

	if (group_dbf.use_table(db_handle, table.s) < 0) {
		LOG(L_ERR, "group:is_user_in: Error in use_table\n");
		return -5;
	}

	if (group_dbf.query(db_handle, keys, 0, vals, col, 2,
			    1, 0, &res) < 0) {
		LOG(L_ERR, "group:is_user_in: Error while querying database\n");
		return -5;
	}
	
	if (res->n == 0) {
		DBG("group:is_user_in: User is not in group '%.*s'\n", 
		    ((str*)_grp)->len, ZSW(((str*)_grp)->s));
		group_dbf.free_result(db_handle, res);
		return -6;
	} else {
		DBG("group:is_user_in: User is in group '%.*s'\n", 
		    ((str*)_grp)->len, ZSW(((str*)_grp)->s));
		group_dbf.free_result(db_handle, res);
		return 1;
	}
}


int group_db_init(char* db_url)
{
	if (group_dbf.init==0){
		LOG(L_CRIT, "BUG: group_db_bind: null dbf \n");
		goto error;
	}
	db_handle=group_dbf.init(db_url);
	if (db_handle==0){
		LOG(L_ERR, "ERROR: group_db_bind: unable to connect to the "
				"database\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


int group_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &group_dbf)<0){
		LOG(L_ERR, "ERROR: group_db_bind: unable to bind to the database"
				" module\n");
		return -1;
	}

	if (!DB_CAPABILITY(group_dbf, DB_CAP_QUERY)) {
		LOG(L_ERR, "ERROR: group_db_bind: Database module does not implement 'query' function\n");
		return -1;
	}

	return 0;
}


void group_db_close()
{
	if (db_handle && group_dbf.close){
		group_dbf.close(db_handle);
		db_handle=0;
	}
}


int group_db_ver(char* db_url, str* name)
{
	db_con_t* dbh;
	int ver;

	if (group_dbf.init==0){
		LOG(L_CRIT, "BUG: group_db_ver: unbound database\n");
		return -1;
	}
	dbh=group_dbf.init(db_url);
	if (dbh==0){
		LOG(L_ERR, "ERROR: group_db_ver: unable to open database "
				"connection\n");
		return -1;
	}
	ver=table_version(&group_dbf, dbh, name);
	group_dbf.close(dbh);
	return ver;
}
