/* 
 * $Id$ 
 */

#include <stdlib.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "defs.h"
#include <string.h>
#include "auth.h"
#include "db.h"


void destroy(void);

static struct module_exports auth_exports = {"auth", 
					     (char*[]) { 
						     "authorize",
						     "challenge"
					     },
					     (cmd_function[]) {
						     authorize, 
						     challenge
					     },
					     (int[]) {1, 1},
					     (fixup_function[]) {
						     NULL, 
						     NULL
					     },
					     3,
					     NULL, /* response function */
					     destroy, /* destroy function */
					     NULL  /* oncancel function */
};


db_con_t* db_handle;


struct module_exports* mod_register()
{
	LOG(L_ERR, "%auth module - registering\n");
	auth_init();
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_register(): Unable to bind database module\n");
	}

	     /* Open a database connection */
	db_handle = db_init(DB_URL);
	if (!db_handle) {
		LOG(L_ERR, "mod_register(): Unable to connect database\n");
	}

	return &auth_exports;
}



void destroy(void)
{
	db_close(db_handle);
}
