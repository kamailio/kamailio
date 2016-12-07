/*
 * Digest Authentication - Database support
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */


#ifndef AUTHDB_MOD_H
#define AUTHDB_MOD_H

#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "../../modules/auth/api.h"
#include "../../parser/msg_parser.h"


/*
 * Module parameters variables
 */

extern str user_column;     /* 'username' column name */
extern str domain_column;   /* 'domain' column name */
extern str pass_column;     /* 'password' column name */
extern str pass_column_2;   /* Column containing HA1 string constructed
			     * of user@domain username
			     */

extern int calc_ha1;          /* if set to 1, ha1 is calculated by the server */
extern int use_domain;        /* If set to 1 then the domain will be used when selecting a row */

extern db1_con_t* auth_db_handle; /* database connection handle */
extern db_func_t auth_dbf;

extern auth_api_s_t auth_api;

extern pv_elem_t *credentials;
extern int credentials_n;

#endif /* AUTHDB_MOD_H */
