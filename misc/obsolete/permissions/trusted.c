/*
 * $Id$
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * History:
 * --------
 *  2004-06-07  updated to the new DB api, moved reload_trusted_table (andrei)
 *  2006-08-14: DB handlers are moved to permission.c (Miklos)
 */

#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include "trusted.h"
#include "permissions.h"
#include "trusted_hash.h"
#include "../../config.h"
#include "../../lib/srdb2/db.h"
#include "../../ip_addr.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"

struct trusted_list ***hash_table = NULL;     /* Pointer to current hash table pointer */
struct trusted_list **hash_table_1 = NULL;   /* Pointer to hash table 1 */
struct trusted_list **hash_table_2 = NULL;;   /* Pointer to hash table 2 */

/* DB commands to query and load the table */
static db_cmd_t	*cmd_load_trusted = NULL;
static db_cmd_t	*cmd_query_trusted = NULL;

/*
 * Initialize data structures
 */
int init_trusted(void)
{
	if (db_mode == ENABLE_CACHE) {

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
	}
	return 0;

 error:
	clean_trusted();
	return -1;
}

/*
 * Close connections and release memory
 */
void clean_trusted(void)
{
	if (hash_table_1) {
		free_hash_table(hash_table_1);
		hash_table_1 = NULL;
	}
	if (hash_table_2) {
		free_hash_table(hash_table_2);
		hash_table_2 = NULL;
	}
	if (hash_table) {
		shm_free(hash_table);
		hash_table = NULL;
	}
}

/* prepare the DB cmds */
int init_trusted_db(void)
{
	db_fld_t load_res_cols[] = {
		{.name = source_col,	.type = DB_CSTR},
		{.name = proto_col,	.type = DB_CSTR},
		{.name = from_col,	.type = DB_CSTR},
		{.name = NULL}
	};

	db_fld_t query_match[] = {
		{.name = source_col,	.type = DB_CSTR},
		{.name = NULL}
	};

	db_fld_t query_res_cols[] = {
		{.name = proto_col,	.type = DB_CSTR},
		{.name = from_col,	.type = DB_CSTR},
		{.name = NULL}
	};

	if (!db_conn) return -1;

	if (db_mode == ENABLE_CACHE) {
		cmd_load_trusted =
			db_cmd(DB_GET, db_conn, trusted_table, load_res_cols, NULL, NULL);
		if (!cmd_load_trusted)
			goto error;
	} else {
		cmd_query_trusted =
			db_cmd(DB_GET, db_conn, trusted_table, query_res_cols, query_match, NULL);
		if (!cmd_query_trusted)
			goto error;
	}
	return 0;
error:
	LOG(L_ERR, "init_trusted_db(): failed to prepare DB commands\n");
	return -1;
}

/* destroy the DB cmds */
void destroy_trusted_db(void)
{
	if (cmd_load_trusted) {
		db_cmd_free(cmd_load_trusted);
		cmd_load_trusted = NULL;
	}
	if (cmd_query_trusted) {
		db_cmd_free(cmd_query_trusted);
		cmd_query_trusted = NULL;
	}
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

#define VAL_NULL_STR(fld) ( \
		((fld).flags & DB_NULL) \
		|| (((fld).type == DB_CSTR) && ((fld).v.cstr[0] == '\0')) \
		|| (((fld).type == DB_STR) && \
			(((fld).v.lstr.len == 0) || ((fld).v.lstr.s[0] == '\0'))) \
	)

/*
 * Matches from uri against patterns returned from database.  Returns 1 when
 * first pattern matches and -1 if none of the patterns match.
 */
static int match_res(struct sip_msg* msg, db_res_t* _r)
{
	str uri;
	char uri_string[MAX_URI_SIZE+1];
	db_rec_t	*rec;
	regex_t preg;

	if (!_r) return -1;

	if (parse_from_header(msg) < 0) return -1;
	uri = get_from(msg)->uri;
	if (uri.len > MAX_URI_SIZE) {
		LOG(L_ERR, "match_res(): From URI too large\n");
		return -1;
	}
	memcpy(uri_string, uri.s, uri.len);
	uri_string[uri.len] = (char)0;

	rec = db_first(_r);
	while (rec) {

		if (VAL_NULL_STR(rec->fld[0])
		|| VAL_NULL_STR(rec->fld[1]))
			goto next;

		/* check the protocol */
		if (match_proto(rec->fld[0].v.cstr, msg->rcv.proto) <= 0)
			goto next;

		/* check the from uri */
		if (regcomp(&preg, rec->fld[1].v.cstr, REG_NOSUB)) {
			LOG(L_ERR, "match_res(): Error in regular expression: %s\n",
					rec->fld[0].v.cstr);
			goto next;
		}
		if (regexec(&preg, uri_string, 0, (regmatch_t *)0, 0)) {
			regfree(&preg);
			goto next;
		}
		regfree(&preg);

		/* everything matched */
		return 1;

next:
		rec = db_next(_r);
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
	int result;
	db_res_t	*res = NULL;

	if (!db_url) {
		LOG(L_ERR, "allow_trusted(): ERROR set db_mode parameter of permissions module first !\n");
		return -1;
	}

	if (db_mode == DISABLE_CACHE) {
		if (!cmd_query_trusted) return -1;

		if (!(cmd_query_trusted->match[0].v.cstr =
			ip_addr2a(&(_msg->rcv.src_ip)))
		) {
			LOG(L_ERR, "allow_trusted(): Error in ip address\n");
			return -1;
		}
		if (db_exec(&res, cmd_query_trusted) < 0) {
			LOG(L_ERR, "allow_trusted(): Error while querying database\n");
			return -1;
		}

		result = match_res(_msg, res);

		if (res) db_res_free(res);
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
	db_res_t	*res = NULL;
	db_rec_t	*rec;
	struct trusted_list **new_hash_table;
	int row;
	char	*source, *proto, *from;

	if (!cmd_load_trusted) return -1;

	if (db_exec(&res, cmd_load_trusted) < 0) {
		LOG(L_ERR, "ERROR: permissions: reload_trusted_table():"
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

	row = 0;
	rec = db_first(res);
	while (rec) {
		if (VAL_NULL_STR(rec->fld[0])
		|| VAL_NULL_STR(rec->fld[1])
		|| VAL_NULL_STR(rec->fld[2])) {
			LOG(L_ERR, "ERROR: permissions: trusted_reload():"
				" Database problem, NULL filed is not allowed\n");
			goto error;
		}
		source = rec->fld[0].v.cstr;
		proto = rec->fld[1].v.cstr;
		from = rec->fld[2].v.cstr;

		if (hash_table_insert(new_hash_table,
					source,
					proto,
					from) == -1) {
			LOG(L_ERR, "ERROR: permissions: "
					"trusted_reload(): Hash table problem\n");
			goto error;
		}
		DBG("Tuple <%s, %s, %s> inserted into trusted hash table\n",
			source, proto, from);

		row++;
		rec = db_next(res);
	}
	DBG("Number of rows in trusted table: %d\n", row);

	*hash_table = new_hash_table;
	DBG("Trusted table reloaded successfully.\n");

	if (res) db_res_free(res);
	return 1;

error:
	if (res) db_res_free(res);
	return -1;
}
