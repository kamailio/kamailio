/* sp-ul_db module
 *
 * Copyright (C) 2007 1&1 Internet AG
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

#ifndef SP_P_USRLOC_UL_DB_H
#define SP_P_USRLOC_UL_DB_H

#include "../../lib/srdb1/db.h"
#include "ul_db_handle.h"

#define UL_DB_QUERY_LEN 2048

#define DB_ERR_PRI 0
#define DB_ERR_SEC 1
#define DB_MODE_PRI 0
#define DB_MODE_SEC 1
#define DB_MODE_BOTH 2
#define DB_MODE_OFF 3

#define DB_POL_OP 0
#define DB_POL_QUERY 1
#define DB_POL_MOD 2

#ifdef __i386__
    #define UL_DB_ZERO_TIME 0x80000000
#else
    #define UL_DB_ZERO_TIME 0xFFFFFFFF80000000
#endif

typedef struct ul_master_db {
	str * url;
	db_func_t dbf;
	db1_con_t * dbh;
} ul_master_db_t;

typedef struct ul_master_db_set {
	ul_master_db_t read;
	ul_master_db_t write;
} ul_master_db_set_t;

extern int required_caps;

int ul_db_init();

int ul_db_child_init();

int ul_db_child_locnr_init();

void ul_db_shutdown();

int db_handle_error(ul_db_handle_t * handle, int no);

int db_reactivate(ul_db_handle_t * handle, int no);

int db_reset_failover_time(ul_db_handle_t * handle, int no);

int db_check_policy(int pol, int ok, int working);

int ul_db_insert(str * table, str * first, str * second,
			  db_key_t* _k, db_val_t* _v, int _n);

int ul_db_update(str * table, str * first, str * second,
				db_key_t* _k, db_op_t * _op, db_val_t* _v,
				db_key_t* _uk, db_val_t* _uv, int _n, int _un);

int ul_db_replace(str * table, str * first, str * second,
			  db_key_t* _k, db_val_t* _v, int _n, int _un);

int ul_db_delete(str * table, str * first, str * second,
			  db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);

int ul_db_query(str * table, str * first, str * second, db1_con_t *** _r_h,
				db_key_t* _k, db_op_t* _op, db_val_t* _v, db_key_t* _c,
				int _n, int _nc, db_key_t _o, db1_res_t** _r);

int ul_db_free_result(db1_con_t ** dbh, db1_res_t * res);

int ul_db_check(ul_db_handle_t * handle);

#endif
