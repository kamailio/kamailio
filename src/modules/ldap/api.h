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



#ifndef LDAP_API_H
#define LDAP_API_H

#include <ldap.h>

#include "../../str.h"
#include "../../sr_module.h"

/*
* LDAP API function types
*/
typedef int (*ldap_params_search_t)(
	int* _ld_result_count,
	char* _lds_name,
	char* _dn,
	int _scope,
	char** _attrs,
	char* _filter,
	...); 

typedef int (*ldap_url_search_t)(
	char* _ldap_url,
	int* _result_count);

typedef int (*ldap_result_attr_vals_t)(
	str* _attr_name,
	struct berval ***_vals);

typedef void (*ldap_value_free_len_t)(struct berval **_vals);

typedef int (*ldap_result_next_t)(void);

typedef int (*ldap_str2scope_t)(char* scope_str);

typedef int (*get_ldap_handle_t)(char* _lds_name, LDAP** _ldap_handle);

typedef void (*get_last_ldap_result_t)(
	LDAP** _last_ldap_handle, 
	LDAPMessage** _last_ldap_result);

typedef int (*ldap_rfc4515_escape_t)(str *sin, str *sout, int url_encode);

/*
* LDAP module API
*/

typedef struct ldap_api {
	ldap_params_search_t    ldap_params_search;
	ldap_url_search_t       ldap_url_search;
	ldap_result_attr_vals_t ldap_result_attr_vals;
	ldap_value_free_len_t   ldap_value_free_len;
	ldap_result_next_t      ldap_result_next;
	ldap_str2scope_t        ldap_str2scope;
	ldap_rfc4515_escape_t   ldap_rfc4515_escape;
	get_ldap_handle_t       get_ldap_handle;
	get_last_ldap_result_t	get_last_ldap_result;
} ldap_api_t;


typedef int (*load_ldap_t)(ldap_api_t *api);

int load_ldap(ldap_api_t *api);

static inline int load_ldap_api(ldap_api_t *api)
{
	load_ldap_t load_ldap;

	if (!(load_ldap = (load_ldap_t) find_export("load_ldap", 0, 0)))
	{
		LM_ERR("can't import load_ldap\n");
		return -1;
	}

	if (load_ldap(api) == -1)
	{
		return -1;
	}

	return 0;
}

#endif /* LDAP_API_H */
