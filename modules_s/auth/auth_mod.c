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


static void destroy(void);
static int child_init(int rank);
static int mod_init(void);

int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);

struct module_exports exports = {
	"auth", 
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
	
	NULL,   /* Module parameter names */
	NULL,   /* Module parameter types */
	NULL,   /* Module parameter variable pointers */
	0,      /* Number of module paramers */
					     
	mod_init,   /* module initialization function */
	NULL,       /* response function */
	destroy,    /* destroy function */
	NULL,       /* oncancel function */
	child_init  /* child initialization function */
};


db_con_t* db_handle;

static int child_init(int rank)
{
	db_handle = db_init(DB_URL);
	if (!db_handle) {
		LOG(L_ERR, "auth:init_child(): Unable to connect database\n");
		return -1;
	}
	return 0;

}


static int mod_init(void)
{
	LOG(L_ERR, "auth module - initializing\n");
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_init(): Unable to bind database module\n");
		return -1;
	}

	sl_reply = find_export("sl_send_reply", 2);

	if (!sl_reply) {
		LOG(L_ERR, "auth:mod_init(): This module requires sl module\n");
		return -2;
	}

	return 0;
}



static void destroy(void)
{
	db_close(db_handle);
}
