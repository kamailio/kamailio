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
 */


#ifndef _DBT_LIB_H_
#define _DBT_LIB_H_

#include "../../str.h"
#include "../../lib/srdb1/db_val.h"
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

/*
 *  * Module parameters variables
 *   */
extern int db_mode; /* Database usage mode: 0 = no cache, 1 = cache */
extern int empty_string; /* If TRUE, an empty string is an empty string, otherwise NULL */

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
	str dbname;
	str name;
	int hash;
	int mark;
	int flag;
	int auto_col;
	int auto_val;
	int nrcols;
	dbt_column_p cols;
	dbt_column_p *colv;
	int nrrows;
	dbt_row_p rows;
	time_t mt;
	struct _dbt_table *next;
	struct _dbt_table *prev;
} dbt_table_t, *dbt_table_p;

typedef struct _dbt_tbl_cachel
{
	gen_lock_t sem;
	dbt_table_p dtp;
} dbt_tbl_cachel_t, *dbt_tbl_cachel_p;

typedef struct _dbt_cache 
{
	str name;
	int flags;
	struct _dbt_cache *next;
} dbt_cache_t, *dbt_cache_p;



int dbt_init_cache(void);
int dbt_cache_destroy(void);
int dbt_cache_print(int);

dbt_cache_p dbt_cache_get_db(str*);
int dbt_cache_check_db(str*);
int dbt_cache_del_db(str*);
dbt_table_p dbt_db_get_table(dbt_cache_p, const str*);
int dbt_release_table(dbt_cache_p, const str*);

int dbt_cache_free(dbt_cache_p);

dbt_column_p dbt_column_new(char*, int);
dbt_row_p dbt_row_new(int);
dbt_table_p dbt_table_new(const str*, const str*, const char*);

int dbt_row_free(dbt_table_p, dbt_row_p);
int dbt_column_free(dbt_column_p);
int dbt_table_free_rows(dbt_table_p);
int dbt_table_free(dbt_table_p);


int dbt_row_set_val(dbt_row_p, dbt_val_p, int, int);
int dbt_row_update_val(dbt_row_p, dbt_val_p, int, int);
int dbt_table_add_row(dbt_table_p, dbt_row_p);
int dbt_table_check_row(dbt_table_p, dbt_row_p);
int dbt_table_update_flags(dbt_table_p, int, int, int);

int dbt_check_mtime(const str *, const str *, time_t *);
dbt_table_p dbt_load_file(const str *, const str *);
int dbt_print_table(dbt_table_p, str *);
int dbt_is_neq_type(db_type_t _t0, db_type_t _t1);

#endif

