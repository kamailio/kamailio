/* $Id$
 *
 * Copyright (C) 2006-2007 Sippy Software, Inc. <sales@sippysoft.com>
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
 *
 */


#ifndef _BDB_RES_H_
#define _BDB_RES_H_

typedef struct _bdb_con {
	DB_ENV	*dbenvp;		/* Env structure handle */
	DB	*dbp;			/* DB structure handle */
	int	col_num;		/* number of columns */
} bdb_con_t, *bdb_con_p;

#define BDB_CON_DBENV(db_con) (((bdb_con_p)((db_con)->tail))->dbenvp)
#define BDB_CON_DB(db_con) (((bdb_con_p)((db_con)->tail))->dbp)
#define BDB_CON_COL_NUM(db_con) (((bdb_con_p)((db_con)->tail))->col_num)

/* * */

typedef struct _bdb_val_t
{
	db_val_t v;
	struct _bdb_val_t *next;
} bdb_val_t, *bdb_val_p;

typedef struct _bdb_row
{
	DBT	key;
	DBT	data;
	str	tail;
        bdb_val_p fields;
        struct _bdb_row *next;

} bdb_row_t, *bdb_row_p;

/* * */

typedef struct _bdb_uval_t
{
	int c_idx;			/* column index number */
	db_val_t v;
	struct _bdb_uval_t *next;
} bdb_uval_t, *bdb_uval_p;

typedef struct _bdb_urow
{
        bdb_uval_p fields;
} bdb_urow_t, *bdb_urow_p;

/* * */

typedef struct _bdb_sval_t
{
	int c_idx;			/* column index number */
	db_val_t v;
	int op;
#define	BDB_OP_EQ	0
#define	BDB_OP_LT	1
#define	BDB_OP_GT	2
#define	BDB_OP_LEQ	3
#define	BDB_OP_GEQ	4
	struct _bdb_sval_t *next;
} bdb_sval_t, *bdb_sval_p;

typedef struct _bdb_srow
{
	DBT	key;
        bdb_sval_p fields;
} bdb_srow_t, *bdb_srow_p;

/* * */

typedef int bdb_rrow_t, *bdb_rrow_p;

/* * */

typedef struct _bdb_column {
        str name;
        int type;
        struct _bdb_column *next;

} bdb_column_t, *bdb_column_p;

typedef struct _bdb_table {
	str	name;
	int	col_num;		/* number of columns */
        bdb_column_p cols;
        struct _bdb_table *next;
} bdb_table_t, *bdb_table_p;

#endif
