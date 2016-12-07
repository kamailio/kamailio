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


#include <string.h>
#include "ld_session.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"


static struct ld_session* ld_sessions = NULL;
static char ini_key_name[512];

int add_ld_session(char* _name, LDAP* _ldh, dictionary* _d)
{
	struct ld_session* current = ld_sessions;
	struct ld_session* new_lds = NULL;
	char *host_name, *bind_dn, *bind_pwd;
	int client_search_timeout_ms, client_bind_timeout_ms, network_timeout_ms;
	
	new_lds = (struct ld_session*)pkg_malloc(sizeof(struct ld_session));
	if (new_lds == NULL)
	{
		LM_ERR("no memory\n");
		return -1;
	}
	memset( new_lds, 0, sizeof(struct ld_session));

	/* name */
	strncpy(new_lds->name, _name, 255);
	/* handle */
	new_lds->handle = _ldh;
	
	/* host_name */
	host_name = iniparser_getstring(
		_d,
		get_ini_key_name(_name, CFG_N_LDAP_HOST),
		CFG_DEF_HOST_NAME);
	new_lds->host_name = (char*)pkg_malloc(strlen(host_name)+1);
	if (new_lds->host_name == NULL) {
		LM_ERR("no memory\n");
		return -1;
	}
	strcpy(new_lds->host_name, host_name);

	/* version */
	new_lds->version = iniparser_getint(
		_d,
		get_ini_key_name(_name, CFG_N_LDAP_VERSION),
		CFG_DEF_LDAP_VERSION);
	
	/* client_search_timeout */
	client_search_timeout_ms = iniparser_getint(
		_d,
		get_ini_key_name(_name, CFG_N_LDAP_CLIENT_SEARCH_TIMEOUT),
		CFG_DEF_LDAP_CLIENT_SEARCH_TIMEOUT);
	if (client_search_timeout_ms < CFG_LDAP_CLIENT_SEARCH_TIMEOUT_MIN)
	{
		LM_INFO("[%s = %d ms] is below allowed min"
			" [%d ms] - [%s] set to [%d ms]\n",
			CFG_N_LDAP_CLIENT_SEARCH_TIMEOUT,
			client_search_timeout_ms,
			CFG_LDAP_CLIENT_SEARCH_TIMEOUT_MIN,
			CFG_N_LDAP_CLIENT_SEARCH_TIMEOUT,
			CFG_LDAP_CLIENT_SEARCH_TIMEOUT_MIN);
		client_search_timeout_ms = CFG_LDAP_CLIENT_SEARCH_TIMEOUT_MIN;
	}
	new_lds->client_search_timeout.tv_sec = client_search_timeout_ms / 1000;
	new_lds->client_search_timeout.tv_usec =
									(client_search_timeout_ms % 1000) * 1000;

	/* client_bind_timeout */
	client_bind_timeout_ms = iniparser_getint(
		_d,
		get_ini_key_name(_name, CFG_N_LDAP_CLIENT_BIND_TIMEOUT),
		CFG_DEF_LDAP_CLIENT_BIND_TIMEOUT);
	new_lds->client_bind_timeout.tv_sec = client_bind_timeout_ms / 1000;
	new_lds->client_bind_timeout.tv_usec = 
									(client_bind_timeout_ms % 1000) * 1000;
	
	/* network_timeout */
	network_timeout_ms = iniparser_getint(
		_d,
		get_ini_key_name(_name, CFG_N_LDAP_NETWORK_TIMEOUT),
		LDAP_NO_LIMIT);
	new_lds->network_timeout.tv_sec = network_timeout_ms / 1000;
	new_lds->network_timeout.tv_usec = (network_timeout_ms % 1000) * 1000;

	/* bind_dn */
	bind_dn = iniparser_getstring(
		_d,
		get_ini_key_name(_name, CFG_N_LDAP_BIND_DN),
		CFG_DEF_LDAP_BIND_DN);
	new_lds->bind_dn = (char*)pkg_malloc(strlen(bind_dn)+1);
	if (new_lds->bind_dn == NULL) {
		LM_ERR("no memory\n");
		return -1;
	}
	strcpy(new_lds->bind_dn, bind_dn);

	/* bind_pwd */
	bind_pwd = iniparser_getstring(
		_d,
		get_ini_key_name(_name, CFG_N_LDAP_BIND_PWD),
		CFG_DEF_LDAP_BIND_PWD);
	new_lds->bind_pwd = (char*)pkg_malloc(strlen(bind_pwd)+1);
	if (new_lds->bind_pwd == NULL) {
		LM_ERR("no memory\n");
		return -1;
	}
	strcpy(new_lds->bind_pwd, bind_pwd);

	/* calculate_ha1 */
	new_lds->calculate_ha1 = iniparser_getboolean(
		_d, 
		get_ini_key_name(_name, CFG_N_CALCULATE_HA1), 
		CFG_DEF_CALCULATE_HA1);
	
	
	if (current == NULL)
	{
		ld_sessions = new_lds;
	} else
	{
		while (current->next != NULL) { current = current->next; };
		current->next = new_lds;
	}

	return 0;
}


struct ld_session* get_ld_session(char* _name)
{
	struct ld_session* current = ld_sessions;
	
	if (_name == NULL)
	{
		LM_ERR("lds_name == NULL\n");
		return NULL;
	}
	while (current != NULL)
	{
		if (strcmp(current->name, _name) == 0)
		{
			return current;
		}
		current = current->next;
	}

	return NULL;
}


int free_ld_sessions(void)
{
	struct ld_session* current = ld_sessions;
	struct ld_session* tmp;

	while (current != NULL)
	{
		tmp = current->next;

		if (current->handle != NULL)
		{
			ldap_unbind_ext(current->handle, NULL, NULL);
		}
		if (current->host_name != NULL)
		{
			pkg_free(current->host_name);
		}
		if (current->bind_dn != NULL)
		{
			pkg_free(current->bind_dn);
		}
		if (current->bind_pwd != NULL)
		{
			pkg_free(current->bind_pwd);
		}
		
		pkg_free(current);
		current = tmp;
	}
	
	ld_sessions = NULL;

	return 0;
}

char* get_ini_key_name(char* _section, char* _key)
{
	sprintf(ini_key_name, "%s:%s", _section, _key);
	return ini_key_name;
}

