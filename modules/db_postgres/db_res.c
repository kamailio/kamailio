/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 *
 * Copyright (C) 2003 August.Net Services, LLC
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
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */

#include <stdlib.h>
#include "../../db/db_res.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "defs.h"
#include "con_postgres.h"
#include "pg_type.h"
#include "aug_std.h"

int str2valp(db_type_t _t, db_val_t* _v, const char* _s, int _l, void *_p);

/*
 * Create a new result structure and initialize it
 */
db_res_t* new_result_pg(char *parent)
{
	db_res_t* r;

	r = (db_res_t*)aug_alloc(sizeof(db_res_t), parent);

	RES_NAMES(r) = 0;
	RES_TYPES(r) = 0;
	RES_COL_N(r) = 0;
	RES_ROWS(r) = 0;
	RES_ROW_N(r) = 0;

	return r;
}

/*
 * Get and convert columns from a result
 */
static inline int get_columns(db_con_t* _h, db_res_t* _r)
{
	int n, i;

	n = PQnfields(CON_RESULT(_h));

	if (!n) {
		LOG(L_ERR, "get_columns(): No columns\n");
		return -2;
	}

        RES_NAMES(_r) = (db_key_t*)aug_alloc(sizeof(db_key_t) * n, _r);
	RES_TYPES(_r) = (db_type_t*)aug_alloc(sizeof(db_type_t) * n, _r);
	RES_COL_N(_r) = n;

	for(i = 0; i < n; i++) {
		int ft;
		RES_NAMES(_r)[i] = aug_strdup(PQfname(CON_RESULT(_h),i),
			RES_NAMES(_r));
		switch(ft = PQftype(CON_RESULT(_h),i))
		{
			case INT2OID:
			case INT4OID:
			case INT8OID:
				RES_TYPES(_r)[i] = DB_INT;
			break;

			case FLOAT4OID:
			case FLOAT8OID:
			case NUMERICOID:
				RES_TYPES(_r)[i] = DB_DOUBLE;
			break;

			case DATEOID:
			case TIMESTAMPOID:
			case TIMESTAMPTZOID:
				RES_TYPES(_r)[i] = DB_DATETIME;
			break;

			case VARCHAROID:
				RES_TYPES(_r)[i] = DB_STRING;
			break;
			
			default:
				LOG(L_ERR, "unknown type %d\n", ft);
				RES_TYPES(_r)[i] = DB_STRING;
			break;
		}		
	}
	return 0;
}

/*
 * Convert a row from result into db API representation
 */
int convert_row_pg(db_con_t* _h, db_res_t* _res, db_row_t* _r, char **row_buf,
	char *parent)
{
	int i;

        ROW_VALUES(_r) = (db_val_t*)aug_alloc(
		sizeof(db_val_t) * RES_COL_N(_res), parent);
	ROW_N(_r) = RES_COL_N(_res);

	/* I'm not sure about this */
	/* PQfsize() gives us the native length in the database, so */
	/* an int would be 4, not the strlen of the value */
	/* however, strlen of the value would be easy to strlen() */

	for(i = 0; i < RES_COL_N(_res); i++) {
		if (str2valp(RES_TYPES(_res)[i], &(ROW_VALUES(_r)[i]), 
			    row_buf[i],
			    PQfsize(CON_RESULT(_h),i),
			    (void *) ROW_VALUES(_r)) < 0) {
		LOG(L_ERR, "convert_row_pg(): Error while converting value\n");
			return -3;
		}
	}
	return 0;
}
/*
 * Convert rows from postgres to db API representation
 */
static inline int convert_rows(db_con_t* _h, db_res_t* _r)
{
	int n, i, j, k;
	char **row_buf, *s;
	n = PQntuples(CON_RESULT(_h));
	RES_ROW_N(_r) = n;
	if (!n) {
		RES_ROWS(_r) = 0;
		return 0;
	}
	RES_ROWS(_r) = (struct db_row*)aug_alloc(sizeof(db_row_t) * n,_r);
	j = RES_COL_N(_r);
	row_buf = (char **) aug_alloc(sizeof(char *) * (j + 1), CON_SQLURL(_h));

	/* j is the number of columns in the answer set */
	/* n is the number of rows in the answer set */
	for(i = 0; i < n; i++) {
		for(k = 0; k < j; k++) {
			if(PQgetisnull(CON_RESULT(_h), i, k)) {
				/*
				** I don't believe there is a NULL
				** representation, so, I'll cheat
				*/
				s = "";
			} else {
				s = PQgetvalue(CON_RESULT(_h), i, k);
			}
			*(row_buf + k) = aug_strdup(s, row_buf);
		}
		*(row_buf + k) = (char *) 0;

		/*
		** ASSERT: row_buf contains an entire row in strings
		*/
		if (convert_row_pg(_h, _r, &(RES_ROWS(_r)[i]), row_buf,
			(char *) RES_ROWS(_r)) < 0) {
			LOG(L_ERR, "convert_rows(): Error converting row #%d\n",
				i);
			RES_ROW_N(_r) = i;
			aug_free(row_buf);
			return -4;
		}
	}
	aug_free(row_buf);
	return 0;
}

/*
 * Fill the structure with data from database
 */
int convert_result(db_con_t* _h, db_res_t* _r)
{
	if (get_columns(_h, _r) < 0) {
		LOG(L_ERR, "convert_result(): Error getting column names\n");
		return -2;
	}

	if (convert_rows(_h, _r) < 0) {
		LOG(L_ERR, "convert_result(): Error while converting rows\n");
		return -3;
	}
	return 0;
}


/*
 * Release memory used by a result structure
 */
int free_result(db_res_t* _r)
{
	aug_free(_r);

	return 0;
}
