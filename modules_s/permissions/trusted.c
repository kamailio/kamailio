/* 
 * allow_trusted related functions
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
/*
 * History:
 * --------
 *  2004-06-07  updated to the new DB api, moved reload_trusted_table (andrei)
 */

#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include "permissions.h"
#include "hash.h"
#include "fifo.h"
#include "unixsock.h"
#include "../../config.h"
#include "../../db/db.h"
#include "../../ip_addr.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"

#define TABLE_VERSION 1

struct trusted_list ***hash_table;     /* Pointer to current hash table pointer */
struct trusted_list **hash_table_1;   /* Pointer to hash table 1 */
struct trusted_list **hash_table_2;   /* Pointer to hash table 2 */


static db_con_t* db_handle = 0;
static db_func_t perm_dbf;

/*
 * Initialize data structures
 */
int init_trusted(void)
{
	int ver;
	str name;
	     /* Check if hash table needs to be loaded from trusted table */

	if (!db_url) {
		LOG(L_INFO, "db_url parameter of permissions module not set, disabling allow_trusted\n");
		return 0;
	} else {
		if (bind_dbmod(db_url, &perm_dbf) < 0) {
			LOG(L_ERR, "ERROR: persmissions: init_trusted: "
					"load a database support module\n");
			return -1;
		}
	}

	hash_table_1 = hash_table_2 = 0;
	hash_table = 0;

	if (db_mode == ENABLE_CACHE) {
		db_handle = perm_dbf.init(db_url);
		if (!db_handle) {
			LOG(L_ERR, "ERROR: permissions: init_trusted():"
					" Unable to connect database\n");
			return -1;
		}

		name.s = trusted_table;
		name.len = strlen(trusted_table);
		ver = table_version(&perm_dbf, db_handle, &name);

		if (ver < 0) {
			LOG(L_ERR, "permissions:init_trusted(): Error while querying table version\n");
			perm_dbf.close(db_handle);
			return -1;
		} else if (ver < TABLE_VERSION) {
			LOG(L_ERR, "permissions:init_trusted(): Invalid table version (use ser_mysql.sh reinstall)\n");
			perm_dbf.close(db_handle);
			return -1;
		}		
		
		/* Initialize fifo interface */
		(void)init_trusted_fifo();
		
		if (init_trusted_unixsock() < 0) {
			LOG(L_ERR, "permissions:init_trusted(): Error while initializing unixsock interface\n");
			perm_dbf.close(db_handle);
			return -1;
		}

		hash_table_1 = new_hash_table();
		if (!hash_table_1) return -1;
		
		hash_table_2  = new_hash_table();
		if (!hash_table_2) goto error;
		
		hash_table = (struct trusted_list ***)shm_malloc(sizeof(struct trusted_list **));
		if (!hash_table) goto error;

		*hash_table = hash_table_1;

		if (reload_trusted_table() == -1) {
			LOG(L_CRIT, "init_trusted(): Reload of trusted table failed\n");
			goto error;
		}
			
		perm_dbf.close(db_handle);
	}
	return 0;

 error:
	if (hash_table_1) free_hash_table(hash_table_1);
	if (hash_table_2) free_hash_table(hash_table_2);
	if (hash_table) shm_free(hash_table);
	return -1;
}


/*
 * Open database connections if necessary
 */
int init_child_trusted(int rank)
{
	str name;
	int ver;

	if (!db_url) {
		return 0;
	}
	
	/* Check if database is needed by child */
	if (((db_mode == DISABLE_CACHE) && (rank > 0)) || 
	    ((db_mode == ENABLE_CACHE) && (rank == PROC_FIFO))
	   ) {
		db_handle = perm_dbf.init(db_url);
		if (!db_handle) {
			LOG(L_ERR, "ERROR: permissions: init_child_trusted():"
					" Unable to connect database\n");
			return -1;
		}

		name.s = trusted_table;
		name.len = strlen(trusted_table);
		ver = table_version(&perm_dbf, db_handle, &name);

		if (ver < 0) {
			LOG(L_ERR, "ERROR: permissions: init_child_trusted():"
					" Error while querying table version\n");
			perm_dbf.close(db_handle);
			return -1;
		} else if (ver < TABLE_VERSION) {
			LOG(L_ERR, "ERROR: permissions: init_child_trusted():"
					" Invalid table version (use ser_mysql.sh reinstall)\n");
			perm_dbf.close(db_handle);
			return -1;
		}		

	}

	return 0;
}


/*
 * Close connections and release memory
 */
void clean_trusted(void)
{
	if (hash_table_1) free_hash_table(hash_table_1);
	if (hash_table_2) free_hash_table(hash_table_2);
	if (hash_table) shm_free(hash_table);
}


/*
 * Matches protocol string against the protocol of the request.  Returns 1 on
 * success and 0 on failure.
 */
static inline int match_proto(char *proto_string, int proto_int)
{
	if (strcasecmp(proto_string, "any") == 0) return 1;
	
	if (proto_int == PROTO_UDP) {
		if (strcasecmp(proto_string, "udp") == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	
	if (proto_int == PROTO_TCP) {
		if (strcasecmp(proto_string, "tcp") == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	
	if (proto_int == PROTO_TLS) {
		if (strcasecmp(proto_string, "tls") == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	
	if (proto_int == PROTO_SCTP) {
		if (strcasecmp(proto_string, "sctp") == 0) {
			return 1;
		} else {
			return 0;
		}
	}

	LOG(L_ERR, "match_proto(): Unknown request protocol\n");

	return 0;
}

/*
 * Matches from uri againts patterns returned from database.  Returns 1 when
 * first pattern matches and 0 if none of the patterns match.
 */
static int match_res(struct sip_msg* msg, db_res_t* _r)
{
	int i;
	str uri;
	char uri_string[MAX_URI_SIZE+1];
	db_row_t* row;
	db_val_t* val;
	regex_t preg;

	if (parse_from_header(msg) < 0) return -1;
	uri = get_from(msg)->uri;
	if (uri.len > MAX_URI_SIZE) {
		LOG(L_ERR, "match_res(): From URI too large\n");
		return -1;
	}
	memcpy(uri_string, uri.s, uri.len);
	uri_string[uri.len] = (char)0;

	row = RES_ROWS(_r);
		
	for(i = 0; i < RES_ROW_N(_r); i++) {
		val = ROW_VALUES(row + i);
		if ((ROW_N(row + i) == 2) &&
		    (VAL_TYPE(val) == DB_STRING) && !VAL_NULL(val) &&
		    match_proto((char *)VAL_STRING(val), msg->rcv.proto) &&
		    (VAL_TYPE(val + 1) == DB_STRING) && !VAL_NULL(val + 1)) {
			if (regcomp(&preg, (char *)VAL_STRING(val + 1), REG_NOSUB)) {
				LOG(L_ERR, "match_res(): Error in regular expression\n");
				continue;
			}
			if (regexec(&preg, uri_string, 0, (regmatch_t *)0, 0)) {
				regfree(&preg);
				continue;
			} else {
				regfree(&preg);
				return 1;
			}
		}
	}
	return -1;
}


/*
 * Checks based on request's source address, protocol, and from field
 * if request can be trusted without authentication.  Possible protocol
 * values are "any" (that matches any protocol), "tcp", "udp", "tls",
 * and "sctp".
 */
int allow_trusted(struct sip_msg* _msg, char* str1, char* str2) 
{
	db_con_t* db_handle = NULL;
	int result;
	db_res_t* res;
	
	db_key_t keys[1];
	db_val_t vals[1];
	db_key_t cols[2];

	if (!db_url) {
		LOG(L_ERR, "allow_trusted(): ERROR set db_mode parameter of permissions module first !\n");
		return -1;
	}

	if (db_mode == DISABLE_CACHE) {
		keys[0] = source_col;
		cols[0] = proto_col;
		cols[1] = from_col;

		if (perm_dbf.use_table(db_handle, trusted_table) < 0) {
			LOG(L_ERR, "allow_trusted(): Error while trying to use trusted table\n");
			return -1;
		}
		
		VAL_TYPE(vals) = DB_STRING;
		VAL_STRING(vals) = ip_addr2a(&(_msg->rcv.src_ip));

		if (perm_dbf.query(db_handle, keys, 0, vals, cols, 1, 2, 0, &res) < 0){
			LOG(L_ERR, "allow_trusted(): Error while querying database\n");
			return -1;
		}

		if (RES_ROW_N(res) == 0) {
			perm_dbf.free_result(db_handle, res);
			return -1;
		}
		
		result = match_res(_msg, res);
		perm_dbf.free_result(db_handle, res);
		return result;
	} else if (db_mode == ENABLE_CACHE) {
		return match_hash_table(*hash_table, _msg);
	} else {
		LOG(L_ERR, "allow_trusted(): Error - set db_mode parameter of permissions module properly\n");
		return -1;
	}
}



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

	if (perm_dbf.use_table(db_handle, trusted_table) < 0) {
		LOG(L_ERR, "ERROR: permissions: reload_trusted_table():"
				" Error while trying to use trusted table\n");
		return -1;
	}

	if (perm_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 3, 0, &res) < 0) {
		LOG(L_ERR, "ERROR: permsissions: reload_trusted_table():"
				" Error while querying database\n");
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
				LOG(L_ERR, "ERROR: permissions: "
						"trusted_reload(): Hash table problem\n");
				perm_dbf.free_result(db_handle, res);
				perm_dbf.close(db_handle);
				return -1;
			}
			DBG("Tuple <%s, %s, %s> inserted into trusted hash table\n",
			    VAL_STRING(val), VAL_STRING(val + 1), VAL_STRING(val + 2));
		} else {
			LOG(L_ERR, "ERROR: permissions: trusted_reload():"
					" Database problem\n");
			perm_dbf.free_result(db_handle, res);
			perm_dbf.close(db_handle);
			return -1;
		}
	}

	perm_dbf.free_result(db_handle, res);

	*hash_table = new_hash_table;

	DBG("Trusted table reloaded successfully.\n");
	
	return 1;
}
