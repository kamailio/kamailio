/* 
 * $Id$ 
 *
 * MySQL module interface
 */

#include "../../sr_module.h"
#include <stdio.h>
#include "dbase.h"


static int mod_init(void);


/*
 * MySQL database module interface
 */

struct module_exports exports = {	
	"mysql",
	(char*[]) {
		"db_use_table",
		"db_init",
		"db_close",
		"db_query",
		"db_free_query",
		"db_insert",
		"db_delete",
		"db_update"
	},
	(cmd_function[]) {
		(cmd_function)use_table,
		(cmd_function)db_init,
		(cmd_function)db_close,
		(cmd_function)db_query,
		(cmd_function)db_free_query,
		(cmd_function)db_insert,
		(cmd_function)db_delete,
		(cmd_function)db_update
	},
	(int[]) {
                2, 1, 2, 2, 2, 2, 2, 2
	},
	(fixup_function[]) {
		0, 0, 0, 0, 0, 0, 0, 0
	},
	8, /* number of functions*/

	NULL,   /* Module parameter names */
	NULL,   /* Module parameter types */
	NULL,   /* Module parameter variable pointers */
	0,      /* Number of module paramers */

	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "mysql - initializing\n");
	return 0;
}
