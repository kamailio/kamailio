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

int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);

static struct module_exports auth_exports = {"auth", 
					     (char*[]) { 
						     "authorize",
						     "challenge",
						     "is_user",
						     "is_in_group",
						     "check_to",
						     "check_from"
					     },
					     (cmd_function[]) {
						     authorize, 
						     challenge,
						     is_user,
						     is_in_group,
						     check_to,
						     check_from
					     },
					     (int[]) {1, 2, 1, 1, 0, 0},
					     (fixup_function[]) {
						     NULL, 
						     NULL,
						     NULL,
						     NULL,
						     NULL,
						     NULL
					     },
					     6,
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
	LOG(L_ERR, "auth module - registering\n");
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_register(): Unable to bind database module\n");
	}

	sl_reply = find_export("sl_send_reply", 2);

	if (!sl_reply) {
		LOG(L_ERR, "auth:mod_register(): This module requires sl module\n");
	}

	return &auth_exports;
}



void destroy(void)
{
	db_close(db_handle);
}
