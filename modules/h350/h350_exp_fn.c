/*
 * Kamailio H.350 Module
 *
 * Copyright (C) 2007 University of North Carolina
 *
 * Original author: Christian Schlatter, cs@unc.edu
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

#include <regex.h>
#include "ldap.h"

#include "h350_mod.h"
#include "h350_exp_fn.h"

#include "../../pvar.h"
#include "../../ut.h"
#include "../../mem/mem.h"

#define H350_SIPURI_LOOKUP_LDAP_FILTER "(&(objectClass=SIPIdentity)(SIPIdentitySIPURI=%s))"
#define H350_AUTH_FILTER_PATTERN "(&(objectClass=SIPIdentity)(SIPIdentityUserName=%s))"

static str h350_call_pref_name = str_init("callPreferenceURI");
static str h350_sip_pwd_name = str_init("SIPIdentityPassword");
static str h350_service_level_name = str_init("SIPIdentityServiceLevel");

#define H350_CALL_PREF_REGEX "^([^ ]+) +([a-zA-Z]+)(:([0-9]+))?$"

#define SIP_URI_ESCAPED_MAX_LEN 1024
#define AVP_NAME_STR_BUF_LEN 1024
#define DIGEST_USERNAME_BUF_SIZE 2048

static regex_t* call_pref_preg;

int h350_sipuri_lookup(struct sip_msg* _msg, pv_elem_t* _sip_uri)
{
	str sip_uri, sip_uri_escaped;
	int ld_result_count;
	static char sip_uri_escaped_buf[SIP_URI_ESCAPED_MAX_LEN];

	/*
	 * get sip_uri
	 */
	if (pv_printf_s(_msg, _sip_uri, &sip_uri) != 0)
	{
		LM_ERR("pv_printf_s failed\n");
		return E_H350_INTERNAL;
	}

	/*
	 * ldap filter escape sip_uri
	 */
	sip_uri_escaped.s = sip_uri_escaped_buf;
	sip_uri_escaped.len = SIP_URI_ESCAPED_MAX_LEN - 1;
	if (ldap_api.ldap_rfc4515_escape(&sip_uri, &sip_uri_escaped, 0))
	{
		LM_ERR("ldap_rfc4515_escape failed\n");
		return E_H350_INTERNAL;
	}
	
	/*
	 * do ldap search
	 */
	if (ldap_api.ldap_params_search(&ld_result_count,
					h350_ldap_session,
					h350_base_dn,
					h350_search_scope_int,
					NULL,
					H350_SIPURI_LOOKUP_LDAP_FILTER,
					sip_uri_escaped.s)
	    != 0)
	{
		LM_ERR("ldap search failed\n");
		return E_H350_INTERNAL;
	}
	
	if (ld_result_count < 1)
	{
		return E_H350_NO_SUCCESS;
	}

	return ld_result_count;
}

int h350_auth_lookup(
        struct sip_msg* _msg,
        pv_elem_t* _digest_username,
        struct h350_auth_lookup_avp_params* _avp_specs)
{
	str                digest_username,
	                   digest_username_escaped,
	                   digest_password;
	static char        digest_username_buf[DIGEST_USERNAME_BUF_SIZE],
	                   username_avp_name_buf[AVP_NAME_STR_BUF_LEN],
			   password_avp_name_buf[AVP_NAME_STR_BUF_LEN];
	struct berval      **attr_vals = NULL;
	int_str            username_avp_name, password_avp_name, avp_val;
	unsigned short     username_avp_type, password_avp_type;
	int                rc, ld_result_count;

	/*
	 * get digest_username str
	 */
	if (_digest_username) 
	{
                if (pv_printf_s(_msg, _digest_username, &digest_username) != 0) 
		{
                        LM_ERR("pv_printf_s failed\n");
                        return E_H350_INTERNAL;
                }
        } else
	{
		LM_ERR("empty digest username\n");
		return E_H350_NO_SUCCESS;
	}

	/*
	 * get AVP names for username and password
	 */

	if (pv_get_avp_name(	_msg,
				&(_avp_specs->username_avp_spec.pvp),
				&username_avp_name,
				&username_avp_type)
		!= 0)
	{
		LM_ERR("error getting AVP name - pv_get_avp_name failed\n");
		return E_H350_INTERNAL;
	}
	if (username_avp_type & AVP_NAME_STR)
	{
		if (username_avp_name.s.len >= AVP_NAME_STR_BUF_LEN)
		{
			LM_ERR("username AVP name too long\n");
			return E_H350_INTERNAL;
		}
		strncpy(username_avp_name_buf, username_avp_name.s.s, username_avp_name.s.len);
                username_avp_name_buf[username_avp_name.s.len] = '\0';
                username_avp_name.s.s = username_avp_name_buf;
	}

	if (pv_get_avp_name(_msg,
						&(_avp_specs->password_avp_spec.pvp),
						&password_avp_name,
						&password_avp_type)
                != 0)
        {
                LM_ERR("error getting AVP name - pv_get_avp_name failed\n");
                return E_H350_INTERNAL;
        }
        if (password_avp_type & AVP_NAME_STR)
        {
                if (password_avp_name.s.len >= AVP_NAME_STR_BUF_LEN)
                {
                        LM_ERR("password AVP name too long\n");
                        return E_H350_INTERNAL;
                }
                strncpy(password_avp_name_buf, 
						password_avp_name.s.s, 
						password_avp_name.s.len);
                password_avp_name_buf[password_avp_name.s.len] = '\0';
                password_avp_name.s.s = password_avp_name_buf;
        }
	

	/* 
	 * search for sip digest username in H.350, store digest password
	 */
	
	/* ldap filter escape digest username */
	digest_username_escaped.s = digest_username_buf;
	digest_username_escaped.len = DIGEST_USERNAME_BUF_SIZE - 1;
	if (ldap_api.ldap_rfc4515_escape(
		&digest_username, 
		&digest_username_escaped, 
		0)
	   )
        {
                LM_ERR("ldap_rfc4515_escape() failed\n");
                return E_H350_INTERNAL;
        }

	/* do ldap search */
	if (ldap_api.ldap_params_search(&ld_result_count,
                                        h350_ldap_session,
                                        h350_base_dn,
                                        h350_search_scope_int,
                                        NULL,
                                        H350_AUTH_FILTER_PATTERN,
                                        digest_username_escaped.s)
            != 0)
        {
                LM_ERR("LDAP search failed\n");
		return E_H350_INTERNAL;
        }

	if (ld_result_count < 1)
	{
		LM_INFO("no H.350 entry found for username [%s]\n",
			digest_username_escaped.s);
		return E_H350_NO_SUCCESS;
	}
	if (ld_result_count > 1)
	{
		LM_WARN("more than one [%d] H.350 entry found for username [%s]\n",
			ld_result_count,
			digest_username_escaped.s);
	}

	/* get ldap result values */
	rc = ldap_api.ldap_result_attr_vals(&h350_sip_pwd_name, &attr_vals);
	if (rc < 0) 
	{
                LM_ERR("getting LDAP attribute values failed\n");
                ldap_api.ldap_value_free_len(attr_vals);
		return E_H350_INTERNAL;
        }
        if ((rc > 0) || (attr_vals == NULL)) 
	{
                LM_INFO("no values found in LDAP entry for username [%s]\n",
			digest_username_escaped.s);
		ldap_api.ldap_value_free_len(attr_vals);
		return E_H350_INTERNAL;
        }

	digest_password.s = attr_vals[0]->bv_val;
        digest_password.len = attr_vals[0]->bv_len;

	/*
	 * write AVPs
	 */
	
	avp_val.s = digest_username;
	if (add_avp(	username_avp_type | AVP_VAL_STR, 
			username_avp_name, 
			avp_val) 
		< 0)
	{
		LM_ERR("failed to create new AVP\n");
		ldap_api.ldap_value_free_len(attr_vals);
		return E_H350_INTERNAL;
	}

	avp_val.s = digest_password;
	if (add_avp(    password_avp_type | AVP_VAL_STR,
                        password_avp_name,
                        avp_val)
                < 0)
        {
                LM_ERR("failed to create new AVP\n");
                ldap_api.ldap_value_free_len(attr_vals);
                return E_H350_INTERNAL;
        }

	ldap_api.ldap_value_free_len(attr_vals);
	return E_H350_SUCCESS;
}

int h350_call_preferences(struct sip_msg* _msg, pv_elem_t* _avp_name_prefix)
{
	int           rc, i, avp_count = 0;
	struct berval **attr_vals;
	size_t        nmatch = 5;
	regmatch_t    pmatch[5];
	int_str       avp_name, avp_val;
	str           avp_val_str, avp_name_str, 
	              avp_name_prefix_str, call_pref_timeout_str;
	int           call_pref_timeout;
	static char   call_pref_avp_name[AVP_NAME_STR_BUF_LEN];

        /*
         * get avp_name_prefix_str
         */
        if (pv_printf_s(_msg, _avp_name_prefix, &avp_name_prefix_str) != 0)
        {
                LM_ERR("pv_printf_s failed\n");
                return E_H350_INTERNAL;
        }

	
	/*
	 * get LDAP attribute values
	 */
	if ((rc = ldap_api.ldap_result_attr_vals(
			&h350_call_pref_name, &attr_vals)) < 0)
	{
		LM_ERR("Getting LDAP attribute values failed\n");
		return E_H350_INTERNAL;
	}

	if (rc > 0)
	{
		/* no LDAP values found */
		return E_H350_NO_SUCCESS;
	}

	/*
	 * loop through call pref values and add AVP(s)
	 */

	/* copy avp name prefix into call_pref_avp_name */
	if (avp_name_prefix_str.len < AVP_NAME_STR_BUF_LEN)
	{
		memcpy(call_pref_avp_name, avp_name_prefix_str.s, avp_name_prefix_str.len);
	} else
	{
		LM_ERR("AVP name prefix too long [%d] (max [%d])", 
			avp_name_prefix_str.len, 
			AVP_NAME_STR_BUF_LEN);
		return E_H350_INTERNAL;
	}

	for (i = 0; attr_vals[i] != NULL; i++)
	{
		if ((rc = regexec(call_pref_preg, attr_vals[i]->bv_val, nmatch, pmatch, 0)) != 0)
		{
			switch (rc)
			{
			case REG_NOMATCH:
				LM_INFO("no h350 call preference regex match for [%s]\n", 
						attr_vals[i]->bv_val);
				continue;
			case REG_ESPACE:
				LM_ERR("regexec returned REG_ESPACE - out of memory\n");
			default:
				LM_ERR("regexec failed\n");
				ldap_api.ldap_value_free_len(attr_vals);
				return E_H350_INTERNAL;
			}
		}

		/* calculate call preference sip uri */
		if (avp_name_prefix_str.len + pmatch[2].rm_eo - pmatch[2].rm_so 
			>= AVP_NAME_STR_BUF_LEN)
		{
			LM_ERR("AVP name too long for [%s]", attr_vals[i]->bv_val);
			continue;
		}
		avp_val_str.s = attr_vals[i]->bv_val + pmatch[1].rm_so;
		avp_val_str.len = pmatch[1].rm_eo - pmatch[1].rm_so;

		avp_val.s = avp_val_str;
		
		/* calculate call preference avp name */
		memcpy(	call_pref_avp_name + avp_name_prefix_str.len, 
			attr_vals[i]->bv_val + pmatch[2].rm_so,
			pmatch[2].rm_eo - pmatch[2].rm_so);

		avp_name_str.s = call_pref_avp_name;
		avp_name_str.len = avp_name_prefix_str.len + pmatch[2].rm_eo - pmatch[2].rm_so;

		avp_name.s = avp_name_str;
		
		/* add avp */
		if (add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_name, avp_val) < 0)
		{
			LM_ERR("failed to create new AVP\n");
			ldap_api.ldap_value_free_len(attr_vals);
			return E_H350_INTERNAL;
		}

		avp_count++;
		
		/* check for call preference timeout */
		if ((pmatch[4].rm_eo - pmatch[4].rm_so) == 0)
		{
			continue;
		}
		
		/* calculate call preference timeout avp name */
		memcpy(	avp_name_str.s + avp_name_str.len, "_t", 2);
		avp_name_str.len += 2;
		avp_name.s = avp_name_str;

		/* calculate timeout avp value */
		call_pref_timeout_str.s = attr_vals[i]->bv_val + pmatch[4].rm_so;
		call_pref_timeout_str.len = pmatch[4].rm_eo - pmatch[4].rm_so;
		if (str2sint(&call_pref_timeout_str, &call_pref_timeout) != 0)
		{
			LM_ERR("str2sint failed\n");
			ldap_api.ldap_value_free_len(attr_vals);
			return E_H350_INTERNAL;
		}
		call_pref_timeout = call_pref_timeout / 1000;

		/* add timeout avp */
		avp_val.n = call_pref_timeout;
		if (add_avp(AVP_NAME_STR, avp_name, avp_val) < 0)
		{
		        LM_ERR("failed to create new AVP\n");
			ldap_api.ldap_value_free_len(attr_vals);
                        return E_H350_INTERNAL;
		}		
	}

	ldap_api.ldap_value_free_len(attr_vals);
	if (avp_count > 0)
	{
		return avp_count;
	} else 
	{
		return E_H350_NO_SUCCESS;
	}
}

int h350_service_level(struct sip_msg* _msg, pv_elem_t* _avp_name_prefix)
{
	int           i, rc, avp_count = 0;
        str           avp_name_prefix;
	int_str       avp_name, avp_val;
	struct berval **attr_vals;
	static char   service_level_avp_name[AVP_NAME_STR_BUF_LEN];

        /*
         * get service_level
         */
        if (pv_printf_s(_msg, _avp_name_prefix, &avp_name_prefix) != 0)
        {
                LM_ERR("pv_printf_s failed\n");
                return E_H350_INTERNAL;
        }

        /*
         * get LDAP attribute values
         */
        if ((rc = ldap_api.ldap_result_attr_vals(&h350_service_level_name, &attr_vals)) < 0)
        {
                LM_ERR("Getting LDAP attribute values failed\n");
                return E_H350_INTERNAL;
        }
        if (rc > 0)
        {
                /* no LDAP values found */
                return E_H350_NO_SUCCESS;
        }

        /* copy avp name prefix into service_level_avp_name */
        if (avp_name_prefix.len < AVP_NAME_STR_BUF_LEN)
        {
                memcpy(service_level_avp_name, avp_name_prefix.s, avp_name_prefix.len);
        } else
        {
                LM_ERR("AVP name prefix too long [%d] (max [%d])\n",
                        avp_name_prefix.len,
                        AVP_NAME_STR_BUF_LEN);
		ldap_api.ldap_value_free_len(attr_vals);
                return E_H350_INTERNAL;
        }	
	

	/*
	 * loop through service level values and add AVP(s)
	 */

	for (i = 0; attr_vals[i] != NULL; i++)
	{
		/* get avp name */
		if (avp_name_prefix.len + attr_vals[i]->bv_len >= AVP_NAME_STR_BUF_LEN)
		{
			LM_ERR("AVP name too long for [%s]\n", attr_vals[i]->bv_val);
			continue;
		}
		memcpy(	service_level_avp_name + avp_name_prefix.len, 
			attr_vals[i]->bv_val,
			attr_vals[i]->bv_len);
		avp_name.s.s = service_level_avp_name;
		avp_name.s.len = avp_name_prefix.len + attr_vals[i]->bv_len;
		
		/* avp value = 1 */
		avp_val.n = 1;

                if (add_avp(AVP_NAME_STR, avp_name, avp_val) < 0)
                {
                        LM_ERR("failed to create new AVP\n");
                        ldap_api.ldap_value_free_len(attr_vals);
                        return E_H350_INTERNAL;
                }
		avp_count++;
	}

	ldap_api.ldap_value_free_len(attr_vals);
	if (avp_count > 0)
        {
                return avp_count;
        } else
        {
                return E_H350_NO_SUCCESS;
        }
}

int h350_exp_fn_init(void)
{
	int rc;

	if ((call_pref_preg = pkg_malloc(sizeof(regex_t))) == 0)
	{
		LM_ERR("allocating memory for regex failed\n");
		return -1;
	}

	if ((rc = regcomp(call_pref_preg, H350_CALL_PREF_REGEX, REG_EXTENDED)) != 0)
	{
		pkg_free(call_pref_preg);
		LM_ERR("regcomp failed - returned [%d]\n", rc);
		return -1;
	}
	return 0;
}
