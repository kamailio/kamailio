/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * 
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
  *  2006-10-10  Added support for retrieving the last inserted ID (Carsten Bock, BASIS AudioNet GmbH)
  */


#include "../dprint.h"
#include "../sr_module.h"
#include "../mem/mem.h"
#include "../str.h"
#include "../ut.h"
#include "db_cap.h"
#include "db_id.h"
#include "db_pool.h"
#include "db.h"



/* fills mydbf with the corresponding db module callbacks
 * returns 0 on success, -1 on error
 * on error mydbf will contain only 0s */
int bind_dbmod(char* mod, db_func_t* mydbf)
{
	char* tmp, *p;
	int len;
	db_func_t dbf;

	if (!mod) {
		LM_CRIT("null database module name\n");
		return -1;
	}
	if (mydbf==0) {
		LM_CRIT("null dbf parameter\n");
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
			LM_ERR("no private memory left\n");
			return -1;
		}
		memcpy(tmp, mod, len);
		tmp[len] = '\0';
	} else {
		tmp = mod;
	}

	dbf.cap = 0;

	     /* All modules must export db_use_table */
	dbf.use_table = (db_use_table_f)find_mod_export(tmp, "db_use_table", 2, 0);
	if (dbf.use_table == 0) {
		LM_ERR("module %s does not export db_use_table function\n", tmp);
		goto err;
	}

	     /* All modules must export db_init */
	dbf.init = (db_init_f)find_mod_export(tmp, "db_init", 1, 0);
	if (dbf.init == 0) {
		LM_ERR("module %s does not export db_init function\n", tmp);
		goto err;
	}

	     /* All modules must export db_close */
	dbf.close = (db_close_f)find_mod_export(tmp, "db_close", 2, 0);
	if (dbf.close == 0) {
		LM_ERR("module %s does not export db_close function\n", tmp);
		goto err;
	}

	dbf.query = (db_query_f)find_mod_export(tmp, "db_query", 2, 0);
	if (dbf.query) {
		dbf.cap |= DB_CAP_QUERY;
	}

	dbf.fetch_result = (db_fetch_result_f)find_mod_export(tmp,
			"db_fetch_result", 2, 0);
	if (dbf.fetch_result) {
		dbf.cap |= DB_CAP_FETCH;
	}

	dbf.raw_query = (db_raw_query_f)find_mod_export(tmp, "db_raw_query", 2, 0);
	if (dbf.raw_query) {
		dbf.cap |= DB_CAP_RAW_QUERY;
	}

	     /* Free result must be exported if DB_CAP_QUERY 
	      * or DB_CAP_RAW_QUERY is set
	      */
	dbf.free_result = (db_free_result_f)find_mod_export(tmp, "db_free_result", 2, 0);
	if ((dbf.cap & (DB_CAP_QUERY | DB_CAP_RAW_QUERY))
	    && (dbf.free_result == 0)) {
		LM_ERR("module %s supports queries but does not export free_result function\n", tmp);
		goto err;
	}

	dbf.insert = (db_insert_f)find_mod_export(tmp, "db_insert", 2, 0);
	if (dbf.insert) {
		dbf.cap |= DB_CAP_INSERT;
	}

	dbf.delete = (db_delete_f)find_mod_export(tmp, "db_delete", 2, 0);
	if (dbf.delete) {
		dbf.cap |= DB_CAP_DELETE;
	}

	dbf.update = (db_update_f)find_mod_export(tmp, "db_update", 2, 0);
	if (dbf.update) {
		dbf.cap |= DB_CAP_UPDATE;
	}

	dbf.replace = (db_replace_f)find_mod_export(tmp, "db_replace", 2, 0);
	if (dbf.replace) {
		dbf.cap |= DB_CAP_REPLACE;
	}

	dbf.last_inserted_id= (db_last_inserted_id_f)find_mod_export(tmp, "db_last_inserted_id", 1, 0);
	if (dbf.last_inserted_id) {
		dbf.cap |= DB_CAP_LAST_INSERTED_ID;
	}

	dbf.insert_update = (db_insert_update_f)find_mod_export(tmp, "db_insert_update", 2, 0);
	if (dbf.insert_update) {
		dbf.cap |= DB_CAP_INSERT_UPDATE;
	}

	*mydbf=dbf; /* copy */
	if (tmp != mod) pkg_free(tmp);
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
	db_res_t* res = NULL;
	db_val_t* ver = 0;
	int ret;

	if (!dbf||!connection || !table) {
		LM_CRIT("invalid parameter value\n");
		return -1;
	}

	if (dbf->use_table(connection, VERSION_TABLE) < 0) {
		LM_ERR("error while changing table\n");
		return -1;
	}

	key[0] = TABLENAME_COLUMN;

	VAL_TYPE(val) = DB_STR;
	VAL_NULL(val) = 0;
	VAL_STR(val) = *table;
	
	col[0] = VERSION_COLUMN;
	
	if (dbf->query(connection, key, 0, val, col, 1, 1, 0, &res) < 0) {
		LM_ERR("error in db_query\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("no row for table %.*s found\n",
			table->len, ZSW(table->s));
		return 0;
	}

	if (RES_ROW_N(res) != 1) {
		LM_ERR("invalid number of rows received:"
			" %d, %.*s\n", RES_ROW_N(res), table->len, ZSW(table->s));
		dbf->free_result(connection, res);
		return -1;
	}

	ver = ROW_VALUES(RES_ROWS(res));
	if ( VAL_TYPE(ver)!=DB_INT || VAL_NULL(ver) ) {
		LM_ERR("invalid type (%d) or nul (%d) version "
			"columns for %.*s\n", VAL_TYPE(ver), VAL_NULL(ver),
			table->len, ZSW(table->s));
		dbf->free_result(connection, res);
		return -1;
	}

	ret = VAL_INT(ver);
	dbf->free_result(connection, res);
	return ret;
}

/*
 * Initialize database module
 * No function should be called before this
 */
db_con_t* db_do_init(const char* url, void* (*new_connection)())
{
	struct db_id* id;
	void* con;
	db_con_t* res;

	int con_size = sizeof(db_con_t) + sizeof(void *);
	id = 0;
	res = 0;

	if (!url) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}
	if (strlen(url) > 255)
	{
		LM_ERR("SQL URL too long\n");
		return 0;
	}
	
	/* this is the root memory for this database connection. */
	res = (db_con_t*)pkg_malloc(con_size);
	if (!res) {
		LM_ERR("no private memory left\n");
		return 0;
	}
	memset(res, 0, con_size);

	id = new_db_id(url);
	if (!id) {
		LM_ERR("cannot parse URL '%s'\n", url);
		goto err;
	}

	/* Find the connection in the pool */
	con = pool_get(id);
	if (!con) {
		LM_DBG("connection %p not found in pool\n", id);
		/* Not in the pool yet */
		con = new_connection(id);
		if (!con) {
			LM_ERR("could not add connection to the pool");
			goto err;
		}
		pool_insert((struct pool_con*)con);
	} else {
		LM_DBG("connection %p found in pool\n", id);
	}

	res->tail = (unsigned long)con;
	return res;

 err:
	if (id) free_db_id(id);
	if (res) pkg_free(res);
	return 0;
}
