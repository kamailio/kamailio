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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef AUTHDB_MOD_H
#define AUTHDB_MOD_H

#include "../../str.h"
#include "../../db/db.h"
#include "../auth/api.h"
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

extern db_con_t* auth_db_handle; /* database connection handle */
extern db_func_t auth_dbf;

extern auth_api_t auth_api;

extern str* credentials;
extern int credentials_n;

/*
 * Pointer to reply function in stateless module
 */
extern int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);

#endif /* AUTHDB_MOD_H */
