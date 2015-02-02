/* 
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

/**
 * \file lib/srdb1/db_res.c
 * \brief Functions to manage result structures
 * \ingroup db1
 *
 * Provides some convenience macros and some memory management
 * functions for result structures.
 */

#include "db_res.h"

#include "db_row.h"
#include "../../dprint.h"
#include "../../mem/mem.h"

#include <string.h>

/*
 * Release memory used by rows
 */
int db_free_rows(db1_res_t* _r)
{
	int i;

	if (!_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if(RES_ROWS(_r)){
		LM_DBG("freeing %d rows\n", RES_ROW_N(_r));
		for(i = 0; i < RES_ROW_N(_r); i++) {
			db_free_row(&(RES_ROWS(_r)[i]));
		}
	}
	RES_ROW_N(_r) = 0;

	if (RES_ROWS(_r)) {
		LM_DBG("freeing rows at %p\n", RES_ROWS(_r));
		pkg_free(RES_ROWS(_r));
		RES_ROWS(_r) = NULL;
	}
	return 0;
}


/*
 * Release memory used by columns
 */
int db_free_columns(db1_res_t* _r)
{
	int col;

	if (!_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	LM_DBG("freeing %d columns\n", RES_COL_N(_r));
	/* free memory previously allocated to save column names */
	for(col = 0; col < RES_COL_N(_r); col++) {
		if (RES_NAMES(_r)[col]!=NULL) {
			LM_DBG("freeing RES_NAMES[%d] at %p\n", col, RES_NAMES(_r)[col]);
			pkg_free((str *)RES_NAMES(_r)[col]);
			RES_NAMES(_r)[col] = NULL;
		}
	}
	RES_COL_N(_r) = 0;

	/* free names and types */
	if (RES_NAMES(_r)) {
		LM_DBG("freeing result names at %p\n", RES_NAMES(_r));
		pkg_free(RES_NAMES(_r));
		RES_NAMES(_r) = NULL;
	}
	if (RES_TYPES(_r)) {
		LM_DBG("freeing result types at %p\n", RES_TYPES(_r));
		pkg_free(RES_TYPES(_r));
		RES_TYPES(_r) = NULL;
	}
	return 0;
}

/*
 * Create a new result structure and initialize it
 */
db1_res_t* db_new_result(void)
{
	db1_res_t* r = NULL;
	r = (db1_res_t*)pkg_malloc(sizeof(db1_res_t));
	if (!r) {
		LM_ERR("no private memory left\n");
		return 0;
	}
	LM_DBG("allocate %d bytes for result set at %p\n",
		(int)sizeof(db1_res_t), r);
	memset(r, 0, sizeof(db1_res_t));
	return r;
}

/*
 * Release memory used by a result structure
 */
int db_free_result(db1_res_t* _r)
{
	if (!_r)
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	db_free_columns(_r);
	db_free_rows(_r);
	LM_DBG("freeing result set at %p\n", _r);
	pkg_free(_r);
	_r = NULL;
	return 0;
}

/*
 * Allocate storage for column names and type in existing
 * result structure.
 */
int db_allocate_columns(db1_res_t* _r, const unsigned int cols)
{
	RES_NAMES(_r) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * cols);
	if (!RES_NAMES(_r)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	memset(RES_NAMES(_r), 0, sizeof(db_key_t) * cols);
	LM_DBG("allocate %d bytes for result names at %p\n",
		(int)(sizeof(db_key_t) * cols),
		RES_NAMES(_r));

	RES_TYPES(_r) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * cols);
	if (!RES_TYPES(_r)) {
		LM_ERR("no private memory left\n");
		pkg_free(RES_NAMES(_r));
		return -1;
	}
	memset(RES_TYPES(_r), 0, sizeof(db_type_t) * cols);
	LM_DBG("allocate %d bytes for result types at %p\n",
		(int)(sizeof(db_type_t) * cols),
		RES_TYPES(_r));

	return 0;
}


/**
 * Allocate memory for rows.
 * \param _res result set
 * \return zero on success, negative on errors
 */
int db_allocate_rows(db1_res_t* _res)
{
	int len = sizeof(db_row_t) * RES_ROW_N(_res);
	RES_ROWS(_res) = (struct db_row*)pkg_malloc(len);
	if (!RES_ROWS(_res)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate %d bytes for rows at %p\n", len, RES_ROWS(_res));
	memset(RES_ROWS(_res), 0, len);
	
	return 0;
}

/**
 * Reallocate memory for rows.
 * \param _res result set
 * \param _nsize new number of rows in result set
 * \return zero on success, negative on errors
 */
int db_reallocate_rows(db1_res_t* _res, int _nsize)
{
	int len;
	int osize;
	db_row_t *orows;

	orows = RES_ROWS(_res);
	osize = RES_ROW_N(_res);

	RES_ROW_N(_res) = _nsize;
	len = sizeof(db_row_t) * RES_ROW_N(_res);
	RES_ROWS(_res) = (struct db_row*)pkg_malloc(len);
	if (!RES_ROWS(_res)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate %d bytes for rows at %p\n", len, RES_ROWS(_res));
	memset(RES_ROWS(_res), 0, len);

	if(orows==NULL)
		return 0;
	memcpy(RES_ROWS(_res), orows,
			((osize<_nsize)?osize:_nsize)*sizeof(db_row_t));
	pkg_free(orows);
	return 0;
}
