/*
 * sleepycat module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
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

/*! \file
 * Berkeley DB : 
 *
 * \ingroup database
 */


#ifndef _KM_BDB_RES_H_
#define _KM_BDB_RES_H_

#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_con.h"
#include "km_bdb_lib.h"
#include "km_bdb_val.h"

typedef struct _con
{
	database_p con;
	db1_res_t*  res;
	row_p row;
} bdb_con_t, *bdb_con_p;

#define BDB_CON_CONNECTION(db_con) (((bdb_con_p)((db_con)->tail))->con)
#define BDB_CON_RESULT(db_con)     (((bdb_con_p)((db_con)->tail))->res)
#define BDB_CON_ROW(db_con)        (((bdb_con_p)((db_con)->tail))->row)

int bdb_get_columns(table_p _tp, db1_res_t* _res, int* _lres, int _nc);
int bdb_convert_row( db1_res_t* _res, char *bdb_result, int* _lres);
int bdb_append_row(db1_res_t* _res, char *bdb_result, int* _lres, int _rx);
int* bdb_get_colmap(table_p _tp, db_key_t* _k, int _n);

int bdb_is_neq_type(db_type_t _t0, db_type_t _t1);
int bdb_row_match(db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n, db1_res_t* _r, int* lkey );
int bdb_cmp_val(db_val_t* _vp, db_val_t* _v);

#endif

