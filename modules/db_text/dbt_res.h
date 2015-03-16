/*
 * DBText module core functions
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
 */


#ifndef _DBT_RES_H_
#define _DBT_RES_H_

#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_res.h"

#include "dbt_lib.h"

typedef struct _dbt_result
{
	int nrcols;
	int nrrows;
	dbt_column_p colv;
	dbt_row_p rows;
} dbt_result_t, *dbt_result_p;

typedef struct _dbt_con
{
	dbt_cache_p con;
	int affected;
} dbt_con_t, *dbt_con_p;

#define DBT_CON_CONNECTION(db_con) (((dbt_con_p)((db_con)->tail))->con)

dbt_result_p dbt_result_new(dbt_table_p, int*, int);
int dbt_result_free(dbt_result_p);
int dbt_row_match(dbt_table_p _dtp, dbt_row_p _drp, int* _lkey,
				 db_op_t* _op, db_val_t* _v, int _n);
int dbt_result_extract_fields(dbt_table_p _dtp, dbt_row_p _drp,
				int* lres, dbt_result_p _dres);
int dbt_result_print(dbt_result_p _dres);

int* dbt_get_refs(dbt_table_p, db_key_t*, int);
int dbt_cmp_val(dbt_val_p _vp, db_val_t* _v);
dbt_row_p dbt_result_new_row(dbt_result_p _dres);

int dbt_parse_orderbyclause(db_key_t **_o_k, char **_o_op, int *_o_n, db_key_t _o);
int dbt_mangle_columnselection(int **_lres, int *_nc, int *_o_nc, int *_o_l, int _o_n);
int dbt_sort_result(dbt_result_p _dres, int *_o_l, char *_o_op, int _o_n, int *_lres, int _nc);
void dbt_project_result(dbt_result_p _dres, int _o_nc);

#endif

