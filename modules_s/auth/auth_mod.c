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
 *
 * History:
 * --------
 * 2003-02-26: checks and group moved to separate modules (janakj)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "auth_mod.h"
#include "defs.h"
#include "authorize.h"
#include "challenge.h"


#define RAND_SECRET_LEN 32


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


/*
 * Pointer to reply function in stateless module
 */
int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Module parameter variables
 */
char* db_url       = "sql://serro:47serro11@localhost/ser";
char* user_column  = "user";
char* domain_column = "domain";
char* pass_column  = "ha1";

#ifdef USER_DOMAIN_HACK
char* pass_column_2 = "ha1b";
#endif

char* sec_param    = 0;        /* If the parameter was not used, the secret phrase
				* will be auto-generated
				*/                   
char* sec_rand     = 0;
int   calc_ha1       = 0;
int   nonce_expire   = 300;
int   retry_count    = 5;

str secret;
db_con_t* db_handle;   /* Database connection handle */


/*
 * Module interface
 */
#ifdef STATIC_AUTH
struct module_exports auth_exports = {
#else
struct module_exports exports = {
#endif
	"auth", 
	(char*[]) { 
		"www_authorize",
		"proxy_authorize",
		"www_challenge",
		"proxy_challenge",
		"consume_credentials",
	},
	(cmd_function[]) {
		www_authorize,
		proxy_authorize,
		www_challenge,
		proxy_challenge,
		consume_credentials,
	},
	(int[]) {2, 2, 2, 2, 0},
	(fixup_function[]) {
		str_fixup, str_fixup, 
		challenge_fixup, challenge_fixup, 
		0
	},
	5,
	
	(char*[]) {
		"db_url",              /* Database URL */
		"user_column",         /* User column name */
		"domain_column",       /* Domain column name */
		"password_column",     /* HA1/password column name */
#ifdef USER_DOMAIN_HACK
		"password_column_2",
#endif

		"secret",              /* Secret phrase used to generate nonce */
		"calculate_ha1",       /* If set to yes, instead of ha1 value auth module will
                                        * fetch plaintext password from database and calculate
                                        * ha1 value itself */
		"nonce_expire",        /* After how many seconds nonce expires */
		"retry_count",         /* How many times a client is allowed to retry */
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
	        INT_PARAM,
		INT_PARAM,
		INT_PARAM,
	},   /* Module parameter types */
	(void*[]) {
		&db_url,
		&user_column,
		&domain_column,
		&pass_column,
#ifdef USER_DOMAIN_HACK
		&pass_column_2,
#endif
		&sec_param,
		&calc_ha1,
		&nonce_expire,
		&retry_count,
	},   /* Module parameter variable pointers */
#ifdef USER_DOMAIN_HACK
	9,      /* Numberof module parameters */
#else
	8,      /* Number of module paramers */
#endif					     
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0,          /* oncancel function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	if (db_url == 0) {
		LOG(L_ERR, "auth:init_child(): Use db_url parameter\n");
		return -1;
	}
	db_handle = db_init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "auth:init_child(): Unable to connect database\n");
		return -1;
	}
	return 0;

}


/*
 * Secret parameter was not used so we generate
 * a random value here
 */
static inline int generate_random_secret(void)
{
	int i;

	sec_rand = (char*)pkg_malloc(RAND_SECRET_LEN);
	if (!sec_rand) {
		LOG(L_ERR, "generate_random_secret(): No memory left\n");		
		return -1;
	}

	srandom(time(0));

	for(i = 0; i < RAND_SECRET_LEN; i++) {
		sec_rand[i] = 32 + (int)(95.0 * rand() / (RAND_MAX + 1.0));
	}

	secret.s = sec_rand;
	secret.len = RAND_SECRET_LEN;

	     /*	DBG("Generated secret: '%.*s'\n", secret.len, secret.s); */

	return 0;
}


static int mod_init(void)
{
	printf("auth module - initializing\n");
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_init(): Unable to bind database module\n");
		return -1;
	}

	sl_reply = find_export("sl_send_reply", 2);

	if (!sl_reply) {
		LOG(L_ERR, "auth:mod_init(): This module requires sl module\n");
		return -2;
	}

	     /* If the parameter was not used */
	if (sec_param == 0) {
		     /* Generate secret using random generator */
		if (generate_random_secret() < 0) {
			LOG(L_ERR, "mod_init(): Error while generating random secret\n");
			return -3;
		}
	} else {
		     /* Otherwise use the parameter's value */
		secret.s = sec_param;
		secret.len = strlen(secret.s);
	}
	
	return 0;
}



static void destroy(void)
{
	if (sec_rand) pkg_free(sec_rand);
	db_close(db_handle);
}


static int challenge_fixup(void** param, int param_no)
{
	unsigned int qop;
	int err;
	
	if (param_no == 1) {
		return str_fixup(param, param_no);
	} else if (param_no == 2) {
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
			LOG(L_ERR, "str_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}
