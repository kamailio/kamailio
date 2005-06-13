/* 
 * $Id$
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem
 *
 * This file is part of a module for SER, a free SIP server.
 *
 * ALIAS_DB module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use this software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact Voice Sistem by e-mail at the following address:
 *    office@voice-sistem.ro
 *
 * ALIAS_DB module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2004-09-01: first version (ramona)
 */


#ifndef _ALIAS_DB_H_
#define _ALIAS_DB_H_

#include "../../db/db.h"
#include "../../parser/msg_parser.h"


/* Module parameters variables */

extern char* user_column;     /* 'username' column name */
extern char* domain_column;   /* 'domain' column name */
extern char* alias_user_column;     /* 'alias_username' column name */
extern char* alias_domain_column;   /* 'alias_domain' column name */
extern int   use_domain;      /* use or not the domain for alias lookup */
extern str   dstrip_s;

extern db_con_t* db_handle;   /* Database connection handle */

extern int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);

#endif /* _ALIAS_DB_H_ */
