/*
 * $Id$
 *
 * allow_trusted related functions
 *
 * Copyright (C) 2003-2012 Juha Heinanen
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
 * History:
 * --------
 *  2004-06-07  updated to the new DB api, moved reload_trusted_table (andrei)
 */

#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include "permissions.h"
#include "hash.h"
#include "../../config.h"
#include "../../lib/srdb1/db.h"
#include "../../ip_addr.h"
#include "../../mod_fix.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../usr_avp.h"

#define TABLE_VERSION 5

struct trusted_list ***hash_table = 0;    /* Pointer to current hash table pointer */
struct trusted_list **hash_table_1 = 0;   /* Pointer to hash table 1 */
struct trusted_list **hash_table_2 = 0;   /* Pointer to hash table 2 */


static db1_con_t* db_handle = 0;
static db_func_t perm_dbf;


/*
 * Reload trusted table to new hash table and when done, make new hash table
 * current one.
 */
int reload_trusted_table(void)
{
	db_key_t cols[4];
	db1_res_t* res = NULL;
	db_row_t* row;
	db_val_t* val;

	struct trusted_list **new_hash_table;
	struct trusted_list **old_hash_table;
	int i;

	char *pattern, *tag;

	if (hash_table == 0) {
	    LM_ERR("in-memory hash table not initialized\n");
	    return -1;
	}

	if (db_handle == 0) {
	    LM_ERR("no connection to database\n");
	    return -1;
	}

	cols[0] = &source_col;
	cols[1] = &proto_col;
	cols[2] = &from_col;
	cols[3] = &tag_col;

	if (perm_dbf.use_table(db_handle, &trusted_table) < 0) {
		LM_ERR("failed to use trusted table\n");
		return -1;
	}

	if (perm_dbf.query(db_handle, NULL, 0, NULL, cols, 0, 4, 0, &res) < 0) {
		LM_ERR("failed to query database\n");
		return -1;
	}

	/* Choose new hash table and free its old contents */
	if (*hash_table == hash_table_1) {
		new_hash_table = hash_table_2;
	} else {
		new_hash_table = hash_table_1;
	}
	empty_hash_table(new_hash_table);

	row = RES_ROWS(res);

	LM_DBG("number of rows in trusted table: %d\n", RES_ROW_N(res));
		
	for (i = 0; i < RES_ROW_N(res); i++) {
	    val = ROW_VALUES(row + i);
	    if ((ROW_N(row + i) == 4) &&
		((VAL_TYPE(val) == DB1_STRING) || (VAL_TYPE(val) == DB1_STR) ) && 
		!VAL_NULL(val) &&
		((VAL_TYPE(val + 1) == DB1_STRING) || (VAL_TYPE(val + 1) == DB1_STR))
		&& !VAL_NULL(val + 1) &&
		(VAL_NULL(val + 2) ||
		 (((VAL_TYPE(val + 2) == DB1_STRING) || (VAL_TYPE(val + 2) == DB1_STR)) &&
		!VAL_NULL(val + 2))) && (VAL_NULL(val + 3) ||
		 (((VAL_TYPE(val + 3) == DB1_STRING) || (VAL_TYPE(val + 3) == DB1_STR) )&& 
		!VAL_NULL(val + 3)))) {
		if (VAL_NULL(val + 2)) {
		    pattern = 0;
		} else {
		    pattern = (char *)VAL_STRING(val + 2);
		}
		if (VAL_NULL(val + 3)) {
		    tag = 0;
		} else {
		    tag = (char *)VAL_STRING(val + 3);
		}
		if (hash_table_insert(new_hash_table,
				      (char *)VAL_STRING(val),
				      (char *)VAL_STRING(val + 1),
				      pattern, tag) == -1) {
		    LM_ERR("hash table problem\n");
		    perm_dbf.free_result(db_handle, res);
		    empty_hash_table(new_hash_table);
		    return -1;
		}
		LM_DBG("tuple <%s, %s, %s, %s> inserted into trusted hash "
		    "table\n", VAL_STRING(val), VAL_STRING(val + 1),
		    pattern, tag);
	    } else {
		LM_ERR("database problem\n");
		perm_dbf.free_result(db_handle, res);
		empty_hash_table(new_hash_table);
		return -1;
	    }
	}

	perm_dbf.free_result(db_handle, res);

	old_hash_table = *hash_table;
	*hash_table = new_hash_table;
	empty_hash_table(old_hash_table);

	LM_DBG("trusted table reloaded successfully.\n");
	
	return 1;
}


/*
 * Initialize data structures
 */
int init_trusted(void)
{
	/* Check if hash table needs to be loaded from trusted table */
	if (!db_url.s) {
		LM_INFO("db_url parameter of permissions module not set, "
			"disabling allow_trusted\n");
		return 0;
	} else {
		if (db_bind_mod(&db_url, &perm_dbf) < 0) {
			LM_ERR("load a database support module\n");
			return -1;
		}

		if (!DB_CAPABILITY(perm_dbf, DB_CAP_QUERY)) {
			LM_ERR("database module does not implement 'query' function\n");
			return -1;
		}
	}

	hash_table_1 = hash_table_2 = 0;
	hash_table = 0;

	if (db_mode == ENABLE_CACHE) {
		db_handle = perm_dbf.init(&db_url);
		if (!db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}

		if(db_check_table_version(&perm_dbf, db_handle, &trusted_table, TABLE_VERSION) < 0) {
			LM_ERR("error during table version check.\n");
			perm_dbf.close(db_handle);
			return -1;
		}

		hash_table_1 = new_hash_table();
		if (!hash_table_1) return -1;
		
		hash_table_2  = new_hash_table();
		if (!hash_table_2) goto error;
		
		hash_table = (struct trusted_list ***)shm_malloc
			(sizeof(struct trusted_list **));
		if (!hash_table) goto error;

		*hash_table = hash_table_1;

		if (reload_trusted_table() == -1) {
			LM_CRIT("reload of trusted table failed\n");
			goto error;
		}

		perm_dbf.close(db_handle);
		db_handle = 0;
	}
	return 0;

error:
	if (hash_table_1) {
		free_hash_table(hash_table_1);
		hash_table_1 = 0;
	}
	if (hash_table_2) {
		free_hash_table(hash_table_2);
		hash_table_2 = 0;
	}
	if (hash_table) {
		shm_free(hash_table);
		hash_table = 0;
	}
	perm_dbf.close(db_handle);
	db_handle = 0;
	return -1;
}


/*
 * Open database connections if necessary
 */
int init_child_trusted(int rank)
{
	if (db_mode == ENABLE_CACHE)
		return 0;

	if ((rank <= 0) && (rank != PROC_RPC) && (rank != PROC_UNIXSOCK))
		return 0;

	if (!db_url.s) {
		return 0;
	}

	db_handle = perm_dbf.init(&db_url);
	if (!db_handle) {
		LM_ERR("unable to connect database\n");
		return -1;
	}

	if (db_check_table_version(&perm_dbf, db_handle, &trusted_table,
				   TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		perm_dbf.close(db_handle);
		return -1;
	}

	return 0;
}


/*
 * Open database connection if necessary
 */
int mi_init_trusted(void)
{
    if (!db_url.s) return 0;
    db_handle = perm_dbf.init(&db_url);
    if (!db_handle) {
	LM_ERR("unable to connect database\n");
	return -1;
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
static inline int match_proto(const char *proto_string, int proto_int)
{
        if ((proto_int == PROTO_NONE) ||
	                (strcasecmp(proto_string, "any") == 0))
	        return 1;
	
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

	if (proto_int == PROTO_WS) {
		if (strcasecmp(proto_string, "ws") == 0) {
			return 1;
		} else {
			return 0;
		}
	}
	
	if (proto_int == PROTO_WSS) {
		if (strcasecmp(proto_string, "wss") == 0) {
			return 1;
		} else {
			return 0;
		}
	}

	LM_ERR("unknown request protocol\n");

	return 0;
}

/*
 * Matches from uri against patterns returned from database.  Returns number
 * of matches or -1 if none of the patterns match.
 */
static int match_res(struct sip_msg* msg, int proto, db1_res_t* _r)
{
	int i, tag_avp_type;
	str uri;
	char uri_string[MAX_URI_SIZE+1];
	db_row_t* row;
	db_val_t* val;
	regex_t preg;
	int_str tag_avp, avp_val;
	int count = 0;

	if (IS_SIP(msg)) {
		if (parse_from_header(msg) < 0) return -1;
		uri = get_from(msg)->uri;
		if (uri.len > MAX_URI_SIZE) {
			LM_ERR("message has From URI too large\n");
			return -1;
		}
		memcpy(uri_string, uri.s, uri.len);
		uri_string[uri.len] = (char)0;
	}
	get_tag_avp(&tag_avp, &tag_avp_type);

	row = RES_ROWS(_r);

	for(i = 0; i < RES_ROW_N(_r); i++) {
		val = ROW_VALUES(row + i);
		if ((ROW_N(row + i) == 3) &&
		    (VAL_TYPE(val) == DB1_STRING) && !VAL_NULL(val) &&
		    match_proto(VAL_STRING(val), proto) &&
		    (VAL_NULL(val + 1) ||
		      ((VAL_TYPE(val + 1) == DB1_STRING) && !VAL_NULL(val + 1))) &&
		    (VAL_NULL(val + 2) ||
		      ((VAL_TYPE(val + 2) == DB1_STRING) && !VAL_NULL(val + 2))))
		{
			if (!VAL_NULL(val + 1) && IS_SIP(msg)) {
				if (regcomp(&preg, (char *)VAL_STRING(val + 1), REG_NOSUB)) {
					LM_ERR("invalid regular expression\n");
					continue;
				}
				if (regexec(&preg, uri_string, 0, (regmatch_t *)0, 0)) {
					regfree(&preg);
					continue;
				}
			    regfree(&preg);
			}
			/* Found a match */
			if (tag_avp.n && !VAL_NULL(val + 2)) {
				avp_val.s.s = (char *)VAL_STRING(val + 2);
				avp_val.s.len = strlen(avp_val.s.s);
				if (add_avp(tag_avp_type|AVP_VAL_STR, tag_avp, avp_val) != 0) {
					LM_ERR("failed to set of tag_avp failed\n");
					return -1;
				}
			}
			if (!peer_tag_mode) 
				return 1;
			count++;
		}
	}
	if (!count)
		return -1;
	else 
		return count;
}


/*
 * Checks based on given source IP address and protocol, and From URI
 * of request if request can be trusted without authentication.
 */
int allow_trusted(struct sip_msg* msg, char *src_ip, int proto) 
{
	int result;
	db1_res_t* res = NULL;
	
	db_key_t keys[1];
	db_val_t vals[1];
	db_key_t cols[3];

	if (db_mode == DISABLE_CACHE) {
	
	        if (db_handle == 0) {
		    LM_ERR("no connection to database\n");
		    return -1;
	        }

		keys[0] = &source_col;
		cols[0] = &proto_col;
		cols[1] = &from_col;
		cols[2] = &tag_col;

		if (perm_dbf.use_table(db_handle, &trusted_table) < 0) {
			LM_ERR("failed to use trusted table\n");
			return -1;
		}
		
		VAL_TYPE(vals) = DB1_STRING;
		VAL_NULL(vals) = 0;
		VAL_STRING(vals) = src_ip;

		if (perm_dbf.query(db_handle, keys, 0, vals, cols, 1, 3, 0,
				   &res) < 0){
			LM_ERR("failed to query database\n");
			return -1;
		}

		if (RES_ROW_N(res) == 0) {
			perm_dbf.free_result(db_handle, res);
			return -1;
		}
		
		result = match_res(msg, proto, res);
		perm_dbf.free_result(db_handle, res);
		return result;
	} else {
		return match_hash_table(*hash_table, msg, src_ip, proto);
	}
}


/*
 * Checks based on request's source address, protocol, and From URI
 * if request can be trusted without authentication.
 */
int allow_trusted_0(struct sip_msg* _msg, char* str1, char* str2) 
{
    return allow_trusted(_msg, ip_addr2a(&(_msg->rcv.src_ip)),
			 _msg->rcv.proto);
}


/*
 * Checks based on source address and protocol given in pvar arguments and
 * and requests's From URI, if request can be trusted without authentication.
 */
int allow_trusted_2(struct sip_msg* _msg, char* _src_ip_sp, char* _proto_sp) 
{
    str src_ip, proto;
    int proto_int;

    if (_src_ip_sp==NULL
	|| (fixup_get_svalue(_msg, (gparam_p)_src_ip_sp, &src_ip) != 0)) {
	LM_ERR("src_ip param does not exist or has no value\n");
	return -1;
    }
    
    if (_proto_sp==NULL
	|| (fixup_get_svalue(_msg, (gparam_p)_proto_sp, &proto) != 0)) {
	LM_ERR("proto param does not exist or has no value\n");
	return -1;
    }

    if(proto.len<2 || proto.len>4)
	goto error;

    switch(proto.s[0]) {
    case 'a': case 'A':
	if (proto.len==3 && strncasecmp(proto.s, "any", 3) == 0) {
	    proto_int = PROTO_NONE;
	} else goto error;
	break;
    case 'u': case 'U':
	if (proto.len==3 && strncasecmp(proto.s, "udp", 3) == 0) {
	    proto_int = PROTO_UDP;
	} else goto error;
	break;
    case 't': case 'T':
	if (proto.len==3 && strncasecmp(proto.s, "tcp", 3) == 0) {
	    proto_int = PROTO_TCP;
	} else if (proto.len==3 && strncasecmp(proto.s, "tls", 3) == 0) {
	    proto_int = PROTO_TLS;
	} else goto error;
	break;
    case 's': case 'S':
	if (proto.len==4 && strncasecmp(proto.s, "sctp", 4) == 0) {
	    proto_int = PROTO_SCTP;
	} else goto error;
	break;
    case 'w': case 'W':
	if (proto.len==2 && strncasecmp(proto.s, "ws", 2) == 0) {
	    proto_int = PROTO_WS;
	} else if (proto.len==3 && strncasecmp(proto.s, "wss", 3) == 0) {
	    proto_int = PROTO_WSS;
	} else goto error;
        break;
    default:
	goto error;
    }

    return allow_trusted(_msg, src_ip.s, proto_int);
error:
    LM_ERR("unknown protocol %.*s\n", proto.len, proto.s);
    return -1;
}


int reload_trusted_table_cmd(void)
{
	if (!db_handle) {
		db_handle = perm_dbf.init(&db_url);
		if (!db_handle) {
			LM_ERR("unable to connect database\n");
			return -1;
		}
	}
	if (reload_trusted_table () != 1) {
		perm_dbf.close(db_handle);
		db_handle = 0;
		return -1;
	}

	perm_dbf.close(db_handle);
	db_handle = 0;

	return 1;
}
