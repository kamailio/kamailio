/*
 * $Id$
 */

#include "group.h"
#include <string.h>
#include "../../dprint.h"
#include "../../db/db.h"
#include "auth.h"
#include "auth_mod.h"
#include "utils.h"

/*
 * Check if the given username matches username in credentials
 */
int is_user(struct sip_msg* _msg, char* _user, char* _str2)
{
	if (!state.cred.username.len) {
		DBG("is_user(): Username not found in credentials\n");
		return -1;
	}
	if (!memcmp(_user, state.cred.username.s, state.cred.username.len)) {
		DBG("is_user(): Username matches\n");
		return 1;
	} else {
		DBG("is_user(): Username differs\n");
		return -1;
	}
}



/*
 * Check if the user specified in credentials is a member
 * of given group
 */
int is_in_group(struct sip_msg* _msg, char* _group, char* _str2)
{
	db_key_t keys[] = {grp_user_col, grp_grp_col};
	db_val_t vals[] = {{DB_STRING, 0, {.string_val = state.cred.username.s}},
			   {DB_STRING, 0, {.string_val = _group}}
	};
	db_key_t col[] = {grp_grp_col};
	db_res_t* res;

	db_use_table(db_handle, grp_table);
	if (db_query(db_handle, keys, vals, col, 2, 1, NULL, &res) < 0) {
		LOG(L_ERR, "is_in_group(): Error while querying database\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("is_in_group(): User %s is not in group %s\n", state.cred.username.s, _group);
		db_free_query(db_handle, res);
		return -1;
	} else {
		DBG("is_in_group(): User %s is member of group %s\n", state.cred.username.s, _group);
		db_free_query(db_handle, res);
		return 1;
	}
}

