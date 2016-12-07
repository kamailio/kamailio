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

#include "ul_check.h"
#include "../../str.h"
#include "../../lib/srdb1/db.h"

#ifndef SP_P_USRLOC_UL_DB_HANDLE_H
#define SP_P_USRLOC_UL_DB_HANDLE_H

#define DB_NUM 2

#define DB_OFF 0
#define DB_ON 1
#define DB_INACTIVE 2

#define UL_DB_URL_LEN 260

typedef struct str2s {
	char s[UL_DB_URL_LEN];
	int len;
} str2;

typedef struct ul_db {
	str2 url;
	int no;
	time_t failover_time;
	time_t retry;
	int errors;
	int status;
	int spare;
	int rg;
	db1_con_t * dbh;
	db_func_t dbf;
}ul_db_t;

typedef struct ul_db_handle {
	unsigned int id;
	struct check_data * check;
	int working;
	time_t expires;
	int active;
	ul_db_t db[DB_NUM];
}ul_db_handle_t;

typedef struct ul_db_handle_list {
	ul_db_handle_t * handle;
	struct ul_db_handle_list * next;
}ul_db_handle_list_t;

void destroy_handles(void);

int refresh_handles(db_func_t * dbf, db1_con_t * dbh);

int load_location_number(db_func_t * dbf, db1_con_t * dbh, int*);
int load_handles(db_func_t * dbf, db1_con_t * dbh);

ul_db_handle_t * get_handle(db_func_t * dbf, db1_con_t * dbh, str * first, str * second);

int load_data(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle, int id);

int refresh_handle(ul_db_handle_t * handle, ul_db_handle_t * new_data, int error_handling);

ul_db_t * get_db_by_num(ul_db_handle_t * handle, int no);

int check_handle(db_func_t * dbf, db1_con_t * dbh, ul_db_handle_t * handle);

#endif
