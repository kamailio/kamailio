/* domain.h v 0.1 2002/12/27
 *
 * Header file for domain table relates functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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


#ifndef DOMAIN_H
#define DOMAIN_H
		

#include "../../parser/msg_parser.h"


/*
 * Check if host in From uri is local
 */
int is_from_local(struct sip_msg* _msg, char* _s1, char* _s2);


/*
 * Check if host in Request URI is local
 */
int is_uri_host_local(struct sip_msg* _msg, char* _s1, char* _s2);

int domain_db_bind(char* db_url);
int domain_db_init(char* db_url);
void domain_db_close();
int domain_db_ver(str* name);

int reload_domain_table();



#endif /* DOMAIN_H */
