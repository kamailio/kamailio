/*
 * $Id$ 
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief Digest Authentication Module
 * \ingroup auth
 * - Module: \ref auth
 */

/*!
 * \defgroup auth AUTH :: The Kamailio auth Module
 * The module provides functions to authentificate users.
 * It also exports a API that can be used from other modules.
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
#include "../../lock_alloc.h"
#include "auth_mod.h"
#include "challenge.h"
#include "api.h"

MODULE_VERSION

/*! length of the random secret */
#define RAND_SECRET_LEN 32

#define DEF_STRIP_REALM ""

/*!
 * Module destroy function prototype
 */
static void destroy(void);

/*!
 * Module initialization function prototype
 */
static int mod_init(void);

int pv_proxy_authorize(struct sip_msg* msg, char* realm, char* str2);
int pv_www_authorize(struct sip_msg* msg, char* realm, char* str2);

/*! SL API structure */
sl_api_t slb;


/*
 * Module parameter variables
 */
char* sec_param    = 0;   /*!< If the parameter is not used, the secret phrase will be auto-generated */
unsigned int   nonce_expire = 30; /*!< Nonce lifetime - default 30 seconds */

str secret;
char* sec_rand = 0;

int auth_calc_ha1 = 0;

/*! Prefix to strip from realm */
str realm_prefix = {DEF_STRIP_REALM, sizeof(DEF_STRIP_REALM) - 1};

/*! definition of AVP containing username value */
char* user_spec_param = 0;
static pv_spec_t user_spec;


/*! definition of AVP containing password value */
char* passwd_spec_param = 0;
static pv_spec_t passwd_spec;

/*! nonce index */
gen_lock_t* nonce_lock= NULL;
char* nonce_buf= NULL;
int* sec_monit= NULL;
int* second= NULL;
int* next_index= NULL;

/*! control nonce usage checking */
int nonce_reuse = 0;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"www_challenge",       (cmd_function)www_challenge,           2,
		fixup_spve_uint, 0, REQUEST_ROUTE},
	{"proxy_challenge",     (cmd_function)proxy_challenge,         2,
		fixup_spve_uint, 0, REQUEST_ROUTE},
	{"pv_www_authorize",    (cmd_function)pv_www_authorize,        1,
		fixup_spve_null, 0, REQUEST_ROUTE},
	{"pv_proxy_authorize",  (cmd_function)pv_proxy_authorize,      1,
		fixup_spve_null, 0, REQUEST_ROUTE},
	{"consume_credentials", (cmd_function)consume_credentials,     0, 0,
			0, REQUEST_ROUTE},
	{"bind_auth_k",           (cmd_function)bind_auth_k, 0, 0,
			0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"secret",          STR_PARAM, &sec_param      },
	{"nonce_expire",    INT_PARAM, &nonce_expire   },
	{"realm_prefix",    STR_PARAM, &realm_prefix.s },
	{"username_spec",   STR_PARAM, &user_spec_param   },
	{"password_spec",   STR_PARAM, &passwd_spec_param },
	{"calculate_ha1",   INT_PARAM, &auth_calc_ha1     },
	{"nonce_reuse",     INT_PARAM, &nonce_reuse       },
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


/*!
 * \brief Generate a random secret
 *
 * Generate a random secret. A secret parameter was not used so we
 * generate a random value here.
 * \return 0 on success, -1 on failure
 */
static inline int generate_random_secret(void)
{
	int i;

	sec_rand = (char*)pkg_malloc(RAND_SECRET_LEN);
	if (!sec_rand) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}

	/* the generator is seeded from the core */

	for(i = 0; i < RAND_SECRET_LEN; i++) {
		sec_rand[i] = 32 + (int)(95.0 * rand() / (RAND_MAX + 1.0));
	}

	secret.s = sec_rand;
	secret.len = RAND_SECRET_LEN;

	/*LM_DBG("Generated secret: '%.*s'\n", secret.len, secret.s); */

	return 0;
}


static int mod_init(void)
{
	str stmp;
	
	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* If the parameter was not used */
	if (sec_param == 0) {
		/* Generate secret using random generator */
		if (generate_random_secret() < 0) {
			LM_ERR("failed to generate random secret\n");
			return -3;
		}
	} else {
		/* Otherwise use the parameter's value */
		secret.s = sec_param;
		secret.len = strlen(secret.s);
	}

	realm_prefix.len = strlen(realm_prefix.s);

	if(user_spec_param!=0)
	{
		stmp.s = user_spec_param; stmp.len = strlen(stmp.s);
		if(pv_parse_spec(&stmp, &user_spec)==NULL)
		{
			LM_ERR("failed to parse username spec\n");
			return -5;
		}
		switch(user_spec.type) {
			case PVT_NONE:
			case PVT_EMPTY:
			case PVT_NULL:
			case PVT_MARKER:
			case PVT_COLOR:
				LM_ERR("invalid username spec\n");
				return -6;
			default: ;
		}
	}
	if(passwd_spec_param!=0)
	{
		stmp.s = passwd_spec_param; stmp.len = strlen(stmp.s);
		if(pv_parse_spec(&stmp, &passwd_spec)==NULL)
		{
			LM_ERR("failed to parse password spec\n");
			return -7;
		}
		switch(passwd_spec.type) {
			case PVT_NONE:
			case PVT_EMPTY:
			case PVT_NULL:
			case PVT_MARKER:
			case PVT_COLOR:
				LM_ERR("invalid password spec\n");
				return -8;
			default: ;
		}
	}

	if(nonce_reuse==0)
	{
	    nonce_lock = (gen_lock_t*)lock_alloc();
		if(nonce_lock== NULL)
	    {
		    LM_ERR("no more shared memory\n");
			return -1;
	    }

		/* initialize lock_nonce */
	    if(lock_init(nonce_lock)== 0)
		{
	        LM_ERR("failed to init lock\n");
		    return -9;
	    }

		nonce_buf= (char*)shm_malloc(NBUF_LEN);
		if(nonce_buf== NULL)
	    {
		    LM_ERR("no more share memory\n");
			return -10;
	    }
		memset(nonce_buf, 255, NBUF_LEN);

		sec_monit= (int*)shm_malloc((nonce_expire +1)* sizeof(int));
		if(sec_monit== NULL)
		{
			LM_ERR("no more share memory\n");
	        return -10;
		}
		memset(sec_monit, -1, (nonce_expire +1)* sizeof(int));
		second= (int*)shm_malloc(sizeof(int));
		next_index= (int*)shm_malloc(sizeof(int));
		if(second==  NULL || next_index== NULL)
	    {
		    LM_ERR("no more share memory\n");
	        return -10;
		}
		*next_index= -1;
	}

	return 0;
}


static void destroy(void)
{
	if (sec_rand) pkg_free(sec_rand);

	if(nonce_reuse==0)
	{
	    if(nonce_lock)
		{
			lock_destroy(nonce_lock);
	        lock_dealloc(nonce_lock);
		}

	    if(nonce_buf)
		    shm_free(nonce_buf);
	    if(second)
		    shm_free(second);
	    if(sec_monit)
		    shm_free(sec_monit);
	    if(next_index)
		    shm_free(next_index);
	}
}


/*!
 * \brief Generate a HA1 response from username and domain
 * \param msg SIP message
 * \param _username user name
 * \param _domain domain
 * \param _ha1 generated HA1
 * \return 0 on success, 1 on error and when the user could not found
 */
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
			LM_DBG("username mismatch [%.*s] [%.*s]\n",
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
		LM_DBG("HA1 string calculated: %s\n", _ha1);
	} else {
		memcpy(_ha1, sval.rs.s, sval.rs.len);
		_ha1[sval.rs.len] = '\0';
	}

	return 0;
}


/*!
 * \brief Check authorization from a pseudo-variable
 * \param msg SIP message
 * \param realm authentification realm
 * \param hftype type of the header field
 * \return 1 when authorized, null on errors, negative on authentification failure
 */
static inline int pv_authorize(struct sip_msg* msg, gparam_p realm,
										hdr_types_t hftype)
{
	static char ha1[256];
	struct hdr_field* h;
	auth_body_t* cred;
	auth_result_t ret;
	str domain;

	if(fixup_get_svalue(msg, realm, &domain)!=0)
	{
		LM_ERR("invalid realm parameter\n");
		return -1;
	}

	if (domain.len==0)
		domain.s = 0;

	ret = pre_auth(msg, &domain, hftype, &h);

	if (ret != DO_AUTHORIZATION)
		return ret;

	cred = (auth_body_t*)h->parsed;

	if ((auth_get_ha1(msg, &cred->digest.username, &domain, ha1)) > 0) {
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


/*!
 * \brief Small wrapper around pv_authorize, use proxy challenge
 * \param msg SIP message
 * \param realm authenfication realm
 * \param str2 unused
 * \return 1 on sucess, 0 on errors, negative on authentification failures
 */
int pv_proxy_authorize(struct sip_msg* msg, char* realm, char* str2)
{
	return pv_authorize(msg, (gparam_p)realm, HDR_PROXYAUTH_T);
}


/*!
 * \brief Small wrapper around pv_authorize, use www challenge
 * \param msg SIP message
 * \param realm authenfication realm
 * \param str2 unused
 * \return 1 on sucess, 0 on errors, negative on authentification failures
 */
int pv_www_authorize(struct sip_msg* msg, char* realm, char* str2)
{
	return pv_authorize(msg, (gparam_p)realm, HDR_AUTHORIZATION_T);
}
