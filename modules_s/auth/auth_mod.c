/*
 * $Id$
 *
 * Digest Authentication Module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * 2003-02-26 checks and group moved to separate modules (janakj)
 * 2003-03-10 New module interface (janakj)
 * 2003-03-16 flags export parameter added (janakj)
 * 2003-03-19 all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 * 2003-04-28 rpid contributed by Juha Heinanen added (janakj)
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/digest/digest.h"
#include "../../data_lump.h"
#include "../../error.h"
#include "../../ut.h"
#include "../sl/sl.h"
#include "auth_mod.h"
#include "challenge.h"
#include "api.h"

MODULE_VERSION

#define RAND_SECRET_LEN 32


/*
 * Module destroy function prototype
 */
static void destroy(void);

/*
 * Module initialization function prototype
 */
static int mod_init(void);

/*
 * Remove used credentials from a SIP message header
 */
int consume_credentials(struct sip_msg* msg, char* s1, char* s2);


/*
 * Module parameter variables
 */
char* sec_param    = 0;     /* If the parameter was not used, the secret phrase will be auto-generated */
int   nonce_expire = 300;   /* Nonce lifetime */
int   protect_contacts = 0; /* Do not include contacts in nonce by default */

str secret;
char* sec_rand = 0;

str challenge_attr = STR_STATIC_INIT("$digest_challenge");
avp_ident_t challenge_avpid;

str proxy_challenge_header = STR_STATIC_INIT("Proxy-Authenticate");
str www_challenge_header = STR_STATIC_INIT("WWW-Authenticate");

struct qp qop = {
    STR_STATIC_INIT("auth"),
    QOP_AUTH
};


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"consume_credentials", consume_credentials,     0, 0, REQUEST_ROUTE},
    {"bind_auth",           (cmd_function)bind_auth, 0, 0, 0            },
    {0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"secret",                 PARAM_STRING, &sec_param             },
    {"nonce_expire",           PARAM_INT,    &nonce_expire          },
    {"protect_contacts",       PARAM_INT,    &protect_contacts      },
    {"challenge_attr",         PARAM_STR,    &challenge_attr        },
    {"proxy_challenge_header", PARAM_STR,    &proxy_challenge_header},
    {"www_challenge_header",   PARAM_STR,    &www_challenge_header  },
    {"qop",                    PARAM_STR,    &qop.qop_str           },
    {0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
    "auth",
    cmds,
    0,          /* RPC methods */
    params,
    mod_init,   /* module initialization function */
    0,          /* response function */
    destroy,    /* destroy function */
    0,          /* oncancel function */
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
	LOG(L_ERR, "auth:generate_random_secret: No memory left\n");
	return -1;
    }
    
    /* srandom(time(0));  -- seeded by core */
    
    for(i = 0; i < RAND_SECRET_LEN; i++) {
	sec_rand[i] = 32 + (int)(95.0 * rand() / (RAND_MAX + 1.0));
    }
    
    secret.s = sec_rand;
    secret.len = RAND_SECRET_LEN;
    
	 /* DBG("Generated secret: '%.*s'\n", secret.len, secret.s); */
    
    return 0;
}


static int mod_init(void)
{
    str attr;
    
    DBG("auth module - initializing\n");
    
	 /* If the parameter was not used */
    if (sec_param == 0) {
	     /* Generate secret using random generator */
	if (generate_random_secret() < 0) {
	    LOG(L_ERR, "auth:mod_init: Error while generating random secret\n");
	    return -3;
	}
    } else {
	     /* Otherwise use the parameter's value */
	secret.s = sec_param;
	secret.len = strlen(secret.s);
    }
    
    if ((!challenge_attr.s || challenge_attr.len == 0) ||
	challenge_attr.s[0] != '$'){
	ERR("auth: Invalid value of challenge_attr module parameter\n");
	return -1;
    }
    
    attr.s = challenge_attr.s + 1;
    attr.len = challenge_attr.len - 1;
    
    if (parse_avp_ident(&attr, &challenge_avpid) < 0) {
	ERR("auth: Error while parsing value of challenge_attr module parameter\n");
	return -1;
    }

    parse_qop(&qop);
    if (qop.qop_parsed == QOP_OTHER) {
	ERR("auth: Unsupported qop parameter value\n");
	return -1;
    }
    
    return 0;
}


static void destroy(void)
{
    if (sec_rand) pkg_free(sec_rand);
}


/*
 * Remove used credentials from a SIP message header
 */
int consume_credentials(struct sip_msg* msg, char* s1, char* s2)
{
    struct hdr_field* h;
    int len;
    
    get_authorized_cred(msg->authorization, &h);
    if (!h) {
	get_authorized_cred(msg->proxy_auth, &h);
	if (!h) { 
	    if (msg->REQ_METHOD!=METHOD_ACK 
		&& msg->REQ_METHOD!=METHOD_CANCEL) {
		LOG(L_ERR, "auth:consume_credentials: No authorized "
		    "credentials found (error in scripts)\n");
	    }
	    return -1;
	}
    }
    
    len=h->len;
    
    if (del_lump(msg, h->name.s - msg->buf, len, 0) == 0) {
	LOG(L_ERR, "auth:consume_credentials: Can't remove credentials\n");
	return -1;
    }
    
    return 1;
}

