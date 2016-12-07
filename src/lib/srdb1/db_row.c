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
 * \file lib/srdb1/db_row.c
 * \brief Type that represents a row in a database.
 *
 * This file holds a type that represents a row in a database, some convenience
 * macros and a function for memory managements.
 * \ingroup db1
 */

#include "db_row.h"

#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"

/*
 * Release memory used by row
 */
int db_free_row(db_row_t* _r)
{
	int col;
	db_val_t* _val;

	if (!_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/*
	 * Loop thru each columm, then check to determine if the storage pointed to
	 * by db_val_t structure must be freed. This is required for all data types
	 * which use a pointer to a buffer like DB1_STRING, DB1_STR and DB1_BLOB and
	 * the database module copied them during the assignment.
	 * If this is not done, a memory leak will happen.
	 * Don't try to free the static dummy string (as indicated from the NULL value),
	 * as this is not valid.
	 */
	for (col = 0; col < ROW_N(_r); col++) {
		_val = &(ROW_VALUES(_r)[col]);
		switch (VAL_TYPE(_val)) {
			case DB1_STRING:
				if ( (!VAL_NULL(_val)) && VAL_FREE(_val)) {
					LM_DBG("free VAL_STRING[%d] '%s' at %p\n", col,
							(char *)VAL_STRING(_val),
							(char *)VAL_STRING(_val));
					pkg_free((char *)VAL_STRING(_val));
					VAL_STRING(_val) = NULL;
				}
				break;
			case DB1_STR:
				if ( (!VAL_NULL(_val)) && VAL_FREE(_val)) {
					LM_DBG("free VAL_STR[%d] '%.*s' at %p\n", col,
							VAL_STR(_val).len,
							VAL_STR(_val).s, VAL_STR(_val).s);
					pkg_free(VAL_STR(_val).s);
					VAL_STR(_val).s = NULL;
				}
				break;
			case DB1_BLOB:
				if ( (!VAL_NULL(_val)) && VAL_FREE(_val)) {
					LM_DBG("free VAL_BLOB[%d] at %p\n", col, VAL_BLOB(_val).s);
					pkg_free(VAL_BLOB(_val).s);
					VAL_BLOB(_val).s = NULL;
				}
				break;
			default:
				break;
		}
	}
	/* now as we freed all, set number of colums to zero again */
	ROW_N(_r) = 0;

	if (ROW_VALUES(_r)) {
		LM_DBG("freeing row values at %p\n", ROW_VALUES(_r));
		pkg_free(ROW_VALUES(_r));
		ROW_VALUES(_r) = NULL;
	}
	return 0;
}


/**
 * Allocate memory for row value.
 * \param _res result set
 * \param _row filled row
 * \return zero on success, negative on errors
 */
int db_allocate_row(const db1_res_t* _res, db_row_t* _row)
{
	int len = sizeof(db_val_t) * RES_COL_N(_res);
	ROW_VALUES(_row) = (db_val_t*)pkg_malloc(len);
	if (!ROW_VALUES(_row)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate %d bytes for row values at %p\n", len, ROW_VALUES(_row));

	memset(ROW_VALUES(_row), 0, len);
	/* Save the number of columns in the ROW structure */
	ROW_N(_row) = RES_COL_N(_res);

	return 0;
}
