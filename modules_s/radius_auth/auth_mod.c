/* 
 * $Id$ 
 *
 * Digest Authentication Module
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "auth_mod.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "defs.h"
#include <string.h>
#include "checks.h"
#include "group.h"
#include "../../ut.h"
#include "../../error.h"
#include "authorize.h"
#include "challenge.h"
#include <radiusclient.h>

#define CONFIG "etc/radiusclient.conf"

/*
 * Module destroy function prototype
 */
static void destroy(void);


/*
 * Module child-init function prototype
 */
static int child_init(int rank);


/*
 * Module initialization function prototype
 */
static int mod_init(void);


static int challenge_fixup(void** param, int param_no);
static int str_fixup(void** param, int param_no);
static int hf_fixup(void** param, int param_no);


/*
 * Pointer to reply function in stateless module
 */
int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Module parameter variables
 */
char* db_url       = "sql://janakj:heslo@localhost/ser";
char* user_column  = "user";
char* realm_column = "realm";
char* pass_column  = "ha1";

#ifdef USER_DOMAIN_HACK
char* pass_column_2 = "ha1b";
#endif

char* sec          = "4e9rhygt90ofw34e8hiof09tg"; /* Secret phrase used to generate nonce value */
char* grp_table    = "grp";                       /* Table name where group definitions are stored */
char* grp_user_col = "user";
char* grp_grp_col  = "grp";
int   calc_ha1     = 0;
int   nonce_expire = 300;
int   retry_count  = 5;

str secret;
db_con_t* db_handle;   /* Database connection handle */


/*
 * Module interface
 */
struct module_exports exports = {
	"radius_auth", 
	(char*[]) { 
		"radius_www_authorize",
		"radius_proxy_authorize",
		"radius_www_challenge",
		"radius_proxy_challenge",
		"is_user",
		"is_in_group",
		"radius_is_in_group",
		"check_to",
		"check_from",
		"consume_credentials",
		"is_user_in"
	},
	(cmd_function[]) {
		radius_www_authorize,
		radius_proxy_authorize,
		radius_www_challenge,
		radius_proxy_challenge,
		is_user,
		is_in_group,
		radius_is_in_group,
		check_to,
		check_from,
		consume_credentials,
		is_user_in
	},
	(int[]) {2, 2, 2, 2, 1, 1, 1, 0, 0, 0, 2},
	(fixup_function[]) {
		str_fixup, str_fixup, 
		challenge_fixup, challenge_fixup, 
		str_fixup, str_fixup, 0, 0,
		0, hf_fixup
	},
	11,
	
	(char*[]) {
		"db_url",              /* Database URL */
		"user_column",         /* User column name */
		"realm_column",        /* Realm column name */
		"password_column",     /* HA1/password column name */
#ifdef USER_DOMAIN_HACK
		"password_column_2",
#endif

		"secret",              /* Secret phrase used to generate nonce */
		"group_table",         /* Group table name */
		"group_user_column",   /* Group table user column name */
		"group_group_column",  /* Group table group column name */
		"calculate_ha1",       /* If set to yes, instead of ha1 value auth module will
                                        * fetch plaintext password from database and calculate
                                        * ha1 value itself */
		"nonce_expire",        /* After how many seconds nonce expires */
		"retry_count"          /* How many times a client is allowed to retry */
		
		
	},   /* Module parameter names */
	(modparam_t[]) {
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
#ifdef USER_DOMAIN_HACK
		STR_PARAM,
#endif
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
	        INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},   /* Module parameter types */
	(void*[]) {
		&db_url,
		&user_column,
		&realm_column,
		&pass_column,
#ifdef USER_DOMAIN_HACK
		&pass_column_2,
#endif
		&sec,
		&grp_table,
		&grp_user_col,
		&grp_grp_col,
		&calc_ha1,
		&nonce_expire,
		&retry_count
		
	},   /* Module parameter variable pointers */
#ifdef USER_DOMAIN_HACK
	12,      /* Numberof module parameters */
#else
	11,      /* Number of module paramers */
#endif					     
	mod_init,   /* module initialization function */
	NULL,       /* response function */
	NULL,    /* destroy function */
	NULL,       /* oncancel function */
	NULL		/* Child Init */
};




static int mod_init(void)
{
	printf("auth module - initializing\n");

    
	sl_reply = find_export("sl_send_reply", 2);

	if (!sl_reply) {
		LOG(L_ERR, "auth:mod_init(): This module requires sl module\n");
		return -2;
	}

	     /* Precalculate secret string length */
	secret.s = sec;
	secret.len = strlen(secret.s);

	if (rc_read_config(CONFIG) != 0) {
    	DBG("radius_authorize(): Error opening configuration file \n");
        return(-1);
    }
    
	if (rc_read_dictionary(rc_conf_str("dictionary")) != 0) {
    	DBG("Error opening dictionary file \n");
        return(-1);
    }

	return 0;
}

static int challenge_fixup(void** param, int param_no)
{
	unsigned int qop;
	int err;
	
	if (param_no == 2) {
		qop = str2s(*param, strlen(*param), &err);

		if (err == 0) {
			free(*param);
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
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;

	if (param_no == 1) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "authorize_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}


/*
 * Convert HF description string to hdr_field pointer
 *
 * Supported strings: 
 * "Request-URI", "To", "From", "Credentials"
 */
static int hf_fixup(void** param, int param_no)
{
	void* ptr;
	str* s;

	if (param_no == 1) {
		ptr = *param;
		
		if (!strcasecmp((char*)*param, "Request-URI")) {
			*param = (void*)1;
		} else if (!strcasecmp((char*)*param, "To")) {
			*param = (void*)2;
		} else if (!strcasecmp((char*)*param, "From")) {
			*param = (void*)3;
		} else if (!strcasecmp((char*)*param, "Credentials")) {
			*param = (void*)4;
		} else {
			LOG(L_ERR, "hf_fixup(): Unsupported Header Field identifier\n");
			return E_UNSPEC;
		}

		free(ptr);
	} else if (param_no == 2) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "hf_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}
