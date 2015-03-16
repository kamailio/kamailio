/*
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
static int dbt_get_columns(db1_res_t* _r, dbt_result_p _dres)
{
	int col;
	
	if (!_r || !_dres) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	
	RES_COL_N(_r) = _dres->nrcols;
	if (!RES_COL_N(_r)) {
		LM_ERR("no columns\n");
		return -2;
	}
	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		LM_ERR("could not allocate columns\n");
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
		LM_DBG("allocate %d bytes for RES_NAMES[%d] at %p\n",
				(int)sizeof(str), col,
				RES_NAMES(_r)[col]);
		RES_NAMES(_r)[col]->s = _dres->colv[col].name.s;
		RES_NAMES(_r)[col]->len = _dres->colv[col].name.len;

		switch(_dres->colv[col].type)
		{
			case DB1_STR:
			case DB1_STRING:
			case DB1_BLOB:
			case DB1_INT:
			case DB1_DATETIME:
			case DB1_DOUBLE:
				RES_TYPES(_r)[col] = _dres->colv[col].type;
			break;
			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use STR as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, _dres->colv[col].type);
				RES_TYPES(_r)[col] = DB1_STR;
			break;
		}
	}
	return 0;
}

/*
 * Convert a row from result into db API representation
 */
static int dbt_convert_row(db1_res_t* _res, db_row_t* _r, dbt_row_p _r1)
{
	int i;
	if (!_r || !_res || !_r1) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_allocate_row(_res, _r) != 0) {
		LM_ERR("could not allocate row\n");
		return -2;
	}

	for(i = 0; i < RES_COL_N(_res); i++) {
		(ROW_VALUES(_r)[i]).nul = _r1->fields[i].nul;
		switch(RES_TYPES(_res)[i])
		{
			case DB1_INT:
				VAL_INT(&(ROW_VALUES(_r)[i])) = 
						_r1->fields[i].val.int_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_INT;
			break;

			case DB1_BIGINT:
				LM_ERR("BIGINT not supported\n");
				return -1;

			case DB1_DOUBLE:
				VAL_DOUBLE(&(ROW_VALUES(_r)[i])) = 
						_r1->fields[i].val.double_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_DOUBLE;
			break;

			case DB1_STRING:
				VAL_STR(&(ROW_VALUES(_r)[i])).s = 
						_r1->fields[i].val.str_val.s;
				VAL_STR(&(ROW_VALUES(_r)[i])).len =
						_r1->fields[i].val.str_val.len;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_STRING;
				VAL_FREE(&(ROW_VALUES(_r)[i])) = 0;
			break;

			case DB1_STR:
				VAL_STR(&(ROW_VALUES(_r)[i])).s = 
						_r1->fields[i].val.str_val.s;
				VAL_STR(&(ROW_VALUES(_r)[i])).len =
						_r1->fields[i].val.str_val.len;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_STR;
				VAL_FREE(&(ROW_VALUES(_r)[i])) = 0;
			break;

			case DB1_DATETIME:
				VAL_INT(&(ROW_VALUES(_r)[i])) = 
						_r1->fields[i].val.int_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_DATETIME;
			break;

			case DB1_BLOB:
				VAL_STR(&(ROW_VALUES(_r)[i])).s =
						_r1->fields[i].val.str_val.s;
				VAL_STR(&(ROW_VALUES(_r)[i])).len =
						_r1->fields[i].val.str_val.len;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_BLOB;
				VAL_FREE(&(ROW_VALUES(_r)[i])) = 0;
			break;

			case DB1_BITMAP:
				VAL_INT(&(ROW_VALUES(_r)[i])) =
						_r1->fields[i].val.bitmap_val;
				VAL_TYPE(&(ROW_VALUES(_r)[i])) = DB1_INT;
			break;

			default:
				LM_ERR("val type [%d] not supported\n", RES_TYPES(_res)[i]);
				return -1;
		}
	}
	return 0;
}


/*
 * Convert rows from internal to db API representation
 */
static int dbt_convert_rows(db1_res_t* _r, dbt_result_p _dres)
{
	int row;
	dbt_row_p _rp = NULL;
	if (!_r || !_dres) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	RES_ROW_N(_r) = _dres->nrrows;
	if (!RES_ROW_N(_r)) {
		return 0;
	}
	if (db_allocate_rows(_r) < 0) {
		LM_ERR("could not allocate rows\n");
		return -2;
	}
	row = 0;
	_rp = _dres->rows;
	while(_rp) {
		if (dbt_convert_row(_r, &(RES_ROWS(_r)[row]), _rp) < 0) {
			LM_ERR("failed to convert row #%d\n", row);
			RES_ROW_N(_r) = row;
			db_free_rows(_r);
			return -4;
		}
		row++;
		_rp = _rp->next;
	}
	return 0;
}


/*
 * Fill the structure with data from database
 */
static int dbt_convert_result(db1_res_t* _r, dbt_result_p _dres)
{
	if (!_r || !_dres) {
		LM_ERR("invalid parameter\n");
		return -1;
	}
	if (dbt_get_columns(_r, _dres) < 0) {
		LM_ERR("failed to get column names\n");
		return -2;
	}

	if (dbt_convert_rows(_r, _dres) < 0) {
		LM_ERR("failed to convert rows\n");
		db_free_columns(_r);
		return -3;
	}
	return 0;
}

/*
 * Retrieve result set
 */
int dbt_get_result(db1_res_t** _r, dbt_result_p _dres)
{
	if ( !_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (!_dres)
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

	if (dbt_convert_result(*_r, _dres) < 0)
	{
		LM_ERR("failed to convert result\n");
		pkg_free(*_r);
		return -4;
	}
	
	(*_r)->ptr = _dres;
	return 0;
}
