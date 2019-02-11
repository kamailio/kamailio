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

#ifndef SP_P_USRLOC_UL_DB_FAILOVER_FUNC_H
#define SP_P_USRLOC_UL_DB_FAILOVER_FUNC_H

#include "../../lib/srdb1/db.h"
#include "ul_db_handle.h"

int ul_db_failover_prepare(db_func_t * dbf, db1_con_t * dbh);

int ul_db_failover_commit(db_func_t * dbf, db1_con_t * dbh);

int ul_db_failover_rollback(db_func_t * dbf, db1_con_t * dbh);

int get_max_no_of_db_id(db_func_t * dbf, db1_con_t * dbh, int id);

int store_handle_data(db_func_t * dbf, db1_con_t * dbh, ul_db_t * db, int id, int old_num, int new_id);

int check_handle_data(db_func_t * dbf, db1_con_t * dbh, ul_db_t * db, int id);

#endif
