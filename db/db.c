/*
 * $Id$
 */

#include "db.h"
#include "../sr_module.h"

db_func_t dbf;


int bind_dbmod(void)
{
	db_use_table = (db_use_table_f)find_export("~db_use_table", 2);
	if (db_use_table == 0) return -1;

	db_init = (db_init_f)find_export("~db_init", 1);
	if (db_init == 0) return -1;

	db_close = (db_close_f)find_export("~db_close", 2);
	if (db_close == 0) return -1;

	db_query = (db_query_f)find_export("~db_query", 2);
	if (db_query == 0) return -1;

	db_free_query = (db_free_query_f)find_export("~db_free_query", 2);
	if (db_free_query == 0) return -1;

	db_insert = (db_insert_f)find_export("~db_insert", 2);
	if (db_insert == 0) return -1;

	db_delete = (db_delete_f)find_export("~db_delete", 2);
	if (db_delete == 0) return -1;

	db_update = (db_update_f)find_export("~db_update", 2);
	if (db_update == 0) return -1;

	return 0;
}
