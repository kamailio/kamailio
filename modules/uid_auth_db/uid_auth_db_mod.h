/*
 * $Id$
 *
 * Digest Authentication - Database support
 *
 * Copyright (C) 2001-2003 FhG Fokus
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


#ifndef UID_AUTH_DB_MOD_H
#define UID_AUTH_DB_MOD_H

#include "../../str.h"
#include "../../lib/srdb2/db.h"
#include "../../modules/auth/api.h"
#include "../../parser/msg_parser.h"


/*
 * Module parameters variables
 */
extern str username_column; /* 'username' column name */
extern str did_column;      /* 'did' column name */
extern str realm_column;    /* 'realm' column name */
extern str pass_column;     /* 'password' column name */
extern str pass_column_2;   /* Column containing HA1 string constructed
			     * of user@domain username
			     */
extern str flags_column;    /* Flags column in credentials table */

extern int calc_ha1;          /* if set to 1, ha1 is calculated by the server */
extern int use_did;           /* Whether query should also use did in query */
extern int check_all;         /* if set to 1, multiple db entries are checked */

extern db_ctx_t* auth_db_handle; /* database connection handle */

extern auth_api_s_t auth_api;

extern str* credentials;
extern int credentials_n;


/* structure holding information for a table (holds
 * only pregenerated DB queries now) */
typedef struct _authdb_table_info_t {
	str table; /* s is zero terminated */
	db_cmd_t *query_pass; /* queries HA1 */
	db_cmd_t *query_pass2; /* queries HA1B */
	db_cmd_t *query_password; /* queries plain password */

	struct _authdb_table_info_t *next;
	char buf[1]; /* used to hold 'table' data */
} authdb_table_info_t;

#endif /* AUTHDB_MOD_H */
