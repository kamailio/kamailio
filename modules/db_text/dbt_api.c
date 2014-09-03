/*
 * $Id$
 *
 * DBText library
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
 *
 * History:
 * --------
 * 2003-02-05  created by Daniel
 * 
 */

#include <string.h>

#include "../../lib/srdb1/db.h"
#include "../../mem/mem.h"

#include "dbt_res.h"
#include "dbt_api.h"

int dbt_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}


/*
 * Get and convert columns from a result
 */
static int dbt_get_columns(db1_con_t* _h, db1_res_t* _r)
{
	int col;
	
	if (!_h || !_r) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	
	RES_COL_N(_r) = DBT_CON_RESULT(_h)->nrcols;
	if (!RES_COL_N(_r)) {
		LM_ERR("no columns\n");
		return -2;
	}
	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		LM_ERR("could not allocate columns");
		return -3;
	}

	for(col = 0; col < RES_COL_N(_r); col++) {
		/* 
		 * Its would be not necessary to allocate here new memory, because of
		 * the internal structure of the db_text module. But we do this anyway
		 * to stay confirm to the other database modules.
		 */
		RES_NAMES(_r)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[col]) {
			LM_ERR("no private memory left\n");
			db_free_columns(_r);
			return -4;
		}
		LM_DBG("allocate %d bytes for RES_NAMES[%d] at %p",
				(int)sizeof(str), col,
				RES_NAMES(_r)[col]);
		RES_NAMES(_r)[col]->s = DBT_CON_RESULT(_h)->colv[col].name.s;
		RES_NAMES(_r)[col]->len = DBT_CON_RESULT(_h)->colv[col].name.len;

		switch(DBT_CON_RESULT(_h)->colv[col].type)
		{
			case DB1_STR:
			case DB1_STRING:
			case DB1_BLOB:
			case DB1_INT:
			case DB1_DATETIME:
			case DB1_DOUBLE:
				RES_TYPES(_r)[col] = DBT_CON_RESULT(_h)->colv[col].type;
			break;
			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use STR as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, DBT_CON_RESULT(_h)->colv[col].type);
				RES_TYPES(_r)[col] = DB1_STR;
			break;
		}
	}
	return 0;
}

/*
 * Convert a row from result into db API representation
 */
static int dbt_convert_row(db1_con_t* _h, db1_res_t* _res, db_row_t* _r)
{
	int i;
	if (!_h || !_r || !_res) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_allocate_row(_res, _r) != 0) {
		LM_ERR("could not allocate row");
		return -2;
	}

	for(i = 0; i < RES_COL_N(_res); i++) {
		(ROW_VALUES(_r)[i]).nul = DBT_CON_ROW(_h)->fields[i].nul;
		switch(RES_TYPES(_res)[i])
		{
			case DB1_INT:
				VAL_INT(&(ROW_VALUES(_r)[i])) = 
						DBT_CON_ROW(_h)->fields[i].val.int_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_INT;
			break;

			case DB1_BIGINT:
				LM_ERR("BIGINT not supported");
				return -1;

			case DB1_DOUBLE:
				VAL_DOUBLE(&(ROW_VALUES(_r)[i])) = 
						DBT_CON_ROW(_h)->fields[i].val.double_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_DOUBLE;
			break;

			case DB1_STRING:
				VAL_STR(&(ROW_VALUES(_r)[i])).s = 
						DBT_CON_ROW(_h)->fields[i].val.str_val.s;
				VAL_STR(&(ROW_VALUES(_r)[i])).len =
						DBT_CON_ROW(_h)->fields[i].val.str_val.len;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_STRING;
				VAL_FREE(&(ROW_VALUES(_r)[i])) = 0;
			break;

			case DB1_STR:
				VAL_STR(&(ROW_VALUES(_r)[i])).s = 
						DBT_CON_ROW(_h)->fields[i].val.str_val.s;
				VAL_STR(&(ROW_VALUES(_r)[i])).len =
						DBT_CON_ROW(_h)->fields[i].val.str_val.len;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_STR;
				VAL_FREE(&(ROW_VALUES(_r)[i])) = 0;
			break;

			case DB1_DATETIME:
				VAL_INT(&(ROW_VALUES(_r)[i])) = 
						DBT_CON_ROW(_h)->fields[i].val.int_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_DATETIME;
			break;

			case DB1_BLOB:
				VAL_STR(&(ROW_VALUES(_r)[i])).s =
						DBT_CON_ROW(_h)->fields[i].val.str_val.s;
				VAL_STR(&(ROW_VALUES(_r)[i])).len =
						DBT_CON_ROW(_h)->fields[i].val.str_val.len;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_BLOB;
				VAL_FREE(&(ROW_VALUES(_r)[i])) = 0;
			break;

			case DB1_BITMAP:
				VAL_INT(&(ROW_VALUES(_r)[i])) =
					DBT_CON_ROW(_h)->fields[i].val.bitmap_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_INT;
			break;

			default:
				LM_ERR("val type [%d] not supported", RES_TYPES(_res)[i]);
				return -1;
		}
	}
	return 0;
}


/*
 * Convert rows from internal to db API representation
 */
static int dbt_convert_rows(db1_con_t* _h, db1_res_t* _r)
{
	int col;
	dbt_row_p _rp = NULL;
	if (!_h || !_r) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	RES_ROW_N(_r) = DBT_CON_RESULT(_h)->nrrows;
	if (!RES_ROW_N(_r)) {
		return 0;
	}
	if (db_allocate_rows(_r) < 0) {
		LM_ERR("could not allocate rows");
		return -2;
	}
	col = 0;
	_rp = DBT_CON_RESULT(_h)->rows;
	while(_rp) {
		DBT_CON_ROW(_h) = _rp;
		if (!DBT_CON_ROW(_h)) {
			LM_ERR("failed to get current row\n");
			RES_ROW_N(_r) = col;
			db_free_rows(_r);
			return -3;
		}
		if (dbt_convert_row(_h, _r, &(RES_ROWS(_r)[col])) < 0) {
			LM_ERR("failed to convert row #%d\n", col);
			RES_ROW_N(_r) = col;
			db_free_rows(_r);
			return -4;
		}
		col++;
		_rp = _rp->next;
	}
	return 0;
}


/*
 * Fill the structure with data from database
 */
static int dbt_convert_result(db1_con_t* _h, db1_res_t* _r)
{
	if (!_h || !_r) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	if (dbt_get_columns(_h, _r) < 0) {
		LM_ERR("failed to get column names\n");
		return -2;
	}

	if (dbt_convert_rows(_h, _r) < 0) {
		LM_ERR("failed to convert rows\n");
		db_free_columns(_r);
		return -3;
	}
	return 0;
}

/*
 * Retrieve result set
 */
int dbt_get_result(db1_con_t* _h, db1_res_t** _r)
{
	if (!_h || !_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (!DBT_CON_RESULT(_h))
	{
		LM_ERR("failed to get result\n");
		*_r = 0;
		return -3;
	}

	*_r = db_new_result();
	if (*_r == 0) 
	{
		LM_ERR("no private memory left\n");
		return -2;
	}

	if (dbt_convert_result(_h, *_r) < 0) 
	{
		LM_ERR("failed to convert result\n");
		pkg_free(*_r);
		return -4;
	}
	
	return 0;
}
