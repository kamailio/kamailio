/*
 * TLS module
 *
 * Copyright (C) 2007 iptelorg GmbH 
 * Copyright (C) Motorola Solutions, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/** Kamailio TLS support :: Module interface.
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
#include "../../shm_init.h"
#include "../../rpc_lookup.h"
#include "../../cfg/cfg.h"
#include "../../dprint.h"
#include "tls_init.h"
#include "tls_server.h"
#include "tls_domain.h"
#include "tls_select.h"
#include "tls_config.h"
#include "tls_rpc.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_cfg.h"

#ifndef TLS_HOOKS
	#error "TLS_HOOKS must be defined, or the tls module won't work"
#endif
#ifdef CORE_TLS
	#error "conflict: CORE_TLS must _not_ be defined"
#endif

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


str sr_tls_xavp_cfg = {0, 0};
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
	STR_STATIC_INIT(TLS_CRL_FILE), /* Certificate revocation list */
	{0, 0},           /* Server name (SNI) */
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
	STR_STATIC_INIT(TLS_CRL_FILE), /* Certificate revocation list */
	{0, 0},           /* Server name (SNI) */
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
	{0, 0}, /* Certificate revocation list */
	{0, 0},           /* Server name (SNI) */
	0                 /* next */
};



/* Current TLS configuration */
tls_domains_cfg_t** tls_domains_cfg = NULL;

/* List lock, used by garbage collector */
gen_lock_t* tls_domains_cfg_lock = NULL;


int sr_tls_renegotiation = 0;

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
	{"tls_method",          PARAM_STR,    &default_tls_cfg.method       },
	{"server_name",         PARAM_STR,    &default_tls_cfg.server_name  },
	{"verify_certificate",  PARAM_INT,    &default_tls_cfg.verify_cert  },
	{"verify_depth",        PARAM_INT,    &default_tls_cfg.verify_depth },
	{"require_certificate", PARAM_INT,    &default_tls_cfg.require_cert },
	{"private_key",         PARAM_STR,    &default_tls_cfg.private_key  },
	{"ca_list",             PARAM_STR,    &default_tls_cfg.ca_list      },
	{"certificate",         PARAM_STR,    &default_tls_cfg.certificate  },
	{"crl",                 PARAM_STR,    &default_tls_cfg.crl          },
	{"cipher_list",         PARAM_STR,    &default_tls_cfg.cipher_list  },
	{"connection_timeout",  PARAM_INT,    &default_tls_cfg.con_lifetime },
	{"tls_log",             PARAM_INT,    &default_tls_cfg.log          },
	{"tls_debug",           PARAM_INT,    &default_tls_cfg.debug        },
	{"session_cache",       PARAM_INT,    &default_tls_cfg.session_cache},
	{"session_id",          PARAM_STR,    &default_tls_cfg.session_id   },
	{"config",              PARAM_STR,    &default_tls_cfg.config_file  },
	{"tls_disable_compression", PARAM_INT,
										 &default_tls_cfg.disable_compression},
	{"ssl_release_buffers",   PARAM_INT, &default_tls_cfg.ssl_release_buffers},
	{"ssl_freelist_max_len",  PARAM_INT,  &default_tls_cfg.ssl_freelist_max},
	{"ssl_max_send_fragment", PARAM_INT,
									   &default_tls_cfg.ssl_max_send_fragment},
	{"ssl_read_ahead",        PARAM_INT,    &default_tls_cfg.ssl_read_ahead},
	{"send_close_notify",   PARAM_INT,    &default_tls_cfg.send_close_notify},
	{"con_ct_wq_max",      PARAM_INT,    &default_tls_cfg.con_ct_wq_max},
	{"ct_wq_max",          PARAM_INT,    &default_tls_cfg.ct_wq_max},
	{"ct_wq_blk_size",     PARAM_INT,    &default_tls_cfg.ct_wq_blk_size},
	{"tls_force_run",       PARAM_INT,    &default_tls_cfg.force_run},
	{"low_mem_threshold1",  PARAM_INT,    &default_tls_cfg.low_mem_threshold1},
	{"low_mem_threshold2",  PARAM_INT,    &default_tls_cfg.low_mem_threshold2},
	{"renegotiation",       PARAM_INT,    &sr_tls_renegotiation},
	{"xavp_cfg",            PARAM_STR,    &sr_tls_xavp_cfg},
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
	tls_encode_f,
	tls_h_tcpconn_init,
	tls_h_tcpconn_clean,
	tls_h_close,
	tls_h_init_si,
	init_tls_h,
	destroy_tls_h,
	tls_mod_pre_init_h,
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

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if (tls_disable) {
		LOG(L_WARN, "tls support is disabled "
				"(set enable_tls=1 in the config to enable it)\n");
		return 0;
	}

	/* shm is used, be sure it is initialized */
	if(!shm_initialized() && init_shm()<0)
		return -1;

	if(tls_pre_init()<0)
		return -1;

	register_tls_hooks(&tls_h);

	return 0;
}

static int mod_init(void)
{
	int method;

	if (tls_disable){
		LOG(L_WARN, "tls support is disabled "
				"(set enable_tls=1 in the config to enable it)\n");
		return 0;
	}
	if (fix_tls_cfg(&default_tls_cfg) < 0 ) {
		ERR("initial tls configuration fixup failed\n");
		return -1;
	}
	/* declare configuration */
	if (cfg_declare("tls", tls_cfg_def, &default_tls_cfg,
							cfg_sizeof(tls), (void **)&tls_cfg)) {
		ERR("failed to register the configuration\n");
		return -1;
	}
	/* Convert tls_method parameter to integer */
	method = tls_parse_method(&cfg_get(tls, tls_cfg, method));
	if (method < 0) {
		ERR("Invalid tls_method parameter value\n");
		return -1;
	}
	/* fill mod_params */
	mod_params.method = method;
	mod_params.verify_cert = cfg_get(tls, tls_cfg, verify_cert);
	mod_params.verify_depth = cfg_get(tls, tls_cfg, verify_depth);
	mod_params.require_cert = cfg_get(tls, tls_cfg, require_cert);
	mod_params.pkey_file = cfg_get(tls, tls_cfg, private_key);
	mod_params.ca_file = cfg_get(tls, tls_cfg, ca_list);
	mod_params.crl_file = cfg_get(tls, tls_cfg, crl);
	mod_params.cert_file = cfg_get(tls, tls_cfg, certificate);
	mod_params.cipher_list = cfg_get(tls, tls_cfg, cipher_list);
	mod_params.server_name = cfg_get(tls, tls_cfg, server_name);

	tls_domains_cfg =
			(tls_domains_cfg_t**)shm_malloc(sizeof(tls_domains_cfg_t*));
	if (!tls_domains_cfg) {
		ERR("Not enough shared memory left\n");
		goto error;
	}
	*tls_domains_cfg = NULL;

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
	if (cfg_get(tls, tls_cfg, config_file).s) {
		*tls_domains_cfg = 
			tls_load_config(&cfg_get(tls, tls_cfg, config_file));
		if (!(*tls_domains_cfg)) goto error;
	} else {
		*tls_domains_cfg = tls_new_cfg();
		if (!(*tls_domains_cfg)) goto error;
	}

	if (tls_check_sockets(*tls_domains_cfg) < 0)
		goto error;

#ifndef OPENSSL_NO_ECDH
	LM_INFO("With ECDH-Support!\n");
#endif
#ifndef OPENSSL_NO_DH
	LM_INFO("With Diffie Hellman\n");
#endif
	tls_lookup_event_routes();
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
		if (cfg_get(tls, tls_cfg, config_file).s){
			if (tls_fix_domains_cfg(*tls_domains_cfg,
									&srv_defaults, &cli_defaults) < 0)
				return -1;
		}else{
			if (tls_fix_domains_cfg(*tls_domains_cfg,
									&mod_params, &mod_params) < 0)
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

	c = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0,
					cfg_get(tls, tls_cfg, con_lifetime));
	if (!c) {
		ERR("connection no longer exits\n");
		return -1;
	}

	if(c->type != PROTO_TLS) {
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
		LM_INFO("tlsops:is_peer_verified: WARNING: peer did not present "
			"a certificate. Thus it could not be verified... return -1\n");
		tcpconn_put(c);
		return -1;
	}

	X509_free(x509_cert);

	tcpconn_put(c);

	LM_DBG("tlsops:is_peer_verified: peer is successfully verified"
		"...done\n");
	return 1;
}
