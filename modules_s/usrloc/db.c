/*
 * $Id$
 */

#include "db.h"
#include "../../sr_module.h"

db_func_t dbf;


int bind_dbmod(void)
{
	dbf.use_table = (db_use_table_f)find_export("db_use_table", 2);

	dbf.init = (db_init_f)find_export("db_init", 1);
	if (!dbf.init) return 1;

	dbf.close = (db_close_f)find_export("db_close", 2);
	if (!dbf.close) return 1;

	dbf.query = (db_query_f)find_export("db_query", 2);
	if (!dbf.query) return 1;

	dbf.free_query = (db_free_query_f)find_export("db_free_query", 2);
	if (!dbf.free_query) return 1;

	dbf.insert = (db_insert_f)find_export("db_insert", 2);
	if (!dbf.insert) return 1;

	dbf.delete = (db_delete_f)find_export("db_delete", 2);
	if (!dbf.delete) return 1;

	dbf.update = (db_update_f)find_export("db_update", 2);
	if (!dbf.update) return 1;

	return 0;
}
