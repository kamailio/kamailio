/* fifo.c v 0.2 2003/1/19
 *
 * Domain fifo functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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


#include "domain_mod.h"
#include "hash.h"
#include "fifo.h"
#include "../../fifo_server.h"
#include "../../dprint.h"
#include "../../db/db.h"


/*
 * Reload domain table to new hash table and when done, make new hash table
 * current one.
 */
int reload_domain_table ( void )
{
/*	db_key_t keys[] = {domain_domain_col}; */
	db_val_t vals[1];
	db_key_t cols[] = {domain_domain_col};
	db_res_t* res;
	db_row_t* row;
	db_val_t* val;

	struct domain_list **new_hash_table;
	int i;

	if (db_use_table(db_handle, domain_table) < 0) {
		LOG(L_ERR, "reload_domain_table(): Error while trying to use domain table\n");
		return -1;
	}

	VAL_TYPE(vals) = DB_STR;
	VAL_NULL(vals) = 0;
    
	if (db_query(db_handle, NULL, 0, NULL, cols, 0, 1, 0, &res) < 0) {
		LOG(L_ERR, "reload_domain_table(): Error while querying database\n");
		return -1;
	}

	/* Choose new hash table and free its old contents */
	if (*hash_table == hash_table_1) {
		hash_table_free(hash_table_2);
		new_hash_table = hash_table_2;
	} else {
		hash_table_free(hash_table_1);
		new_hash_table = hash_table_1;
	}

	row = RES_ROWS(res);

	DBG("Number of rows in domain table: %d\n", RES_ROW_N(res));
		
	for (i = 0; i < RES_ROW_N(res); i++) {
		val = ROW_VALUES(row + i);
		if ((ROW_N(row) == 1) && (VAL_TYPE(val) == DB_STRING)) {
			
			DBG("Value: %s inserted into domain hash table\n", VAL_STRING(val));

			if (hash_table_install(new_hash_table, (char *)(VAL_STRING(val))) == -1) {
				LOG(L_ERR, "domain_reload(): Hash table problem\n");
				db_free_query(db_handle, res);
				return -1;
			}
		} else {
			LOG(L_ERR, "domain_reload(): Database problem\n");
			db_free_query(db_handle, res);
			return -1;
		}
	}
	db_free_query(db_handle, res);

	*hash_table = new_hash_table;
	
	return 1;
}


/*
 * Fifo function to reload domain table
 */
static int domain_reload ( FILE* pipe, char* response_file )
{
	if (reload_domain_table () == 1) {
		fifo_reply (response_file, "200 OK\n");
		return 1;
	} else {
		fifo_reply (response_file, "400 Domain table reload failed\n");
		return -1;
	}
}


/*
 * Fifo function to print domains from current hash table
 */
static int domain_dump ( FILE* pipe, char* response_file )
{
	FILE *reply_file;
	
	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "domain_dump(): Opening of response file failed\n");
		return -1;
	}
	fputs( "200 OK\n", reply_file );
	hash_table_print( *hash_table, reply_file );
	fclose(reply_file);
	return 1;
}


/*
 * Register domain fifo functions
 */
int init_domain_fifo( void ) 
{
	if (register_fifo_cmd(domain_reload, DOMAIN_RELOAD, 0) < 0) {
		LOG(L_CRIT, "Cannot register domain_reload\n");
		return -1;
	}

	if (register_fifo_cmd(domain_dump, DOMAIN_DUMP, 0) < 0) {
		LOG(L_CRIT, "Cannot register domain_dump\n");
		return -1;
	}

	return 1;
}
