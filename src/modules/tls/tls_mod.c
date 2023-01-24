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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../../core/locking.h"
#include "../../core/sr_module.h"
#include "../../core/ip_addr.h"
#include "../../core/trim.h"
#include "../../core/globals.h"
#include "../../core/timer_ticks.h"
#include "../../core/timer.h" /* ticks_t */
#include "../../core/tls_hooks.h"
#include "../../core/ut.h"
#include "../../core/shm_init.h"
#include "../../core/rpc_lookup.h"
#include "../../core/cfg/cfg.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "tls_init.h"
#include "tls_server.h"
#include "tls_domain.h"
#include "tls_select.h"
#include "tls_config.h"
#include "tls_rpc.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_cfg.h"
#include "tls_rand.h"

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

static int w_is_peer_verified(struct sip_msg* msg, char* p1, char* p2);
static int w_tls_set_connect_server_id(sip_msg_t* msg, char* psrvid, char* p2);

int ksr_rand_engine_param(modparam_t type, void* val);

MODULE_VERSION


extern str sr_tls_event_callback;
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
	STR_STATIC_INIT(TLS_CA_PATH),      /* CA path */
	0,                /* Require certificate */
	{0, 0},                /* Cipher list */
	TLS_USE_TLSv1_PLUS,   /* TLS method */
	STR_STATIC_INIT(TLS_CRL_FILE), /* Certificate revocation list */
	{0, 0},           /* Server name (SNI) */
	0,                /* Server name (SNI) mode */
	{0, 0},           /* Server id */
	TLS_VERIFY_CLIENT_OFF,             /* Verify client */
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
	STR_STATIC_INIT(TLS_CA_PATH),      /* CA path */
	0,                /* Require certificate */
	{0, 0},                /* Cipher list */
	TLS_USE_TLSv1_PLUS,    /* TLS method */
	STR_STATIC_INIT(TLS_CRL_FILE), /* Certificate revocation list */
	{0, 0},           /* Server name (SNI) */
	0,                /* Server name (SNI) mode */
	{0, 0},           /* Server id */
	TLS_VERIFY_CLIENT_OFF,             /* Verify client */
	0                 /* next */
};


#ifndef OPENSSL_NO_ENGINE

typedef struct tls_engine {
        str engine;
        str engine_config;
        str engine_algorithms;
} tls_engine_t;
#include <openssl/conf.h>
#include <openssl/engine.h>

static ENGINE *ksr_tls_engine;
static tls_engine_t tls_engine_settings = {
	STR_STATIC_INIT("NONE"),
	STR_STATIC_INIT("NONE"),
	STR_STATIC_INIT("ALL"),
};
#endif /* OPENSSL_NO_ENGINE */
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
	STR_STATIC_INIT(TLS_CA_PATH),      /* CA path */
	0,                /* Require certificate */
	{0, 0},                /* Cipher list */
	TLS_USE_TLSv1_PLUS,    /* TLS method */
	{0, 0}, /* Certificate revocation list */
	{0, 0},           /* Server name (SNI) */
	0,                /* Server name (SNI) mode */
	{0, 0},           /* Server id */
	TLS_VERIFY_CLIENT_OFF,             /* Verify client */
	0                 /* next */
};



/* Current TLS configuration */
tls_domains_cfg_t** tls_domains_cfg = NULL;

/* List lock, used by garbage collector */
gen_lock_t* tls_domains_cfg_lock = NULL;


int sr_tls_renegotiation = 0;
int ksr_tls_init_mode = 0;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_peer_verified", (cmd_function)w_is_peer_verified,   0, 0, 0,
			REQUEST_ROUTE},
	{"tls_set_connect_server_id", (cmd_function)w_tls_set_connect_server_id,
		1, fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
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
	{"verify_client",       PARAM_STR,    &default_tls_cfg.verify_client},
	{"private_key",         PARAM_STR,    &default_tls_cfg.private_key  },
	{"ca_list",             PARAM_STR,    &default_tls_cfg.ca_list      },
	{"ca_path",             PARAM_STR,    &default_tls_cfg.ca_path      },
	{"certificate",         PARAM_STR,    &default_tls_cfg.certificate  },
	{"crl",                 PARAM_STR,    &default_tls_cfg.crl          },
	{"cipher_list",         PARAM_STR,    &default_tls_cfg.cipher_list  },
	{"connection_timeout",  PARAM_INT,    &default_tls_cfg.con_lifetime },
#ifndef OPENSSL_NO_ENGINE
	{"engine",              PARAM_STR,    &tls_engine_settings.engine  },
	{"engine_config",       PARAM_STR,    &tls_engine_settings.engine_config },
	{"engine_algorithms",   PARAM_STR,    &tls_engine_settings.engine_algorithms },
#endif /* OPENSSL_NO_ENGINE */
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
	{"event_callback",      PARAM_STR,    &sr_tls_event_callback},
	{"rand_engine",         PARAM_STR|USE_FUNC_PARAM, (void*)ksr_rand_engine_param},
	{"init_mode",           PARAM_INT,    &ksr_tls_init_mode},

	{0, 0, 0}
};

#ifndef MOD_NAME
#define MOD_NAME "tls"
#endif

/*
 * Module interface
 */
struct module_exports exports = {
	MOD_NAME,        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* exported rpc command */
	tls_pv,          /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module init function */
	mod_child,       /* child initi function */
	destroy          /* destroy function */
};



static struct tls_hooks tls_h = {
	tls_h_read_f,
	tls_h_encode_f,
	tls_h_tcpconn_init_f,
	tls_h_tcpconn_clean_f,
	tls_h_tcpconn_close_f,
	tls_h_init_si_f,
	tls_h_mod_init_f,
	tls_h_mod_destroy_f,
	tls_h_mod_pre_init_f,
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


static int mod_init(void)
{
	int method;
	int verify_client;

	if (tls_disable){
		LM_WARN("tls support is disabled "
				"(set enable_tls=1 in the config to enable it)\n");
		return 0;
	}
	if (fix_tls_cfg(&default_tls_cfg) < 0 ) {
		LM_ERR("initial tls configuration fixup failed\n");
		return -1;
	}
	/* declare configuration */
	if (cfg_declare("tls", tls_cfg_def, &default_tls_cfg,
							cfg_sizeof(tls), (void **)&tls_cfg)) {
		LM_ERR("failed to register the configuration\n");
		return -1;
	}
	/* Convert tls_method parameter to integer */
	method = tls_parse_method(&cfg_get(tls, tls_cfg, method));
	if (method < 0) {
		LM_ERR("Invalid tls_method parameter value\n");
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
	/* Convert verify_client parameter to integer */
	verify_client = tls_parse_verify_client(&cfg_get(tls, tls_cfg, verify_client));
	if (verify_client < 0) {
		LM_ERR("Invalid tls_method parameter value\n");
		return -1;
	}
	mod_params.verify_client = verify_client;

	tls_domains_cfg =
			(tls_domains_cfg_t**)shm_malloc(sizeof(tls_domains_cfg_t*));
	if (!tls_domains_cfg) {
		LM_ERR("Not enough shared memory left\n");
		goto error;
	}
	*tls_domains_cfg = NULL;

	register_select_table(tls_sel);
	/* register the rpc interface */
	if (rpc_register_array(tls_rpc)!=0) {
		LM_ERR("failed to register RPC commands\n");
		goto error;
	}

	/* if (init_tls() < 0) return -1; */

	tls_domains_cfg_lock = lock_alloc();
	if (tls_domains_cfg_lock == 0) {
		LM_ERR("Unable to create TLS configuration lock\n");
		goto error;
	}
	if (lock_init(tls_domains_cfg_lock) == 0) {
		lock_dealloc(tls_domains_cfg_lock);
		ERR("Unable to initialize TLS configuration lock\n");
		goto error;
	}
	if (tls_ct_wq_init() < 0) {
		LM_ERR("Unable to initialize TLS buffering\n");
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

	if (ksr_tls_lock_init() < 0) {
		goto error;
	}

	LM_INFO("use OpenSSL version: %08x\n", (uint32_t)(OPENSSL_VERSION_NUMBER));
#ifndef OPENSSL_NO_ECDH
	LM_INFO("With ECDH-Support!\n");
#endif
#ifndef OPENSSL_NO_DH
	LM_INFO("With Diffie Hellman\n");
#endif
	if(sr_tls_event_callback.s==NULL || sr_tls_event_callback.len<=0) {
		tls_lookup_event_routes();
	}
	return 0;
error:
	tls_h_mod_destroy_f();
	return -1;
}


#ifndef OPENSSL_NO_ENGINE
static int tls_engine_init();
int tls_fix_engine_keys(tls_domains_cfg_t*, tls_domain_t*, tls_domain_t*);
#endif
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
#ifndef OPENSSL_NO_ENGINE
	/*
	 * after the child is fork()ed we go through the TLS domains
	 * and fix up private keys from engine
	 */
	if (!strncmp(tls_engine_settings.engine.s, "NONE", 4)) return 0;

	if (rank > 0) {
		if (tls_engine_init() < 0)
			return -1;
		if (tls_fix_engine_keys(*tls_domains_cfg, &srv_defaults, &cli_defaults)<0)
			return -1;
		LM_INFO("OpenSSL Engine loaded private keys in child: %d\n", rank);
	}
#endif
	return 0;
}


static void destroy(void)
{
	/* tls is destroyed via the registered destroy_tls_h callback
	 *   => nothing to do here */
}


int ksr_rand_engine_param(modparam_t type, void* val)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	str *reng;

	if(val==NULL) {
		return -1;
	}
	reng = (str*)val;
	LM_DBG("random engine: %.*s\n", reng->len, reng->s);
	if(reng->len == 5 && strncasecmp(reng->s, "krand", 5) == 0) {
		LM_DBG("setting krand random engine\n");
		RAND_set_rand_method(RAND_ksr_krand_method());
	} else if(reng->len == 8 && strncasecmp(reng->s, "fastrand", 8) == 0) {
		LM_DBG("setting fastrand random engine\n");
		RAND_set_rand_method(RAND_ksr_fastrand_method());
	} else if (reng->len == 10 && strncasecmp(reng->s, "cryptorand", 10) == 0) {
		LM_DBG("setting cryptorand random engine\n");
		RAND_set_rand_method(RAND_ksr_cryptorand_method());
	} else if (reng->len == 8 && strncasecmp(reng->s, "kxlibssl", 8) == 0) {
		LM_DBG("setting kxlibssl random engine\n");
		RAND_set_rand_method(RAND_ksr_kxlibssl_method());
	}
#endif
	return 0;
}

static int ki_is_peer_verified(sip_msg_t* msg)
{
	struct tcp_connection *c;
	SSL *ssl;
	long ssl_verify;
	X509 *x509_cert;

	LM_DBG("started...\n");
	if (msg->rcv.proto != PROTO_TLS) {
		LM_ERR("proto != TLS --> peer can't be verified, return -1\n");
		return -1;
	}

	LM_DBG("trying to find TCP connection of received message...\n");

	c = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0,
					cfg_get(tls, tls_cfg, con_lifetime));
	if (!c) {
		LM_ERR("connection no longer exists\n");
		return -1;
	}

	if(c->type != PROTO_TLS) {
		LM_ERR("Connection found but is not TLS\n");
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

static int w_is_peer_verified(struct sip_msg* msg, char* foo, char* foo2)
{
	return ki_is_peer_verified(msg);
}

static int ki_tls_set_connect_server_id(sip_msg_t* msg, str* srvid)
{
	if(ksr_tls_set_connect_server_id(srvid)<0) {
		return -1;
	}

	return 1;
}

static int w_tls_set_connect_server_id(sip_msg_t* msg, char* psrvid, char* p2)
{
	str ssrvid = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)psrvid, &ssrvid)<0) {
		LM_ERR("failed to get server id parameter\n");
		return -1;
	}

	return ki_tls_set_connect_server_id(msg, &ssrvid);
}

/**
 *
 */
static sr_kemi_xval_t* ki_tls_cget(sip_msg_t *msg, str *aname)
{
	return ki_tls_cget_attr(msg, aname);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_tls_exports[] = {
	{ str_init("tls"), str_init("is_peer_verified"),
		SR_KEMIP_INT, ki_is_peer_verified,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tls"), str_init("set_connect_server_id"),
		SR_KEMIP_INT, ki_tls_set_connect_server_id,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tls"), str_init("cget"),
		SR_KEMIP_XVAL, ki_tls_cget,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if (tls_disable) {
		LM_WARN("tls support is disabled "
				"(set enable_tls=1 in the config to enable it)\n");
		return 0;
	}

	/* shm is used, be sure it is initialized */
	if(!shm_initialized() && init_shm()<0)
		return -1;

	if(tls_pre_init()<0)
		return -1;

	register_tls_hooks(&tls_h);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	LM_DBG("setting cryptorand random engine\n");
	RAND_set_rand_method(RAND_ksr_cryptorand_method());
#endif

	sr_kemi_modules_add(sr_kemi_tls_exports);

	return 0;
}



#ifndef OPENSSL_NO_ENGINE
/*
 * initialize OpenSSL engine in child process
 * PKCS#11 libraries are not guaranteed to be fork() safe
 *
 */
static int tls_engine_init()
{
	char *err, *section, *engines_section, *engine_section;
	char *engine_id;
	int rc;
	long errline;
	CONF* config;
	STACK_OF(CONF_VALUE) *stack;
	CONF_VALUE *confval;
	ENGINE *e;

	LM_INFO("With OpenSSL engine support %*s\n", tls_engine_settings.engine_config.len, tls_engine_settings.engine_config.s);

	/*
	 * #2839: don't use CONF_modules_load_file():
	 * We are in the child process and the global engine linked-list
	 * is initialized in the parent.
	 */
	e  = ENGINE_by_id("dynamic");
	if (!e) {
		err = "Error loading dynamic engine";
		goto error;
	}
	engine_id = tls_engine_settings.engine.s;

	config = NCONF_new(NULL);
	rc = NCONF_load(config, tls_engine_settings.engine_config.s, &errline);
	if (!rc) {
		err = "Error loading OpenSSL configuration file";
		goto error;
	}

	section = NCONF_get_string(config, NULL, "kamailio");
	engines_section = NCONF_get_string(config, section, "engines");
	engine_section = NCONF_get_string(config, engines_section, engine_id);
	stack = NCONF_get_section(config, engine_section);

	if (!ENGINE_ctrl_cmd_string(e, "SO_PATH", NCONF_get_string(config, engine_section, "dynamic_path"), 0)) {
		err = "SO_PATH";
		goto error;
	}
	if (!ENGINE_ctrl_cmd_string(e, "ID", engine_id, 0)) {
		err = "ID";
		goto error;
	}
	if (!ENGINE_ctrl_cmd(e, "LOAD", 1, NULL, NULL, 0)) {
		err = "LOAD";
		goto error;
	}
	while((confval = sk_CONF_VALUE_pop(stack))) {
		if (strcmp(confval->name, "dynamic_path") == 0) continue;
		LM_DBG("Configuring OpenSSL engine %s: %s(%s)\n", engine_id, confval->name, confval->value);
		if (!ENGINE_ctrl_cmd_string(e, confval->name, confval->value, 0)) {
			err = confval->name;
			goto error;
		}
	}

	if (!ENGINE_init(e)) {
		err = "ENGINE_init()";
		goto error;
	}
	if (strncmp(tls_engine_settings.engine_algorithms.s, "NONE", 4)) {
		rc = ENGINE_set_default_string(e, tls_engine_settings.engine_algorithms.s);
		if (!rc) {
			err = "OpenSSL ENGINE could not set algorithms";
			goto error;
		}
	}
	ENGINE_free(e);
	ksr_tls_engine = e;
	return 0;
error:
	LM_ERR("TLS Engine: %s\n", err);
	return -1;
}

EVP_PKEY *tls_engine_private_key(const char* key_id) {
	return ENGINE_load_private_key(ksr_tls_engine, key_id, NULL, NULL);
}
#endif
