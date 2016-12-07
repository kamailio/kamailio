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


#include <unistd.h>
#include <stdarg.h>

#include "../../ut.h"

#include <ldap.h>

#include "ldap_api_fn.h"
#include "api.h"
#include "ldap_connect.h"
#include "ldap_escape.h"
#include "ld_session.h"

static LDAP* last_ldap_handle = NULL;
static LDAPMessage* last_ldap_result = NULL;

int get_connected_ldap_session(
	char* _lds_name, 
	struct ld_session** _lds);
	int lds_search(char* _lds_name,
	char* _dn,
	int _scope,
	char* _filter,
	char** _attrs,
	struct timeval* _search_timeout,
	int* _ld_result_count,
	int* _ld_error);


int load_ldap(ldap_api_t *api)
{
	if (api == NULL)
	{
		return -1;
	}

	api->ldap_params_search    = ldap_params_search;
	api->ldap_url_search       = ldap_url_search;
	api->ldap_result_attr_vals = ldap_get_attr_vals;
	api->ldap_value_free_len   = ldap_value_free_len;
	api->ldap_result_next      = ldap_inc_result_pointer;
	api->ldap_str2scope        = ldap_str2scope;
	api->ldap_rfc4515_escape   = ldap_rfc4515_escape;
	api->get_ldap_handle       = get_ldap_handle;
	api->get_last_ldap_result  = get_last_ldap_result;

	return 1;
}

int get_ldap_handle(char* _lds_name, LDAP** _ldap_handle)
{
	int rc;
	struct ld_session* lds;

	rc = get_connected_ldap_session(_lds_name, &lds);
	if (rc == 0)
	{
		*_ldap_handle = lds->handle;
	}
	return rc;
}

int get_connected_ldap_session(char* _lds_name, struct ld_session** _lds)
{	
	/*
	* get ld session
	*/
	if ((*_lds = get_ld_session(_lds_name)) == NULL)
	{
		LM_ERR("[%s]: ldap_session not found\n", _lds_name);
		return -1;
	}

	/* try to reconnect if ldap session handle is NULL */
	if ((*_lds)->handle == NULL)
	{
		if (ldap_reconnect(_lds_name) == 0)
		{
			if ((*_lds = get_ld_session(_lds_name)) == NULL)
			{
				LM_ERR("[%s]: ldap_session not found\n", _lds_name);
				return -1;
			}
		}
		else
		{
			if (last_ldap_result != NULL)
			{
				ldap_msgfree(last_ldap_result);
				last_ldap_result = NULL;
			}
			ldap_disconnect(_lds_name);
			LM_ERR("[%s]: reconnect failed\n", _lds_name);
			return -1;
		}
	}

	/* free old last_ldap_result */
	/*
	 * this is done now in lds_search
	 *

	if (last_ldap_result != NULL) {
		ldap_msgfree(last_ldap_result);
		last_ldap_result = NULL;
	}
	*/
	
	return 0;
}

void get_last_ldap_result(LDAP** _last_ldap_handle, LDAPMessage** _last_ldap_result)
{
	*_last_ldap_handle = last_ldap_handle;
	*_last_ldap_result = last_ldap_result;
}

int ldap_params_search(
	int* _ld_result_count,
	char* _lds_name,
	char* _dn,
	int _scope,
	char** _attrs,
	char* _filter,
	...)
{
	int rc;
	static char filter_str[LDAP_MAX_FILTER_LEN];
	char *filter_ptr = NULL;
	va_list filter_vars;

	/*
	* check _scope
	*/
	switch (_scope)
	{
	case LDAP_SCOPE_ONELEVEL:
	case LDAP_SCOPE_BASE:
	case LDAP_SCOPE_SUBTREE:
		break;
	default:
		LM_ERR("[%s]: invalid scope argument [%d]\n", _lds_name, _scope);
		return -1;
	}

	if (_filter) {
		/*
		* vsnprintf
		*/
		va_start(filter_vars, _filter);
		rc = vsnprintf(filter_str, (size_t)LDAP_MAX_FILTER_LEN, _filter,
				filter_vars);
		va_end(filter_vars);

		if (rc >= LDAP_MAX_FILTER_LEN)
		{
			LM_ERR(	"[%s]: filter string too long (len [%d], max len [%d])\n",
				_lds_name,
				rc,
				LDAP_MAX_FILTER_LEN);
			return -1;
		}
		else if (rc < 0)
		{
			LM_ERR("vsnprintf failed\n");
			return -1;
		}
		filter_ptr = filter_str;
	}

	/*
	* ldap search
	*/
	if (lds_search(_lds_name,
			_dn,
			_scope,
			filter_ptr,
			_attrs,
			NULL,
			_ld_result_count,
			&rc)
		!= 0)
	{
		/* try again if LDAP API ERROR */
		if (LDAP_API_ERROR(rc) && 
				(lds_search(_lds_name,
						_dn,
						_scope,
						filter_str,
						_attrs,
						NULL,
						_ld_result_count,
						&rc) != 0))
		{
			LM_ERR(	"[%s]: LDAP search (dn [%s], scope [%d],"
				" filter [%s]) failed: %s\n",
				_lds_name,
				_dn,
				_scope,
				filter_str,
				ldap_err2string(rc));
			return -1;
		}
	}
	
	LM_DBG(	"[%s]: [%d] LDAP entries found\n", 
		_lds_name,
		*_ld_result_count);
	
	return 0;
}


int ldap_url_search(
	char* _ldap_url,
	int* _ld_result_count)
{
	LDAPURLDesc *ludp;
	int rc;

	if (ldap_url_parse(_ldap_url, &ludp) != 0) {
		LM_ERR("invalid LDAP URL [%s]\n", ZSW(_ldap_url));
		if (ludp != NULL) {
			ldap_free_urldesc(ludp);
		}
		return -2;
	}
	if (ludp->lud_host == NULL)
	{
		LM_ERR(	"no ldap session name found in ldap URL [%s]\n",
			ZSW(_ldap_url));
		return -2;
	}


	LM_DBG(	"LDAP URL parsed into session_name"
		" [%s], base [%s], scope [%d], filter [%s]\n",
		ZSW(ludp->lud_host),
		ZSW(ludp->lud_dn),
		ludp->lud_scope,
		ZSW(ludp->lud_filter));

	rc = ldap_params_search(_ld_result_count,
		ludp->lud_host,
		ludp->lud_dn,
		ludp->lud_scope,
		ludp->lud_attrs,
		ludp->lud_filter);
	ldap_free_urldesc(ludp);
	return rc;
}


int ldap_inc_result_pointer(void)
{
	LDAPMessage *next_result = NULL;

	/*
	* check for last_ldap_result
	*/
	if (last_ldap_result == NULL) {
		LM_ERR("last_ldap_result == NULL\n");
		return -1;
	}
	if (last_ldap_handle == NULL)
	{
		LM_ERR("last_ldap_handle == NULL\n");
		return -1;
	}

	/*
	* get next LDAP result pointer
	*/
	if ((next_result = ldap_next_entry(last_ldap_handle, last_ldap_result))
			== NULL)
	{
		/* no more LDAP entries */
		return 1;
	}
	last_ldap_result = next_result;
	return 0;
}


int ldap_get_attr_vals(str *_attr_name, struct berval ***_vals)
{
	BerElement *ber;
	char *a;

	/*
	* check for last_ldap_result
	*/
	if (last_ldap_result == NULL) {
		LM_ERR("last_ldap_result == NULL\n");
		return -1;
	}
	if (last_ldap_handle == NULL)
	{
		LM_ERR("last_ldap_handle == NULL\n");
		return -1;
	}

	/*
	* search for attribute named _attr_name
	*/
	*_vals = NULL;
	for (a = ldap_first_attribute(last_ldap_handle,
			last_ldap_result,
			&ber);
		a != NULL;
		a = ldap_next_attribute(last_ldap_handle,
			last_ldap_result,
			ber))
	{
		if (strncmp(a, _attr_name->s, _attr_name->len) == 0) {
			*_vals = ldap_get_values_len(
				last_ldap_handle,
				last_ldap_result,
				a);
			ldap_memfree(a);
			break;
		}
		ldap_memfree(a);
	}

	if (ber != NULL) {
		ber_free(ber, 0);
	}

	if (*_vals != NULL)
	{
		return 0;
	} else {
		return 1;
	}
}

int ldap_str2scope(char* scope_str)
{
	if ( strcasecmp( scope_str, "one" ) == 0 ) {
		return LDAP_SCOPE_ONELEVEL;

	} else if ( strcasecmp( scope_str, "onelevel" ) == 0 ) {
		return LDAP_SCOPE_ONELEVEL;

	} else if ( strcasecmp( scope_str, "base" ) == 0 ) {
		return LDAP_SCOPE_BASE;

	} else if ( strcasecmp( scope_str, "sub" ) == 0 ) {
		return LDAP_SCOPE_SUBTREE;

	} else if ( strcasecmp( scope_str, "subtree" ) == 0 ) {
		return LDAP_SCOPE_SUBTREE;
	};

	return( -1 );
}
/*
 * sets last_ldap_result and last_ldap_handle
 */
int lds_search(
	char* _lds_name,
	char* _dn,
	int _scope,
	char* _filter,
	char** _attrs,
	struct timeval* _search_timeout,
	int* _ld_result_count,
	int* _ld_error)
{
	struct ld_session* lds;
#ifdef LDAP_PERF
	struct timeval before_search = { 0, 0 }, after_search = { 0, 0 };
#endif

	/*
	 * get ld_handle
	 */
	if (get_connected_ldap_session(_lds_name, &lds) != 0)
	{
		LM_ERR("[%s]: couldn't get ldap session\n", _lds_name);
		return -1;
	}

	/*
	 * free last_ldap_result
	 */
        if (last_ldap_result != NULL) {
                ldap_msgfree(last_ldap_result);
                last_ldap_result = NULL;
        }

	
	LM_DBG(	"[%s]: performing LDAP search: dn [%s],"
		" scope [%d], filter [%s], client_timeout [%d] usecs\n",
		_lds_name,
		_dn,
		_scope,
		_filter,
		(int)(lds->client_search_timeout.tv_sec * 1000000 
			+ lds->client_search_timeout.tv_usec));
	
#ifdef LDAP_PERF
	gettimeofday(&before_search, NULL);
#endif

	/*
	 * perform ldap search
	 */
	*_ld_error = ldap_search_ext_s(
		lds->handle,
		_dn,
		_scope,
		_filter,
		_attrs,
		0,
		NULL,
		NULL,
		&lds->client_search_timeout,
		0,
		&last_ldap_result);

#ifdef LDAP_PERF
	gettimeofday(&after_search, NULL);

	LM_INFO("[%s]: LDAP search took [%d] usecs\n",
		_lds_name,
		(int)((after_search.tv_sec * 1000000 + after_search.tv_usec)
		- (before_search.tv_sec * 1000000 + before_search.tv_usec)));
#endif

	if (*_ld_error != LDAP_SUCCESS)
	{
		if (last_ldap_result != NULL)
		{
			ldap_msgfree(last_ldap_result);
			last_ldap_result = NULL;
		}

		if (LDAP_API_ERROR(*_ld_error))
		{
			ldap_disconnect(_lds_name);
		}
		
		LM_DBG( "[%s]: ldap_search_ext_st failed: %s\n",
			_lds_name,
			ldap_err2string(*_ld_error));
		return -1;
	}

	last_ldap_handle = lds->handle;
	*_ld_result_count = ldap_count_entries(lds->handle, last_ldap_result);
	if (*_ld_result_count < 0)
	{
		LM_DBG("[%s]: ldap_count_entries failed\n", _lds_name);
		return -1;
	}

	return 0;
}

