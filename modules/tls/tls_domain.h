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
 * Kamailio TLS support :: virtual domain configuration support
 * @file
 * @ingroup tls
 * Module: @ref tls
 */


#ifndef _TLS_DOMAIN_H
#define _TLS_DOMAIN_H

#include "../../str.h"
#include "../../ip_addr.h"
#include "../../atomic_ops.h"
#include <openssl/ssl.h>


#define TLS_OP_SSLv2_PLUS   0
#define TLS_OP_SSLv3_PLUS   (TLS_OP_SSLv2_PLUS   | SSL_OP_NO_SSLv2)
#define TLS_OP_TLSv1_PLUS   (TLS_OP_SSLv3_PLUS   | SSL_OP_NO_SSLv3)

#ifdef SSL_OP_NO_TLSv1
#  define TLS_OP_TLSv1_1_PLUS (TLS_OP_TLSv1_PLUS   | SSL_OP_NO_TLSv1)

#  ifdef SSL_OP_NO_TLSv1_1
#    define TLS_OP_TLSv1_2_PLUS (TLS_OP_TLSv1_1_PLUS | SSL_OP_NO_TLSv1_1)
#  endif /*SSL_OP_NO_TLSv1_1*/

#endif /*SSL_OP_NO_TLSv1*/

/**
 * Available TLS methods
 */
enum tls_method {
	TLS_METHOD_UNSPEC = 0,
	TLS_USE_SSLv23_cli,
	TLS_USE_SSLv23_srv,
	TLS_USE_SSLv23,     /* any SSL/TLS version */
	TLS_USE_SSLv2_cli,
	TLS_USE_SSLv2_srv,
	TLS_USE_SSLv2,      /* only SSLv2 (deprecated) */
	TLS_USE_SSLv3_cli,
	TLS_USE_SSLv3_srv,
	TLS_USE_SSLv3,      /* only SSLv3 (insecure) */
	TLS_USE_TLSv1_cli,
	TLS_USE_TLSv1_srv,
	TLS_USE_TLSv1,      /* only TLSv1.0 */
	TLS_USE_TLSv1_1_cli,
	TLS_USE_TLSv1_1_srv,
	TLS_USE_TLSv1_1,    /* only TLSv1.1 */
	TLS_USE_TLSv1_2_cli,
	TLS_USE_TLSv1_2_srv,
	TLS_USE_TLSv1_2,    /* only TLSv1.2 */
	TLS_USE_TLSvRANGE,    /* placeholder - TLSvX ranges must be after it */
	TLS_USE_TLSv1_PLUS,   /* TLSv1.0 or greater */
	TLS_USE_TLSv1_1_PLUS, /* TLSv1.1 or greater */
	TLS_USE_TLSv1_2_PLUS, /* TLSv1.1 or greater */
	TLS_METHOD_MAX
};


/**
 * TLS configuration domain type
 */
enum tls_domain_type {
	TLS_DOMAIN_DEF = (1 << 0), /**< Default domain */
	TLS_DOMAIN_SRV = (1 << 1), /**< Server domain */
	TLS_DOMAIN_CLI = (1 << 2)  /**< Client domain */
};


/**
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
	str server_name;
	struct tls_domain* next;
} tls_domain_t;


/**
 * TLS configuration structures
 */
typedef struct tls_domains_cfg {
	tls_domain_t* srv_default; /**< Default server domain */
	tls_domain_t* cli_default; /**< Default client domain */
	tls_domain_t* srv_list;    /**< Server domain list */
	tls_domain_t* cli_list;    /**< Client domain list */
	struct tls_domains_cfg* next; /**< Next element in the garbage list */
	atomic_t ref_count;        /**< How many connections use this configuration */
} tls_domains_cfg_t;


/**
 * @brief Create a new TLS domain structure
 * 
 * Create a new domain structure in new allocated shared memory.
 * @param type domain Type
 * @param ip domain IP
 * @param port domain port
 * @return new domain
 */
tls_domain_t *tls_new_domain(int type, struct ip_addr *ip, 
			     unsigned short port);


/**
 * @brief Free all memory used by TLS configuration domain
 * @param d freed domain
 */
void tls_free_domain(tls_domain_t* d);


/**
 * @brief Generate TLS domain identifier
 * @param d printed domain
 * @return printed domain, with zero termination
 */
char* tls_domain_str(tls_domain_t* d);



/**
 * @brief Create new TLS configuration structure
 * 
 * Create new configuration structure in new allocated shared memory.
 * @return configuration structure or zero on error
 */
tls_domains_cfg_t* tls_new_cfg(void);


/**
 * @brief Add a domain to the configuration set
 * @param cfg configuration set
 * @param d TLS domain
 * @return 1 if domain already exists, 0 after addition, -1 on error
 */
int tls_add_domain(tls_domains_cfg_t* cfg, tls_domain_t* d);


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
int tls_fix_domains_cfg(tls_domains_cfg_t* cfg, tls_domain_t* srv_defaults,
				tls_domain_t* cli_defaults);


/**
 * @brief Lookup TLS configuration based on type, ip, and port
 * @param cfg configuration set
 * @param type type of configuration
 * @param ip IP for configuration
 * @param port port for configuration
 * @param sname server name
 * @return found configuration or default, if not found
 */
tls_domain_t* tls_lookup_cfg(tls_domains_cfg_t* cfg, int type,
				struct ip_addr* ip, unsigned short port, str *sname);


/**
 * @brief Free TLS configuration structure
 * @param cfg freed configuration
 */
void tls_free_cfg(tls_domains_cfg_t* cfg);


/**
 * @brief Destroy all TLS configuration data
 */
void tls_destroy_cfg(void);

#endif /* _TLS_DOMAIN_H */
