/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */


#include "db.h"
#include "../sr_module.h"

db_func_t dbf;


int bind_dbmod(void)
{
	db_use_table = (db_use_table_f)find_export("db_use_table", 2, 0);
	if (db_use_table == 0) return -1;

	db_init = (db_init_f)find_export("db_init", 1, 0);
	if (db_init == 0) return -1;

	db_close = (db_close_f)find_export("db_close", 2, 0);
	if (db_close == 0) return -1;

	db_query = (db_query_f)find_export("db_query", 2, 0);
	if (db_query == 0) return -1;

	db_raw_query = (db_raw_query_f)find_export("db_raw_query", 2, 0);
	if (db_raw_query == 0) return -1;

	db_free_query = (db_free_query_f)find_export("db_free_query", 2, 0);
	if (db_free_query == 0) return -1;

	db_insert = (db_insert_f)find_export("db_insert", 2, 0);
	if (db_insert == 0) return -1;

	db_delete = (db_delete_f)find_export("db_delete", 2, 0);
	if (db_delete == 0) return -1;

	db_update = (db_update_f)find_export("db_update", 2, 0);
	if (db_update == 0) return -1;

	return 0;
}
