/*
 * $Id$
 *
 * DBText library
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * DBText library
 *   
 * 2003-01-30 created by Daniel
 * 
 */


#ifndef _DBT_LIB_H_
#define _DBT_LIB_H_

#include "../../str.h"
#include "../../db/db_val.h"
#include "../../locking.h"

#define DBT_FLAG_UNSET  0
#define DBT_FLAG_NULL   1
#define DBT_FLAG_AUTO   2

#define DBT_TBFL_ZERO	0
#define DBT_TBFL_MODI	1

#define DBT_FL_IGN		-1
#define DBT_FL_SET		0
#define DBT_FL_UNSET	1

#define DBT_DELIM	':'
#define DBT_DELIM_C	' '
#define DBT_DELIM_R	'\n'

/***
typedef struct _dbt_val
{
	int null;
	union
	{
		int int_val;
		double double_val;
		str str_val;
	} val;
} dbt_val_t, *dbt_val_p;
***/

typedef db_val_t dbt_val_t, *dbt_val_p;

typedef struct _dbt_row
{
	dbt_val_p fields;
	struct _dbt_row *prev;
	struct _dbt_row *next;
	
} dbt_row_t, *dbt_row_p;

typedef struct _dbt_column
{
	str name;
	int type;
	int flag;
	struct _dbt_column *prev;
	struct _dbt_column *next;
	
} dbt_column_t, *dbt_column_p;


typedef struct _dbt_table
{
	str name;
	int mark;
	int flag;
	int auto_col;
	int auto_val;
	int nrcols;
	dbt_column_p cols;
	dbt_column_p *colv;
	int nrrows;
	dbt_row_p rows;
} dbt_table_t, *dbt_table_p;

typedef struct _tbl_cache
{
	gen_lock_t sem;
	dbt_table_p dtp;
	struct _tbl_cache *prev;
	struct _tbl_cache *next;	
} tbl_cache_t, *tbl_cache_p;

typedef struct _dbt_database
{
	str name;
	tbl_cache_p tables;
} dbt_db_t, *dbt_db_p;

typedef struct _dbt_cache 
{
	gen_lock_t sem;
	dbt_db_p dbp;
	struct _dbt_cache *prev;
	struct _dbt_cache *next;
	
} dbt_cache_t, *dbt_cache_p;



int dbt_init_cache();
int dbt_cache_destroy();
int dbt_cache_print(int);

dbt_cache_p dbt_cache_get_db(str*);
int dbt_cache_check_db(str*);
int dbt_cache_del_db(str*);
tbl_cache_p dbt_db_get_table(dbt_cache_p, str*);
int dbt_db_del_table(dbt_cache_p, str*);

int dbt_db_free(dbt_db_p);
int dbt_cache_free(dbt_cache_p);

dbt_column_p dbt_column_new(char*, int);
dbt_row_p dbt_row_new(int);
dbt_table_p dbt_table_new(char*, int);
tbl_cache_p tbl_cache_new();

int dbt_row_free(dbt_table_p, dbt_row_p);
int dbt_column_free(dbt_column_p);
int dbt_table_free_rows(dbt_table_p);
int dbt_table_free(dbt_table_p);
int tbl_cache_free(tbl_cache_p);


int dbt_row_set_val(dbt_row_p, dbt_val_p, int, int);
int dbt_row_update_val(dbt_row_p, dbt_val_p, int, int);
int dbt_table_add_row(dbt_table_p, dbt_row_p);
int dbt_table_check_row(dbt_table_p, dbt_row_p);
int dbt_table_update_flags(dbt_table_p, int, int, int);

dbt_table_p dbt_load_file(str *, str *);
int dbt_print_table(dbt_table_p, str *);

#endif

