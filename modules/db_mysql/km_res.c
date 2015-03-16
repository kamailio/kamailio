/* 
 * MySQL module result related functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
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


/*! \file
 *  \brief DB_MYSQL :: Result related functions
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */


#include <string.h>
#include <mysql/mysql.h>
#include "../../lib/srdb1/db_res.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "km_row.h"
#include "km_my_con.h"
#include "km_res.h"


/*!
 * \brief Get and convert columns from a result
 *
 * Get and convert columns from a result, fills the result structure
 * with data from the database.
 * \param _h database connection
 * \param _r database result set
 * \return 0 on success, negative on failure
 */
int db_mysql_get_columns(const db1_con_t* _h, db1_res_t* _r)
{
	int col;
	MYSQL_FIELD* fields;

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	RES_COL_N(_r) = mysql_field_count(CON_CONNECTION(_h));
	if (!RES_COL_N(_r)) {
		LM_ERR("no columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d columns returned from the query\n", RES_COL_N(_r));
	}
	
	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		RES_COL_N(_r) = 0;
		LM_ERR("could not allocate columns\n");
		return -3;
	}

	fields = mysql_fetch_fields(RES_RESULT(_r));
	for(col = 0; col < RES_COL_N(_r); col++) {
		RES_NAMES(_r)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[col]) {
			LM_ERR("no private memory left\n");
			db_free_columns(_r);
			return -4;
		}
		LM_DBG("allocate %lu bytes for RES_NAMES[%d] at %p\n",
				(unsigned long)sizeof(str), col, RES_NAMES(_r)[col]);

		/* The pointer that is here returned is part of the result structure. */
		RES_NAMES(_r)[col]->s = fields[col].name;
		RES_NAMES(_r)[col]->len = strlen(fields[col].name);

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_r)[col], col,
				RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s);

		switch(fields[col].type) {
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_INT24:
			case MYSQL_TYPE_TIMESTAMP:
				LM_DBG("use DB1_INT result type\n");
				RES_TYPES(_r)[col] = DB1_INT;
				break;

			case MYSQL_TYPE_LONGLONG:
				LM_DBG("use DB1_BIGINT result type\n");
				RES_TYPES(_r)[col] = DB1_BIGINT;
				break;

			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
				LM_DBG("use DB1_DOUBLE result type\n");
				RES_TYPES(_r)[col] = DB1_DOUBLE;
				break;

			case MYSQL_TYPE_DATETIME:
				LM_DBG("use DB1_DATETIME result type\n");
				RES_TYPES(_r)[col] = DB1_DATETIME;
				break;

			case MYSQL_TYPE_BLOB:
				LM_DBG("use DB1_BLOB result type\n");
				RES_TYPES(_r)[col] = DB1_BLOB;
				break;

			case FIELD_TYPE_SET:
				LM_DBG("use DB1_BITMAP result type\n");
				RES_TYPES(_r)[col] = DB1_BITMAP;
				break;

			case MYSQL_TYPE_DECIMAL:
			#if MYSQL_VERSION_ID > 49999
			case MYSQL_TYPE_NEWDECIMAL:
			#endif
			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_VAR_STRING:
				LM_DBG("use DB1_STRING result type\n");
				RES_TYPES(_r)[col] = DB1_STRING;
				break;

			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use DB1_STRING as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, fields[col].type);
				RES_TYPES(_r)[col] = DB1_STRING;
				break;
		}
	}
	return 0;
}


/*!
 * \brief Convert rows from mysql to db API representation
 * \param _h database connection
 * \param _r database result set
 * \return 0 on success, negative on failure
 */
static inline int db_mysql_convert_rows(const db1_con_t* _h, db1_res_t* _r)
{
	int row;

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	RES_ROW_N(_r) = mysql_num_rows(RES_RESULT(_r));
	if (!RES_ROW_N(_r)) {
		LM_DBG("no rows returned from the query\n");
		RES_ROWS(_r) = 0;
		return 0;
	}

	if (db_allocate_rows(_r) < 0) {
		LM_ERR("could not allocate rows");
		RES_ROW_N(_r) = 0;
		return -2;
	}

	for(row = 0; row < RES_ROW_N(_r); row++) {
		RES_ROW(_r) = mysql_fetch_row(RES_RESULT(_r));
		if (!RES_ROW(_r)) {
			LM_ERR("driver error: %s\n", mysql_error(CON_CONNECTION(_h)));
			RES_ROW_N(_r) = row;
			db_free_rows(_r);
			return -3;
		}
		if (db_mysql_convert_row(_h, _r, &(RES_ROWS(_r)[row])) < 0) {
			LM_ERR("error while converting row #%d\n", row);
			RES_ROW_N(_r) = row;
			db_free_rows(_r);
			return -4;
		}
	}
	return 0;
}


/*!
 * \brief Fill the result structure with data from database
 * \param _h database connection
 * \param _r database result
 * \return 0 on success, negative on failure
 */
int db_mysql_convert_result(const db1_con_t* _h, db1_res_t* _r)
{
	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (db_mysql_get_columns(_h, _r) < 0) {
		LM_ERR("error while getting column names\n");
		return -2;
	}

	if (db_mysql_convert_rows(_h, _r) < 0) {
		LM_ERR("error while converting rows\n");
		db_free_columns(_r);
		return -3;
	}
	return 0;
}


/*!
 * \brief Allocate new result set with private structure
 * \return db1_res_t object on success, NULL on failure
 */
db1_res_t* db_mysql_new_result(void)
{
	db1_res_t* obj;

	obj = db_new_result();
	if (!obj)
		return NULL;
	RES_PTR(obj) = pkg_malloc(sizeof(struct my_res));
	if (!RES_PTR(obj)) {
		db_free_result(obj);
		return NULL;
	}
	return obj;
}
