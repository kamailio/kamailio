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

#ifndef UL_DB_API_H
#define UL_DB_API_H

#include "ul_db.h"
#include "../../sr_module.h"

typedef int (* ul_db_insert_t) (str * table, str * first, str * second,
				 db_key_t* _k, db_val_t* _v, int _n);

typedef int (* ul_db_update_t) (str * table, str * first, str * second,
				db_key_t* _k, db_op_t * _op, db_val_t* _v, db_key_t* _uk,
				db_val_t* _uv, int _n, int _un);

typedef int (* ul_db_insert_update_t) (str * table, str * first, str * second,
				db_key_t* _k, db_val_t* _v, int _n);

typedef int (* ul_db_replace_t) (str * table, str * first, str * second,
				db_key_t* _k, db_val_t* _v, int _n, int _un);

typedef int (* ul_db_delete_t) (str * table, str * first, str * second,
				 db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);

typedef int (* ul_db_query_t) (str * table, str * first, str * second, db1_con_t *** _r_h,
				db_key_t* _k, db_op_t* _op,	db_val_t* _v, db_key_t* _c,
				int _n, int _nc, db_key_t _o, db1_res_t** _r);

typedef int (* ul_db_free_result_t)(db1_con_t ** dbh, db1_res_t * res);

typedef struct ul_db_api{
	ul_db_update_t			update;
	ul_db_insert_t			insert;
	ul_db_insert_update_t	insert_update;
	ul_db_replace_t			replace;
	ul_db_delete_t			delete;
	ul_db_query_t			query;
	ul_db_free_result_t		free_result;
}ul_db_api_t;

typedef int (*bind_ul_db_t)(ul_db_api_t * api);


int bind_ul_db(ul_db_api_t* api);

#endif
