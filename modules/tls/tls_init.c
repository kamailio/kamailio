/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
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


#include <stdio.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
# include <openssl/ui.h>
#endif

 
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../tcp_init.h"
#include "../../socket_info.h"
#include "../../pt.h"
#include "tls_verify.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_init.h"

#if OPENSSL_VERSION_NUMBER < 0x00907000L
#    warning ""
#    warning "==============================================================="
#    warning " Your version of OpenSSL is < 0.9.7."
#    warning " Upgrade for better compatibility, features and security fixes!"
#    warning "==============================================================="
#    warning ""
#endif

#if OPENSSL_VERSION_NUMBER >= 0x00908000L  /* 0.9.8*/
#    ifndef OPENSSL_NO_COMP
#        warning "openssl zlib compression not supported, replacing with our version"
/*       #define TLS_DISABLE_COMPRESSION */
#        define TLS_FIX_ZLIB_COMPRESSION
#        include "fixed_c_zlib.h"
#    endif
#endif


static SSL_METHOD* ssl_methods[TLS_USE_SSLv23 + 1];


/*
 * Wrappers around SER shared memory functions
 * (which can be macros)
 */
static void* ser_malloc(size_t size)
{
	return shm_malloc(size);
}


static void* ser_realloc(void *ptr, size_t size)
{
	return shm_realloc(ptr, size);
}


static void ser_free(void *ptr)
{
	shm_free(ptr);
}


static int passwd_cb(char *buf, int size, int rwflag, void *filename)
{
#if OPENSSL_VERSION_NUMBER >= 0x00907000L	
	UI             *ui;
	const char     *prompt;
	
	ui = UI_new();
	if (ui == NULL)
		goto err;

	prompt = UI_construct_prompt(ui, "passphrase", filename);
	UI_add_input_string(ui, prompt, 0, buf, 0, size - 1);
	UI_process(ui);
	UI_free(ui);
	return strlen(buf);
 
 err:
	ERR("passwd_cb: Error in passwd_cb\n");
	if (ui) {
		UI_free(ui);
	}
	return 0;
	
#else
	if (des_read_pw_string(buf, size-1, "Enter Private Key password:", 0)) {
		ERR("Error in passwd_cb\n");
		return 0;
	}
	return strlen(buf);
#endif
}


/*
 * Initialize TLS socket
 */
int tls_init(struct socket_info *si)
{
	int ret;
	     /*
	      * reuse tcp initialization 
	      */
	ret = tcp_init(si);
	if (ret != 0) {
		ERR("Error while initializing TCP part of TLS socket %.*s:%d\n",
		    si->address_str.len, si->address_str.s, si->port_no);
		goto error;
	}
	
	si->proto = PROTO_TLS;
	return 0;
	
 error:
	if (si->socket != -1) {
		close(si->socket);
		si->socket = -1;
	}
	return ret;
}


#define NUM_RETRIES 3
/*
 * load a private key from a file 
 */
static int load_private_key(tls_domain_t* d)
{
	int idx, ret_pwd, i;
	
	if (!d->pkey_file) {
		DBG("%s: No private key specified\n", tls_domain_str(d));
		return 0;
	}

	for(i = 0; i < process_count; i++) {
		SSL_CTX_set_default_passwd_cb((*d->ctx)[i], passwd_cb);
		SSL_CTX_set_default_passwd_cb_userdata((*d->ctx)[i], d->pkey_file);
		
		for(idx = 0, ret_pwd = 0; idx < NUM_RETRIES; idx++) {
			ret_pwd = SSL_CTX_use_PrivateKey_file((*d->ctx)[i], d->pkey_file, SSL_FILETYPE_PEM);
			if (ret_pwd) {
				break;
			} else {
				ERR("%s: Unable to load private key '%s'\n",
				    tls_domain_str(d), d->pkey_file);
				TLS_ERR("load_private_key:");
				continue;
			}
		}
		
		if (!ret_pwd) {
			ERR("%s: Unable to load private key file '%s'\n", 
			    tls_domain_str(d), d->pkey_file);
			TLS_ERR("load_private_key:");
			return -1;
		}
		
		if (!SSL_CTX_check_private_key((*d->ctx)[i])) {
			ERR("%s: Key '%s' does not match the public key of the certificate\n", 
			    tls_domain_str(d), d->pkey_file);
			TLS_ERR("load_private_key:");
			return -1;
		}
	}		

	DBG("%s: Key '%s' successfuly loaded\n",
	    tls_domain_str(d), d->pkey_file);
	return 0;
}


/* 
 * Load CA list from file 
 */
static int load_ca_list(tls_domain_t* d)
{
	int i;

	if (!d->ca_file) {
		DBG("%s: No CA list configured\n", tls_domain_str(d));
		return 0;
	}

	for(i = 0; i < process_count; i++) {
		if (SSL_CTX_load_verify_locations((*d->ctx)[i], d->ca_file, 0) != 1) {
			ERR("%s: Unable to load CA list '%s'\n", tls_domain_str(d), d->ca_file);
			TLS_ERR("load_ca_list:");
			return -1;
		}
		SSL_CTX_set_client_CA_list((*d->ctx)[i], SSL_load_client_CA_file(d->ca_file));
		if (SSL_CTX_get_client_CA_list((*d->ctx)[i]) == 0) {
			ERR("%s: Error while setting client CA list\n", tls_domain_str(d));
			TLS_ERR("load_ca_list:");
			return -1;
		}
	}
	return 0;
}


/* 
 * Load certificate from file 
 */
static int load_cert(tls_domain_t* d)
{
	int i;

	if (!d->cert_file) {
		DBG("%s: No certificate configured\n", tls_domain_str(d));
		return 0;
	}

	for(i = 0; i < process_count; i++) {
		if (!SSL_CTX_use_certificate_chain_file((*d->ctx)[i], d->cert_file)) {
			ERR("%s: Unable to load certificate file '%s'\n",
			    tls_domain_str(d), d->cert_file);
			TLS_ERR("load_cert:");
			return -1;
		}
		
	}
	return 0;
}


/*
 * initialize ssl methods 
 */
static void init_ssl_methods(void)
{
	ssl_methods[TLS_USE_SSLv2_cli - 1] = SSLv2_client_method();
	ssl_methods[TLS_USE_SSLv2_srv - 1] = SSLv2_server_method();
	ssl_methods[TLS_USE_SSLv2 - 1] = SSLv2_method();
	
	ssl_methods[TLS_USE_SSLv3_cli - 1] = SSLv3_client_method();
	ssl_methods[TLS_USE_SSLv3_srv - 1] = SSLv3_server_method();
	ssl_methods[TLS_USE_SSLv3 - 1] = SSLv3_method();
	
	ssl_methods[TLS_USE_TLSv1_cli - 1] = TLSv1_client_method();
	ssl_methods[TLS_USE_TLSv1_srv - 1] = TLSv1_server_method();
	ssl_methods[TLS_USE_TLSv1 - 1] = TLSv1_method();
	
	ssl_methods[TLS_USE_SSLv23_cli - 1] = SSLv23_client_method();
	ssl_methods[TLS_USE_SSLv23_srv - 1] = SSLv23_server_method();
	ssl_methods[TLS_USE_SSLv23 - 1] = SSLv23_method();
}


/* 
 * Configure cipher list 
 */
static int set_cipher_list(tls_domain_t* d)
{
	int i;

	if (!d->cipher_list) return 0;
	for(i = 0; i < process_count; i++) {
		if (SSL_CTX_set_cipher_list((*d->ctx)[i], d->cipher_list) == 0 ) {
			ERR("%s: Failure to set SSL context cipher list\n", tls_domain_str(d));
			return -1;
		}
	}
	return 0;
}


/* 
 * Enable/disable certificate verification 
 */
static int set_verification(tls_domain_t* d)
{
	int verify_mode, i;

	if (d->require_cert) {
		verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
		INFO("%s: %s MUST present valid certificate\n", 
		     tls_domain_str(d), d->type & TLS_DOMAIN_SRV ? "Client" : "Server");
	} else {
		if (d->verify_cert) {
			verify_mode = SSL_VERIFY_PEER;
			if (d->type & TLS_DOMAIN_SRV) {
				INFO("%s: IF client provides certificate then it MUST be valid\n", 
				     tls_domain_str(d));
			} else {
				INFO("%s: Server MUST present valid certificate\n", 
				     tls_domain_str(d));
			}
		} else {
			verify_mode = SSL_VERIFY_NONE;
			if (d->type & TLS_DOMAIN_SRV) {
				INFO("%s: No client certificate required and no checks performed\n", 
				     tls_domain_str(d));
			} else {
				INFO("%s: Server MAY present invalid certificate\n", 
				     tls_domain_str(d));
			}
		}
	}
	
	for(i = 0; i < process_count; i++) {
		SSL_CTX_set_verify((*d->ctx)[i], verify_mode, 0);
		SSL_CTX_set_verify_depth((*d->ctx)[i], d->verify_depth);
		
	}
	return 0;
}


/* 
 * Configure generic SSL parameters 
 */
static int set_ssl_options(tls_domain_t* d)
{
	int i;
	for(i = 0; i < process_count; i++) {
#if OPENSSL_VERSION_NUMBER >= 0x000907000
		SSL_CTX_set_options((*d->ctx)[i], 
				    SSL_OP_ALL | SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE);
#else
		SSL_CTX_set_options((*d->ctx)[i], 
				    SSL_OP_ALL);
#endif
	}
	return 0;
}


/* 
 * Configure session cache parameters 
 */
static int set_session_cache(tls_domain_t* d)
{
	int i;
	for(i = 0; i < process_count; i++) {
		     /* janakj: I am not sure if session cache makes sense in ser, session 
		      * cache is stored in SSL_CTX and we have one SSL_CTX per process, thus 
		      * sessions among processes will not be reused
		      */
		SSL_CTX_set_session_cache_mode((*d->ctx)[i], 
				   tls_session_cache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);
		SSL_CTX_set_session_id_context((*d->ctx)[i], 
					       (unsigned char*)tls_session_id.s, tls_session_id.len);
	}
	return 0;
}


/*
 * Fix openssl compression bugs if necessary
 */
static int init_tls_compression(void)
{
	int n, r;
#if defined(TLS_DISABLE_COMPRESSION) || defined(TLS_FIX_ZLIB_COMPRESSION)
	STACK_OF(SSL_COMP)* comp_methods;
#    ifdef TLS_FIX_ZLIB_COMPRESSION
	SSL_COMP* zlib_comp;
#    endif
#endif

	     /* disabling compression */
#if defined(TLS_DISABLE_COMPRESSION) || defined(TLS_FIX_ZLIB_COMPRESSION)
#    ifndef SSL_COMP_ZLIB_IDX
#        define SSL_COMP_ZLIB_IDX 1 /* openssl/ssl/ssl_ciph.c:84 */
#    endif 
	LOG(L_INFO, "init_tls: fixing compression problems...\n");
	comp_methods = SSL_COMP_get_compression_methods();
	if (comp_methods == 0) {
		LOG(L_CRIT, "init_tls: BUG: null openssl compression methods\n");
	} else {
#ifdef TLS_DISABLE_COMPRESSION
		LOG(L_INFO, "init_tls: disabling compression...\n");
		sk_SSL_COMP_zero(comp_methods);
#else
		LOG(L_INFO, "init_tls: fixing zlib compression...\n");
		if (fixed_c_zlib_init() != 0) {
			LOG(L_CRIT, "init_tls: BUG: failed to initialize zlib compression"
			    " fix\n");
			sk_SSL_COMP_zero(comp_methods); /* delete compression */
		} else {
			     /* the above SSL_COMP_get_compression_methods() call has the side
			      * effect of initializing the compression stack (if not already
			      * initialized) => after it zlib is initialized and in the stack */
			     /* find zlib_comp (cannot use ssl3_comp_find, not exported) */
			n = sk_SSL_COMP_num(comp_methods);
			zlib_comp = 0;
			for (r = 0; r < n; r++) {
				zlib_comp = sk_SSL_COMP_value(comp_methods, r);
				if (zlib_comp->id == SSL_COMP_ZLIB_IDX) {
					break /* found */;
				} else {
					zlib_comp = 0;
				}
			}
			if (zlib_comp == 0) {
				LOG(L_INFO, "init_tls: no openssl zlib compression found\n");
			}
			     /* "fix" it */
			zlib_comp->method = &zlib_method;
		}
#endif
	}
#endif
	return 0;
}


/*
 * First step of TLS initialization
 */
int init_tls(void)
{
	struct socket_info* si;
	tls_domain_t* d;

#if OPENSSL_VERSION_NUMBER < 0x00907000L
	WARN("You are using an old version of OpenSSL (< 0.9.7). Upgrade!\n");
#endif

	     /*
	      * this has to be called before any function calling CRYPTO_malloc,
	      * CRYPTO_malloc will set allow_customize in openssl to 0 
	      */
	if (!CRYPTO_set_mem_functions(ser_malloc, ser_realloc, ser_free)) {
		ERR("Unable to set the memory allocation functions\n");
		return -1;
	}

	init_tls_compression();
	SSL_library_init();
	SSL_load_error_strings();
	init_ssl_methods();

	if (tls_fix_domains() < 0) return -1;

	     /* Now initialize TLS sockets */
	for(si = tls_listen; si; si = si->next) {
		if (tls_init(si) < 0)  return -1;
		     /* get first ipv4/ipv6 socket*/
		if ((si->address.af == AF_INET) &&
		    ((sendipv4_tls == 0) || (sendipv4_tls->flags & SI_IS_LO))) {
			sendipv4_tls = si;
		}
#ifdef USE_IPV6
		if ((sendipv6_tls == 0) && (si->address.af == AF_INET6)) {
			sendipv6_tls = si;
		}
#endif
	}

	     /* Make sure that all configured domains have a corresponding listeining
	      * socket
	      */
	d = tls_srv_list;
	while(d) {
		if (d->ip.len && !find_si(&d->ip, d->port, PROTO_TLS)) {
			ERR("%s: No listening socket found\n", tls_domain_str(d));
			return -1;
		}
		d = d->next;
	}

	return 0;
}


/*
 * Second step of TLS initialization
 */
int init_tls_child(void)
{
	tls_domain_t* lists[4] = {tls_def_cli, tls_def_srv, tls_cli_list, tls_srv_list};
	tls_domain_t* d;
	int i, j;

	for(j = 0; j < 4; j++) {
		d = lists[j];

		while (d) {
			*d->ctx = (SSL_CTX**)shm_malloc(sizeof(SSL_CTX*) * process_count);
			if (!*d->ctx) {
				ERR("%s: Cannot allocate shared memory\n", tls_domain_str(d));
				return -1;
			}
			memset(*d->ctx, 0, sizeof(SSL_CTX*) * process_count);
			for(i = 0; i < process_count; i++) {
				(*d->ctx)[i] = SSL_CTX_new(ssl_methods[d->method - 1]);
				if ((*d->ctx)[i] == NULL) {
					ERR("%s: Cannot create SSL context\n", tls_domain_str(d));
					return -1;
				}
			}
			
			if (load_cert(d) < 0) return -1;
			if (load_ca_list(d) < 0) return -1;
			if (set_cipher_list(d) < 0) return -1;
			if (set_verification(d) < 0) return -1;
			if (set_ssl_options(d) < 0) return -1;
			if (set_session_cache(d) < 0) return -1;
			d = d->next;
		}

		d = lists[j];
		while (d) {
			if (load_private_key(d) < 0) return -1;
			d = d->next;
		}
	}

        return 0;
}


/*
 * TLS cleanup when SER exits
 */
void destroy_tls(void)
{
	tls_free_domains();
	ERR_free_strings();
}
