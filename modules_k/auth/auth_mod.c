/* 
 * $Id$ 
 *
 * Digest Authentication Module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-02-26 checks and group moved to separate modules (janakj)
 * 2003-03-10 New module interface (janakj)
 * 2003-03-16 flags export parameter added (janakj)
 * 2003-03-19 all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 * 2003-04-28 rpid contributed by Juha Heinanen added (janakj) 
 * 2005-05-31 general avp specification added for rpid (bogdan)
 * 2006-03-01 pseudo variables support for domain name (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../error.h"
#include "../../pvar.h"
#include "../../ut.h"
#include "../../mod_fix.h"
#include "../sl/sl_api.h"
#include "auth_mod.h"
#include "challenge.h"
#include "rpid.h"
#include "api.h"

MODULE_VERSION

#define RAND_SECRET_LEN 32

#define DEF_RPID_PREFIX ""
#define DEF_RPID_SUFFIX ";party=calling;id-type=subscriber;screen=yes"
#define DEF_STRIP_REALM ""
#define DEF_RPID_AVP "$avp(s:rpid)"

static str auth_500_err = str_init("Server Internal Error");

/*
 * Module destroy function prototype
 */
static void destroy(void);

/*
 * Module initialization function prototype
 */
static int mod_init(void);


static int challenge_fixup(void** param, int param_no);


/*
 * Convert both parameters to str* representation
 */
static int rpid_fixup(void** param, int param_no);

static int auth_fixup(void** param, int param_no);

int pv_proxy_authorize(struct sip_msg* msg, char* realm, char* str2);
int pv_www_authorize(struct sip_msg* msg, char* realm, char* str2);

/** SL binds */
struct sl_binds slb;


/*
 * Module parameter variables
 */
char* sec_param    = 0;   /* If the parameter was not used, the secret phrase will be auto-generated */
int   nonce_expire = 300; /* Nonce lifetime */

str secret;
char* sec_rand = 0;

int auth_calc_ha1 = 0;

/* Default Remote-Party-ID prefix */
str rpid_prefix = {DEF_RPID_PREFIX, sizeof(DEF_RPID_PREFIX) - 1};
/* Default Remote-Party-IDD suffix */
str rpid_suffix = {DEF_RPID_SUFFIX, sizeof(DEF_RPID_SUFFIX) - 1};
/* Prefix to strip from realm */
str realm_prefix = {DEF_STRIP_REALM, sizeof(DEF_STRIP_REALM) - 1};

/* definition of AVP containing rpid value */
char* rpid_avp_param = DEF_RPID_AVP;

/* definition of AVP containing username value */
char* user_spec_param = 0;
static pv_spec_t user_spec;


/* definition of AVP containing password value */
char* passwd_spec_param = 0;
static pv_spec_t passwd_spec;

/*
 * Exported functions 
 */
static cmd_export_t cmds[] = {
	{"www_challenge",       www_challenge,           2, challenge_fixup,
			REQUEST_ROUTE},
	{"proxy_challenge",     proxy_challenge,         2, challenge_fixup,
			REQUEST_ROUTE},
	{"pv_www_authorize",    pv_www_authorize,        1, auth_fixup,
			REQUEST_ROUTE},
	{"pv_proxy_authorize",  pv_proxy_authorize,      1, auth_fixup,
			REQUEST_ROUTE},
	{"consume_credentials", consume_credentials,     0, 0,
			REQUEST_ROUTE},
	{"is_rpid_user_e164",   is_rpid_user_e164,       0, 0,
			REQUEST_ROUTE},
	{"append_rpid_hf",      append_rpid_hf,          0, 0,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"append_rpid_hf",      append_rpid_hf_p,        2, rpid_fixup,
			REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE},
	{"bind_auth",           (cmd_function)bind_auth, 0, 0,
			0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"secret",          STR_PARAM, &sec_param      },
	{"nonce_expire",    INT_PARAM, &nonce_expire   },
	{"rpid_prefix",     STR_PARAM, &rpid_prefix.s  },
	{"rpid_suffix",     STR_PARAM, &rpid_suffix.s  },
	{"realm_prefix",    STR_PARAM, &realm_prefix.s },
	{"rpid_avp",        STR_PARAM, &rpid_avp_param },
	{"username_spec",   STR_PARAM, &user_spec_param   },
	{"password_spec",   STR_PARAM, &passwd_spec_param },
	{"calculate_ha1",   INT_PARAM, &auth_calc_ha1     },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"auth", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0           /* child initialization function */
};


/*
 * Secret parameter was not used so we generate
 * a random value here
 */
static inline int generate_random_secret(void)
{
	int i;

	sec_rand = (char*)pkg_malloc(RAND_SECRET_LEN);
	if (!sec_rand) {
		LOG(L_ERR, "ERROR:auth:generate_random_secret(): No pkg memory left\n");
		return -1;
	}

	/* the generator is seeded from the core */

	for(i = 0; i < RAND_SECRET_LEN; i++) {
		sec_rand[i] = 32 + (int)(95.0 * rand() / (RAND_MAX + 1.0));
	}

	secret.s = sec_rand;
	secret.len = RAND_SECRET_LEN;

	/*DBG("Generated secret: '%.*s'\n", secret.len, secret.s); */

	return 0;
}


static int mod_init(void)
{
	str stmp;
	LOG(L_INFO,"AUTH module - initializing\n");
	
	/* load the SL API */
	if (load_sl_api(&slb)!=0) {
		LOG(L_ERR, "ERROR:auth:mod_init: can't load SL API\n");
		return -1;
	}

	/* If the parameter was not used */
	if (sec_param == 0) {
		/* Generate secret using random generator */
		if (generate_random_secret() < 0) {
			LOG(L_ERR,"ERROR:auth:mod_init: Error while generating "
				"random secret\n");
			return -3;
		}
	} else {
		/* Otherwise use the parameter's value */
		secret.s = sec_param;
		secret.len = strlen(secret.s);
	}

	if ( init_rpid_avp(rpid_avp_param)<0 ) {
		LOG(L_ERR,"ERROR:auth:mod_init: failed to process rpid AVPs\n");
		return -4;
	}

	rpid_prefix.len = strlen(rpid_prefix.s);
	rpid_suffix.len = strlen(rpid_suffix.s);
	realm_prefix.len = strlen(realm_prefix.s);

	if(user_spec_param!=0)
	{
		stmp.s = user_spec_param; stmp.len = strlen(stmp.s);
		if(pv_parse_spec(&stmp, &user_spec)==NULL)
		{
			LOG(L_ERR,"ERROR:auth:mod_init: failed to parse username spec\n");
			return -5;
		}
		switch(user_spec.type) {
			case PVT_NONE:
			case PVT_EMPTY:
			case PVT_NULL:
			case PVT_MARKER:
			case PVT_COLOR:
				LOG(L_ERR,"ERROR:auth:mod_init: invalid username spec\n");
				return -6;
			default: ;
		}
	}
	if(passwd_spec_param!=0)
	{
		stmp.s = passwd_spec_param; stmp.len = strlen(stmp.s);
		if(pv_parse_spec(&stmp, &passwd_spec)==NULL)
		{
			LOG(L_ERR,"ERROR:auth:mod_init: failed to parse password spec\n");
			return -7;
		}
		switch(passwd_spec.type) {
			case PVT_NONE:
			case PVT_EMPTY:
			case PVT_NULL:
			case PVT_MARKER:
			case PVT_COLOR:
				LOG(L_ERR,"ERROR:auth:mod_init: invalid password spec\n");
				return -8;
			default: ;
		}
	}

	return 0;
}



static void destroy(void)
{
	if (sec_rand) pkg_free(sec_rand);
}

static inline int auth_get_ha1(struct sip_msg *msg, struct username* _username,
		str* _domain, char* _ha1)
{
	pv_value_t sval;
	
	/* get username from PV */
	memset(&sval, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(msg, &user_spec, &sval)==0)
	{
		if(sval.flags==PV_VAL_NONE || (sval.flags&PV_VAL_NULL)
				|| (sval.flags&PV_VAL_EMPTY) || (!(sval.flags&PV_VAL_STR)))
		{
			pv_value_destroy(&sval);
			return 1;
		}
		if(sval.rs.len!= _username->user.len
				|| strncasecmp(sval.rs.s, _username->user.s, sval.rs.len))
		{
			DBG("auth: auth_get_ha1: username mismatch [%.*s] [%.*s]\n",
				_username->user.len, _username->user.s, sval.rs.len, sval.rs.s);
			pv_value_destroy(&sval);
			return 1;
		}
	} else {
		return 1;
	}
	/* get password from PV */
	memset(&sval, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(msg, &passwd_spec, &sval)==0)
	{
		if(sval.flags==PV_VAL_NONE || (sval.flags&PV_VAL_NULL)
				|| (sval.flags&PV_VAL_EMPTY) || (!(sval.flags&PV_VAL_STR)))
		{
			pv_value_destroy(&sval);
			return 1;
		}
	} else {
		return 1;
	}
	if (auth_calc_ha1) {
		/* Only plaintext passwords are stored in database,
		 * we have to calculate HA1 */
		calc_HA1(HA_MD5, &_username->whole, _domain, &sval.rs, 0, 0, _ha1);
		DBG("auth:auth_get_ha1: HA1 string calculated: %s\n", _ha1);
	} else {
		memcpy(_ha1, sval.rs.s, sval.rs.len);
		_ha1[sval.rs.len] = '\0';
	}

	return 0;
}

static inline int pv_authorize(struct sip_msg* msg, pv_elem_t* realm,
										hdr_types_t hftype)
{
	static char ha1[256];
	int res;
	struct hdr_field* h;
	auth_body_t* cred;
	auth_result_t ret;
	str domain;

	if (realm) {
		if (pv_printf_s(msg, realm, &domain)!=0) {
			LOG(L_ERR, "ERROR:auth:authorize: pv_printf_s failed\n");
			return AUTH_ERROR;
		}
	} else {
		domain.len = 0;
		domain.s = 0;
	}

	ret = pre_auth(msg, &domain, hftype, &h);

	if (ret != DO_AUTHORIZATION)
		return ret;

	cred = (auth_body_t*)h->parsed;

	res = auth_get_ha1(msg, &cred->digest.username, &domain, ha1);
	if (res < 0) {
		/* Error */
		if (slb.reply(msg, 500, &auth_500_err) == -1) {
			LOG(L_ERR, "auth:authorize: Error while sending 500 reply\n");
		}
		return ERROR;
	}
	if (res > 0) {
		/* Username not found */
		return USER_UNKNOWN;
	}

	/* Recalculate response, it must be same to authorize successfully */
	if (!check_response(&(cred->digest),&msg->first_line.u.request.method,ha1))
	{
		return post_auth(msg, h);
	}
	return AUTH_ERROR;
}


int pv_proxy_authorize(struct sip_msg* msg, char* realm, char* str2)
{
	return pv_authorize(msg, (pv_elem_t*)realm, HDR_PROXYAUTH_T);
}


int pv_www_authorize(struct sip_msg* msg, char* realm, char* str2)
{
	return pv_authorize(msg, (pv_elem_t*)realm, HDR_AUTHORIZATION_T);
}


static int challenge_fixup(void** param, int param_no)
{
	pv_elem_t *model;
	unsigned long qop;
	int err;
	str s;
	
	if (param_no == 1) {
		s.s = (char*)*param;
		if (s.s==0 || s.s[0]==0) {
			model = 0;
		} else {
			s.len = strlen(s.s);
			if (pv_parse_format(&s,&model)<0) {
				LOG(L_ERR, "ERROR:auth:challenge_fixup: pv_parse_format "
					"failed\n");
				return E_OUT_OF_MEM;
			}
		}
		*param = (void*)model;
	} else if (param_no == 2) {
		qop = str2s(*param, strlen(*param), &err);
		
		if (err == 0) {
			pkg_free(*param);
			*param=(void*)qop;
		} else {
			LOG(L_ERR, "challenge_fixup(): Bad number <%s>\n",
			    (char*)(*param));
			return E_UNSPEC;
		}
	}

	return 0;
}


/*
 * Convert both parameters to str* representation
 */
static int rpid_fixup(void** param, int param_no)
{
	if (param_no == 1) {
		return str_fixup(param, 1);
	} else if (param_no == 2) {
		return str_fixup(param, 1);
	}
	return 0;
}

/*
 * Convert the char* parameters
 */
static int auth_fixup(void** param, int param_no)
{
	pv_elem_t *model;
	str s;

	if (param_no == 1) {
		s.s = (char*)*param;
		if (s.s==0 || s.s[0]==0) {
			model = 0;
		} else {
			s.len =  strlen(s.s);
			if (pv_parse_format(&s,&model)<0) {
				LOG(L_ERR, "ERROR:auth:auth_fixup: pv_parse_format "
					"failed\n");
				return E_OUT_OF_MEM;
			}
		}
		*param = (void*)model;
	}

	return 0;
}
