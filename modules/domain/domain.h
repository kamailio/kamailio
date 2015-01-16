/* 
 * Header file for domain table relates functions
 *
 * Copyright (C) 2002-2012 Juha Heinanen
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


/*
 * Check if domain given by parameter is local
 *
 * parameter can be one of:
 * - $ruri             - check domain from request uri
 * - $from             - check domain from From header
 * - avp name or alias - check the domain given by the value
 *                       pointed by the avp name/alias
 */
int w_is_domain_local(struct sip_msg* _msg, char* _s1, char* _s2);

int w_lookup_domain(struct sip_msg* _msg, char* _s1, char* _s2);
int w_lookup_domain_no_prefix(struct sip_msg* _msg, char* _s1, char* _s2);

int is_domain_local(str* domain);

int domain_check_self(str* host, unsigned short port, unsigned short proto);

int domain_db_bind(const str* db_url);
int domain_db_init(const str* db_url);
void domain_db_close(void);
int domain_db_ver(str* name, int version);

int reload_tables(void);

#endif /* DOMAIN_H */
