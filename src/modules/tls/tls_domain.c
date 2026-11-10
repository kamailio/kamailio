/*
 * TLS module
 *
 * Copyright (C) 2005,2006 iptelorg GmbH
 * Copyright (C) 2013 Motorola Solutions, Inc.
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

/**
 * Kamailio TLS support :: Virtual domain configuration support
 * @file
 * @ingroup tls
 * Module: @ref tls
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#include <openssl/bn.h>
#include <openssl/dh.h>

#include "tls_openssl.h"
#ifdef KSR_SSL_ENGINE
#include <openssl/engine.h>
#endif /* KSR_SSL_ENGINE */
#ifdef KSR_SSL_COMMON
extern EVP_PKEY *tls_engine_private_key(const char *key_id);
#endif /* KSR_SSL_COMMON */

#include <openssl/ui.h>
#include "../../core/ut.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/pt.h"
#include "../../core/cfg/cfg.h"
#include "../../core/dprint.h"
#include "../../core/sr_module.h"
#include "tls_config.h"
#include "tls_server.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_init.h"
#include "tls_domain.h"
#include "tls_cfg.h"
#include "tls_verify.h"

extern int ksr_tls_key_password_mode;
extern int *ksr_tls_keylog_mode;

#ifndef OPENSSL_NO_DH

static void setup_dh(SSL_CTX *ctx)
{
#if !defined(OPENSSL_IS_AWSLC)
	SSL_CTX_set_dh_auto(ctx, 1);
#endif
}
#endif


/**
 * @brief Create a new TLS domain structure
 *
 * Create a new domain structure in new allocated shared memory.
 * @param type domain Type
 * @param ip domain IP
 * @param port domain port
 * @return new domain
 */
tls_domain_t *tls_new_domain(int type, struct ip_addr *ip, unsigned short port)
{
	tls_domain_t *d;

	d = shm_malloc(sizeof(tls_domain_t));
	if(d == NULL) {
		ERR("Memory allocation failure\n");
		return 0;
	}
	memset(d, '\0', sizeof(tls_domain_t));

	d->type = type;
	if(ip)
		memcpy(&d->ip, ip, sizeof(struct ip_addr));
	d->port = port;
	d->verify_cert = -1;
	d->verify_depth = -1;
	d->require_cert = -1;
	d->verify_client = -1;
	d->verify_server_name = -1;
	return d;
}


/**
 * @brief Free all memory used by TLS configuration domain
 * @param d freed domain
 */
void tls_free_domain(tls_domain_t *d)
{
	int i;
	int procs_no;

	if(!d)
		return;

	if(d->ctx) {
		if(ksr_tcp_main_threads == 0) {
			procs_no = TLS_CTX_NO();
			for(i = 0; i < procs_no; i++) {
				if(d->ctx[i]) {
					SSL_CTX_free(d->ctx[i]);
				}
			}
		} else {
			/* In multi-threaded mode this must be freed in PROC_TCP_MAIN */
			if(d->ctx[0] && tls_child_rank == PROC_TCP_MAIN) {
				SSL_CTX_free(d->ctx[0]);
			}
		}
		shm_free(d->ctx);
	}

	if(d->cipher_list.s)
		shm_free(d->cipher_list.s);
	if(d->ca_file.s)
		shm_free(d->ca_file.s);
	if(d->ca_path.s)
		shm_free(d->ca_path.s);
	if(d->crl_file.s)
		shm_free(d->crl_file.s);
	if(d->pkey_file.s)
		shm_free(d->pkey_file.s);
	if(d->cert_file.s)
		shm_free(d->cert_file.s);
	if(d->server_name.s)
		shm_free(d->server_name.s);
	if(d->server_id.s)
		shm_free(d->server_id.s);
	shm_free(d);
}


/**
 * @brief Free TLS configuration structure
 * @param cfg freed configuration
 */
void tls_free_cfg(tls_domains_cfg_t *cfg)
{
	tls_domain_t *p;
	while(cfg->srv_list) {
		p = cfg->srv_list;
		cfg->srv_list = cfg->srv_list->next;
		tls_free_domain(p);
	}
	while(cfg->cli_list) {
		p = cfg->cli_list;
		cfg->cli_list = cfg->cli_list->next;
		tls_free_domain(p);
	}
	if(cfg->srv_default)
		tls_free_domain(cfg->srv_default);
	if(cfg->cli_default)
		tls_free_domain(cfg->cli_default);
	shm_free(cfg);
}


/**
 * @brief Destroy all TLS configuration data
 */
void tls_destroy_cfg(void)
{
	tls_domains_cfg_t *ptr;

	if(tls_domains_cfg_lock) {
		lock_destroy(tls_domains_cfg_lock);
		lock_dealloc(tls_domains_cfg_lock);
		tls_domains_cfg_lock = 0;
	}

	if(tls_domains_cfg) {
		while(*tls_domains_cfg) {
			ptr = *tls_domains_cfg;
			*tls_domains_cfg = (*tls_domains_cfg)->next;
			tls_free_cfg(ptr);
		}

		shm_free(tls_domains_cfg);
		tls_domains_cfg = 0;
	}
}


/**
 * @brief Generate TLS domain identifier
 * @param d printed domain
 * @return printed domain, with zero termination
 */
char *tls_domain_str(tls_domain_t *d)
{
	/* per thread: mode 2 completes the TLS init on tcp reactor pool threads */
	static _Thread_local char buf[1024];
	char *p;

	buf[0] = '\0';
	p = buf;
	p = strcat(p, d->type & TLS_DOMAIN_SRV ? "TLSs<" : "TLSc<");
	if(d->type & TLS_DOMAIN_DEF) {
		p = strcat(p, "default>");
	} else if(d->type & TLS_DOMAIN_ANY) {
		p = strcat(p, "any:");
		if(d->server_name.s && d->server_name.len > 0) {
			p = strncat(p, d->server_name.s, d->server_name.len);
		}
		p = strcat(p, ">");
	} else {
		p = strcat(p, ip_addr2a(&d->ip));
		p = strcat(p, ":");
		p = strcat(p, int2str(d->port, 0));
		p = strcat(p, ">");
	}
	return buf;
}


/**
 * @brief Initialize TLS domain parameters that have not been configured yet
 *
 * Initialize TLS domain parameters that have not been configured from
 * parent domain (usually one of default domains)
 * @param d initialized domain
 * @param parent parent domain
 * @return 0 on success, -1 on error
 */
static int ksr_tls_fill_missing(tls_domain_t *d, tls_domain_t *parent)
{
	if(d->method == TLS_METHOD_UNSPEC)
		d->method = parent->method;
	LOG(L_INFO, "%s: tls_method=%d\n", tls_domain_str(d), d->method);

	if(d->method < 1 || d->method >= TLS_METHOD_MAX) {
		ERR("%s: Invalid TLS method value\n", tls_domain_str(d));
		return -1;
	}

	if(!d->cert_file.s) {
		if(shm_asciiz_dup(&d->cert_file.s, parent->cert_file.s) < 0)
			return -1;
		d->cert_file.len = parent->cert_file.len;
	}
	LOG(L_INFO, "%s: certificate='%s'\n", tls_domain_str(d), d->cert_file.s);

	if(!d->ca_file.s) {
		if(shm_asciiz_dup(&d->ca_file.s, parent->ca_file.s) < 0)
			return -1;
		d->ca_file.len = parent->ca_file.len;
	}
	LOG(L_INFO, "%s: ca_list='%s'\n", tls_domain_str(d), d->ca_file.s);

	if(!d->ca_path.s) {
		if(shm_asciiz_dup(&d->ca_path.s, parent->ca_path.s) < 0)
			return -1;
		d->ca_path.len = parent->ca_path.len;
	}
	LOG(L_INFO, "%s: ca_path='%s'\n", tls_domain_str(d), d->ca_path.s);

	if(!d->crl_file.s) {
		if(shm_asciiz_dup(&d->crl_file.s, parent->crl_file.s) < 0)
			return -1;
		d->crl_file.len = parent->crl_file.len;
	}
	LOG(L_INFO, "%s: crl='%s'\n", tls_domain_str(d), d->crl_file.s);

	if(d->require_cert == -1)
		d->require_cert = parent->require_cert;
	LOG(L_INFO, "%s: require_certificate=%d\n", tls_domain_str(d),
			d->require_cert);

	if(!d->cipher_list.s) {
		if(shm_asciiz_dup(&d->cipher_list.s, parent->cipher_list.s) < 0)
			return -1;
		d->cipher_list.len = parent->cipher_list.len;
	}
	LOG(L_INFO, "%s: cipher_list='%s'\n", tls_domain_str(d), d->cipher_list.s);

	if(!d->pkey_file.s) {
		if(shm_asciiz_dup(&d->pkey_file.s, parent->pkey_file.s) < 0)
			return -1;
		d->pkey_file.len = parent->pkey_file.len;
	}
	LOG(L_INFO, "%s: private_key='%s'\n", tls_domain_str(d), d->pkey_file.s);

	if(!d->pkey_password.s) {
		if(shm_asciiz_dup(&d->pkey_password.s, parent->pkey_password.s) < 0)
			return -1;
		d->pkey_password.len = parent->pkey_password.len;
	}
	LOG(L_INFO, "%s: private_key_password='%s'\n", tls_domain_str(d),
			d->pkey_password.s ? "..." : NULL);

	if(d->cert_file2.s) {
		LOG(L_INFO, "%s: certificate2='%s'\n", tls_domain_str(d),
				d->cert_file2.s);
	}
	if(d->pkey_file2.s) {
		LOG(L_INFO, "%s: private_key2='%s'\n", tls_domain_str(d),
				d->pkey_file2.s);
	}

	if(d->verify_cert == -1)
		d->verify_cert = parent->verify_cert;
	LOG(L_INFO, "%s: verify_certificate=%d\n", tls_domain_str(d),
			d->verify_cert);

	if(d->verify_depth == -1)
		d->verify_depth = parent->verify_depth;
	LOG(L_INFO, "%s: verify_depth=%d\n", tls_domain_str(d), d->verify_depth);

	if(d->verify_client == -1)
		d->verify_client = parent->verify_client;
	LOG(L_INFO, "%s: verify_client=%d\n", tls_domain_str(d), d->verify_client);

	if(d->verify_server_name == -1)
		d->verify_server_name = parent->verify_server_name;
	LOG(L_INFO, "%s: verify_server_name=%d\n", tls_domain_str(d),
			d->verify_server_name);

	return 0;
}


/**
 * @brief Called for ctx, with 2 args
 * @param ctx SSL context
 * @param larg ?
 * @param parg ?
 * @return return 0 on success, <0 on critical error
 */
typedef int (*per_ctx_cbk_f)(SSL_CTX *ctx, long larg, void *parg);


/**
 * @brief Execute callback on all the CTX'es on a domain
 * @param d domain
 * @param ctx_cbk callback function
 * @param l1 parameter passed to the callback
 * @param p2 parameter passed to the callback
 * @return 0 on success, <0 on error
 */
static int tls_domain_foreach_CTX(
		tls_domain_t *d, per_ctx_cbk_f ctx_cbk, long l1, void *p2)
{
	int i, ret;
	int procs_no;

	procs_no = TLS_CTX_NO();
	for(i = 0; i < procs_no; i++) {
		if((ret = ctx_cbk(d->ctx[i], l1, p2)) < 0)
			return ret;
	}
	return 0;
}


/**
 * @brief Execute callback on all the CTX'es on in a domain list
 * @param d domain
 * @param ctx_cbk callback function
 * @param l1 parameter passed to the callback
 * @param p2 parameter passed to the callback
 * @return 0 on success, <0 on error
 */
static int tls_foreach_CTX_in_domain_lst(
		tls_domain_t *d, per_ctx_cbk_f ctx_cbk, long l1, void *p2)
{
	int ret;
	for(; d; d = d->next)
		if((ret = tls_domain_foreach_CTX(d, ctx_cbk, l1, p2)) < 0)
			return ret;
	return 0;
}


/**
 * @brief Execute callback on all the CTX'es in all the srv domains in a tls cfg
 * @param cfg tls cfg.
 * @param ctx_cbk callback function
 * @param l1 parameter passed to the callback
 * @param p2 parameter passed to the callback
 * @return 0 on success, <0 on error
 */
static int tls_foreach_CTX_in_srv_domains(
		tls_domains_cfg_t *cfg, per_ctx_cbk_f ctx_cbk, long l1, void *p2)
{
	int ret;
	if((ret = tls_domain_foreach_CTX(cfg->srv_default, ctx_cbk, l1, p2)) < 0)
		return ret;
	if((ret = tls_foreach_CTX_in_domain_lst(cfg->srv_list, ctx_cbk, l1, p2))
			< 0)
		return ret;
	return 0;
}


/**
 * @brief Execute callback on all the CTX'es in all the client domains in a tls cfg
 * @param cfg tls cfg.
 * @param ctx_cbk callback function
 * @param l1 parameter passed to the callback
 * @param p2 parameter passed to the callback
 * @return 0 on success, <0 on error.
 */
static int tls_foreach_CTX_in_cli_domains(
		tls_domains_cfg_t *cfg, per_ctx_cbk_f ctx_cbk, long l1, void *p2)
{
	int ret;
	if((ret = tls_domain_foreach_CTX(cfg->cli_default, ctx_cbk, l1, p2)) < 0)
		return ret;
	if((ret = tls_foreach_CTX_in_domain_lst(cfg->cli_list, ctx_cbk, l1, p2))
			< 0)
		return ret;
	return 0;
}


/**
 * @brief Execute callback on all the CTX'es in all the domains in a tls cfg
 * @param cfg tls cfg
 * @param ctx_cbk callback function
 * @param l1 parameter passed to the callback
 * @param p2 parameter passed to the callback
 * @return 0 on success, <0 on error
 */
static int tls_foreach_CTX_in_cfg(
		tls_domains_cfg_t *cfg, per_ctx_cbk_f ctx_cbk, long l1, void *p2)
{
	int ret;

	if((ret = tls_foreach_CTX_in_srv_domains(cfg, ctx_cbk, l1, p2)) < 0)
		return ret;
	if((ret = tls_foreach_CTX_in_cli_domains(cfg, ctx_cbk, l1, p2)) < 0)
		return ret;
	return 0;
}


/**
 * @brief Fix pathnames when loading domain keys or other list
 *
 * Fix pathnames, to be used when loading the domain key, cert, ca list a.s.o.
 * It will replace path with a fixed shm allocated version. Assumes path->s
 * was shm allocated.
 * @param path path to be fixed. If it starts with '.' or '/' is left alone
 * (forced "relative" or "absolute" path). Otherwise the path is considered
 * to be relative to the main config file directory
 * (e.g. for /etc/ser/ser.cfg => /etc/ser/\<path\>).
 * @return  0 on success, -1 on error
 */
int fix_shm_pathname(str *path)
{
	str new_path;
	char *abs_path;

	if(path->s && path->len && *path->s != '.' && *path->s != '/') {
		abs_path = get_abs_pathname(0, path);
		if(abs_path == 0) {
			LM_ERR("get abs pathname failed\n");
			return -1;
		}
		new_path.len = strlen(abs_path);
		new_path.s = shm_malloc(new_path.len + 1);
		if(new_path.s == 0) {
			LM_ERR("no more shm memory\n");
			pkg_free(abs_path);
			return -1;
		}
		memcpy(new_path.s, abs_path, new_path.len);
		new_path.s[new_path.len] = 0;
		shm_free(path->s);
		pkg_free(abs_path);
		*path = new_path;
	}
	return 0;
}


/**
 * @brief Load certificate from file
 * @param d domain
 * @return 0 if not configured or on success, -1 on error
 */
static int load_cert(tls_domain_t *d)
{
	int i;
	int procs_no;

	if(!d->cert_file.s || !d->cert_file.len) {
		DBG("%s: No certificate configured\n", tls_domain_str(d));
		return 0;
	}
	if(fix_shm_pathname(&d->cert_file) < 0)
		return -1;
	procs_no = TLS_CTX_NO();
	for(i = 0; i < procs_no; i++) {
		if(!SSL_CTX_use_certificate_chain_file(d->ctx[i], d->cert_file.s)) {
			ERR("%s: Unable to load certificate file '%s'\n", tls_domain_str(d),
					d->cert_file.s);
			TLS_ERR("load_cert:");
			return -1;
		}
	}

	if(d->cert_file2.s && d->cert_file2.len) {
		if(fix_shm_pathname(&d->cert_file2) < 0)
			return -1;
		for(i = 0; i < procs_no; i++) {
			if(!SSL_CTX_use_certificate_chain_file(
					   d->ctx[i], d->cert_file2.s)) {
				ERR("%s: Unable to load certificate2 file '%s'\n",
						tls_domain_str(d), d->cert_file2.s);
				TLS_ERR("load_cert:");
				return -1;
			}
		}
	}

	return 0;
}


/**
 * @brief Load CA list from file
 * @param d domain
 * @return 0 if not configured or on success, -1 on error
 */
static int load_ca_list(tls_domain_t *d)
{
	int i;
	int procs_no;

	if((!d->ca_file.s || !d->ca_file.len)
			&& (!d->ca_path.s || !d->ca_path.len)) {
		DBG("%s: No CA list configured\n", tls_domain_str(d));
		return 0;
	}
	if(d->ca_file.s && d->ca_file.len > 0 && fix_shm_pathname(&d->ca_file) < 0)
		return -1;
	if(d->ca_path.s && d->ca_path.len > 0 && fix_shm_pathname(&d->ca_path) < 0)
		return -1;
	procs_no = TLS_CTX_NO();
	for(i = 0; i < procs_no; i++) {
		if(SSL_CTX_load_verify_locations(d->ctx[i], d->ca_file.s, d->ca_path.s)
				!= 1) {
			ERR("%s: Unable to load CA list file '%s' dir '%s'\n",
					tls_domain_str(d), (d->ca_file.s) ? d->ca_file.s : "",
					(d->ca_path.s) ? d->ca_path.s : "");
			TLS_ERR("load_ca_list:");
			return -1;
		}
		if(d->ca_file.s && d->ca_file.len > 0) {
			SSL_CTX_set_client_CA_list(
					d->ctx[i], SSL_load_client_CA_file(d->ca_file.s));
			if(SSL_CTX_get_client_CA_list(d->ctx[i]) == 0) {
				ERR("%s: Error while setting client CA list file [%.*s/%d]\n",
						tls_domain_str(d), (d->ca_file.s) ? d->ca_file.len : 0,
						(d->ca_file.s) ? d->ca_file.s : "", d->ca_file.len);
				TLS_ERR("load_ca_list:");
				return -1;
			}
		}
	}
	return 0;
}


/**
 * @brief Load CRL from file
 * @param d domain
 * @return 0 if not configured or on success, -1 on error
 */
static int load_crl(tls_domain_t *d)
{
	int i;
	int procs_no;
	X509_STORE *store;

	if(!d->crl_file.s) {
		DBG("%s: No CRL configured\n", tls_domain_str(d));
		return 0;
	}
	if(fix_shm_pathname(&d->crl_file) < 0)
		return -1;
	LOG(L_INFO, "%s: Certificate revocation lists will be checked (%.*s)\n",
			tls_domain_str(d), d->crl_file.len, d->crl_file.s);
	procs_no = TLS_CTX_NO();
	for(i = 0; i < procs_no; i++) {
		if(SSL_CTX_load_verify_locations(d->ctx[i], d->crl_file.s, 0) != 1) {
			ERR("%s: Unable to load certificate revocation list '%s'\n",
					tls_domain_str(d), d->crl_file.s);
			TLS_ERR("load_crl:");
			return -1;
		}
		store = SSL_CTX_get_cert_store(d->ctx[i]);
		X509_STORE_set_flags(
				store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
	}
	return 0;
}


/**
 * @brief Configure cipher list
 * @param d domain
 * @return 0 on success, -1 on error
 */
static int set_cipher_list(tls_domain_t *d)
{
	int i;
	int procs_no;
	char *cipher_list;

	cipher_list = d->cipher_list.s;
	if(!cipher_list)
		return 0;
	procs_no = TLS_CTX_NO();
	for(i = 0; i < procs_no; i++) {
		if(d->method == TLS_USE_TLSv1_3 || d->method == TLS_USE_TLSv1_3_PLUS) {
			if(SSL_CTX_set_ciphersuites(d->ctx[i], cipher_list) == 0) {
				ERR("%s: Failure to set SSL context cipher suites \"%s\"\n",
						tls_domain_str(d), cipher_list);
				return -1;
			}
		} else {
			if(SSL_CTX_set_cipher_list(d->ctx[i], cipher_list) == 0) {
				ERR("%s: Failure to set SSL context cipher list \"%s\"\n",
						tls_domain_str(d), cipher_list);
				return -1;
			}
		}
#ifndef OPENSSL_NO_DH
		setup_dh(d->ctx[i]);
#endif
	}
	return 0;
}


/**
 * @brief Enable/disable TLS certificate verification
 * @param d domain
 * @return 0
 */
static int set_verification(tls_domain_t *d)
{
	int verify_mode, i;
	int procs_no;

	if(d->require_cert || d->verify_client == TLS_VERIFY_CLIENT_ON) {
		verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
		LOG(L_INFO, "%s: %s MUST present valid certificate\n",
				tls_domain_str(d),
				d->type & TLS_DOMAIN_SRV ? "Client" : "Server");
	} else {
		if(d->verify_cert || d->verify_client >= TLS_VERIFY_CLIENT_OPTIONAL) {
			verify_mode = SSL_VERIFY_PEER;
			if(d->type & TLS_DOMAIN_SRV) {
				LOG(L_INFO,
						"%s: IF client provides certificate then it"
						" MUST be valid\n",
						tls_domain_str(d));
			} else {
				LOG(L_INFO, "%s: Server MUST present valid certificate\n",
						tls_domain_str(d));
			}
		} else {
			verify_mode = SSL_VERIFY_NONE;
			if(d->type & TLS_DOMAIN_SRV) {
				LOG(L_INFO,
						"%s: No client certificate required and no checks"
						" performed\n",
						tls_domain_str(d));
			} else {
				LOG(L_INFO, "%s: Server MAY present invalid certificate\n",
						tls_domain_str(d));
			}
		}
	}

	procs_no = TLS_CTX_NO();
	for(i = 0; i < procs_no; i++) {
		if(d->verify_client >= TLS_VERIFY_CLIENT_OPTIONAL_NO_CA) {
			/* Note that actual verification result is available in $tls_peer_verified */
			SSL_CTX_set_verify(d->ctx[i], verify_mode,
					verify_callback_unconditional_success);
		} else {
			SSL_CTX_set_verify(d->ctx[i], verify_mode, 0);
		}
		SSL_CTX_set_verify_depth(d->ctx[i], d->verify_depth);
	}
	return 0;
}


/* This callback function is executed when libssl processes the SSL
 * handshake and does SSL record layer stuff. It's used to trap
 * client-initiated renegotiations (when renegotiation is disabled) and to
 * refresh the cached ssl metadata (peer certificate, verify result, cipher,
 * ...) once an allowed renegotiation is completed.
 */

static void sr_ssl_ctx_info_callback(const SSL *ssl, int event, int ret)
{
	struct tls_extra_data *data = 0;
	int tls_dbg;

	if(!(event & (SSL_CB_HANDSHAKE_START | SSL_CB_HANDSHAKE_DONE)))
		return;
	data = (struct tls_extra_data *)SSL_get_app_data(ssl);
	if(data == 0)
		return; /* not (yet) a connection of this module */

	if(event & SSL_CB_HANDSHAKE_START) {
		tls_dbg = cfg_get(tls, tls_cfg, debug);
		LOG(tls_dbg, "SSL handshake started\n");
		if((data->flags & F_TLS_CON_HANDSHAKED) && sr_tls_renegotiation == 0) {
			LOG(tls_dbg, "SSL renegotiation initiated by client\n");
			data->flags |= F_TLS_CON_RENEGOTIATION;
		}
	}
	if(event & SSL_CB_HANDSHAKE_DONE) {
		tls_dbg = cfg_get(tls, tls_cfg, debug);
		LOG(tls_dbg, "SSL handshake done\n");
		if((data->flags & F_TLS_CON_HANDSHAKED) && sr_tls_renegotiation != 0) {
			/* allowed renegotiation completed: the values cached at the end
			 * of the initial handshake (tls_accept()/tls_connect()) are stale */
			LOG(tls_dbg, "SSL renegotiation done - refreshing ssl cache\n");
			tls_refresh_ssl_cache(data);
		}
		data->flags |= F_TLS_CON_HANDSHAKED;
	}
}

/**
 * @brief Configure generic SSL parameters
 * @param d domain
 * @return 0
 */
static int set_ssl_options(tls_domain_t *d)
{
	int i;
	int procs_no;
	long options;

	procs_no = TLS_CTX_NO();
	options = SSL_OP_ALL; /* all the bug workarounds by default */
	options |= SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
			   | SSL_OP_CIPHER_SERVER_PREFERENCE;
	for(i = 0; i < procs_no; i++) {
		SSL_CTX_set_options(d->ctx[i], options);
		/* always needed: blocks client renegotiations when disabled,
		 * refreshes the ssl cache after a renegotiation when enabled */
		SSL_CTX_set_info_callback(d->ctx[i], sr_ssl_ctx_info_callback);
	}
	return 0;
}


/**
 * @brief Configure TLS session cache parameters
 * @param d domain
 * @return 0
 */
static int set_session_cache(tls_domain_t *d)
{
	int i;
	int procs_no;
	str tls_session_id;

	procs_no = TLS_CTX_NO();
	tls_session_id = cfg_get(tls, tls_cfg, session_id);
	for(i = 0; i < procs_no; i++) {
		/* janakj: I am not sure if session cache makes sense in ser, session
		 * cache is stored in SSL_CTX and we have one SSL_CTX per process,
		 * thus sessions among processes will not be reused
		 */
		SSL_CTX_set_session_cache_mode(d->ctx[i],
				cfg_get(tls, tls_cfg, session_cache) ? SSL_SESS_CACHE_SERVER
													 : SSL_SESS_CACHE_OFF);
		/* not really needed is SSL_SESS_CACHE_OFF */
		SSL_CTX_set_session_id_context(d->ctx[i],
				(unsigned char *)tls_session_id.s, tls_session_id.len);
	}
	return 0;
}


/**
 * @brief TLS SSL_CTX_set_mode and SSL_CTX_clear_mode wrapper
 * @param ctx SSL context
 * @param mode SSL_MODE_*
 * @param clear if set to !=0 will do a clear, else (==0) a set
 * @return 0 (always succeeds)
 */
static int tls_ssl_ctx_mode(SSL_CTX *ctx, long mode, void *clear)
{
	if(clear)
		SSL_CTX_clear_mode(ctx, mode);
	else
		SSL_CTX_set_mode(ctx, mode);
	return 0;
}


/**
 * @brief TLS SSL_CTX_set_max_send_fragment wrapper
 * @param ctx TLS context
 * @param val value (<0 ignored). Should be between 512 and 16k
 * @param unused unused
 * @return 0 on success, < 0 on failure (invalid value)
 */
static int tls_ssl_ctx_set_max_send_fragment(
		SSL_CTX *ctx, long val, void *unused)
{
	if(val >= 0)
		return SSL_CTX_set_max_send_fragment(ctx, val) - 1;
	return 0;
}


/**
 * @brief TLS SSL_CTX_set_read_ahead wrapper
 * @param ctx TLS context
 * @param val value (<0 ignored, 0 or >0)
 * @param unused unused
 * @return 0 (always success).
 */
static int tls_ssl_ctx_set_read_ahead(SSL_CTX *ctx, long val, void *unused)
{
	if(val >= 0)
		SSL_CTX_set_read_ahead(ctx, val);
	return 0;
}


/**
 * @brief SNI callback function
 *
 * callback on server_name -> trigger context switch if a TLS domain
 * for the server_name is found (checks socket too) */
static int tls_server_name_cb(SSL *ssl, int *ad, void *private)
{
	tls_domain_t *orig_domain, *new_domain;
	str server_name;

	orig_domain = (tls_domain_t *)private;
	server_name.s = (char *)SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if(server_name.s) {
		LM_DBG("received server_name (TLS extension): '%s'\n", server_name.s);
	} else {
		LM_DBG("SSL_get_servername returned NULL: return "
			   "SSL_TLSEXT_ERR_NOACK\n");
		return SSL_TLSEXT_ERR_NOACK;
	}

	server_name.len = strlen(server_name.s);

	new_domain = tls_lookup_cfg(*tls_domains_cfg, TLS_DOMAIN_SRV,
			&orig_domain->ip, orig_domain->port, &server_name, 0);
	if(new_domain == NULL) {
		LM_DBG("TLS domain for socket [%s:%d] and server_name='%s' "
			   "not found\n",
				ip_addr2a(&orig_domain->ip), orig_domain->port, server_name.s);
		/* we do not perform SSL_CTX switching, thus the default server domain
		   for this socket (or the default server domain) will be used. */
		return SSL_TLSEXT_ERR_ALERT_WARNING;
	}

	LM_DBG("TLS cfg domain selected for received server name [%s]:"
		   " socket [%s:%d] server name='%s' -"
		   " switching SSL CTX to %p dom %p%s\n",
			server_name.s, ip_addr2a(&new_domain->ip), new_domain->port,
			ZSW(new_domain->server_name.s), DOM_CTX(new_domain), new_domain,
			(new_domain->type & TLS_DOMAIN_DEF) ? " (default)" : "");
	SSL_set_SSL_CTX(ssl, DOM_CTX(new_domain));
	/* SSL_set_SSL_CTX only sets the correct certificate parameters, but does
	   set the proper verify options. Thus this will be done manually! */

	SSL_set_options(ssl, SSL_CTX_get_options(SSL_get_SSL_CTX(ssl)));
	if((SSL_get_verify_mode(ssl) == SSL_VERIFY_NONE)
			|| (SSL_num_renegotiations(ssl) == 0)) {
		/*
		 * Only initialize the verification settings from the ctx
		 * if they are not yet set, or if we're called when a new
		 * SSL connection is set up (num_renegotiations == 0).
		 * Otherwise, we would possibly reset a per-directory
		 * configuration which was put into effect by ssl_hook_access.
		 */
		SSL_set_verify(ssl, SSL_CTX_get_verify_mode(SSL_get_SSL_CTX(ssl)),
				SSL_CTX_get_verify_callback(SSL_get_SSL_CTX(ssl)));
	}

	return SSL_TLSEXT_ERR_OK;
}

/**
 * write a comment line with the connection and handshake parameters before
 * a key line (keylog_mode value 32: 1 << 5, KSR_TLS_KEYLOG_MODE_TUPLE), so
 * a capture that missed the hellos can match the keys to a flow and derive
 * the record keys (tls 1.2: server random + cipher suite, tls 1.3: cipher
 * suite)
 */
static void ksr_tls_keylog_emit_tuple(const SSL *ssl)
{
	struct tls_extra_data *data;
	struct tcp_connection *c;
	const SSL_CIPHER *cipher;
	struct sockaddr_storage sa;
	socklen_t sa_len;
	char src_ip[IP_ADDR_MAX_STR_SIZE + 2];
	char dst_ip[IP_ADDR_MAX_STR_SIZE + 2];
	char sr_hex[SSL3_RANDOM_SIZE * 2 + 1];
	unsigned char sr[SSL3_RANDOM_SIZE];
	char buf[512];
	unsigned int dst_port;
	size_t sr_len;
	size_t i;
	int fd;
	int n;

	data = (struct tls_extra_data *)SSL_get_app_data(ssl);
	if(data == NULL || data->tcp_conn == NULL) {
		return;
	}
	c = data->tcp_conn;
	/* own buffers (ip_addr2a() uses one static buffer); ipv6 as [addr] */
	if(ip_addr2sbufz(&c->rcv.src_ip, src_ip, sizeof(src_ip)) <= 0
			|| ip_addr2sbufz(&c->rcv.dst_ip, dst_ip, sizeof(dst_ip)) <= 0) {
		return;
	}

	dst_port = c->rcv.dst_port;
	if(!(c->flags & F_CONN_PASSIVE)) {
		/* outbound: read the local port from the socket of this process
		 * (c->s in tcp main, c->fd in a worker) */
		fd = _tconfd(c);
		sa_len = sizeof(sa);
		if(fd >= 0 && getsockname(fd, (struct sockaddr *)&sa, &sa_len) == 0) {
			if(sa.ss_family == AF_INET) {
				dst_port = ntohs(((struct sockaddr_in *)&sa)->sin_port);
			} else if(sa.ss_family == AF_INET6) {
				dst_port = ntohs(((struct sockaddr_in6 *)&sa)->sin6_port);
			}
		}
	} else if(dst_port == 0 && c->rcv.bind_address != NULL) {
		dst_port = c->rcv.bind_address->port_no;
	}

	sr_len = SSL_get_server_random(ssl, sr, sizeof(sr));
	for(i = 0; i < sr_len; i++) {
		snprintf(sr_hex + i * 2, 3, "%02x", sr[i]);
	}
	sr_hex[sr_len * 2] = '\0';
	cipher = SSL_get_current_cipher(ssl);

	n = snprintf(buf, sizeof(buf),
			"# TUPLE src=%s:%u dst=%s:%u sr=%s cs=%04x v=%04x", src_ip,
			(unsigned)c->rcv.src_port, dst_ip, dst_port, sr_hex,
			cipher ? (unsigned)(SSL_CIPHER_get_id(cipher) & 0xFFFF) : 0,
			(unsigned)SSL_version(ssl));
	if(n <= 0 || (size_t)n >= sizeof(buf)) {
		return;
	}
	if(*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_MLOG) {
		LM_NOTICE("tlskeylog: %s\n", buf);
	}
	ksr_tls_keylog_file_write(ssl, buf);
	ksr_tls_keylog_peer_send(ssl, buf);
}

static void ksr_tls_keylog_callback(const SSL *ssl, const char *line)
{
	if(ksr_tls_keylog_mode == NULL) {
		return;
	}
	/* an LD_PRELOAD key logger can chain other libraries' sessions here
	 * (e.g. libpq's, whose app data is not a tls_extra_data) */
	if(SSL_get_ex_data(ssl, ksr_tls_ssl_ex_idx) == NULL) {
		return;
	}
	if(!(*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_ACTIVE)) {
		return;
	}
	if(*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_VFILTER) {
		if(ksr_tls_keylog_vfilter_match(line) == 0) {
			return;
		}
	}
	if(*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_TUPLE) {
		ksr_tls_keylog_emit_tuple(ssl);
	}
	if(*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_MLOG) {
		LM_NOTICE("tlskeylog: %s\n", line);
	}
	ksr_tls_keylog_file_write(ssl, line);
	ksr_tls_keylog_peer_send(ssl, line);
}

/**
 * @brief Initialize all domain attributes from default domains if necessary
 * @param d initialized TLS domain
 * @param def default TLS domains
 */
static int ksr_tls_fix_domain(tls_domain_t *d, tls_domain_t *def)
{
	int i;
	int procs_no;

	if(ksr_tls_fill_missing(d, def) < 0)
		return -1;

	if(d->verify_server_name > 0) {
		if(d->type & TLS_DOMAIN_SRV) {
			LM_WARN("%s: verify_server_name is only used by client profiles"
					" - ignored\n",
					tls_domain_str(d));
		} else if(d->verify_cert <= 0) {
			LM_WARN("%s: verify_server_name is used only with"
					" verify_certificate enabled - ignored\n",
					tls_domain_str(d));
		}
	}

	if(d->type & TLS_DOMAIN_ANY) {
		if(d->server_name.s == NULL || d->server_name.len < 0) {
			LM_ERR("%s: tls domain for any address but no server name\n",
					tls_domain_str(d));
			return -1;
		}
	}

	procs_no = TLS_CTX_NO();
	d->ctx = (SSL_CTX **)shm_malloc(sizeof(SSL_CTX *) * procs_no);
	if(!d->ctx) {
		ERR("%s: Cannot allocate shared memory\n", tls_domain_str(d));
		return -1;
	}
	if(d->method > TLS_USE_TLSvRANGE) {
		LM_DBG("using tls methods range: %d\n", d->method);
	} else {
		LM_DBG("using one tls method version: %d\n", d->method);
	}
	memset(d->ctx, 0, sizeof(SSL_CTX *) * procs_no);
	for(i = 0; i < procs_no; i++) {

		d->ctx[i] = SSL_CTX_new(sr_tls_methods[d->method - 1].TLSMethod);
		if(d->ctx[i] == NULL) {
			unsigned long e = 0;
			e = ERR_peek_last_error();
			ERR("%s: Cannot create SSL context [%d] (%lu: %s / %s)\n",
					tls_domain_str(d), i, e, ERR_error_string(e, NULL),
					ERR_reason_error_string(e));
			return -1;
		}
		if((ksr_tls_keylog_mode != NULL)
				&& (*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_INIT)) {
			SSL_CTX_set_keylog_callback(d->ctx[i], ksr_tls_keylog_callback);
		}
		if(d->method > TLS_USE_TLSvRANGE) {
			if(sr_tls_methods[d->method - 1].TLSMethodMin) {
				SSL_CTX_set_min_proto_version(
						d->ctx[i], sr_tls_methods[d->method - 1].TLSMethodMin);
			}
		} else {
			if(sr_tls_methods[d->method - 1].TLSMethodMin) {
				SSL_CTX_set_min_proto_version(
						d->ctx[i], sr_tls_methods[d->method - 1].TLSMethodMin);
			}
			if(sr_tls_methods[d->method - 1].TLSMethodMax) {
				SSL_CTX_set_max_proto_version(
						d->ctx[i], sr_tls_methods[d->method - 1].TLSMethodMax);
			}
		}

		/*
		* check server domains for server_name extension and register
		* callback function
		*/
		if((d->type & TLS_DOMAIN_SRV)
				&& (d->server_name.len > 0 || (d->type & TLS_DOMAIN_DEF))) {
			if(!SSL_CTX_set_tlsext_servername_callback(
					   d->ctx[i], tls_server_name_cb)) {
				LM_ERR("register server_name callback handler for socket "
					   "[%s:%d], server_name='%s' failed for proc %d\n",
						(d->ip.af > 0) ? ip_addr2a(&d->ip) : "0.0.0.0", d->port,
						(d->server_name.s) ? d->server_name.s : "<default>", i);
				return -1;
			}
			if(!SSL_CTX_set_tlsext_servername_arg(d->ctx[i], d)) {
				LM_ERR("register server_name callback handler data for socket "
					   "[%s:%d], server_name='%s' failed for proc %d\n",
						(d->ip.af > 0) ? ip_addr2a(&d->ip) : "0.0.0.0", d->port,
						(d->server_name.s) ? d->server_name.s : "<default>", i);
				return -1;
			}
		}
	}

	if((d->type & TLS_DOMAIN_SRV)
			&& (d->server_name.len > 0 || (d->type & TLS_DOMAIN_DEF))) {
		LM_NOTICE("registered server_name callback handler for socket "
				  "[%s:%d], server_name='%s' ...\n",
				(d->ip.af > 0) ? ip_addr2a(&d->ip) : "0.0.0.0", d->port,
				(d->server_name.s) ? d->server_name.s : "<default>");
	}

	if(load_cert(d) < 0)
		return -1;
	if(load_ca_list(d) < 0)
		return -1;
	if(load_crl(d) < 0)
		return -1;
	if(set_cipher_list(d) < 0)
		return -1;
	if(set_verification(d) < 0)
		return -1;
	if(set_ssl_options(d) < 0)
		return -1;
	if(set_session_cache(d) < 0)
		return -1;
	return 0;
}


/**
 * @brief Password callback to take it from tls domain structure
 * @param buf buffer
 * @param size buffer size
 * @param rwflag not used
 * @param tlsd tls domain
 * @return length of password on success, 0 on error
 */
static int ksr_passwd_cfg_cb(char *buf, int size, int rwflag, void *tlsd)
{
	tls_domain_t *d;

	d = (tls_domain_t *)tlsd;
	if(d->pkey_password.s == NULL || d->pkey_password.len <= 0) {
		return 0;
	}
	if(d->pkey_password.len >= size - 1) {
		LM_WARN("key password is too long (%d / %d) - truncating\n",
				d->pkey_password.len, size);
		memcpy(buf, d->pkey_password.s, size - 1);
		buf[size - 1] = '\0';
	} else {
		memcpy(buf, d->pkey_password.s, d->pkey_password.len);
		buf[d->pkey_password.len] = '\0';
	}
	LM_DBG("returning password for private key\n");
	return strlen(buf);
}

/**
 * @brief Password callback for second private key, using pkey_password2
 */
static int ksr_passwd_cfg_cb2(char *buf, int size, int rwflag, void *tlsd)
{
	tls_domain_t *d;

	d = (tls_domain_t *)tlsd;
	if(d->pkey_password2.s == NULL || d->pkey_password2.len <= 0) {
		return 0;
	}
	if(d->pkey_password2.len >= size - 1) {
		LM_WARN("key2 password is too long (%d / %d) - truncating\n",
				d->pkey_password2.len, size);
		memcpy(buf, d->pkey_password2.s, size - 1);
		buf[size - 1] = '\0';
	} else {
		memcpy(buf, d->pkey_password2.s, d->pkey_password2.len);
		buf[d->pkey_password2.len] = '\0';
	}
	LM_DBG("returning password for private key2\n");
	return strlen(buf);
}

/**
 * @brief Password callback, ask for private key password on CLI
 * @param buf buffer
 * @param size buffer size
 * @param rwflag not used
 * @param filename filename
 * @return length of password on success, 0 on error
 */
static int ksr_passwd_ui_cb(char *buf, int size, int rwflag, void *filename)
{
#if defined(OPENSSL_IS_AWSLC)
	ERR("aws-lc: no passphrase prompt, set private_key_password\n");
	return 0;
#else
	UI *ui;
	const char *prompt;

	ui = UI_new();
	if(ui == NULL)
		goto err;

	prompt = UI_construct_prompt(ui, "passphrase", filename);
	UI_add_input_string(ui, prompt, 0, buf, 0, size - 1);
	UI_process(ui);
	UI_free(ui);
	return strlen(buf);

err:
	ERR("failure in password callback\n");
	return 0;
#endif
}

#ifdef KSR_SSL_COMMON
/**
 * @brief Load a private key from an OpenSSL engine
 * @param d TLS domain
 * @return 0 on success, -1 on error
 *
 * Do this in mod_child() as PKCS#11 libraries are not guaranteed
 * to be fork() safe
 *
 * private_key setting which starts with /engine: is assumed to be
 * an HSM key and not a file-based key (/uri: for OpenSSL 3 key URIs)
 *
 * We store the private key in a local memory hash table as
 * HSM keys must be process-local. We use the SSL_CTX* address
 * as the key. We cannot put the key into d->ctx[i] as that is
 * in shared memory.
 */
static int load_engine_private_key(tls_domain_t *d)
{
	int idx, ret_pwd, i;
	EVP_PKEY *pkey = 0;

	if(!d->pkey_file.s || !d->pkey_file.len) {
		DBG("%s: No private key specified\n", tls_domain_str(d));
		return 0;
	}
	if(strncmp(d->pkey_file.s, KEY_PREFIX, KEY_PREFIX_LEN) != 0)
		return 0;

	do {
		i = TLS_CTX_IDX();
		for(idx = 0, ret_pwd = 0; idx < 3; idx++) {
			pkey = tls_engine_private_key(d->pkey_file.s + KEY_PREFIX_LEN);
			if(pkey) {
				ret_pwd = SSL_CTX_use_PrivateKey(d->ctx[i], pkey);
			} else {
				ret_pwd = 0;
			}
			if(ret_pwd) {
				break;
			} else {
				ERR("%s: Unable to load private key '%s'\n", tls_domain_str(d),
						d->pkey_file.s);
				TLS_ERR("load_private_key:");
				continue;
			}
		}

		if(!ret_pwd) {
			ERR("%s: Unable to load engine key label '%s'\n", tls_domain_str(d),
					d->pkey_file.s);
			TLS_ERR("load_private_key:");
			return -1;
		}
		if(!SSL_CTX_check_private_key(d->ctx[i])) {
			ERR("%s: Key '%s' does not match the public key of the"
				" certificate\n",
					tls_domain_str(d), d->pkey_file.s);
			TLS_ERR("load_engine_private_key:");
			return -1;
		}
	} while(0);


	LM_INFO("%s: Key '%s' successfully loaded\n", tls_domain_str(d),
			d->pkey_file.s);
	return 0;
}
#endif /* KSR_SSL_COMMON */
/**
 * @brief Load a private key from a file
 * @param d TLS domain
 * @return 0 on success, -1 on error
 */
static int load_private_key(tls_domain_t *d)
{
	int idx, ret_pwd, i;
	int procs_no;

	if(!d->pkey_file.s || !d->pkey_file.len) {
		DBG("%s: No private key specified\n", tls_domain_str(d));
		return 0;
	}
	if(fix_shm_pathname(&d->pkey_file) < 0)
		return -1;
#ifdef KSR_SSL_COMMON
	if(ksr_tcp_main_threads == 0
			&& strncmp(d->pkey_file.s, KEY_PREFIX, KEY_PREFIX_LEN) == 0) {
		ERR("%s: engine/provider key '%s' requires tcp_main_threads = 1 or"
			" 2\n",
				tls_domain_str(d), d->pkey_file.s);
		return -1;
	}
#endif /* KSR_SSL_COMMON */

	procs_no = TLS_CTX_NO();
	for(i = 0; i < procs_no; i++) {
		if(ksr_tls_key_password_mode == 1) {
			SSL_CTX_set_default_passwd_cb(d->ctx[i], ksr_passwd_ui_cb);
			SSL_CTX_set_default_passwd_cb_userdata(d->ctx[i], d->pkey_file.s);
		} else {
			SSL_CTX_set_default_passwd_cb(d->ctx[i], ksr_passwd_cfg_cb);
			SSL_CTX_set_default_passwd_cb_userdata(d->ctx[i], d);
		}

		for(idx = 0, ret_pwd = 0; idx < 3; idx++) {
#ifdef KSR_SSL_COMMON
			/* engine/provider keys: loaded in PROC_TCP_MAIN by
			 * tls_fix_engine_keys() (PKCS#11 modules break across fork) */
			if(strncmp(d->pkey_file.s, KEY_PREFIX, KEY_PREFIX_LEN) != 0) {
				ret_pwd = SSL_CTX_use_PrivateKey_file(
						d->ctx[i], d->pkey_file.s, SSL_FILETYPE_PEM);
			} else {
				ret_pwd = 1;
			}
#else
			ret_pwd = SSL_CTX_use_PrivateKey_file(
					d->ctx[i], d->pkey_file.s, SSL_FILETYPE_PEM);
#endif /* KSR_SSL_COMMON */
			if(ret_pwd) {
				break;
			} else {
				ERR("%s: Unable to load private key '%s'\n", tls_domain_str(d),
						d->pkey_file.s);
				TLS_ERR("load_private_key:");
				continue;
			}
		}

		if(!ret_pwd) {
			ERR("%s: Unable to load private key file '%s'\n", tls_domain_str(d),
					d->pkey_file.s);
			TLS_ERR("load_private_key:");
			return -1;
		}
#ifdef KSR_SSL_COMMON
		if(strncmp(d->pkey_file.s, KEY_PREFIX, KEY_PREFIX_LEN) == 0) {
			// skip private key validity check for HSM keys
			continue;
		}
#endif /* KSR_SSL_COMMON */
		if(!SSL_CTX_check_private_key(d->ctx[i])) {
			ERR("%s: Key '%s' does not match the public key of the"
				" certificate\n",
					tls_domain_str(d), d->pkey_file.s);
			TLS_ERR("load_private_key:");
			return -1;
		}
	}

	DBG("%s: Key '%s' successfully loaded\n", tls_domain_str(d),
			d->pkey_file.s);

	/* Load second private key if configured (for dual-cert support) */
	if(d->pkey_file2.s && d->pkey_file2.len) {
		if(fix_shm_pathname(&d->pkey_file2) < 0)
			return -1;

		for(i = 0; i < procs_no; i++) {
			if(d->pkey_password2.s && d->pkey_password2.len > 0) {
				SSL_CTX_set_default_passwd_cb(d->ctx[i], ksr_passwd_cfg_cb2);
				SSL_CTX_set_default_passwd_cb_userdata(d->ctx[i], d);
			} else if(ksr_tls_key_password_mode == 1) {
				SSL_CTX_set_default_passwd_cb(d->ctx[i], ksr_passwd_ui_cb);
				SSL_CTX_set_default_passwd_cb_userdata(
						d->ctx[i], d->pkey_file2.s);
			} else {
				SSL_CTX_set_default_passwd_cb(d->ctx[i], ksr_passwd_cfg_cb2);
				SSL_CTX_set_default_passwd_cb_userdata(d->ctx[i], d);
			}

			ret_pwd = SSL_CTX_use_PrivateKey_file(
					d->ctx[i], d->pkey_file2.s, SSL_FILETYPE_PEM);
			if(!ret_pwd) {
				ERR("%s: Unable to load private key2 file '%s'\n",
						tls_domain_str(d), d->pkey_file2.s);
				TLS_ERR("load_private_key:");
				return -1;
			}
		}
		DBG("%s: Key2 '%s' successfully loaded\n", tls_domain_str(d),
				d->pkey_file2.s);
	}

	return 0;
}


#ifdef KSR_SSL_COMMON
/**
 * @brief Initialize engine private keys
 *
 * PKCS#11 libraries are not guaranteed to be fork() safe
 * so we fix private keys in the child
 */
int tls_fix_engine_keys(tls_domains_cfg_t *cfg, tls_domain_t *srv_defaults,
		tls_domain_t *cli_defaults)
{
	tls_domain_t *d;
	d = cfg->srv_list;
	while(d) {
		if(load_engine_private_key(d) < 0)
			return -1;
		d = d->next;
	}

	d = cfg->cli_list;
	while(d) {
		if(load_engine_private_key(d) < 0)
			return -1;
		d = d->next;
	}

	if(load_engine_private_key(cfg->srv_default) < 0)
		return -1;
	if(load_engine_private_key(cfg->cli_default) < 0)
		return -1;

	return 0;
}
#endif /* KSR_SSL_COMMON */
/**
 * @brief Initialize attributes of all domains from default domains if necessary
 *
 * Initialize attributes of all domains from default domains if necessary,
 * fill in missing parameters.
 * @param cfg initialized domain
 * @param srv_defaults server defaults
 * @param cli_defaults command line interface defaults
 * @return 0 on success, -1 on error
 */
int tls_fix_domains_cfg(tls_domains_cfg_t *cfg, tls_domain_t *srv_defaults,
		tls_domain_t *cli_defaults)
{
	tls_domain_t *d;
	int ssl_mode_release_buffers;
	int ssl_max_send_fragment;
	int ssl_read_ahead;

	if(!cfg->cli_default) {
		cfg->cli_default =
				tls_new_domain(TLS_DOMAIN_DEF | TLS_DOMAIN_CLI, 0, 0);
	}

	if(!cfg->srv_default) {
		cfg->srv_default =
				tls_new_domain(TLS_DOMAIN_DEF | TLS_DOMAIN_SRV, 0, 0);
	}

	if(ksr_tls_fix_domain(cfg->srv_default, srv_defaults) < 0)
		return -1;
	if(ksr_tls_fix_domain(cfg->cli_default, cli_defaults) < 0)
		return -1;

	d = cfg->srv_list;
	while(d) {
		if(ksr_tls_fix_domain(d, srv_defaults) < 0)
			return -1;
		d = d->next;
	}

	d = cfg->cli_list;
	while(d) {
		if(ksr_tls_fix_domain(d, cli_defaults) < 0)
			return -1;
		d = d->next;
	}

	/* Ask for passwords as the last step */
	d = cfg->srv_list;
	while(d) {
		if(load_private_key(d) < 0)
			return -1;
		d = d->next;
	}

	d = cfg->cli_list;
	while(d) {
		if(load_private_key(d) < 0)
			return -1;
		d = d->next;
	}

	if(load_private_key(cfg->srv_default) < 0)
		return -1;
	if(load_private_key(cfg->cli_default) < 0)
		return -1;

	/* set various global per CTX options
	 * (done here to show possible missing features messages only once)
	 */
	ssl_mode_release_buffers = cfg_get(tls, tls_cfg, ssl_release_buffers);
	ssl_max_send_fragment = cfg_get(tls, tls_cfg, ssl_max_send_fragment);
	ssl_read_ahead = cfg_get(tls, tls_cfg, ssl_read_ahead);
	/* set SSL_MODE_RELEASE_BUFFERS if ssl_mode_release_buffers !=0,
	   reset if == 0 and ignore if < 0 */
	if(ssl_mode_release_buffers >= 0
			&& tls_foreach_CTX_in_cfg(cfg, tls_ssl_ctx_mode,
					   SSL_MODE_RELEASE_BUFFERS,
					   (void *)(long)(ssl_mode_release_buffers == 0))
					   < 0) {
		ERR("invalid ssl_release_buffers value (%d)\n",
				ssl_mode_release_buffers);
		return -1;
	}
	if(tls_foreach_CTX_in_cfg(
			   cfg, tls_ssl_ctx_set_max_send_fragment, ssl_max_send_fragment, 0)
			< 0) {
		ERR("invalid ssl_max_send_fragment value (%d)\n",
				ssl_max_send_fragment);
		return -1;
	}
	if(tls_foreach_CTX_in_cfg(
			   cfg, tls_ssl_ctx_set_read_ahead, ssl_read_ahead, 0)
			< 0) {
		ERR("invalid ssl_read_ahead value (%d)\n", ssl_read_ahead);
		return -1;
	}
	/* set options for SSL_write:
		SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER - needed when queueing
		  clear text for a future write (WANTS_READ). In this case the
		  buffer address will change for the repeated SSL_write() and
		  without this option it will trigger the openssl sanity checks.
		SSL_MODE_ENABLE_PARTIAL_WRITE - needed to deal with potentially
		  huge multi-record writes that don't fit in the default buffer
		  (the default buffer must have space for at least 1 record) */
	if(tls_foreach_CTX_in_cfg(cfg, tls_ssl_ctx_mode,
			   SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER
					   | SSL_MODE_ENABLE_PARTIAL_WRITE,
			   0)
			< 0) {
		ERR("could not set SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER and"
			" SSL_MODE_ENABLE_PARTIAL_WRITE\n");
		return -1;
	}

	return 0;
}


/**
 * @brief Create new configuration structure
 *
 * Create new configuration structure in new allocated shared memory
 * @return configuration structure or zero on error
 */
tls_domains_cfg_t *tls_new_cfg(void)
{
	tls_domains_cfg_t *r;

	r = (tls_domains_cfg_t *)shm_malloc(sizeof(tls_domains_cfg_t));
	if(!r) {
		ERR("No memory left\n");
		return 0;
	}
	memset(r, 0, sizeof(tls_domains_cfg_t));
	atomic_set(&r->ref_count, 0);
	return r;
}


/**
 * @brief Lookup TLS configuration based on type, ip, and port
 * @param cfg configuration set
 * @param type type of configuration
 * @param ip IP for configuration
 * @param port port for configuration
 * @return found configuration or default, if not found
 */
tls_domain_t *tls_lookup_cfg(tls_domains_cfg_t *cfg, int type,
		struct ip_addr *ip, unsigned short port, str *sname, str *srvid)
{
	tls_domain_t *p;
	int dotpos;

	if(type & TLS_DOMAIN_DEF) {
		if(type & TLS_DOMAIN_SRV)
			return cfg->srv_default;
		else
			return cfg->cli_default;
	} else {
		if(type & TLS_DOMAIN_SRV)
			p = cfg->srv_list;
		else
			p = cfg->cli_list;
	}

	while(p) {
		if(srvid && srvid->len > 0) {
			LM_DBG("comparing addr: [%s:%d]  [%s:%d] -- id: [%.*s] [%.*s]\n",
					ip_addr2a(&p->ip), p->port, ip_addr2a(ip), port,
					p->server_id.len, ZSW(p->server_id.s), srvid->len,
					ZSW(srvid->s));
			if(p->server_id.s && p->server_id.len == srvid->len
					&& strncasecmp(p->server_id.s, srvid->s, srvid->len) == 0) {
				LM_DBG("TLS config found by server id\n");
				return p;
			}
		}
		if(sname) {
			LM_DBG("comparing addr: l[%s:%d]  r[%s:%d] -- sni: l[%.*s] r[%.*s] "
				   "%d"
				   " -- type: %d\n",
					ip_addr2a(&p->ip), p->port, ip_addr2a(ip), port,
					p->server_name.len, ZSW(p->server_name.s), sname->len,
					ZSW(sname->s), p->server_name_mode, p->type);
		}
		if((p->type & TLS_DOMAIN_ANY)
				|| ((p->port == 0 || p->port == port)
						&& ip_addr_cmp(&p->ip, ip))) {
			if(sname && sname->s && sname->len > 0 && p->server_name.s
					&& p->server_name.len > 0) {
				if(p->server_name_mode != KSR_TLS_SNM_SUBDOM) {
					/* match sni domain */
					if(p->server_name.len == sname->len
							&& strncasecmp(
									   p->server_name.s, sname->s, sname->len)
									   == 0) {
						LM_DBG("socket+server_name based TLS server domain "
							   "found\n");
						return p;
					}
				}
				if((p->server_name_mode == KSR_TLS_SNM_INCDOM
						   || p->server_name_mode == KSR_TLS_SNM_SUBDOM)
						&& (p->server_name.len < sname->len)) {
					dotpos = sname->len - p->server_name.len - 1;
					if(sname->s[dotpos] == '.'
							&& strncasecmp(p->server_name.s,
									   sname->s + dotpos + 1,
									   p->server_name.len)
									   == 0) {
						LM_DBG("socket+server_name based TLS server sub-domain "
							   "found\n");
						return p;
					}
				}
			} else {
				if(!(p->type & TLS_DOMAIN_ANY)) {
					LM_DBG("socket based TLS server domain found\n");
					return p;
				}
			}
		}
		p = p->next;
	}

	/* No matching domain found, return default */
	if(type & TLS_DOMAIN_SRV)
		return cfg->srv_default;
	else
		return cfg->cli_default;
}


/**
 * @brief Check whether configuration domain is duplicated
 * @param cfg configuration set
 * @param d checked domain
 * @return 1 if domain is duplicated, 0 if it's not
 */
int ksr_tls_domain_duplicated(tls_domains_cfg_t *cfg, tls_domain_t *d)
{
	tls_domain_t *p;

	if(d->type & TLS_DOMAIN_DEF) {
		if(d->type & TLS_DOMAIN_SRV) {
			if(cfg->srv_default == d) {
				return 0;
			}
			return cfg->srv_default != NULL;
		} else {
			if(cfg->cli_default == d) {
				return 0;
			}
			return cfg->cli_default != NULL;
		}
	} else {
		if(d->type & TLS_DOMAIN_SRV)
			p = cfg->srv_list;
		else
			p = cfg->cli_list;
	}

	if(d->type & TLS_DOMAIN_ANY) {
		/* any address, it must have server_name for SNI */
		if(d->server_name.len == 0) {
			LM_WARN("duplicate definition for a tls profile (same address)"
					" and no server name provided\n");
			return 1;
		} else {
			return 0;
		}
	}

	while(p) {
		if(p != d) {
			if((p->port == d->port) && ip_addr_cmp(&p->ip, &d->ip)) {
				if(d->server_name.len == 0 || p->server_name.len == 0) {
					LM_WARN("duplicate definition for a tls profile (same "
							"address)"
							" and no server name provided\n");
					return 1;
				}
			}
		}
		p = p->next;
	}

	return 0;
}


/**
 * @brief Add a domain to the configuration set
 * @param cfg configuration set
 * @param d TLS domain
 * @return 1 if domain already exists, 0 after addition, -1 on error
 */
int tls_add_domain(tls_domains_cfg_t *cfg, tls_domain_t *d)
{
	if(!cfg) {
		ERR("TLS configuration structure missing\n");
		return -1;
	}

	if(d->type & TLS_DOMAIN_DEF) {
		if(d->type & TLS_DOMAIN_CLI) {
			cfg->cli_default = d;
		} else {
			cfg->srv_default = d;
		}
	} else {
		if(d->type & TLS_DOMAIN_SRV) {
			d->next = cfg->srv_list;
			cfg->srv_list = d;
		} else {
			d->next = cfg->cli_list;
			cfg->cli_list = d;
		}
	}
	return 0;
}
