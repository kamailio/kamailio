/* 
 * $Id$ 
 */

#include "dbase.h"
#include "utils.h"
#include "../../dprint.h"

db_hooks_t dbase;



int init_db(void)
{
	int res = TRUE;

	dbase.q_loc = (query_loc_func_t)find_export("query_loc", 2);
	if (!dbase.q_loc) {
		LOG(L_ERR, "init_db(): Unable to get query_location function hook\n");
		res = FALSE;
	}

	dbase.i_loc = (insert_loc_func_t)find_export("insert_loc", 2);
	if (!dbase.i_loc) {
		LOG(L_ERR, "init_db(): Unable to get insert_location function hook\n");
		res = FALSE;
	}

	dbase.i_con = (insert_con_func_t)find_export("insert_con", 2);
	if (!dbase.i_con) {
		LOG(L_ERR, "init_db(): Unable to get insert_contact function hook\n");
		res = FALSE;
	}

	dbase.d_loc = (delete_loc_func_t)find_export("delete_loc", 2);
	if (!dbase.d_loc) {
		LOG(L_ERR, "init_db(): Unable to get delete_location function hook\n");
		res = FALSE;
	}

	dbase.u_loc = (update_loc_func_t)find_export("update_loc", 2);
	if (!dbase.u_loc) {
		LOG(L_ERR, "init_db(): Unable to get update_location function hook\n");
		res = FALSE;
	}	

	dbase.u_con = (update_con_func_t)find_export("update_con", 2);
	if (!dbase.u_loc) {
		LOG(L_ERR, "init_db(): Unable to get update_contact function hook\n");
		res = FALSE;
	}	

	return res;
}



void close_db(void)
{

}

