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


#ifndef AUTHORIZE_H
#define AUTHORIZE_H


#include "../../parser/msg_parser.h"

int auth_db_init(char* db_url);
int auth_db_bind(char* db_url);
void auth_db_close();

/*
 * Authorize using Proxy-Authorization header field
 */
int proxy_authenticate(struct sip_msg* msg, char* realm, char* table);


/*
 * Authorize using WWW-Authorization header field
 */
int www_authenticate(struct sip_msg* msg, char* realm, char* table);

#endif /* AUTHORIZE_H */
