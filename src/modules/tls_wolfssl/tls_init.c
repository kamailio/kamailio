/*
 * TLS module
 *
 * Copyright (C) 2005,2006 iptelorg GmbH
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

/*! \defgroup tls Kamailio TLS support
 *
 * This module implements SIP over TCP with TLS encryption.
 * Make sure you read the README file that describes configuration
 * of TLS for single servers and servers hosting multiple domains,
 * and thus using multiple SSL/TLS certificates.
 *
 *
 */
/*!
 * \file
 * \brief Kamailio TLS support :: Initialization
 * \ingroup tls
 * Module: \ref tls
 */


#include <stdio.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <string.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/tcp_init.h"
#include "../../core/socket_info.h"
#include "../../core/pt.h"
#include "../../core/cfg/cfg.h"
#include "../../core/cfg/cfg_ctx.h"
#include "tls_verify.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_wolfssl_mod.h"
#include "tls_init.h"
#include "tls_ct_wrq.h"
#include "tls_cfg.h"

/* will be set to 1 when the TLS env is initialized to make destroy safe */
static int tls_mod_preinitialized = 0;
static int tls_mod_initialized = 0;

#define TLS_COMP_SUPPORT
#define TLS_KERBEROS_SUPPORT

sr_tls_methods_t sr_tls_methods[TLS_METHOD_MAX];

/*
 * Initialize TLS socket
 */
int tls_h_init_si_f(struct socket_info *si)
{
	int ret;
	/*
	 * reuse tcp initialization
	 */
	ret = tcp_init(si);
	if(ret != 0) {
		LM_ERR("Error while initializing TCP part of TLS socket %.*s:%d\n",
				si->address_str.len, si->address_str.s, si->port_no);
		goto error;
	}

	si->proto = PROTO_TLS;
	return 0;

error:
	if(si->socket != -1) {
		close(si->socket);
		si->socket = -1;
	}
	return ret;
}


/*
 * initialize ssl methods
 */
static void init_ssl_methods(void)
{
	/* openssl 1.1.0+ */
	memset(sr_tls_methods, 0, sizeof(sr_tls_methods));

	/* any SSL/TLS version */
	sr_tls_methods[TLS_USE_SSLv23_cli - 1].TLSMethod = wolfTLS_client_method();
	sr_tls_methods[TLS_USE_SSLv23_srv - 1].TLSMethod = wolfTLS_server_method();
	sr_tls_methods[TLS_USE_SSLv23 - 1].TLSMethod = wolfSSLv23_method();

	sr_tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethod = wolfTLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethodMin = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_cli - 1].TLSMethodMax = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethod = wolfTLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethodMin = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_srv - 1].TLSMethodMax = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1 - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1 - 1].TLSMethodMin = TLS1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1 - 1].TLSMethodMax = TLS1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethod = wolfTLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethodMin = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1_cli - 1].TLSMethodMax = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethod = wolfTLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethodMin = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1_srv - 1].TLSMethodMax = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethodMin = TLS1_1_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_1 - 1].TLSMethodMax = TLS1_1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethod = wolfTLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethodMin = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2_cli - 1].TLSMethodMax = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethod = wolfTLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethodMin = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2_srv - 1].TLSMethodMax = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethodMin = TLS1_2_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_2 - 1].TLSMethodMax = TLS1_2_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethod = wolfTLS_client_method();
	sr_tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethodMin = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3_cli - 1].TLSMethodMax = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethod = wolfTLS_server_method();
	sr_tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethodMin = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3_srv - 1].TLSMethodMax = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethodMin = TLS1_3_VERSION;
	sr_tls_methods[TLS_USE_TLSv1_3 - 1].TLSMethodMax = TLS1_3_VERSION;

	/* ranges of TLS versions (require a minimum TLS version) */
	sr_tls_methods[TLS_USE_TLSv1_PLUS - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1_PLUS - 1].TLSMethodMin = TLS1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_1_PLUS - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1_1_PLUS - 1].TLSMethodMin = TLS1_1_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_2_PLUS - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1_2_PLUS - 1].TLSMethodMin = TLS1_2_VERSION;

	sr_tls_methods[TLS_USE_TLSv1_3_PLUS - 1].TLSMethod = wolfSSLv23_method();
	sr_tls_methods[TLS_USE_TLSv1_3_PLUS - 1].TLSMethodMin = TLS1_3_VERSION;
}


/*
 * Fix openssl compression bugs if necessary
 */
static int init_tls_compression(void)
{
	return 0;
}


/**
 * tls pre-init function
 * - executed when module is loaded
 */
int tls_pre_init(void)
{
#ifdef KSR_LIBSSL_STATIC
	LM_INFO("libssl linked mode: static\n");
#endif

	if(ksr_tcp_main_threads == 0) {
		LM_ERR("tls_wolfssl requires tcp_main_threads = 1 or 2\n");
		return -1;
	}

	init_tls_compression();
	return 0;
}

/**
 * tls mod pre-init function
 * - executed before any mod_init()
 */
int tls_h_mod_pre_init_f(void)
{
	if(tls_mod_preinitialized == 1) {
		LM_DBG("already mod pre-initialized\n");
		return 0;
	}
	LM_DBG("preparing tls env for modules initialization\n");

	LM_DBG("preparing tls env for modules initialization (libssl >=1.1)\n");
	wolfSSL_OPENSSL_init_ssl(0, NULL);
	wolfSSL_load_error_strings();
	tls_mod_preinitialized = 1;
	return 0;
}

/*
 * First step of TLS initialization
 */
int tls_h_mod_init_f(void)
{
	if(tls_mod_initialized == 1) {
		LM_DBG("already initialized\n");
		return 0;
	}
	LM_DBG("initializing tls system\n");

	init_ssl_methods();
	tls_mod_initialized = 1;
	return 0;
}


/*
 * Make sure that all server domains in the configuration have corresponding
 * listening socket in SER
 */
int tls_check_sockets(tls_domains_cfg_t *cfg)
{
	tls_domain_t *d;

	if(!cfg)
		return 0;

	d = cfg->srv_list;
	while(d) {
		if(d->ip.len && !find_si(&d->ip, d->port, PROTO_TLS)) {
			LM_ERR("%s: No listening socket found\n", tls_domain_str(d));
			return -1;
		}
		d = d->next;
	}
	return 0;
}


/*
 * TLS cleanup when application exits
 */
void tls_h_mod_destroy_f(void)
{
	LM_DBG("tls module final tls destroy\n");
	/* TODO: free all the ctx'es */
	tls_destroy_cfg();
	tls_ct_wq_destroy();
	/* explicit execution of libssl cleanup to avoid being executed again
	 * by atexit(), when shm is gone */
	LM_DBG("executing wolfSSL cleanup\n");
	wolfSSL_Cleanup();
}
