/*
 * $Id$
 *
 * Digest Authentication Module
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#ifndef AUTH_MOD_H
#define AUTH_MOD_H

#include "../../db/db.h"
#include "defs.h"
#include "../../str.h"
#include "../../parser/digest/digest.h" /* auth_body_t */
#include "../../parser/msg_parser.h"    /* struct sip_msg */


/*
 * Module parameters variables
 */

extern char* db_url;          /* Database URL */
extern char* user_column;     /* 'user' column name */
extern char* realm_column;    /* 'realm' column name */
extern char* pass_column;     /* 'password' column name */

#ifdef USER_DOMAIN_HACK
extern char* pass_column_2;   /* Column containg HA1 string constructed
			       * of user@domain username
			       */
#endif

extern str secret;            /* secret phrase used to generate nonce */
extern char* grp_table;       /* 'group' table name */
extern char* grp_user_col;    /* 'user' column name in group table */
extern char* grp_domain_col;  /* 'domain' column name in group table */
extern char* grp_grp_col;     /* "group' column name in group table */
extern int calc_ha1;          /* if set to 1, ha1 is calculated by the server */
extern int nonce_expire;      /* nonce expire interval */
extern int retry_count;       /* How many time a client can retry */
extern int grp_use_domain;    /* Use domain in is_user_in */

extern db_con_t* db_handle;   /* Database connection handle */



/* Stateless reply function pointer */
extern int (*sl_reply)(struct sip_msg* _m, char* _str1, char* _str2);


#endif /* AUTH_MOD_H */
