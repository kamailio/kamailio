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
 /*
  * History:
  * --------
  *  2004-06-06  bind_dbmod takes dbf as parameter (andrei)
  */


#include "db.h"
#include "../dprint.h"
#include "../sr_module.h"
#include "../mem/mem.h"
#include "../str.h"
#include "../ut.h"



/* fills mydbf with the corresponding db module callbacks
 * returns 0 on success, -1 on error
 * on error mydbf will contain only 0s */
int bind_dbmod(char* mod, db_func_t* mydbf)
{
	char* tmp, *p;
	int len;
	db_func_t dbf;

	if (!mod) {
		LOG(L_CRIT, "BUG: bind_dbmod(): null database module name\n");
		return -1;
	}
	if (mydbf==0) {
		LOG(L_CRIT, "BUG: bind_dbmod(): null dbf parameter\n");
		return -1;
	}
	/* for safety we initialize mydbf with 0 (this will cause
	 *  a segfault immediately if someone tries to call a function
	 *  from it without checking the return code from bind_dbmod -- andrei */
	memset((void*)mydbf, 0, sizeof(db_func_t));

	p = strchr(mod, ':');
	if (p) {
		len = p - mod;
		tmp = (char*)pkg_malloc(len + 1);
		if (!tmp) {
			LOG(L_ERR, "ERROR: bind_dbmod(): No memory left\n");
			return -1;
		}
		memcpy(tmp, mod, len);
		tmp[len] = '\0';
	} else {
		tmp = mod;
	}

	dbf.use_table = (db_use_table_f)find_mod_export(tmp, "db_use_table", 2, 0);
	if (dbf.use_table == 0) goto err;

	dbf.init = (db_init_f)find_mod_export(tmp, "db_init", 1, 0);
	if (dbf.init == 0) goto err;

	dbf.close = (db_close_f)find_mod_export(tmp, "db_close", 2, 0);
	if (dbf.close == 0) goto err;

	dbf.query = (db_query_f)find_mod_export(tmp, "db_query", 2, 0);
	if (dbf.query == 0) goto err;

	dbf.raw_query = (db_raw_query_f)find_mod_export(tmp, "db_raw_query", 2, 0);
	if (dbf.raw_query == 0) goto err;

	dbf.free_result = (db_free_result_f)find_mod_export(tmp, "db_free_result", 2,
														0);
	if (dbf.free_result == 0) goto err;

	dbf.insert = (db_insert_f)find_mod_export(tmp, "db_insert", 2, 0);
	if (dbf.insert == 0) goto err;

	dbf.delete = (db_delete_f)find_mod_export(tmp, "db_delete", 2, 0);
	if (dbf.delete == 0) goto err;

	dbf.update = (db_update_f)find_mod_export(tmp, "db_update", 2, 0);
	if (dbf.update == 0) goto err;

	*mydbf=dbf; /* copy */
	return 0;

 err:
	if (tmp != mod) pkg_free(tmp);
	return -1;
}


/*
 * Get version of a table
 * If there is no row for the given table, return version 0
 */
int table_version(db_func_t* dbf, db_con_t* connection, const str* table)
{
	db_key_t key[1], col[1];
	db_val_t val[1];
	db_res_t* res;
	int ret;

	if (!dbf||!connection || !table) {
		LOG(L_CRIT, "BUG: table_version(): Invalid parameter value\n");
		return -1;
	}

	if (dbf->use_table(connection, VERSION_TABLE) < 0) {
		LOG(L_ERR, "table_version(): Error while changing table\n");
		return -1;
	}

	key[0] = TABLENAME_COLUMN;

	VAL_TYPE(val) = DB_STR;
	VAL_NULL(val) = 0;
	VAL_STR(val) = *table;
	
	col[0] = VERSION_COLUMN;
	
	if (dbf->query(connection, key, 0, val, col, 1, 1, 0, &res) < 0) {
		LOG(L_ERR, "table_version(): Error in db_query\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("table_version(): No row for table %.*s found\n", table->len, ZSW(table->s));
		return 0;
	}

	if (RES_ROW_N(res) != 1) {
		LOG(L_ERR, "table_version(): Invalid number of rows received: %d, %.*s\n", RES_ROW_N(res), table->len, ZSW(table->s));
		dbf->free_result(connection, res);
		return -1;
	}

	ret = VAL_INT(ROW_VALUES(RES_ROWS(res)));
	dbf->free_result(connection, res);
	return ret;
}
