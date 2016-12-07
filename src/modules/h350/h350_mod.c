/* 
 * Kamailio H.350 Module
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

#include "../../ut.h"
#include "../../sr_module.h"
#include "h350_mod.h"
#include "h350_exp_fn.h"


MODULE_VERSION

/*
 * Module management function prototypes
 */
static int mod_init(void);
static int child_init(int rank);

/*
 * fixup functions
 */
static int one_str_pv_elem_fixup(void** param, int param_no);
static int h350_auth_lookup_fixup(void** param, int param_no);

/*
 * exported functions
 */

static int w_h350_sipuri_lookup(struct sip_msg* msg, char* sip_uri, char* s2);
static int w_h350_auth_lookup(struct sip_msg* msg, char* digest_username, char* avp_specs);
static int w_h350_call_preferences(struct sip_msg* msg, char* avp_name_prefix, char* s2);
static int w_h350_service_level(struct sip_msg* msg, char* avp_name_prefix, char* s2);

/*
 * Module parameter variables
 */
char* h350_ldap_session = H350_LDAP_SESSION;
char* h350_base_dn = H350_BASE_DN;
char* h350_search_scope = H350_SEARCH_SCOPE;
int h350_search_scope_int = -1;


/*
 * LDAP API
 */
ldap_api_t ldap_api;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"h350_sipuri_lookup",           (cmd_function)w_h350_sipuri_lookup,     1,
	 one_str_pv_elem_fixup, 0,
	 REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"h350_auth_lookup",             (cmd_function)w_h350_auth_lookup,       2,
	 h350_auth_lookup_fixup, 0,
	 REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"h350_result_call_preferences", (cmd_function)w_h350_call_preferences,  1,
	 one_str_pv_elem_fixup, 0,
	 REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"h350_result_service_level",    (cmd_function)w_h350_service_level,     1,
	 one_str_pv_elem_fixup, 0,
	 REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"ldap_session",     PARAM_STRING, &h350_ldap_session},
	{"base_dn",          PARAM_STRING, &h350_base_dn},
	{"search_scope",     PARAM_STRING, &h350_search_scope},
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"h350", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	child_init  /* child initialization function */
};

static int child_init(int rank)
{
	
	/* don't do anything for non-worker process */
        if (rank < 1) {
        	return 0;
	}

	h350_search_scope_int = ldap_api.ldap_str2scope(h350_search_scope);

        /*
         * initialize h350_exp_fn
         */
        if (h350_exp_fn_init() != 0)
        {
                 LM_ERR("h350_exp_fn_init failed\n");
                 return -1;
        }

	
	return 0;
}


static int mod_init(void)
{
	/*
	 * load the LDAP API
	 */
	if (load_ldap_api(&ldap_api) != 0)
	{
		LM_ERR("Unable to load LDAP API - this module requires ldap module\n");
		return -1;
	}

	return 0;

	/*
	 * check module parameters
	 */
	if (ldap_api.ldap_str2scope(h350_search_scope) == -1)
	{
		LM_ERR("Invalid search_scope [%s]\n", h350_search_scope);
		return -1;
	}
	
}


/*
 * EXPORTED functions
 */
static int w_h350_sipuri_lookup(struct sip_msg* msg, char* sip_uri, char* s2)
{
	return h350_sipuri_lookup(msg, (pv_elem_t*)sip_uri);
}

static int w_h350_auth_lookup(struct sip_msg* msg, char* digest_username, char* avp_specs)
{
	return h350_auth_lookup(
		msg, 
		(pv_elem_t*)digest_username, 
		(struct h350_auth_lookup_avp_params*)avp_specs);
}

static int w_h350_call_preferences(struct sip_msg* msg, char* avp_name_prefix, char* s2)
{
	return h350_call_preferences(msg, (pv_elem_t*)avp_name_prefix);
}

static int w_h350_service_level(struct sip_msg* msg, char* avp_name_prefix, char* s2)
{
	return h350_service_level(msg, (pv_elem_t*)avp_name_prefix);
}

/*
 * FIXUP functions
 */

static int one_str_pv_elem_fixup(void** param, int param_no)
{
	pv_elem_t *model;
	str s;

	if (param_no == 1) {
		s.s = (char*)*param;
		if (s.s==0 || s.s[0]==0) {
			model = 0;
		} else {
			s.len = strlen(s.s);
			if (pv_parse_format(&s,&model)<0) {
				LM_ERR("pv_parse_format failed\n");
				return E_OUT_OF_MEM;
			}
		}
		*param = (void*)model;
	}

	return 0;
}

static int h350_auth_lookup_fixup(void** param, int param_no)
{
	pv_elem_t *model;
    char *p, *username_avp_spec_str, *pwd_avp_spec_str;
	str s;
	struct h350_auth_lookup_avp_params *params;

    if (param_no == 1) 
	{
		s.s = (char*)*param;
		if (s.s==0 || s.s[0]==0) {
            model = 0;
		} else {
            if (pv_parse_format(&s,&model)<0) {
                LM_ERR("pv_parse_format failed\n");
                return E_OUT_OF_MEM;
            }
        }
        *param = (void*)model;
    } else if (param_no == 2) {
		/*
		 * parse *param into username_avp_spec_str and pwd_avp_spec_str
		 */
		
		username_avp_spec_str = (char*)*param;
		if ((pwd_avp_spec_str = strchr(username_avp_spec_str, '/')) == 0)
		{
			/* no '/' found in username_avp_spec_str */
			LM_ERR("invalid second argument [%s]\n", username_avp_spec_str);
			return E_UNSPEC;
		}
		*(pwd_avp_spec_str++) = 0;

		/*
		 * parse avp specs into pv_spec_t and store in params
		 */
		params = (struct h350_auth_lookup_avp_params*)pkg_malloc
				(sizeof(struct h350_auth_lookup_avp_params));
		if (params == NULL)
		{
			LM_ERR("no memory\n");
			return E_OUT_OF_MEM;
		}
		memset(params, 0, sizeof(struct h350_auth_lookup_avp_params));
		s.s = username_avp_spec_str; s.len = strlen(s.s);
		p = pv_parse_spec(&s, &params->username_avp_spec);
		if (p == 0)
		{
			pkg_free(params);
			LM_ERR("parse error for [%s]\n", username_avp_spec_str);
			return E_UNSPEC;
		}
		if (params->username_avp_spec.type != PVT_AVP)
		{
			pkg_free(params);
			LM_ERR("invalid AVP specification [%s]\n", username_avp_spec_str);
			return E_UNSPEC;
		}
		s.s = pwd_avp_spec_str; s.len =  strlen(s.s);
		p = pv_parse_spec(&s, &params->password_avp_spec);
                if (p == 0)
                {
                        pkg_free(params);
                        LM_ERR("parse error for [%s]\n", pwd_avp_spec_str);
                        return E_UNSPEC;
                }
                if (params->password_avp_spec.type != PVT_AVP)
                {
                        pkg_free(params);
                        LM_ERR("invalid AVP specification [%s]\n", pwd_avp_spec_str);
                        return E_UNSPEC;
                }

		*param = (void*)params;
	}

        return 0;
}
