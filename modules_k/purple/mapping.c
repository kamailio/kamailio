/* Kamailio PURPLE MODULE
 * 
 * Copyright (C) 2008 Atos Worldline
 * Contact: Eric PTAK <eric.ptak@atosorigin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"

#include "mapping.h"



/*
 * CREATE TABLE purple_map (
 * 	id INT UNSIGNED PRIMARY KEY NOT NULL AUTO_INCREMENT,
 * 	sip_user VARCHAR(128) NOT NULL,
 * 	ext_user VARCHAR(128) NOT NULL,
 * 	ext_prot VARCHAR(16) NOT NULL,
 * 	ext_pass VARCHAR(64)
 * );
 *
 * +----------+------------------+------+-----+---------+----------------+
 * | Field    | Type             | Null | Key | Default | Extra          |
 * +----------+------------------+------+-----+---------+----------------+
 * | id       | int(10) unsigned | NO   | PRI | NULL    | auto_increment |
 * | sip_user | varchar(128)     | NO   |     | NULL    |                |
 * | ext_user | varchar(128)     | NO   |     | NULL    |                |
 * | ext_prot | varchar(16)      | NO   |     | NULL    |                |
 * | ext_pass | varchar(64)      | YES  |     | NULL    |                |
 * +----------+------------------+------+-----+---------+----------------+
 * 
 */


extern db_func_t pa_dbf;
extern str db_url;
extern str db_table;

void extern_account_free(extern_account_t *accounts, int count) {
	if (accounts) {
		int i;
		for (i = 0; i < count; i++) {
			if (accounts[i].protocol)
				pkg_free(accounts[i].protocol);
			if (accounts[i].username)
				pkg_free(accounts[i].username);
			if (accounts[i].password)
				pkg_free(accounts[i].password);
		}
		pkg_free(accounts);
	}
}

void extern_user_free(extern_user_t *users, int count) {
	if (users) {
		int i;
		for (i = 0; i < count; i++) {
			if (users[i].protocol)
				pkg_free(users[i].protocol);
			if (users[i].username)
				pkg_free(users[i].username);
		}
		pkg_free(users);
	}
}

char *find_sip_user(char *extern_user) {
	LM_DBG("looking up sip user for %s\n", extern_user);
	char *sip_user_r;
	str sip_user;

	str ext_user;
	ext_user.s = extern_user;
	ext_user.len = strlen(extern_user);
	
	db_key_t query_cols[6];
	db_op_t  query_ops[6];
	db_val_t query_vals[6];
	db_key_t result_cols[6];
	int n_query_cols = 0, n_result_cols = 0;
	db_row_t *row;
	db_val_t *row_vals;
	db1_res_t *result = NULL;
	int sip_user_col;

	str q_ext_user = {"ext_user", 8};
	query_cols[n_query_cols] = &q_ext_user;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STR;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.str_val = ext_user;
	n_query_cols++;
	
	str r_sip_user = {"sip_user", 8};
	result_cols[sip_user_col=n_result_cols++] = &r_sip_user;

	db1_con_t *pa_db;
	pa_db = pa_dbf.init(&db_url);
	if (!pa_db) {
		LM_ERR("error connecting database\n");
		return NULL;
	}
	
	if (pa_dbf.use_table(pa_db, &db_table) < 0) {
		LM_ERR("error in use_table\n");
		return NULL;
	}

	if (pa_dbf.query(pa_db, query_cols, query_ops, query_vals, result_cols, n_query_cols, n_result_cols, 0, &result) < 0) {
		LM_ERR("error in sql query\n");
		pa_dbf.close(pa_db);
		return NULL;
	}
	
	if (result == NULL)
		return NULL;
	if (result->n <= 0)
		return NULL;

	row = &result->rows[0];
	row_vals = ROW_VALUES(row);

	sip_user_r = (char*) row_vals[sip_user_col].val.string_val;
	
	if (sip_user_r == NULL)
		return NULL;

	sip_user.s = (char*) pkg_malloc(sizeof(char) * (strlen(sip_user_r)+1));
	sip_user.len = sprintf(sip_user.s, "%s", sip_user_r);

	pa_dbf.free_result(pa_db, result);
	pa_dbf.close(pa_db);

	return sip_user.len ? sip_user.s : NULL;
}

extern_account_t *find_accounts(char* sip_user, int* count) {
	LM_DBG("looking up external account for %s\n", sip_user);
	extern_account_t* accounts = NULL;
	*count = 0;

	db_key_t query_cols[6];
	db_op_t  query_ops[6];
	db_val_t query_vals[6];
	db_key_t result_cols[6];
	int n_query_cols = 0, n_result_cols = 0;
	db_row_t *row;
	db_val_t *row_vals;
	db1_res_t *result = NULL;
	int ext_prot_col, ext_user_col, ext_pass_col;

	str q_sip_user = {"sip_user", 8};
	query_cols[n_query_cols] = &q_sip_user;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STRING;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.string_val = sip_user;
	n_query_cols++;
	
	str r_ext_prot = {"ext_prot", 8};
	str r_ext_user = {"ext_user", 8};
	str r_ext_pass = {"ext_pass", 8};
	result_cols[ext_prot_col=n_result_cols++] = &r_ext_prot;
	result_cols[ext_user_col=n_result_cols++] = &r_ext_user;
	result_cols[ext_pass_col=n_result_cols++] = &r_ext_pass;

	db1_con_t *pa_db;
	pa_db = pa_dbf.init(&db_url);
	if (!pa_db) {
		LM_ERR("error connecting database\n");
		return NULL;
	}
	
	if (pa_dbf.use_table(pa_db, &db_table) < 0) {
		LM_ERR("error in use_table\n");
		return NULL;
	}

	if (pa_dbf.query(pa_db, query_cols, query_ops, query_vals, result_cols, n_query_cols, n_result_cols, 0, &result) < 0) {
		LM_ERR("in sql query\n");
		pa_dbf.close(pa_db);
		return NULL;
	}
	else
		LM_DBG("sql query done\n");

	if (result == NULL) {
		LM_ERR("result = NULL\n");
		return NULL;
	}
	if (result->n <= 0) {
		LM_ERR("result count = %d\n", result->n);
		return NULL;
	}

	accounts = (extern_account_t*) pkg_malloc(sizeof(extern_account_t)*result->n);

	int i;
	char *val;
	for (i = 0; i < result->n; i++) {
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);

		val = (char*) row_vals[ext_user_col].val.string_val;
		accounts[i].username = (char*)pkg_malloc(sizeof(char)*(strlen(val)+1));
		strcpy(accounts[i].username, val);
		
		val = (char*) row_vals[ext_pass_col].val.string_val;
		accounts[i].password = (char*)pkg_malloc(sizeof(char)*(strlen(val)+1));
		strcpy(accounts[i].password, val);
		
		val = (char*) row_vals[ext_prot_col].val.string_val;
		accounts[i].protocol = (char*)pkg_malloc(sizeof(char)*(strlen(val)+1));
		strcpy(accounts[i].protocol, val);

	}

	*count = result->n;
	
	pa_dbf.free_result(pa_db, result);
	pa_dbf.close(pa_db);
	
	return accounts;
}

extern_user_t *find_users(char *sip_user, int* count) {
	LM_DBG("looking up external users for %s\n", sip_user);
	extern_user_t* users = NULL;
	*count = 0;

	db_key_t query_cols[6];
	db_op_t  query_ops[6];
	db_val_t query_vals[6];
	db_key_t result_cols[6];
	int n_query_cols = 0, n_result_cols = 0;
	db_row_t *row;
	db_val_t *row_vals;
	db1_res_t *result = NULL;
	int ext_prot_col, ext_user_col;
	
	str q_sip_user = {"sip_user", 8};
	query_cols[n_query_cols] = &q_sip_user;
	query_ops[n_query_cols] = OP_EQ;
	query_vals[n_query_cols].type = DB1_STRING;
	query_vals[n_query_cols].nul = 0;
	query_vals[n_query_cols].val.string_val = sip_user;
	n_query_cols++;
	
	str r_ext_prot = {"ext_prot", 8};
	str r_ext_user = {"ext_user", 8};
	result_cols[ext_prot_col=n_result_cols++] = &r_ext_prot;
	result_cols[ext_user_col=n_result_cols++] = &r_ext_user;

	db1_con_t *pa_db;
	pa_db = pa_dbf.init(&db_url);
	if (!pa_db) {
		LM_ERR("error connecting database\n");
		return NULL;
	}
	
	if (pa_dbf.use_table(pa_db, &db_table) < 0) {
		LM_ERR("error in use_table\n");
		return NULL;
	}

	if (pa_dbf.query(pa_db, query_cols, query_ops, query_vals, result_cols, n_query_cols, n_result_cols, 0, &result) < 0) {
		LM_ERR("in sql query\n");
		pa_dbf.close(pa_db);
		return NULL;
	}

	if (result == NULL)
		return NULL;
	if (result->n <= 0)
		return NULL;

	users = (extern_user_t*) pkg_malloc(sizeof(extern_user_t)*result->n);

	int i;
	char *val;
	for (i = 0; i < result->n; i++) {
		row = &result->rows[i];
		row_vals = ROW_VALUES(row);

		val = (char*) row_vals[ext_user_col].val.string_val;
		users[i].username = (char*) pkg_malloc(sizeof(char)*(strlen(val)+1));
		strcpy(users[i].username, val);
		
		val = (char*) row_vals[ext_prot_col].val.string_val;
		users[i].protocol = (char*) pkg_malloc(sizeof(char)*(strlen(val)+1));
		strcpy(users[i].protocol, val);

	}

	*count = result->n;
	
	pa_dbf.free_result(pa_db, result);
	pa_dbf.close(pa_db);
	
	return users;
}


