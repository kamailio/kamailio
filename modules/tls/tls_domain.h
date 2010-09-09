/*
 * $Id$
 *
 * TLS module - virtual configuration domain support
 * 
 * Copyright (C) 2001-2003 FhG FOKUS
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
/** SIP-router TLS support :: virtual configuration domain support.
 * @file tls_domain.h
 * @ingroup tls
 * Module: @ref tls
 */


#ifndef _TLS_DOMAIN_H
#define _TLS_DOMAIN_H

#include "../../str.h"
#include "../../ip_addr.h"
#include <openssl/ssl.h>


/*
 * Available TLS methods
 */
enum tls_method {
	TLS_METHOD_UNSPEC = 0,
	TLS_USE_SSLv2_cli,
	TLS_USE_SSLv2_srv,
	TLS_USE_SSLv2,
	TLS_USE_SSLv3_cli,
	TLS_USE_SSLv3_srv,
	TLS_USE_SSLv3,
	TLS_USE_TLSv1_cli,
	TLS_USE_TLSv1_srv,
	TLS_USE_TLSv1,
	TLS_USE_SSLv23_cli,
	TLS_USE_SSLv23_srv,
	TLS_USE_SSLv23,
	TLS_METHOD_MAX
};


/*
 * TLS configuration domain type
 */
enum tls_domain_type {
	TLS_DOMAIN_DEF = (1 << 0), /* Default domain */
	TLS_DOMAIN_SRV = (1 << 1), /* Server domain */
	TLS_DOMAIN_CLI = (1 << 2)  /* Client domain */
};


/*
 * separate configuration per ip:port
 */
typedef struct tls_domain {
	int type;
	struct ip_addr ip;
	unsigned short port;
	SSL_CTX** ctx;
	str cert_file;
	str pkey_file;
	int verify_cert;
	int verify_depth;
	str ca_file;
	int require_cert;
	str cipher_list;
	enum tls_method method;
	str crl_file;
	struct tls_domain* next;
} tls_domain_t;


/*
 * TLS configuration structures
 */
typedef struct tls_domains_cfg {
	tls_domain_t* srv_default; /* Default server domain */
	tls_domain_t* cli_default; /* Default client domain */
	tls_domain_t* srv_list;    /* Server domain list */
	tls_domain_t* cli_list;    /* Client domain list */
	struct tls_domains_cfg* next; /* Next element in the garbage list */
	int ref_count;             /* How many connections use this configuration */
} tls_domains_cfg_t;


/*
 * create a new domain 
 */
tls_domain_t *tls_new_domain(int type, struct ip_addr *ip, 
			     unsigned short port);


/*
 * Free all memory used for configuration domain
 */
void tls_free_domain(tls_domain_t* d);


/*
 * Generate tls domain string identifier
 */
char* tls_domain_str(tls_domain_t* d);



/*
 * Create new instance of TLS configuration data
 */
tls_domains_cfg_t* tls_new_cfg(void);


/*
 * Add a new configuration domain
 */
int tls_add_domain(tls_domains_cfg_t* cfg, tls_domain_t* d);


/*
 * Fill in missing parameters
 */
int tls_fix_domains_cfg(tls_domains_cfg_t* cfg, tls_domain_t* srv_defaults,
				tls_domain_t* cli_defaults);


/*
 * Lookup TLS configuration
 */
tls_domain_t* tls_lookup_cfg(tls_domains_cfg_t* cfg, int type,
								struct ip_addr* ip, unsigned short port);


/*
 * Free TLS configuration data
 */
void tls_free_cfg(tls_domains_cfg_t* cfg);

/*
 * Destroy all the config data
 */
void tls_destroy_cfg(void);

#endif /* _TLS_DOMAIN_H */
