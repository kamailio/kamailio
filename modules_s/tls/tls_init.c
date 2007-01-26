/*
 * $Id$
 *
 * TLS module - OpenSSL initialization funtions
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


SSL_METHOD* ssl_methods[TLS_USE_SSLv23 + 1];


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
			}else{
				/* "fix" it */
				zlib_comp->method = &zlib_method;
			}
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
	if (tls_init_locks()<0)
		return -1;
	init_tls_compression();
	SSL_library_init();
	SSL_load_error_strings();
	init_ssl_methods();

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

	return 0;
}


/*
 * Make sure that all server domains in the configuration have corresponding
 * listening socket in SER
 */
int tls_check_sockets(tls_cfg_t* cfg)
{
	tls_domain_t* d;

	if (!cfg) return 0;

	d = cfg->srv_list;
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
 * TLS cleanup when SER exits
 */
void destroy_tls(void)
{
	ERR_free_strings();
	/* TODO: free all the ctx'es */
	tls_destroy_locks();
}
