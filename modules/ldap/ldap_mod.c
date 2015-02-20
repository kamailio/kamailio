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


#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "../../ut.h"
#include "../../parser/hf.h"
#include "../../sr_module.h"
#include "../../pvar.h"
#include "../../mem/mem.h"

#include "ld_session.h"
#include "ldap_exp_fn.h"
#include "api.h"
#include "ldap_connect.h"
#include "ldap_api_fn.h"
#include "iniparser.h"

MODULE_VERSION

/*
* Module management function prototypes
*/
static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

/*
* fixup functions
*/
static int ldap_search_fixup(void** param, int param_no);
static int ldap_result_fixup(void** param, int param_no);
static int ldap_filter_url_encode_fixup(void** param, int param_no);
static int ldap_result_check_fixup(void** param, int param_no);

/*
* exported functions
*/

static int w_ldap_search(struct sip_msg* msg, char* ldap_url, char* param);
static int w_ldap_result1(struct sip_msg* msg, char* src, char* param);
static int w_ldap_result2(struct sip_msg* msg, char* src, char* subst);
static int w_ldap_result_next(struct sip_msg* msg, char* foo, char *bar);
static int w_ldap_filter_url_encode(struct sip_msg* msg, 
		char* filter_component, char* dst_avp_name);
static int w_ldap_result_check_1(struct sip_msg* msg, 
		char* attr_name_check_str, char* param);
static int w_ldap_result_check_2(struct sip_msg* msg,
		char* attr_name_check_str, char* attr_val_re);


/* 
* Default module parameter values 
*/
#define DEF_LDAP_CONFIG "/usr/local/etc/kamailio/ldap.cfg"

/*
* Module parameter variables
*/
str ldap_config = str_init(DEF_LDAP_CONFIG);
static dictionary* config_vals = NULL;

/*
* Exported functions
*/
static cmd_export_t cmds[] = {
	{"ldap_search",            (cmd_function)w_ldap_search,            1, 
		ldap_search_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"ldap_result",            (cmd_function)w_ldap_result1,           1, 
		ldap_result_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"ldap_result",            (cmd_function)w_ldap_result2,           2, 
		ldap_result_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"ldap_result_next",       (cmd_function)w_ldap_result_next,       0, 
		0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"ldap_result_check",      (cmd_function)w_ldap_result_check_1,    1, 
		ldap_result_check_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"ldap_result_check",      (cmd_function)w_ldap_result_check_2,    2, 
		ldap_result_check_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"ldap_filter_url_encode", (cmd_function)w_ldap_filter_url_encode, 2, 
		ldap_filter_url_encode_fixup, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"load_ldap",              (cmd_function)load_ldap,  0,
		0, 0,
		0},
	{0, 0, 0, 0, 0, 0}
};


/*
* Exported parameters
*/
static param_export_t params[] = {

	{"config_file",          PARAM_STR, &ldap_config},
	{0, 0, 0}
};


/*
* Module interface
*/
struct module_exports exports = {
	"ldap", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	int i = 0, ld_count = 0;
	char* ld_name;
	
	/* don't do anything for non-worker processes */
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;

	/*
	* build ld_sessions and connect all sessions
	*/
	ld_count = iniparser_getnsec(config_vals);
	for (i = 0; i < ld_count; i++)
	{
		ld_name = iniparser_getsecname(config_vals, i);
		if (add_ld_session(ld_name,
					NULL,
					config_vals)
				!= 0)
		{
			LM_ERR("[%s]: add_ld_session failed\n", ld_name);
			return -1;
		}

		if (ldap_connect(ld_name) != 0)
		{
			LM_ERR("[%s]: failed to connect to LDAP host(s)\n", ld_name);
			ldap_disconnect(ld_name);
			return -1;
		}
		
	}
	
	return 0;
}


static int mod_init(void)
{
	int ld_count = 0, i = 0;
	char* section_name;
	char* ldap_version;
	
	/*
	* read config file
	*/
	if (ldap_config.len <= 0)
	{
		LM_ERR("config_file is empty - this module param is mandatory\n");
		return -2;
	}
	if ((config_vals = iniparser_new(ldap_config.s)) == NULL)
	{
		LM_ERR("failed to read config_file [%s]\n", ldap_config.s);
		return -2;
	}
	if ((ld_count = iniparser_getnsec(config_vals)) < 1)
	{
		LM_ERR("no section found in config_file [%s]\n", ldap_config.s);
		return -2;
	}
	/* check if mandatory settings are present */
	for (i = 0; i < ld_count; i++)
	{
		section_name = iniparser_getsecname(config_vals, i);
		if (strlen(section_name) > 255)
		{
			LM_ERR(	"config_file section name [%s]"
				" longer than allowed 255 characters",
				section_name);
			return -2;
		}
		if (!iniparser_find_entry(config_vals,
					get_ini_key_name(section_name, CFG_N_LDAP_HOST)))
		{
			LM_ERR(	"mandatory %s not defined in [%s]\n", 
				CFG_N_LDAP_HOST, 
				section_name);
			return -2;
		}
	}	
	
	/*
	* print ldap version string
	*/
	if (ldap_get_vendor_version(&ldap_version) != 0)
	{
		LM_ERR("ldap_get_vendor_version failed\n");
		return -2;
	}
	LM_INFO("%s\n", ldap_version);

	return 0;
}


static void destroy(void)
{
	/* ldap_unbind */
	free_ld_sessions();

	/* free config file memory */
	iniparser_free(config_vals);
}


/*
* EXPORTED functions
*/

static int w_ldap_search(struct sip_msg* msg, char* ldap_url, char* param)
{
	return ldap_search_impl(msg, (pv_elem_t*)ldap_url);
}

static int w_ldap_result1(struct sip_msg* msg, char* src, char* param)
{
	return ldap_write_result(msg, (struct ldap_result_params*)src, NULL);
}

static int w_ldap_result2(struct sip_msg* msg, char* src, char* subst)
{
	return ldap_write_result(msg, (struct ldap_result_params*)src,
			(struct subst_expr*)subst);
}

static int w_ldap_result_next(struct sip_msg* msg, char* foo, char *bar)
{
	return ldap_result_next();
}

static int w_ldap_filter_url_encode(struct sip_msg* msg,
		char* filter_component, char* dst_avp_name)
{
	return ldap_filter_url_encode(msg, (pv_elem_t*)filter_component,
			(pv_spec_t*)dst_avp_name);
}

static int w_ldap_result_check_1(struct sip_msg* msg,
		char* attr_name_check_str, char* param)
{
	return ldap_result_check(msg,
			(struct ldap_result_check_params*)attr_name_check_str, NULL);
}

static int w_ldap_result_check_2(struct sip_msg* msg,
		char* attr_name_check_str, char* attr_val_re)
{
	return ldap_result_check( msg, 
		(struct ldap_result_check_params*)attr_name_check_str, 
		(struct subst_expr*)attr_val_re);
}

/*
* FIXUP functions
*/

static int ldap_search_fixup(void** param, int param_no)
{
	pv_elem_t *model;
	str s;

	if (param_no == 1) {
		s.s = (char*)*param;
		s.len = strlen(s.s);
		if (s.len==0) {
			LM_ERR("ldap url is empty string!\n");
			return E_CFG;
		}
		if ( pv_parse_format(&s,&model) || model==NULL) {
			LM_ERR("wrong format [%s] for ldap url!\n", s.s);
			return E_CFG;
		}
		*param = (void*)model;
	}

	return 0;
}

static int ldap_result_fixup(void** param, int param_no)
{
	struct ldap_result_params* lp;
	struct subst_expr* se;
	str subst;
	char *arg_str, *dst_avp_str, *dst_avp_val_type_str;
	char *p;
	str s;
	int dst_avp_val_type = 0;
	
	if (param_no == 1) {
		arg_str = (char*)*param;
		if ((dst_avp_str = strchr(arg_str, '/')) == 0) 
		{
			/* no / found in arg_str */
			LM_ERR("invalid first argument [%s]\n", arg_str);
			return E_UNSPEC;
		}
		*(dst_avp_str++) = 0;

		if ((dst_avp_val_type_str = strchr(dst_avp_str, '/')))
		{
			*(dst_avp_val_type_str++) = 0;
			if (!strcmp(dst_avp_val_type_str, "int"))
			{
				dst_avp_val_type = 1;
			}
			else if (strcmp(dst_avp_val_type_str, "str"))
			{
				LM_ERR(	"invalid avp_type [%s]\n",
					dst_avp_val_type_str);
				return E_UNSPEC;
			}
		}

		lp = (struct ldap_result_params*)pkg_malloc(sizeof(struct ldap_result_params));
		if (lp == NULL) {
			LM_ERR("no memory\n");
			return E_OUT_OF_MEM;
		}
		memset(lp, 0, sizeof(struct ldap_result_params));
		
		lp->ldap_attr_name.s = arg_str;
		lp->ldap_attr_name.len = strlen(arg_str);

		lp->dst_avp_val_type = dst_avp_val_type;
		s.s = dst_avp_str; s.len = strlen(s.s);
		p = pv_parse_spec(&s, &lp->dst_avp_spec);
		if (p == 0) {
			pkg_free(lp);
			LM_ERR("parse error for [%s]\n",
					dst_avp_str);
			return E_UNSPEC;
		}
		if (lp->dst_avp_spec.type != PVT_AVP) {
			pkg_free(lp);
			LM_ERR(	"bad attribute name [%s]\n",
				dst_avp_str);
			return E_UNSPEC;
		}
		*param = (void*)lp;
		
	} else if (param_no == 2) {
		subst.s = *param;
		subst.len = strlen(*param);
		se = subst_parser(&subst);
		if (se == 0) {
			LM_ERR("bad subst re [%s]\n",
			(char*)*param);
			return E_BAD_RE;
		}
		*param = (void*)se;
	}

	return 0;
}

static int ldap_result_check_fixup(void** param, int param_no)
{
	struct ldap_result_check_params *lp;
	struct subst_expr *se;
	str subst;
	str s;
	char *arg_str, *check_str;
	int arg_str_len;
	
	if (param_no == 1)
	{
		arg_str = (char*)*param;
		arg_str_len = strlen(arg_str);
		if ((check_str = strchr(arg_str, '/')) == 0)
		{
			/* no / found in arg_str */
			LM_ERR(	"invalid first argument [%s] (no '/' found)\n",
				arg_str);
			return E_UNSPEC;
		}
		*(check_str++) = 0;
		
		lp = (struct ldap_result_check_params*)pkg_malloc(sizeof(struct ldap_result_check_params));
		if (lp == NULL) {
			LM_ERR("no memory\n");
			return E_OUT_OF_MEM;
		}
		memset(lp, 0, sizeof(struct ldap_result_check_params));

		lp->ldap_attr_name.s = arg_str;
		lp->ldap_attr_name.len = strlen(arg_str);

		if (lp->ldap_attr_name.len + 1 == arg_str_len)
		{
			/* empty check_str */
			lp->check_str_elem_p = 0;
		}
		else
		{
			s.s = check_str; s.len = strlen(s.s);
			if (pv_parse_format(&s, &(lp->check_str_elem_p)) < 0)
			{
				LM_ERR("pv_parse_format failed\n");
				return E_OUT_OF_MEM;
			}
		}	
		*param = (void*)lp;
	}
	else if (param_no == 2)
	{
		subst.s = *param;
		subst.len = strlen(*param);
		se = subst_parser(&subst);
		if (se == 0) {
			LM_ERR(	"bad subst re [%s]\n",
				(char*)*param);
			return E_BAD_RE;
		}
		*param = (void*)se;
	}

	return 0;	
}

static int ldap_filter_url_encode_fixup(void** param, int param_no)
{
	pv_elem_t *elem_p;
	pv_spec_t *spec_p;
	str s;

	if (param_no == 1) {
		s.s = (char*)*param;
		if (s.s==0 || s.s[0]==0) {
			elem_p = 0;
		} else {
			s.len = strlen(s.s);
			if (pv_parse_format(&s, &elem_p) < 0) {
				LM_ERR("pv_parse_format failed\n");
				return E_OUT_OF_MEM;
			}
		}
		*param = (void*)elem_p;
	}
	else if (param_no == 2)
	{
		spec_p = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if (spec_p == NULL) {
			LM_ERR("no memory\n");
			return E_OUT_OF_MEM;
		}
		s.s = (char*)*param; s.len = strlen(s.s);
		if (pv_parse_spec(&s, spec_p)
				== 0)
		{
			pkg_free(spec_p);
			LM_ERR("parse error for [%s]\n",
				(char*)*param);
			return E_UNSPEC;
		}
		if (spec_p->type != PVT_AVP) {
			pkg_free(spec_p);
			LM_ERR("bad attribute name"
				" [%s]\n", (char*)*param);
			return E_UNSPEC;
		}
		*param = (void*)spec_p;
	}

	return 0;
}
