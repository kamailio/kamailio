/*
 * allow_trusted fifo functions
 *
 * Copyright (C) 2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "permissions.h"
#include "hash.h"
#include "fifo.h"
#include "trusted.h"
#include "../../fifo_server.h"
#include "../../dprint.h"
#include "../../db/db.h"


#define TRUSTED_RELOAD "trusted_reload"
#define TRUSTED_DUMP "trusted_dump"


/*
 * Reload trusted table to new hash table and when done, make new hash table
 * current one.
 */
int reload_trusted_table(void)
{
	db_key_t cols[2];
	db_res_t* res;
	db_row_t* row;
	db_val_t* val;

	struct trusted_list **new_hash_table;
	int i;

	cols[0] = source_col;
	cols[1] = proto_col;
	cols[2] = from_col;

	if (db_use_table(db_handle, trusted_table) < 0) {
		LOG(L_ERR, "reload_trusted_table(): Error while trying to use trusted table\n");
		return -1;
	}

	if (db_query(db_handle, NULL, 0, NULL, cols, 0, 3, 0, &res) < 0) {
		LOG(L_ERR, "reload_trusted_table(): Error while querying database\n");
		return -1;
	}

	/* Choose new hash table and free its old contents */
	if (*hash_table == hash_table_1) {
		empty_hash_table(hash_table_2);
		new_hash_table = hash_table_2;
	} else {
		empty_hash_table(hash_table_1);
		new_hash_table = hash_table_1;
	}

	row = RES_ROWS(res);

	DBG("Number of rows in trusted table: %d\n", RES_ROW_N(res));
		
	for (i = 0; i < RES_ROW_N(res); i++) {
		val = ROW_VALUES(row + i);
		if ((ROW_N(row + i) == 3) &&
		    (VAL_TYPE(val) == DB_STRING) && !VAL_NULL(val) &&
		    (VAL_TYPE(val + 1) == DB_STRING) && !VAL_NULL(val + 1) &&
		    (VAL_TYPE(val + 2) == DB_STRING) && !VAL_NULL(val + 2)) {
			if (hash_table_insert(new_hash_table,
					       (char *)VAL_STRING(val),
					       (char *)VAL_STRING(val + 1),
					       (char *)VAL_STRING(val + 2)) == -1) {
				LOG(L_ERR, "trusted_reload(): Hash table problem\n");
				db_free_query(db_handle, res);
				db_close(db_handle);
				return -1;
			}
			DBG("Tuple <%s, %s, %s> inserted into trusted hash table\n",
			    VAL_STRING(val), VAL_STRING(val + 1), VAL_STRING(val + 2));
		} else {
			LOG(L_ERR, "trusted_reload(): Database problem\n");
			db_free_query(db_handle, res);
			db_close(db_handle);
			return -1;
		}
	}

	db_free_query(db_handle, res);

	*hash_table = new_hash_table;

	DBG("Trusted table reloaded successfully.\n");
	
	return 1;
}


/*
 * Fifo function to reload trusted table
 */
static int trusted_reload(FILE* pipe, char* response_file)
{
	if (reload_trusted_table () == 1) {
		fifo_reply (response_file, "200 OK\n");
		return 1;
	} else {
		fifo_reply (response_file, "400 Trusted table reload failed\n");
		return -1;
	}
}


/*
 * Fifo function to print trusted entries from current hash table
 */
static int trusted_dump(FILE* pipe, char* response_file)
{
	FILE *reply_file;
	
	reply_file = open_reply_pipe(response_file);
	if (reply_file == 0) {
		LOG(L_ERR, "domain_dump(): Opening of response file failed\n");
		return -1;
	}
	fputs("200 OK\n", reply_file);
	hash_table_print(*hash_table, reply_file);
	fclose(reply_file);
	return 1;
}


/*
 * Register domain fifo functions
 */
int init_trusted_fifo(void) 
{
	if (register_fifo_cmd(trusted_reload, TRUSTED_RELOAD, 0) < 0) {
		LOG(L_CRIT, "Cannot register trusted_reload\n");
		return -1;
	}

	if (register_fifo_cmd(trusted_dump, TRUSTED_DUMP, 0) < 0) {
		LOG(L_CRIT, "Cannot register trusted_dump\n");
		return -1;
	}

	return 1;
}
