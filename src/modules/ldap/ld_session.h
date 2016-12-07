/*
* Kamailio LDAP Module
*
* Copyright (C) 2007 University of North Carolina
*
* Original author: Christian Schlatter, cs@unc.edu
*
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
*
*/


#ifndef LD_SESSION_H
#define LD_SESSION_H

#include <ldap.h>

#include "iniparser.h"


struct ld_session {
	char                    name[256];
	LDAP*                   handle;
	char*                   host_name;
	int                     version;
	int                     server_search_timeout;
	struct timeval          client_search_timeout;
	struct timeval		client_bind_timeout;
	struct timeval          network_timeout;
	char*                   bind_dn;
	char*                   bind_pwd;
	int                     calculate_ha1;
	struct ld_session*      next;
};

#define CFG_N_LDAP_HOST "ldap_server_url"
#define CFG_N_LDAP_VERSION "ldap_version"
#define CFG_N_LDAP_SERVER_SEARCH_TIMEOUT "ldap_server_search_timeout"
#define CFG_N_LDAP_CLIENT_SEARCH_TIMEOUT "ldap_client_search_timeout"
#define CFG_N_LDAP_CLIENT_BIND_TIMEOUT "ldap_client_bind_timeout"
#define CFG_N_LDAP_NETWORK_TIMEOUT "ldap_network_timeout"
#define CFG_N_LDAP_BIND_DN "ldap_bind_dn"
#define CFG_N_LDAP_BIND_PWD "ldap_bind_password"
#define CFG_N_CALCULATE_HA1 "calculate_ha1"


#define CFG_DEF_HOST_NAME ""
#define CFG_DEF_LDAP_SERVER_URL NULL
#define CFG_DEF_LDAP_VERSION 3
#define CFG_DEF_LDAP_CLIENT_BIND_TIMEOUT 1000
#define CFG_DEF_LDAP_CLIENT_SEARCH_TIMEOUT 5000
#define CFG_DEF_LDAP_NETWORK_TIMEOUT 1000
#define CFG_DEF_LDAP_BIND_DN ""
#define CFG_DEF_LDAP_BIND_PWD ""
#define CFG_DEF_CALCULATE_HA1 1

#define CFG_LDAP_CLIENT_SEARCH_TIMEOUT_MIN 2000

extern int add_ld_session(char* _name, LDAP* _ldh, dictionary* _d);
extern struct ld_session* get_ld_session(char* _name);
extern int free_ld_sessions(void);

extern char* get_ini_key_name(char* _section, char* _key);

#endif /* LD_SESSION_H */

