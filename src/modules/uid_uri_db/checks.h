/*
 * $Id$
 *
 * Various URI checks
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
 *
 * History:
 * --------
 * 2003-03-26 created by janakj
 */


#ifndef CHECKS_H
#define CHECKS_H

#include "../../parser/msg_parser.h"


/*
 * Check if To header field contains the same username
 * as digest credentials
 */
int check_to(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Check if From header field contains the same username
 * as digest credentials
 */
int check_from(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Check if uri belongs to a local user, contributed by Juha Heinanen
 */
int does_uri_exist(struct sip_msg* _msg, char* _table, char* _s2);


int uridb_db_init(char* db_url);
int uridb_db_bind(char* db_url);
void uridb_db_close();
int uridb_db_ver(char* db_url, str* name);

#endif /* CHECKS_H */
