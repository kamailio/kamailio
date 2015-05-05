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
 */

#define _GNU_SOURCE
#include <string.h>

#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
 
#include "dbtext.h"
#include "dbt_res.h"
#include "dbt_api.h"
#include "dbt_raw_util.h"


int dbt_raw_query_select(db1_con_t* _h, str* _s, db1_res_t** _r)
{
    int res = -1;
    int i, len;
    char* table_ptr = NULL;
    char* fields_end_ptr = NULL;
    char* fields_ptr = NULL;
    char* where_ptr = NULL;
    char** tokens = NULL;
    str table;
    dbt_table_p _tbc = NULL;
	int cols;
	int n = 0;
	int ncols = 0;
	int nc = 0;
	db_key_t *result_cols = NULL;
	db_key_t* _k = NULL;
	db_op_t* _op = NULL;
	db_val_t* _v = NULL;

    fields_end_ptr = strcasestr(_s->s, " from ");
    if(fields_end_ptr == NULL)
    	return res;

    len = fields_end_ptr - (_s->s + 6) + 1;
    fields_ptr = pkg_malloc(len);
    strncpy(fields_ptr, _s->s + 6, len);
    fields_ptr[len] = '\0';
    fields_ptr = dbt_trim(fields_ptr);



    where_ptr = strcasestr(_s->s, " where ");
    if(where_ptr == NULL) {
    	len = strlen(fields_end_ptr + 6);
    } else {
    	len = where_ptr - (fields_end_ptr + 6);
    	nc = dbt_build_where(where_ptr + 7, &_k, &_op, &_v);
    }

    table_ptr = pkg_malloc(len);
    strncpy(table_ptr, fields_end_ptr + 6, len);
    table_ptr[len] = '\0';
    dbt_trim(table_ptr);

    table.s = table_ptr;
    table.len = len;

	if(dbt_use_table(_h, &table) != 0) {
		LM_ERR("use table is invalid %.*s\n", table.len, table.s);
		goto error;
	}

	_tbc = dbt_db_get_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));
	if(!_tbc)
	{
		LM_ERR("table %.*s does not exist!\n", CON_TABLE(_h)->len, CON_TABLE(_h)->s);
		goto error;
	}

    tokens = dbt_str_split(fields_ptr, ',', &ncols);
    pkg_free(fields_ptr);
    fields_ptr = NULL;
    if (!tokens) {
		LM_ERR("error extracting tokens\n");
    	goto error;
    }

    if(ncols == 1 && strncmp(*tokens, "*", 1) == 0) {
    	cols = _tbc->nrcols;
    	result_cols = pkg_malloc(sizeof(db_key_t) * cols);
    	memset(result_cols, 0, sizeof(db_key_t) * cols);
    	for(n=0; n < cols; n++) {
    		result_cols[n] = &_tbc->colv[n]->name;
    	}
    } else {
    	cols = ncols;
    	result_cols = pkg_malloc(sizeof(db_key_t) * cols);
    	memset(result_cols, 0, sizeof(db_key_t) * cols);
    	for(n=0; *(tokens + n); n++) {
    		result_cols[n]->s = *(tokens + n);
    		result_cols[n]->len = strlen(*(tokens + n));
    	}
    }


	dbt_release_table(DBT_CON_CONNECTION(_h), CON_TABLE(_h));

	res = dbt_query(_h, _k, _op, _v, result_cols, nc, cols, NULL, _r);

error:
	if(tokens) {
	    for (i = 0; *(tokens + i); i++) {
	    	pkg_free(*(tokens + i));
	    }
	    pkg_free(tokens);
	}
    if(fields_ptr)
    	pkg_free(fields_ptr);

    if(table_ptr)
    	pkg_free(table_ptr);

    dbt_clean_where(nc, _k, _op, _v);

    if(result_cols) {
    	pkg_free(result_cols);
    }

 	return res;
}

int dbt_raw_query_update(db1_con_t* _h, str* _s, db1_res_t** _r)
{
    int res = -1;



 	return res;
}

int dbt_raw_query_delete(db1_con_t* _h, str* _s, db1_res_t** _r)
{
    int res = -1;



 	return res;
}

/*
 * Raw SQL query -- is not the case to have this method
 */
int dbt_raw_query(db1_con_t* _h, str* _s, db1_res_t** _r)
{
	*_r = NULL;
    int res = -1;

	if(!_h) {
		LM_ERR("invalid connection\n");
		return res;
	}

	if(!_s) {
		LM_ERR("sql query is null\n");
		return res;
	}

	if(_s->s == NULL) {
		LM_ERR("sql query is null\n");
    	return res;
	}

    dbt_trim(_s->s);
    _s->len = strlen(_s->s);

    if(strncasecmp(_s->s, "select", 6) == 0) {
    	return dbt_raw_query_select(_h, _s, _r);
    } else if(strncasecmp(_s->s, "update", 6) == 0) {
    	return dbt_raw_query_update(_h, _s, _r);
    } else if(strncasecmp(_s->s, "delete", 6) == 0) {
    	return dbt_raw_query_delete(_h, _s, _r);
    };

    return res;
}
