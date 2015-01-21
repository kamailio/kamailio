/*
 * Digest Authentication Module
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../pvapi.h"
#include "../../lvalue.h"
#include "../../mod_fix.h"
#include "../../modules/sl/sl.h"
#include "auth_mod.h"
#include "challenge.h"
#include "api.h"
#include "nid.h"
#include "nc.h"
#include "ot_nonce.h"
#include "rfc2617.h"

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
int w_consume_credentials(struct sip_msg* msg, char* s1, char* s2);
/*
 * Check for credentials with given realm
 */
int w_has_credentials(struct sip_msg* msg, char* s1, char* s2);

static int pv_proxy_authenticate(struct sip_msg* msg, char* realm,
		char *passwd, char *flags);
static int pv_www_authenticate(struct sip_msg* msg, char* realm,
		char *passwd, char *flags);
static int pv_www_authenticate2(struct sip_msg* msg, char* realm,
		char *passwd, char *flags, char *method);
static int fixup_pv_auth(void **param, int param_no);
static int pv_auth_check(sip_msg_t *msg, char *realm,
		char *passwd, char *flags, char *checks);
static int fixup_pv_auth_check(void **param, int param_no);

static int proxy_challenge(struct sip_msg *msg, char* realm, char *flags);
static int www_challenge(struct sip_msg *msg, char* realm, char *flags);
static int w_auth_challenge(struct sip_msg *msg, char* realm, char *flags);
static int fixup_auth_challenge(void **param, int param_no);

static int w_auth_get_www_authenticate(sip_msg_t* msg, char* realm,
		char *flags, char *dst);
static int fixup_auth_get_www_authenticate(void **param, int param_no);

/*
 * Module parameter variables
 */
char* sec_param    = 0;     /* If the parameter was not used, the secret phrase will be auto-generated */
int   nonce_expire = 300;   /* Nonce lifetime */
/*int   auth_extra_checks = 0;  -- in nonce.c */
int   protect_contacts = 0; /* Do not include contacts in nonce by default */
int force_stateless_reply = 0; /* Always send reply statelessly */

/*! Prefix to strip from realm */
str auth_realm_prefix = {"", 0};

static int auth_use_domain = 0;

str secret1;
str secret2;
char* sec_rand1 = 0;
char* sec_rand2 = 0;

str challenge_attr = STR_STATIC_INIT("$digest_challenge");
avp_ident_t challenge_avpid;

str proxy_challenge_header = STR_STATIC_INIT("Proxy-Authenticate");
str www_challenge_header = STR_STATIC_INIT("WWW-Authenticate");

struct qp auth_qop = {
    STR_STATIC_INIT("auth"),
    QOP_AUTH
};

static struct qp auth_qauth = {
    STR_STATIC_INIT("auth"),
    QOP_AUTH
};

static struct qp auth_qauthint = {
    STR_STATIC_INIT("auth-int"),
    QOP_AUTHINT
};

/*! SL API structure */
sl_api_t slb;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"consume_credentials",    w_consume_credentials,                0,
			0, REQUEST_ROUTE},
    {"www_challenge",          (cmd_function)www_challenge,          2,
			fixup_auth_challenge, REQUEST_ROUTE},
    {"proxy_challenge",        (cmd_function)proxy_challenge,        2,
			fixup_auth_challenge, REQUEST_ROUTE},
    {"auth_challenge",         (cmd_function)w_auth_challenge,       2,
			fixup_auth_challenge, REQUEST_ROUTE},
    {"pv_www_authorize",       (cmd_function)pv_www_authenticate,    3,
			fixup_pv_auth, REQUEST_ROUTE},
    {"pv_www_authenticate",    (cmd_function)pv_www_authenticate,    3,
			fixup_pv_auth, REQUEST_ROUTE},
    {"pv_www_authenticate",    (cmd_function)pv_www_authenticate2,   4,
			fixup_pv_auth, REQUEST_ROUTE},
    {"pv_proxy_authorize",     (cmd_function)pv_proxy_authenticate,  3,
			fixup_pv_auth, REQUEST_ROUTE},
    {"pv_proxy_authenticate",  (cmd_function)pv_proxy_authenticate,  3,
			fixup_pv_auth, REQUEST_ROUTE},
    {"auth_get_www_authenticate",  (cmd_function)w_auth_get_www_authenticate,  3,
			fixup_auth_get_www_authenticate, REQUEST_ROUTE},
    {"has_credentials",        w_has_credentials,                    1,
			fixup_spve_null, REQUEST_ROUTE},
    {"pv_auth_check",         (cmd_function)pv_auth_check,           4,
			fixup_pv_auth_check, REQUEST_ROUTE},
    {"bind_auth_s",           (cmd_function)bind_auth_s, 0, 0, 0        },
    {0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"secret",                 PARAM_STRING, &sec_param             },
    {"nonce_expire",           PARAM_INT,    &nonce_expire          },
	{"nonce_auth_max_drift",   PARAM_INT,    &nonce_auth_max_drift  },
    {"protect_contacts",       PARAM_INT,    &protect_contacts      },
    {"challenge_attr",         PARAM_STR,    &challenge_attr        },
    {"proxy_challenge_header", PARAM_STR,    &proxy_challenge_header},
    {"www_challenge_header",   PARAM_STR,    &www_challenge_header  },
    {"qop",                    PARAM_STR,    &auth_qop.qop_str      },
	{"auth_checks_register",   PARAM_INT,    &auth_checks_reg       },
	{"auth_checks_no_dlg",     PARAM_INT,    &auth_checks_ood       },
	{"auth_checks_in_dlg",     PARAM_INT,    &auth_checks_ind       },
	{"nonce_count"  ,          PARAM_INT,    &nc_enabled            },
	{"nc_array_size",          PARAM_INT,    &nc_array_size         },
	{"nc_array_order",         PARAM_INT,    &nc_array_k            },
	{"one_time_nonce"  ,       PARAM_INT,    &otn_enabled           },
	{"otn_in_flight_no",       PARAM_INT,    &otn_in_flight_no      },
	{"otn_in_flight_order",    PARAM_INT,    &otn_in_flight_k       },
	{"nid_pool_no",            PARAM_INT,    &nid_pool_no           },
    {"force_stateless_reply",  PARAM_INT,    &force_stateless_reply },
	{"realm_prefix",           PARAM_STRING, &auth_realm_prefix.s   },
    {"use_domain",             PARAM_INT,    &auth_use_domain       },
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
    
    sec_rand1 = (char*)pkg_malloc(RAND_SECRET_LEN);
    sec_rand2 = (char*)pkg_malloc(RAND_SECRET_LEN);
    if (!sec_rand1 || !sec_rand2) {
	LOG(L_ERR, "auth:generate_random_secret: No memory left\n");
	if (sec_rand1){
		pkg_free(sec_rand1);
		sec_rand1=0;
	}
	return -1;
    }
    
    /* srandom(time(0));  -- seeded by core */
    
    for(i = 0; i < RAND_SECRET_LEN; i++) {
	sec_rand1[i] = 32 + (int)(95.0 * rand() / (RAND_MAX + 1.0));
    }
    
    secret1.s = sec_rand1;
    secret1.len = RAND_SECRET_LEN;
	
    for(i = 0; i < RAND_SECRET_LEN; i++) {
	sec_rand2[i] = 32 + (int)(95.0 * rand() / (RAND_MAX + 1.0));
    }
    
    secret2.s = sec_rand2;
    secret2.len = RAND_SECRET_LEN;
    
	 /* DBG("Generated secret: '%.*s'\n", secret.len, secret.s); */
    
    return 0;
}


static int mod_init(void)
{
    str attr;
    
    DBG("auth module - initializing\n");
    
	auth_realm_prefix.len = strlen(auth_realm_prefix.s);

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	/* If the parameter was not used */
    if (sec_param == 0) {
		/* Generate secret using random generator */
		if (generate_random_secret() < 0) {
			LOG(L_ERR, "auth:mod_init: Error while generating random secret\n");
			return -3;
		}
    } else {
		/* Otherwise use the parameter's value */
		secret1.s = sec_param;
		secret1.len = strlen(secret1.s);
		
		if (auth_checks_reg || auth_checks_ind || auth_checks_ood) {
			/* divide the secret in half: one half for secret1 and one half for
			 *  secret2 */
			secret2.len = secret1.len/2;
			secret1.len -= secret2.len;
			secret2.s = secret1.s + secret1.len;
			if (secret2.len < 16) {
				WARN("auth: consider a longer secret when extra auth checks are"
					 " enabled (the config secret is divided in 2!)\n");
			}
		}
    }
    
    if ((!challenge_attr.s || challenge_attr.len == 0) ||
		challenge_attr.s[0] != '$') {
		ERR("auth: Invalid value of challenge_attr module parameter\n");
		return -1;
    }
    
    attr.s = challenge_attr.s + 1;
    attr.len = challenge_attr.len - 1;
    
    if (parse_avp_ident(&attr, &challenge_avpid) < 0) {
		ERR("auth: Error while parsing value of challenge_attr module"
				" parameter\n");
		return -1;
    }
	
    parse_qop(&auth_qop);
	switch(auth_qop.qop_parsed){
		case QOP_OTHER:
			ERR("auth: Unsupported qop parameter value\n");
			return -1;
		case QOP_AUTH:
		case QOP_AUTHINT:
			if (nc_enabled){
#ifndef USE_NC
				WARN("auth: nounce count support enabled from config, but"
					" disabled at compile time (recompile with -DUSE_NC)\n");
				nc_enabled=0;
#else
				if (nid_crt==0)
					init_nonce_id();
				if (init_nonce_count()!=0)
					return -1;
#endif
			}
#ifdef USE_NC
			else{
				INFO("auth: qop set, but nonce-count (nc_enabled) support"
						" disabled\n");
			}
#endif
			break;
		default:
			if (nc_enabled){
				WARN("auth: nonce-count support enabled, but qop not set\n");
				nc_enabled=0;
			}
			break;
	}
	if (otn_enabled){
#ifdef USE_OT_NONCE
		if (nid_crt==0) init_nonce_id();
		if (init_ot_nonce()!=0) 
			return -1;
#else
		WARN("auth: one-time-nonce support enabled from config, but "
				"disabled at compile time (recompile with -DUSE_OT_NONCE)\n");
		otn_enabled=0;
#endif /* USE_OT_NONCE */
	}

    return 0;
}


static void destroy(void)
{
    if (sec_rand1) pkg_free(sec_rand1);
    if (sec_rand2) pkg_free(sec_rand2);
#ifdef USE_NC
	destroy_nonce_count();
#endif
#ifdef USE_OT_NONCE
	destroy_ot_nonce();
#endif
#if defined USE_NC || defined USE_OT_NONCE
	destroy_nonce_id();
#endif
}


/*
 * Remove used credentials from a SIP message header
 */
int consume_credentials(struct sip_msg* msg)
{
    struct hdr_field* h;
    int len;

	/* skip requests that can't be authenticated */
	if (msg->REQ_METHOD & (METHOD_ACK|METHOD_CANCEL|METHOD_PRACK))
		return -1;
    get_authorized_cred(msg->authorization, &h);
    if (!h) {
		get_authorized_cred(msg->proxy_auth, &h);
		if (!h) { 
			LOG(L_ERR, "auth:consume_credentials: No authorized "
					"credentials found (error in scripts)\n");
			return -1;
		}
    }
    
    len = h->len;
    
    if (del_lump(msg, h->name.s - msg->buf, len, 0) == 0) {
		LOG(L_ERR, "auth:consume_credentials: Can't remove credentials\n");
		return -1;
    }
    
    return 1;
}

/**
 *
 */
int w_consume_credentials(struct sip_msg* msg, char* s1, char* s2)
{
	return consume_credentials(msg);
}

/**
 *
 */
int w_has_credentials(sip_msg_t *msg, char* realm, char* s2)
{
    str srealm  = {0, 0};
	hdr_field_t *hdr = NULL;
	int ret;

	if (fixup_get_svalue(msg, (gparam_t*)realm, &srealm) < 0) {
		LM_ERR("failed to get realm value\n");
		return -1;
	}

	ret = find_credentials(msg, &srealm, HDR_PROXYAUTH_T, &hdr);
	if(ret==0) {
		LM_DBG("found www credentials with realm [%.*s]\n", srealm.len, srealm.s);
		return 1;
	}
	ret = find_credentials(msg, &srealm, HDR_AUTHORIZATION_T, &hdr);
	if(ret==0) {
		LM_DBG("found proxy credentials with realm [%.*s]\n", srealm.len, srealm.s);
		return 1;
	}

	LM_DBG("no credentials with realm [%.*s]\n", srealm.len, srealm.s);
	return -1;
}

/**
 * @brief do WWW-Digest authentication with password taken from cfg var
 */
int pv_authenticate(struct sip_msg *msg, str *realm, str *passwd,
		int flags, int hftype, str *method)
{
	struct hdr_field* h;
	auth_body_t* cred;
	int ret;
    str hf = {0, 0};
    avp_value_t val;
	static char ha1[256];
	struct qp *qop = NULL;

	cred = 0;
	ret = AUTH_ERROR;

	switch(pre_auth(msg, realm, hftype, &h, NULL)) {
		case NONCE_REUSED:
			LM_DBG("nonce reused");
			ret = AUTH_NONCE_REUSED;
			goto end;
		case STALE_NONCE:
			LM_DBG("stale nonce\n");
			ret = AUTH_STALE_NONCE;
			goto end;
		case NO_CREDENTIALS:
			LM_DBG("no credentials\n");
			ret = AUTH_NO_CREDENTIALS;
			goto end;
		case ERROR:
		case BAD_CREDENTIALS:
			LM_DBG("error or bad credentials\n");
			ret = AUTH_ERROR;
			goto end;
		case CREATE_CHALLENGE:
			LM_ERR("CREATE_CHALLENGE is not a valid state\n");
			ret = AUTH_ERROR;
			goto end;
		case DO_RESYNCHRONIZATION:
			LM_ERR("DO_RESYNCHRONIZATION is not a valid state\n");
			ret = AUTH_ERROR;
			goto end;
		case NOT_AUTHENTICATED:
			LM_DBG("not authenticated\n");
			ret = AUTH_ERROR;
			goto end;
		case DO_AUTHENTICATION:
			break;
		case AUTHENTICATED:
			ret = AUTH_OK;
			goto end;
	}

	cred = (auth_body_t*)h->parsed;

	/* compute HA1 if needed */
	if ((flags&1)==0) {
		/* Plaintext password is stored in PV, calculate HA1 */
		calc_HA1(HA_MD5, &cred->digest.username.whole, realm,
				passwd, 0, 0, ha1);
		LM_DBG("HA1 string calculated: %s\n", ha1);
	} else {
		memcpy(ha1, passwd->s, passwd->len);
		ha1[passwd->len] = '\0';
	}

	/* Recalculate response, it must be same to authorize successfully */
	ret = auth_check_response(&(cred->digest), method, ha1);
	if(ret==AUTHENTICATED) {
		ret = AUTH_OK;
		switch(post_auth(msg, h)) {
			case AUTHENTICATED:
				break;
			default:
				ret = AUTH_ERROR;
				break;
		}
	} else {
		if(ret==NOT_AUTHENTICATED)
			ret = AUTH_INVALID_PASSWORD;
		else
			ret = AUTH_ERROR;
	}

end:
	if (ret < 0) {
		/* check if required to add challenge header as avp */
		if(!(flags&14))
			return ret;
		if(flags&8) {
			qop = &auth_qauthint;
		} else if(flags&4) {
			qop = &auth_qauth;
		}
		if (get_challenge_hf(msg, (cred ? cred->stale : 0),
				realm, NULL, NULL, qop, hftype, &hf) < 0) {
			ERR("Error while creating challenge\n");
			ret = AUTH_ERROR;
		} else {
			val.s = hf;
			if(add_avp(challenge_avpid.flags | AVP_VAL_STR,
							challenge_avpid.name, val) < 0) {
				LM_ERR("Error while creating attribute with challenge\n");
				ret = AUTH_ERROR;
			}
			pkg_free(hf.s);
		}
	}

	return ret;
}

/**
 *
 */
static int pv_proxy_authenticate(struct sip_msg *msg, char* realm,
		char *passwd, char *flags)
{
    int vflags = 0;
    str srealm  = {0, 0};
    str spasswd = {0, 0};

	if (get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if (get_str_fparam(&spasswd, msg, (fparam_t*)passwd) < 0) {
		LM_ERR("failed to get passwd value\n");
		goto error;
	}

	if(spasswd.len==0) {
		LM_ERR("invalid password value - empty content\n");
		goto error;
	}

	if (get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}
	return pv_authenticate(msg, &srealm, &spasswd, vflags, HDR_PROXYAUTH_T,
				&msg->first_line.u.request.method);

error:
	return AUTH_ERROR;
}

/**
 *
 */
static int pv_www_authenticate(struct sip_msg *msg, char* realm,
		char *passwd, char *flags)
{
    int vflags = 0;
    str srealm  = {0, 0};
    str spasswd = {0, 0};

	if (get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if (get_str_fparam(&spasswd, msg, (fparam_t*)passwd) < 0) {
		LM_ERR("failed to get passwd value\n");
		goto error;
	}

	if(spasswd.len==0) {
		LM_ERR("invalid password value - empty content\n");
		goto error;
	}

	if (get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}
	return pv_authenticate(msg, &srealm, &spasswd, vflags, HDR_AUTHORIZATION_T,
				&msg->first_line.u.request.method);

error:
	return AUTH_ERROR;
}

static int pv_www_authenticate2(struct sip_msg *msg, char* realm,
		char *passwd, char *flags, char *method)
{
    int vflags = 0;
    str srealm  = {0, 0};
    str spasswd = {0, 0};
    str smethod = {0, 0};

	if (get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if (get_str_fparam(&spasswd, msg, (fparam_t*)passwd) < 0) {
		LM_ERR("failed to get passwd value\n");
		goto error;
	}

	if(spasswd.len==0) {
		LM_ERR("invalid password value - empty content\n");
		goto error;
	}

	if (get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	if (get_str_fparam(&smethod, msg, (fparam_t*)method) < 0) {
		LM_ERR("failed to get method value from msg %p var %p\n", msg, method);
		goto error;
	}

	if(smethod.len==0) {
		LM_ERR("invalid method value - empty content\n");
		goto error;
	}

	return pv_authenticate(msg, &srealm, &spasswd, vflags, HDR_AUTHORIZATION_T,
				&smethod);

error:
	return AUTH_ERROR;
}

/**
 *
 */
static int pv_auth_check(sip_msg_t *msg, char *realm,
		char *passwd, char *flags, char *checks)
{
    int vflags = 0;
    int vchecks = 0;
    str srealm  = {0, 0};
    str spasswd = {0, 0};
	int ret;
	hdr_field_t *hdr;
	sip_uri_t *uri = NULL;
	sip_uri_t *turi = NULL;
	sip_uri_t *furi = NULL;

	if(msg==NULL) {
		LM_ERR("invalid msg parameter\n");
		return AUTH_ERROR;
	}

	if ((msg->REQ_METHOD == METHOD_ACK) || (msg->REQ_METHOD == METHOD_CANCEL)) {
		return AUTH_OK;
	}

	if(realm==NULL || passwd==NULL || flags==NULL || checks==NULL) {
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		return AUTH_ERROR;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&spasswd, msg, (fparam_t*)passwd) < 0) {
		LM_ERR("failed to get passwd value\n");
		return AUTH_ERROR;
	}

	if(spasswd.len==0) {
		LM_ERR("invalid password value - empty content\n");
		return AUTH_ERROR;
	}

	if (get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		return AUTH_ERROR;
	}

	if (get_int_fparam(&vchecks, msg, (fparam_t*)checks) < 0) {
		LM_ERR("invalid checks value\n");
		return AUTH_ERROR;
	}
	LM_DBG("realm [%.*s] flags [%d] checks [%d]\n", srealm.len, srealm.s,
			vflags, vchecks);

	if(msg->REQ_METHOD==METHOD_REGISTER)
		ret = pv_authenticate(msg, &srealm, &spasswd, vflags, HDR_AUTHORIZATION_T,
					&msg->first_line.u.request.method);
	else
		ret = pv_authenticate(msg, &srealm, &spasswd, vflags, HDR_PROXYAUTH_T,
					&msg->first_line.u.request.method);

	if(ret==AUTH_OK && (vchecks&AUTH_CHECK_ID_F)) {
		hdr = (msg->proxy_auth==0)?msg->authorization:msg->proxy_auth;
		srealm = ((auth_body_t*)(hdr->parsed))->digest.username.user;

		if((furi=parse_from_uri(msg))==NULL)
			return AUTH_ERROR;

		if(msg->REQ_METHOD==METHOD_REGISTER || msg->REQ_METHOD==METHOD_PUBLISH) {
			if((turi=parse_to_uri(msg))==NULL)
				return AUTH_ERROR;
			uri = turi;
		} else {
			uri = furi;
		}
		if(srealm.len!=uri->user.len
					|| strncmp(srealm.s, uri->user.s, srealm.len)!=0)
			return AUTH_USER_MISMATCH;

		if(msg->REQ_METHOD==METHOD_REGISTER || msg->REQ_METHOD==METHOD_PUBLISH) {
			/* check from==to */
			if(furi->user.len!=turi->user.len
					|| strncmp(furi->user.s, turi->user.s, furi->user.len)!=0)
				return AUTH_USER_MISMATCH;
			if(auth_use_domain!=0 && (furi->host.len!=turi->host.len
					|| strncmp(furi->host.s, turi->host.s, furi->host.len)!=0))
				return AUTH_USER_MISMATCH;
			/* check r-uri==from for publish */
			if(msg->REQ_METHOD==METHOD_PUBLISH) {
				if(parse_sip_msg_uri(msg)<0)
					return AUTH_ERROR;
				uri = &msg->parsed_uri;
				if(furi->user.len!=uri->user.len
						|| strncmp(furi->user.s, uri->user.s, furi->user.len)!=0)
					return AUTH_USER_MISMATCH;
				if(auth_use_domain!=0 && (furi->host.len!=uri->host.len
						|| strncmp(furi->host.s, uri->host.s, furi->host.len)!=0))
					return AUTH_USER_MISMATCH;
				}
		}
		return AUTH_OK;
	}

	return ret;
}

/**
 * @brief fixup function for pv_{www,proxy}_authenticate
 */
static int fixup_pv_auth(void **param, int param_no)
{
	if(strlen((char*)*param)<=0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
		case 2:
		case 4:
			return fixup_var_pve_str_12(param, 1);
		case 3:
			return fixup_var_int_12(param, 1);
	}
	return 0;
}

/**
 * @brief fixup function for pv_{www,proxy}_authenticate
 */
static int fixup_pv_auth_check(void **param, int param_no)
{
	if(strlen((char*)*param)<=0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
		case 2:
			return fixup_var_pve_str_12(param, 1);
		case 3:
		case 4:
			return fixup_var_int_12(param, 1);
	}
	return 0;
}



/**
 *
 */
static int auth_send_reply(struct sip_msg *msg, int code, char *reason,
					char *hdr, int hdr_len)
{
        str reason_str;

	/* Add new headers if there are any */
	if ((hdr!=NULL) && (hdr_len>0)) {
		if (add_lump_rpl(msg, hdr, hdr_len, LUMP_RPL_HDR)==0) {
			LM_ERR("failed to append hdr to reply\n");
			return -1;
		}
	}

	reason_str.s = reason;
	reason_str.len = strlen(reason);

	return force_stateless_reply ?
	    slb.sreply(msg, code, &reason_str) :
	    slb.freply(msg, code, &reason_str);
}

/**
 *
 */
int auth_challenge_helper(struct sip_msg *msg, str *realm, int flags, int hftype,
		str *res)
{
    int ret, stale;
    str hf = {0, 0};
	struct qp *qop = NULL;

	ret = -1;

	if(flags&2) {
		qop = &auth_qauthint;
	} else if(flags&1) {
		qop = &auth_qauth;
	}
	if (flags & 16) {
	    stale = 1;
	} else {
	    stale = 0;
	}
	if (get_challenge_hf(msg, stale, realm, NULL, NULL, qop, hftype, &hf)
	    < 0) {
		ERR("Error while creating challenge\n");
		ret = -2;
		goto error;
	}
	
	ret = 1;
	if(res!=NULL)
	{
		*res = hf;
		return ret;
	}
	switch(hftype) {
		case HDR_AUTHORIZATION_T:
			if(auth_send_reply(msg, 401, "Unauthorized",
						hf.s, hf.len) <0 )
				ret = -3;
		break;
		case HDR_PROXYAUTH_T:
			if(auth_send_reply(msg, 407, "Proxy Authentication Required",
						hf.s, hf.len) <0 )
				ret = -3;
		break;
	}
	if(hf.s) pkg_free(hf.s);
	return ret;

error:
	if(hf.s) pkg_free(hf.s);
	if(!(flags&4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) <0 )
			ret = -4;
	}
	return ret;
}

/**
 *
 */
int auth_challenge(struct sip_msg *msg, str *realm, int flags, int hftype)
{
	return auth_challenge_helper(msg, realm, flags, hftype, NULL);
}

/**
 *
 */
static int proxy_challenge(struct sip_msg *msg, char* realm, char *flags)
{
	int vflags = 0;
	str srealm  = {0, 0};

	if (get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if (get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	return auth_challenge(msg, &srealm, vflags, HDR_PROXYAUTH_T);

error:
	if(!(vflags&4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) <0 )
			return -4;
	}
	return -1;
}

/**
 *
 */
static int www_challenge(struct sip_msg *msg, char* realm, char *flags)
{
	int vflags = 0;
	str srealm  = {0, 0};

	if (get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if (get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	return auth_challenge(msg, &srealm, vflags, HDR_AUTHORIZATION_T);

error:
	if(!(vflags&4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) <0 )
			return -4;
	}
	return -1;
}

/**
 *
 */
static int w_auth_challenge(struct sip_msg *msg, char* realm, char *flags)
{
	int vflags = 0;
	str srealm  = {0, 0};

	if((msg->REQ_METHOD == METHOD_ACK) || (msg->REQ_METHOD == METHOD_CANCEL)) {
		return 1;
	}

	if(get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	if(msg->REQ_METHOD==METHOD_REGISTER)
		return auth_challenge(msg, &srealm, vflags, HDR_AUTHORIZATION_T);
	else
		return auth_challenge(msg, &srealm, vflags, HDR_PROXYAUTH_T);

error:
	if(!(vflags&4)) {
		if(auth_send_reply(msg, 500, "Internal Server Error", 0, 0) <0 )
			return -4;
	}
	return -1;
}


/**
 * @brief fixup function for {www,proxy}_challenge
 */
static int fixup_auth_challenge(void **param, int param_no)
{
	if(strlen((char*)*param)<=0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
			return fixup_var_str_12(param, 1);
		case 2:
			return fixup_var_int_12(param, 1);
	}
	return 0;
}


/**
 *
 */
static int w_auth_get_www_authenticate(sip_msg_t* msg, char* realm,
		char *flags, char *dst)
{
	int vflags = 0;
	str srealm  = {0};
	str hf = {0};
	pv_spec_t *pv;
	pv_value_t val;
	int ret;

	if(get_str_fparam(&srealm, msg, (fparam_t*)realm) < 0) {
		LM_ERR("failed to get realm value\n");
		goto error;
	}

	if(srealm.len==0) {
		LM_ERR("invalid realm value - empty content\n");
		goto error;
	}

	if(get_int_fparam(&vflags, msg, (fparam_t*)flags) < 0) {
		LM_ERR("invalid flags value\n");
		goto error;
	}

	pv = (pv_spec_t *)dst;

	ret = auth_challenge_helper(NULL, &srealm, vflags,
			HDR_AUTHORIZATION_T, &hf);

	if(ret<0)
		return ret;

	val.rs.s = pv_get_buffer();
	val.rs.len = 0;
	if(hf.s!=NULL)
	{
		memcpy(val.rs.s, hf.s, hf.len);
		val.rs.len = hf.len;
		val.rs.s[val.rs.len] = '\0';
		pkg_free(hf.s);
	}
	val.flags = PV_VAL_STR;
	pv->setf(msg, &pv->pvp, (int)EQ_T, &val);

	return ret;

error:
	return -1;
}


static int fixup_auth_get_www_authenticate(void **param, int param_no)
{
	if(strlen((char*)*param)<=0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	switch(param_no) {
		case 1:
			return fixup_var_str_12(param, 1);
		case 2:
			return fixup_var_int_12(param, 1);
		case 3:
		if (fixup_pvar_null(param, 1) != 0) {
		    LM_ERR("failed to fixup result pvar\n");
		    return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
		    LM_ERR("result pvar is not writeble\n");
		    return -1;
		}
		return 0;
	}
	return 0;
}
