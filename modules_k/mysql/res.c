/* 
 * $Id$ 
 *
 * MySQL module result related functions
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


#include <string.h>
#include <mysql/mysql.h>
#include "../../db/db_res.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "row.h"
#include "my_con.h"
#include "res.h"


/*
 * Get and convert columns from a result
 */
int db_mysql_get_columns(const db_con_t* _h, db_res_t* _r)
{
	int n, i;
	MYSQL_FIELD* fields;

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	n = mysql_field_count(CON_CONNECTION(_h));
	if (!n) {
		LM_ERR("no columns\n");
		return -2;
	}
	
    RES_NAMES(_r) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * n);
	if (!RES_NAMES(_r)) {
		LM_ERR("no private memory left\n");
		return -3;
	}

	RES_TYPES(_r) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * n);
	if (!RES_TYPES(_r)) {
		LM_ERR("no private memory left\n");
		pkg_free(RES_NAMES(_r));
		return -4;
	}

	RES_COL_N(_r) = n;

	fields = mysql_fetch_fields(CON_RESULT(_h));
	for(i = 0; i < n; i++) {
		RES_NAMES(_r)[i] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[i]) {
			LM_ERR("no private memory left\n");
			pkg_free(RES_NAMES(_r));
			pkg_free(RES_TYPES(_r));
			// FIXME we should also free all previous allocated RES_NAMES[i]
			return -5;
		}

		RES_NAMES(_r)[i]->s = fields[i].name;
		RES_NAMES(_r)[i]->len = strlen(fields[i].name);

		switch(fields[i].type) {
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_LONGLONG:
		case FIELD_TYPE_DECIMAL:
		case FIELD_TYPE_TIMESTAMP:
			RES_TYPES(_r)[i] = DB_INT;
			break;

		case FIELD_TYPE_FLOAT:
		case FIELD_TYPE_DOUBLE:
			RES_TYPES(_r)[i] = DB_DOUBLE;
			break;

		case FIELD_TYPE_DATETIME:
			RES_TYPES(_r)[i] = DB_DATETIME;
			break;

		case FIELD_TYPE_BLOB:
		case FIELD_TYPE_TINY_BLOB:
		case FIELD_TYPE_MEDIUM_BLOB:
		case FIELD_TYPE_LONG_BLOB:
			RES_TYPES(_r)[i] = DB_BLOB;
			break;

		case FIELD_TYPE_SET:
			RES_TYPES(_r)[i] = DB_BITMAP;
			break;

		default:
			RES_TYPES(_r)[i] = DB_STRING;
			break;
		}		
	}
	return 0;
}


/*
 * Convert rows from mysql to db API representation
 */
static inline int db_mysql_convert_rows(const db_con_t* _h, db_res_t* _r)
{
	int n, i;

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	n = mysql_num_rows(CON_RESULT(_h));
	RES_ROW_N(_r) = n;
	if (!n) {
		RES_ROWS(_r) = 0;
		return 0;
	}
	RES_ROWS(_r) = (struct db_row*)pkg_malloc(sizeof(db_row_t) * n);
	if (!RES_ROWS(_r)) {
		LM_ERR("no private memory left\n");
		return -2;
	}

	for(i = 0; i < n; i++) {
		CON_ROW(_h) = mysql_fetch_row(CON_RESULT(_h));
		if (!CON_ROW(_h)) {
			LM_ERR("driver error: %s\n", mysql_error(CON_CONNECTION(_h)));
			RES_ROW_N(_r) = i;
			db_free_rows(_r);
			return -3;
		}
		if (db_mysql_convert_row(_h, _r, &(RES_ROWS(_r)[i])) < 0) {
			LM_ERR("error while converting row #%d\n", i);
			RES_ROW_N(_r) = i;
			db_free_rows(_r);
			return -4;
		}
	}
	return 0;
}


/*
 * Fill the structure with data from database
 */
int db_mysql_convert_result(const db_con_t* _h, db_res_t* _r)
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


/*
 * Release memory used by a result structure
 */
int db_mysql_free_dbresult(db_res_t* _r)
{
	if (!_r) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	db_free_columns(_r);
	db_free_rows(_r);
	pkg_free(_r);
	return 0;
}
