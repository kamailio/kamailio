#include "../../sr_module.h"
#include <stdio.h>
#include "dbase.h"

typedef int (*mod_func)(struct sip_msg* msg, char* str, char* str2);


static struct module_exports mysql_exports = { "mysql", 
					      (char*[]) {
						      "db_init",
						      "db_close",
						      "db_query",
						      "db_free_query"
						      "db_insert",
						      "db_delete",
						      "db_update"
					      },
					      (cmd_function[]) {
						      (mod_func)db_init,
						      (mod_func)db_close,
						      (mod_func)db_query,
						      (mod_func)db_free_query,
						      (mod_func)db_insert,
						      (mod_func)db_delete,
						      (mod_func)db_update
					      },
					      (int[]) {
						      1,
						      1,
						      2,
						      2,
						      2,
						      2,
						      2,
					      },
					      (fixup_function[]) {
						      0,
						      0,
						      0,
						      0,
						      0,
						      0,
						      0
					      },
					      7, /* number of functions*/
					      0, /* response function*/
					      0  /* destroy function */
};


struct module_exports* mod_register()
{
	fprintf(stderr, "mysql - registering...\n");
	return &mysql_exports;
}
