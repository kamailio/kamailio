/* 
 * $Id$ 
 */

#include "mysql.h"
#include "dbcon_mysql.h"
#include "../../dprint.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

#define SQL_URL "sql://localhost/ser"


void destroy(void);

/*
static struct module_exports mysql_exports = { "mysql",
					       (char*[]) {"query_loc", 
							  "insert_loc", 
							  "delete_loc", 
							  "update_loc",
							  "insert_con", 
							  "update_con"},
					       (cmd_function[]) {(cmd_function)query_location,
								 (cmd_function)insert_location,
								 (cmd_function)delete_location,
								 (cmd_function)update_location,
								 (cmd_function)insert_contact,
								 (cmd_function)update_contact},
					       (int[]) { 2, 2, 2, 2, 2, 2},
					       (fixup_function[]) { NULL, NULL, NULL, NULL, NULL, NULL},
					       6,
					       0,
					       destroy
};




struct module_exports* mod_register(void)
{
	fprintf(stderr, "%s - registering...\n", mysql_exports.name);
	if (db_init(SQL_URL) == FALSE) {
		LOG(L_ERR, "mod_register(): Error while connecting to database\n");
	}
	return &mysql_exports;
}





void destroy(void)
{
        fprintf(stderr, "mysql: Closing database connection");
	db_close();
}

*/
