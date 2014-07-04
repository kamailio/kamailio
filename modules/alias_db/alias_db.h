/* 
 * $Id$
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem
 *
 * This file is part of a module for Kamailio, a free SIP server.
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
 * 2004-09-01: first version (ramona)
 */


#ifndef _ALIAS_DB_H_
#define _ALIAS_DB_H_

#include "../../lib/srdb1/db.h"
#include "../../parser/msg_parser.h"


/* Module parameters variables */

extern str user_column;     /* 'username' column name */
extern str domain_column;   /* 'domain' column name */
extern str alias_user_column;     /* 'alias_username' column name */
extern str alias_domain_column;   /* 'alias_domain' column name */
extern str domain_prefix;
extern int use_domain;      /* use or not the domain for alias lookup */
extern int ald_append_branches;  /* append branches after an alias lookup */

extern db1_con_t* db_handle;   /* Database connection handle */

#endif /* _ALIAS_DB_H_ */
