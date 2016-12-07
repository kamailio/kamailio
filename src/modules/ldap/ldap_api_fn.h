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


#ifndef LDAP_API_FN_H
#define LDAP_API_FN_H

#include <ldap.h>

#include "../../str.h"
#include "../../sr_module.h"

#define LDAP_MAX_FILTER_LEN 1024

/*
* LDAP API functions
*/
int ldap_params_search(
	int* _ld_result_count,
	char* _lds_name,
	char* _dn,
	int _scope,
	char** _attrs,
	char* _filter,
	...);

int ldap_url_search(
	char* _ldap_url,
	int* _ld_result_count);

int ldap_get_attr_vals(
	str *_attr_name,
	struct berval ***_vals);

int ldap_inc_result_pointer(void);

int ldap_str2scope(char* scope_str);

int get_ldap_handle(char* _lds_name, LDAP** _ldap_handle);

void get_last_ldap_result(LDAP** _last_ldap_handle, 
		LDAPMessage** _last_ldap_result);

#endif /* LDAP_API_FN_H */
