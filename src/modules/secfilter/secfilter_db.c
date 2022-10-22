/**
 * Copyright (C) 2018 Jose Luis Verdeguer
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "../../core/mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "secfilter.h"


int mod_version = 1;

/* Database variables */
static db_func_t db_funcs;		 /* Database API functions */
static db1_con_t *db_handle = 0; /* Database connection handle */


/* Check module version */
static int check_version(void)
{
	/* Connect to DB */
	db_handle = db_funcs.init(&secf_db_url);
	if(db_handle == NULL) {
		LM_ERR("Invalid db handle\n");
		return -1;
	}

	if (db_check_table_version(&db_funcs, db_handle, &secf_table_name, mod_version) < 0) {
		DB_TABLE_VERSION_ERROR(secf_table_name);
		db_funcs.close(db_handle);
		return -1;
	}

	return 0;
}


/**
 * @brief Add a new allocated list element to an existing list
 *
 * Add a new allocated list element to an existing list, the allocation is done
 * from the private memory pool
 * @param s input character
 * @param len length of input character
 * @param last existing list
 * @param total length of total characters in list
 * @return extended list
**/
static struct str_list *shm_append_str_list(
		char *s, int len, struct str_list **last, int *total)
{
	struct str_list *new;

	new = shm_malloc(sizeof(struct str_list));
	if(!new) {
		SHM_MEM_ERROR;
		return 0;
	}
	new->s.s = s;
	new->s.len = len;
	new->next = 0;

	if(*last) {
		(*last)->next = new;
		*last = new;
	}
	*total += len;

	return new;
}


/**
	Action => 0 = blacklist
	          1 = whitelist
	          2 = destination blacklist
	Type (for action = 0 or 1) => 0 = user-agent
				      1 = country
				      2 = domain
				      3 = IP address
				      4 = user
**/
int secf_append_rule(int action, int type, str *value)
{
	secf_info_p ini = NULL;
	secf_info_p last = NULL;
	struct str_list **ini_node = NULL;
	struct str_list **last_node = NULL;
	struct str_list *new = NULL;
	int total = 0;
	char *v = NULL;

	if(action < 0 || action > 2) {
		LM_ERR("Unknown action value %d", action);
		return -1;
	}

	if(action == 1) {
		ini = &(*secf_data)->wl;
		last = &(*secf_data)->wl_last;
	} else {
		ini = &(*secf_data)->bl;
		last = &(*secf_data)->bl_last;
	}

	switch(type) {
		case 0:
			if(action == 2) {
				ini_node = &ini->dst;
				last_node = &last->dst;
			} else {
				ini_node = &ini->ua;
				last_node = &last->ua;
			}
			break;
		case 1:
			ini_node = &ini->country;
			last_node = &last->country;
			break;
		case 2:
			ini_node = &ini->domain;
			last_node = &last->domain;
			break;
		case 3:
			ini_node = &ini->ip;
			last_node = &last->ip;
			break;
		case 4:
			ini_node = &ini->user;
			last_node = &last->user;
			break;
		default:
			LM_ERR("Unknown type value %d", type);
			return -1;
	}

	v = (char *)shm_malloc(sizeof(char) * value->len);
	if(!v) {
		SHM_MEM_ERROR;
		return -1;
	}
	memcpy(v, value->s, value->len);
	LM_DBG("ini_node:%p last_node:%p\n", *ini_node, *last_node);

	new = shm_append_str_list(v, value->len, last_node, &total);
	if(!new) {
		LM_ERR("can't append new node\n");
		shm_free(v);
		return -1;
	}
	LM_DBG("new node[%p] str:'%.*s'[%d]\n", new, new->s.len, new->s.s,
			new->s.len);

	*last_node = new;
	if(!*ini_node) {
		LM_DBG("ini_node[%p] was NULL, this is the first node\n", ini_node);
		*ini_node = new;
	}
	LM_DBG("ini_node:%p last_node:%p\n", *ini_node, *last_node);

	return 0;
}


/* Load data from database */
int secf_load_db(void)
{
	db_key_t db_cols[3];
	db1_res_t *db_res = NULL;
	str str_data = STR_NULL;
	int i, action, type;
	int rows = 0;
	int res = 0;

	/* Connect to DB */
	db_handle = db_funcs.init(&secf_db_url);
	if(db_handle == NULL) {
		LM_ERR("Invalid db handle\n");
		return -1;
	}

	/* Choose new hash table and free its old contents */
	if (*secf_data == secf_data_1) {
		*secf_data = secf_data_2;
	} else {
		*secf_data = secf_data_1;
	}
	secf_free_data(*secf_data);

	/* Prepare the data for the query */
	db_cols[0] = &secf_action_col;
	db_cols[1] = &secf_type_col;
	db_cols[2] = &secf_data_col;

	/* Execute query */
	if(db_funcs.use_table(db_handle, &secf_table_name) < 0) {
		LM_ERR("Unable to use table '%.*s'\n", secf_table_name.len,
				secf_table_name.s);
		return -1;
	}
	if(db_funcs.query(db_handle, NULL, NULL, NULL, db_cols, 0, 3, NULL, &db_res)
			< 0) {
		LM_ERR("Failed to query database\n");
		db_funcs.close(db_handle);
		return -1;
	}

	rows = RES_ROW_N(db_res);
	if(rows == 0) {
		LM_DBG("No data found in database\n");
		res = 0;
		goto clean;
	}

	lock_get(&(*secf_data)->lock);
	for(i = 0; i < rows; i++) {
		action = (int)RES_ROWS(db_res)[i].values[0].val.int_val;
		type = (int)RES_ROWS(db_res)[i].values[1].val.int_val;
		str_data.s = (char *)RES_ROWS(db_res)[i].values[2].val.string_val;
		str_data.len = strlen(str_data.s);
		LM_DBG("[%d] append_rule for action:%d type:%d data:%.*s\n", i, action,
				type, str_data.len, str_data.s);

		if(secf_append_rule(action, type, &str_data) < 0) {
			LM_ERR("Can't append_rule with action:%d type:%d\n", action, type);
			res = -1;
			lock_release(&(*secf_data)->lock);
			goto clean;
		}
	}
	lock_release(&(*secf_data)->lock);

clean:
	if(db_res) {
		if(db_funcs.free_result(db_handle, db_res) < 0) {
			LM_DBG("Failed to free the result\n");
		}
	}
	db_funcs.close(db_handle);
	return res;
}


/* Init database connection */
int secf_init_db(void)
{
	if(secf_db_url.s == NULL) {
		LM_ERR("Database not configured\n");
		return -1;
	}

	secf_db_url.len = strlen(secf_db_url.s);

	if(db_bind_mod(&secf_db_url, &db_funcs) < 0) {
		LM_ERR("Unable to bind to db driver - %.*s\n", secf_db_url.len,
				secf_db_url.s);
		return -1;
	}

	if(check_version() == -1)
		return -1;

	return 0;
}
