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


int child_init(int rank);

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
					     NULL,  /* oncancel function */
					     child_init
};


db_con_t* db_handle;

int child_init(int rank)
{
	db_handle = db_init(DB_URL);
	if (!db_handle) {
		LOG(L_ERR, "auth:init_child(): Unable to connect database\n");
		return -1;
	}
	return 0;

}



#ifdef STATIC_AUTH
struct module_exports* auth_mod_register()
#else
struct module_exports* mod_register()
#endif
{
	auth_init();

	LOG(L_ERR, "auth module - registering\n");
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_register(): Unable to bind database module\n");
	}

	return &auth_exports;
}



void destroy(void)
{
	db_close(db_handle);
}
