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
#include "../../ut.h"
#include "../../error.h"
#include "authorize.h"
#include "challenge.h"
#include "../../mem/mem.h"
#include <radiusclient.h>

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
char* radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";

char* sec            = 0;      /* If the parameter was not used, the secret phrase
				* will be auto-generated
				*/
int   calc_ha1       = 0;
int   nonce_expire   = 300;
int   retry_count    = 5;

str secret;


/*
 * Module interface
 */
struct module_exports exports = {
	"radius_auth", 
	(char*[]) { 
		"radius_www_authorize",
		"radius_proxy_authorize",
		"www_challenge",
		"proxy_challenge",
		"consume_credentials",
		"radius_does_uri_exist",
		"radius_is_in_group"
	},
	(cmd_function[]) {
		radius_www_authorize,
		radius_proxy_authorize,
		www_challenge,
		proxy_challenge,
		consume_credentials,
		radius_does_uri_exist,
		radius_is_in_group
	},
	(int[]) {1, 1, 2, 2, 0, 0, 1},
	(fixup_function[]) {
		str_fixup, str_fixup,
		challenge_fixup, challenge_fixup, 
		0, 0, str_fixup
	},
	7,
	(char*[]) {
		"radius_config",       /* Radius client config file */
		"secret",              /* Secret phrase used to generate nonce */
		"nonce_expire",        /* After how many seconds nonce expires */
		"retry_count"          /* How many times a client is allowed to retry */
	},   /* Module parameter names */
	(modparam_t[]) {
		STR_PARAM,
		STR_PARAM,
	        INT_PARAM,
		INT_PARAM
	},   /* Module parameter types */
	(void*[]) {
		&radius_config,
		&sec,
		&nonce_expire,
		&retry_count
	},   /* Module parameter variable pointers */
	4,   /* Number of module paramers */
	mod_init,   /* module initialization function */
	NULL,       /* response function */
	destroy,    /* destroy function */
	NULL,       /* oncancel function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	return 0;
}


/*
 * Secret parameter was not used so we generate
 * a random value here
 */
static inline int generate_random_secret(void)
{
	int i;

	sec = (char*)pkg_malloc(RAND_SECRET_LEN);
	if (!sec) {
		LOG(L_ERR, "generate_random_secret(): No memory left\n");		
		return -1;
	}

	srandom(time(0));

	for(i = 0; i < RAND_SECRET_LEN; i++) {
		sec[i] = 32 + (int)(95.0 * rand() / (RAND_MAX + 1.0));
	}

	secret.s = sec;
	secret.len = RAND_SECRET_LEN;

	     /*	DBG("Generated secret: '%.*s'\n", secret.len, secret.s); */

	return 0;
}


static int mod_init(void)
{
	printf("auth module - initializing\n");

	sl_reply = find_export("sl_send_reply", 2);

	if (!sl_reply) {
		LOG(L_ERR, "auth:mod_init(): This module requires sl module\n");
		return -2;
	}

	/* If the parameter was not used */
	if (sec == 0) {
		/* Generate secret using random generator */
		if (generate_random_secret() < 0) {
			LOG(L_ERR, "mod_init(): Error while generating random secret\n");
			return -3;
		}
	} else {
		/* Otherwise use the parameter's value */
		secret.s = sec;
		secret.len = strlen(secret.s);
	}
	
	if (rc_read_config(radius_config) != 0) {
		DBG("radius_authorize(): Error opening configuration file \n");
		return(-1);
	}
    
	if (rc_read_dictionary(rc_conf_str("dictionary")) != 0) {
		DBG("Error opening dictionary file \n");
		return(-1);
	}
	return 0;
}


static void destroy(void)
{
	return;
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
			LOG(L_ERR, "radius_auth: str_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}

