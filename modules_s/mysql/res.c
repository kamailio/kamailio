/* 
 * $Id$ 
 *
 * MySQL module result related functions
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


#include <mysql.h>
#include <mem.h>
#include <dprint.h>
#include "row.h"
#include "defs.h"
#include "con_mysql.h"
#include "res.h"


/*
 * Get and convert columns from a result
 */
static inline int get_columns(db_con_t* _h, db_res_t* _r)
{
	int n, i;
	MYSQL_FIELD* fields;
#ifdef PARANOID
	if ((!_h) || (!_r)) {
		LOG(L_ERR, "get_columns(): Invalid parameter\n");
		return -1;
	}
#endif
	n = mysql_field_count(CON_CONNECTION(_h));
	if (!n) {
		LOG(L_ERR, "get_columns(): No columns\n");
		return -2;
	}
	
        RES_NAMES(_r) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * n);
	if (!RES_NAMES(_r)) {
		LOG(L_ERR, "get_columns(): No memory left\n");
		return -3;
	}

	RES_TYPES(_r) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * n);
	if (!RES_TYPES(_r)) {
		LOG(L_ERR, "get_columns(): No memory left\n");
		pkg_free(RES_NAMES(_r));
		return -4;
	}

	RES_COL_N(_r) = n;

	fields = mysql_fetch_fields(CON_RESULT(_h));
	for(i = 0; i < n; i++) {
		RES_NAMES(_r)[i] = fields[i].name;
		switch(fields[i].type) {
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_INT24:
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
 * Release memory used by rows
 */
static inline int free_rows(db_res_t* _r)
{
	int i;
#ifdef PARANOID
	if (!_r) {
		LOG(L_ERR, "free_rows(): Invalid parameter value\n");
		return -1;
	}
#endif
	for(i = 0; i < RES_ROW_N(_r); i++) {
		free_row(&(RES_ROWS(_r)[i]));
	}
	if (RES_ROWS(_r)) pkg_free(RES_ROWS(_r));
	return 0;
}


/*
 * Convert rows from mysql to db API representation
 */
static inline int convert_rows(db_con_t* _h, db_res_t* _r)
{
	int n, i;
#ifdef PARANOID
	if ((!_h) || (!_r)) {
		LOG(L_ERR, "convert_rows(): Invalid parameter\n");
		return -1;
	}
#endif
	n = mysql_num_rows(CON_RESULT(_h));
	RES_ROW_N(_r) = n;
	if (!n) {
		RES_ROWS(_r) = 0;
		return 0;
	}
	RES_ROWS(_r) = (struct db_row*)pkg_malloc(sizeof(db_row_t) * n);
	if (!RES_ROWS(_r)) {
		LOG(L_ERR, "convert_rows(): No memory left\n");
		return -2;
	}

	for(i = 0; i < n; i++) {
		CON_ROW(_h) = mysql_fetch_row(CON_RESULT(_h));
		if (!CON_ROW(_h)) {
			LOG(L_ERR, "convert_rows(): %s\n", mysql_error(CON_CONNECTION(_h)));
			RES_ROW_N(_r) = i;
			free_rows(_r);
			return -3;
		}
		if (convert_row(_h, _r, &(RES_ROWS(_r)[i])) < 0) {
			LOG(L_ERR, "convert_rows(): Error while converting row #%d\n", i);
			RES_ROW_N(_r) = i;
			free_rows(_r);
			return -4;
		}
	}
	return 0;
}


/*
 * Release memory used by columns
 */
static inline int free_columns(db_res_t* _r)
{
#ifdef PARANOID
	if (!_r) {
		LOG(L_ERR, "free_columns(): Invalid parameter\n");
		return -1;
	}
#endif
	if (RES_NAMES(_r)) pkg_free(RES_NAMES(_r));
	if (RES_TYPES(_r)) pkg_free(RES_TYPES(_r));
	return 0;
}


/*
 * Create a new result structure and initialize it
 */
db_res_t* new_result(void)
{
	db_res_t* r;
	r = (db_res_t*)pkg_malloc(sizeof(db_res_t));
	if (!r) {
		LOG(L_ERR, "new_result(): No memory left\n");
		return 0;
	}
	RES_NAMES(r) = 0;
	RES_TYPES(r) = 0;
	RES_COL_N(r) = 0;
	RES_ROWS(r) = 0;
	RES_ROW_N(r) = 0;
	return r;
}


/*
 * Fill the structure with data from database
 */
int convert_result(db_con_t* _h, db_res_t* _r)
{
#ifdef PARANOID
	if ((!_h) || (!_r)) {
		LOG(L_ERR, "convert_result(): Invalid parameter\n");
		return -1;
	}
#endif
	if (get_columns(_h, _r) < 0) {
		LOG(L_ERR, "convert_result(): Error while getting column names\n");
		return -2;
	}

	if (convert_rows(_h, _r) < 0) {
		LOG(L_ERR, "convert_result(): Error while converting rows\n");
		free_columns(_r);
		return -3;
	}
	return 0;
}


/*
 * Release memory used by a result structure
 */
int free_result(db_res_t* _r)
{
#ifdef PARANOID
	if (!_r) {
		LOG(L_ERR, "free_result(): Invalid parameter\n");
		return -1;
	}
#endif
	free_columns(_r);
	free_rows(_r);
	pkg_free(_r);
	return 0;
}
