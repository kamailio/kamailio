/*
 * $Id$
 *
 * TLS module - module interface
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-06: db connection closed in mod_init (janakj)
 * 2004-06-06  updated to the new DB api, cleanup: static dbf & handler,
 *              calls to domain_db_{bind,init,close,ver} (andrei)
 * 2007-02-09  updated to the new tls_hooks api and renamed tls hooks hanlder
 *              functions to avoid conflicts: s/tls_/tls_h_/   (andrei)
 * 2010-03-19  new parameters to control advanced openssl lib options
 *              (mostly work on 1.0.0+): ssl_release_buffers, ssl_read_ahead,
 *              ssl_freelist_max_len, ssl_max_send_fragment   (andrei)
 */
/** SIP-router TLS support :: Module interface.
 * @file
 * @ingroup tls
 * Module: @ref tls
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../../locking.h"
#include "../../sr_module.h"
#include "../../ip_addr.h"
#include "../../trim.h"
#include "../../globals.h"
#include "../../timer_ticks.h"
#include "../../timer.h" /* ticks_t */
#include "../../tls_hooks.h"
#include "../../ut.h"
#include "../../rpc_lookup.h"
#include "tls_init.h"
#include "tls_server.h"
#include "tls_domain.h"
#include "tls_select.h"
#include "tls_config.h"
#include "tls_rpc.h"
#include "tls_util.h"
#include "tls_mod.h"

#ifndef TLS_HOOKS
	#error "TLS_HOOKS must be defined, or the tls module won't work"
#endif
#ifdef CORE_TLS
	#error "conflict: CORE_TLS must _not_ be defined"
#endif


/* maximum accepted lifetime (maximum possible is  ~ MAXINT/2)
 *  (it should be kept in sync w/ MAX_TCP_CON_LIFETIME from tcp_main.c:
 *   MAX_TLS_CON_LIFETIME <= MAX_TCP_CON_LIFETIME )*/
#define MAX_TLS_CON_LIFETIME	(1U<<(sizeof(ticks_t)*8-1))



/*
 * FIXME:
 * - How do we ask for secret key password ? Mod_init is called after
 *   daemonize and thus has no console access
 * - forward_tls and t_relay_to_tls should be here
 * add tls_log
 * - Currently it is not possible to reset certificate in a domain,
 *   for example if you specify client certificate in the default client
 *   domain then there is no way to define another client domain which would
 *   have no client certificate configured
 */


/*
 * Module management function prototypes
 */
static int mod_init(void);
static int mod_child(int rank);
static void destroy(void);

static int is_peer_verified(struct sip_msg* msg, char* foo, char* foo2);

MODULE_VERSION


/*
 * Default settings when modparams are used 
 */
static tls_domain_t mod_params = {
	TLS_DOMAIN_DEF | TLS_DOMAIN_SRV,   /* Domain Type */
	{},               /* IP address */
	0,                /* Port number */
	0,                /* SSL ctx */
	STR_STATIC_INIT(TLS_CERT_FILE),    /* Certificate file */
	STR_STATIC_INIT(TLS_PKEY_FILE),    /* Private key file */
	0,                /* Verify certificate */
	9,                /* Verify depth */
	STR_STATIC_INIT(TLS_CA_FILE),      /* CA file */
	0,                /* Require certificate */
	{0, },                /* Cipher list */
	TLS_USE_TLSv1,    /* TLS method */
	0                 /* next */
};


/*
 * Default settings for server domains when using external config file
 */
tls_domain_t srv_defaults = {
	TLS_DOMAIN_DEF | TLS_DOMAIN_SRV,   /* Domain Type */
	{},               /* IP address */
	0,                /* Port number */
	0,                /* SSL ctx */
	STR_STATIC_INIT(TLS_CERT_FILE),    /* Certificate file */
	STR_STATIC_INIT(TLS_PKEY_FILE),    /* Private key file */
	0,                /* Verify certificate */
	9,                /* Verify depth */
	STR_STATIC_INIT(TLS_CA_FILE),      /* CA file */
	0,                /* Require certificate */
	{0, 0},                /* Cipher list */
	TLS_USE_TLSv1,    /* TLS method */
	0                 /* next */
};


/*
 * Default settings for client domains when using external config file
 */
tls_domain_t cli_defaults = {
	TLS_DOMAIN_DEF | TLS_DOMAIN_CLI,   /* Domain Type */
	{},               /* IP address */
	0,                /* Port number */
	0,                /* SSL ctx */
	{0, 0},                /* Certificate file */
	{0, 0},                /* Private key file */
	0,                /* Verify certificate */
	9,                /* Verify depth */
	STR_STATIC_INIT(TLS_CA_FILE),      /* CA file */
	0,                /* Require certificate */
	{0, 0},                /* Cipher list */
	TLS_USE_TLSv1,    /* TLS method */
	0                 /* next */
};


/*
 * Defaults for client and server domains when using modparams
 */
static str tls_method = STR_STATIC_INIT("TLSv1");


int tls_handshake_timeout = 30;
int tls_send_timeout = 30;
int tls_con_lifetime = 600; /* this value will be adjusted to ticks later */
int tls_log = 3;
int tls_session_cache = 0;
str tls_session_id = STR_STATIC_INIT("ser-tls-2.1.0");
/* release internal openssl read or write buffer when they are no longer used
 * (complete read or write that does not have to buffer anything).
 * Should be used together with tls_free_list_max_len. Might have some
 * performance impact (and extra *malloc pressure), but has also the potential
 * of saving a lot of memory (at least 32k/idle connection in the default
 * config, or ~ 16k+tls_max_send_fragment)) */
int ssl_mode_release_buffers = -1; /* don't set, leave the default (off) */
/* maximum length of free/unused memory buffers/chunks per connection.
 * Setting it to 0 would cause any unused buffers to be immediately freed
 * and hence a lower memory footprint (at the cost of a possible performance
 * decrease and more *malloc pressure).
 * Too large value would result in extra memory consumption.
 * The default is 32 in openssl.
 * For lowest memory usage set it to 0 and tls_mode_release_buffers to 1
 */
int ssl_freelist_max_len = -1;   /* don't set, leave the default value (32) */
/* maximum number of bytes (clear text) sent into one record.
 * The default and maximum value are ~16k. Lower values would lead to a lower
 *  memory footprint. 
 * Values lower then the typical  app. write size might decrease performance 
 * (extra write() syscalls), so it should be kept ~2k for ser.
 */
int ssl_max_send_fragment = -1;  /* don't set, leave the default (16k) */
/* enable read ahead. Should increase performance (1 less syscall when
 * enabled, else openssl makes 1 read() for each record header and another
 * for the content), but might interact with SSL_pending() (not used right now)
 */
int ssl_read_ahead = 1; /* set (use -1 for the default value) */

str tls_domains_cfg_file = STR_NULL;


/* Current TLS configuration */
tls_domains_cfg_t** tls_domains_cfg = NULL;

/* List lock, used by garbage collector */
gen_lock_t* tls_domains_cfg_lock = NULL;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_peer_verified", (cmd_function)is_peer_verified,   0, 0, 0,
			REQUEST_ROUTE},
	{0,0,0,0,0,0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"tls_method",          PARAM_STR,    &tls_method             },
	{"verify_certificate",  PARAM_INT,    &mod_params.verify_cert },
	{"verify_depth",        PARAM_INT,    &mod_params.verify_depth},
	{"require_certificate", PARAM_INT,    &mod_params.require_cert},
	{"private_key",         PARAM_STR,    &mod_params.pkey_file   },
	{"ca_list",             PARAM_STR,    &mod_params.ca_file     },
	{"certificate",         PARAM_STR,    &mod_params.cert_file   },
	{"cipher_list",         PARAM_STR,    &mod_params.cipher_list },
	{"handshake_timeout",   PARAM_INT,    &tls_handshake_timeout  },
	{"send_timeout",        PARAM_INT,    &tls_send_timeout       },
	{"connection_timeout",  PARAM_INT,    &tls_con_lifetime       },
	{"tls_log",             PARAM_INT,    &tls_log                },
	{"session_cache",       PARAM_INT,    &tls_session_cache      },
	{"session_id",          PARAM_STR,    &tls_session_id         },
	{"config",              PARAM_STR,    &tls_domains_cfg_file   },
	{"tls_disable_compression", PARAM_INT,&tls_disable_compression},
	{"ssl_release_buffers",   PARAM_INT, &ssl_mode_release_buffers},
	{"ssl_freelist_max_len",  PARAM_INT,    &ssl_freelist_max_len},
	{"ssl_max_send_fragment", PARAM_INT,    &ssl_max_send_fragment},
	{"ssl_read_ahead",        PARAM_INT,    &ssl_read_ahead},
	{"tls_force_run",       PARAM_INT,    &tls_force_run},
	{"low_mem_threshold1",  PARAM_INT,    &openssl_mem_threshold1},
	{"low_mem_threshold2",  PARAM_INT,    &openssl_mem_threshold2},
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"tls", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	0,           /* exported statistics */
	0,           /* exported MI functions */
	tls_pv,      /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,    /* module initialization function */
	0,           /* response function */
	destroy,     /* destroy function */
	mod_child    /* child initialization function */
};



static struct tls_hooks tls_h = {
	tls_read_f,
	tls_do_send_f,
	tls_1st_send_f,
	tls_h_tcpconn_init,
	tls_h_tcpconn_clean,
	tls_h_close,
	tls_h_init_si,
	init_tls_h,
	destroy_tls_h
};



#if 0
/*
 * Create TLS configuration from modparams
 */
static tls_domains_cfg_t* tls_use_modparams(void)
{
	tls_domains_cfg_t* ret;
	
	ret = tls_new_cfg();
	if (!ret) return;

	
}
#endif


static int fix_rel_pathnames(void)
{
	if (tls_domains_cfg_file.s) {
		tls_domains_cfg_file.s = get_abs_pathname(NULL, &tls_domains_cfg_file);
		if (tls_domains_cfg_file.s == NULL) return -1;
		tls_domains_cfg_file.len = strlen(tls_domains_cfg_file.s);
	}
	
	if (mod_params.pkey_file.s) {
		mod_params.pkey_file.s = get_abs_pathname(NULL, &mod_params.pkey_file);
		if (mod_params.pkey_file.s == NULL) return -1;
		mod_params.pkey_file.len = strlen(mod_params.pkey_file.s);
	}
	
	if (mod_params.ca_file.s) {
		mod_params.ca_file.s = get_abs_pathname(NULL, &mod_params.ca_file);
		if (mod_params.ca_file.s == NULL) return -1;
		mod_params.ca_file.len = strlen(mod_params.ca_file.s);
	}
	
	if (mod_params.cert_file.s) {
		mod_params.cert_file.s = get_abs_pathname(NULL, &mod_params.cert_file);
		if (mod_params.cert_file.s == NULL) return -1;
		mod_params.cert_file.len = strlen(mod_params.cert_file.s);
	}
	
	return 0;
}

static int mod_init(void)
{
	int method;

	if (tls_disable){
		LOG(L_WARN, "WARNING: tls: mod_init: tls support is disabled "
				"(set enable_tls=1 in the config to enable it)\n");
		return 0;
	}

/*
	if (cfg_get(tcp, tcp_cfg, async) && !tls_force_run){
		ERR("tls does not support tcp in async mode, please use"
				" tcp_async=no in the config file\n");
		return -1;
	}
*/
	     /* Convert tls_method parameter to integer */
	method = tls_parse_method(&tls_method);
	if (method < 0) {
		ERR("Invalid tls_method parameter value\n");
		return -1;
	}
	mod_params.method = method;

	/* Update relative paths of files configured through modparams, relative
	 * pathnames will be converted to absolute and the directory of the main
	 * SER configuration file will be used as reference.
	 */
	if (fix_rel_pathnames() < 0) return -1;

	tls_domains_cfg = 
		(tls_domains_cfg_t**)shm_malloc(sizeof(tls_domains_cfg_t*));
	if (!tls_domains_cfg) {
		ERR("Not enough shared memory left\n");
		goto error;
	}
	*tls_domains_cfg = NULL;

	register_tls_hooks(&tls_h);
	register_select_table(tls_sel);
	/* register the rpc interface */
	if (rpc_register_array(tls_rpc)!=0) {
		LOG(L_ERR, "failed to register RPC commands\n");
		goto error;
	}

	 /* if (init_tls() < 0) return -1; */
	
	tls_domains_cfg_lock = lock_alloc();
	if (tls_domains_cfg_lock == 0) {
		ERR("Unable to create TLS configuration lock\n");
		goto error;
	}
	if (lock_init(tls_domains_cfg_lock) == 0) {
		lock_dealloc(tls_domains_cfg_lock);
		ERR("Unable to initialize TLS configuration lock\n");
		goto error;
	}
	if (tls_ct_wq_init() < 0) {
		ERR("Unable to initialize TLS buffering\n");
		goto error;
	}
	if (tls_domains_cfg_file.s) {
		*tls_domains_cfg = tls_load_config(&tls_domains_cfg_file);
		if (!(*tls_domains_cfg)) goto error;
	} else {
		*tls_domains_cfg = tls_new_cfg();
		if (!(*tls_domains_cfg)) goto error;
	}

	if (tls_check_sockets(*tls_domains_cfg) < 0)
		goto error;

	/* fix the timeouts from s to ticks */
	if (tls_con_lifetime<0){
		/* set to max value (~ 1/2 MAX_INT) */
		tls_con_lifetime=MAX_TLS_CON_LIFETIME;
	}else{
		if ((unsigned)tls_con_lifetime > 
				(unsigned)TICKS_TO_S(MAX_TLS_CON_LIFETIME)){
			LOG(L_WARN, "tls: mod_init: tls_con_lifetime too big (%u s), "
					" the maximum value is %u\n", tls_con_lifetime,
					TICKS_TO_S(MAX_TLS_CON_LIFETIME));
			tls_con_lifetime=MAX_TLS_CON_LIFETIME;
		}else{
			tls_con_lifetime=S_TO_TICKS(tls_con_lifetime);
		}
	}
	return 0;
error:
	destroy_tls_h();
	return -1;
}


static int mod_child(int rank)
{
	if (tls_disable || (tls_domains_cfg==0))
		return 0;
	/* fix tls config only from the main proc/PROC_INIT., when we know 
	 * the exact process number and before any other process starts*/
	if (rank == PROC_INIT){
		if (tls_domains_cfg_file.s){
			if (tls_fix_cfg(*tls_domains_cfg, &srv_defaults,
							&cli_defaults) < 0)
				return -1;
		}else{
			if (tls_fix_cfg(*tls_domains_cfg, &mod_params, &mod_params) < 0)
				return -1;
		}
	}
	return 0;
}


static void destroy(void)
{
	/* tls is destroyed via the registered destroy_tls_h callback
	   => nothing to do here */
}


static int is_peer_verified(struct sip_msg* msg, char* foo, char* foo2)
{
	struct tcp_connection *c;
	SSL *ssl;
	long ssl_verify;
	X509 *x509_cert;

	DBG("started...\n");
	if (msg->rcv.proto != PROTO_TLS) {
		ERR("proto != TLS --> peer can't be verified, return -1\n");
		return -1;
	}

	DBG("trying to find TCP connection of received message...\n");

	c = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, tls_con_lifetime);
	if (c && c->type != PROTO_TLS) {
		ERR("Connection found but is not TLS\n");
		tcpconn_put(c);
		return -1;
	}

	if (!c->extra_data) {
		LM_ERR("no extra_data specified in TLS/TCP connection found."
				" This should not happen... return -1\n");
		tcpconn_put(c);
		return -1;
	}

	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;

	ssl_verify = SSL_get_verify_result(ssl);
	if ( ssl_verify != X509_V_OK ) {
		LM_WARN("verification of presented certificate failed... return -1\n");
		tcpconn_put(c);
		return -1;
	}

	/* now, we have only valid peer certificates or peers without certificates.
	 * Thus we have to check for the existence of a peer certificate
	 */
	x509_cert = SSL_get_peer_certificate(ssl);
	if ( x509_cert == NULL ) {
		LM_WARN("tlsops:is_peer_verified: WARNING: peer did not presented "
			"a certificate. Thus it could not be verified... return -1\n");
		tcpconn_put(c);
		return -1;
	}

	X509_free(x509_cert);

	tcpconn_put(c);

	LM_DBG("tlsops:is_peer_verified: peer is successfuly verified"
		"...done\n");
	return 1;
}
