/*
 * $Id$
 *
 * TLS module - virtual configuration domain support
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
	char* cert_file;
	char* pkey_file;
	int verify_cert;
	int verify_depth;
	char* ca_file;
        int require_cert;
	char* cipher_list;
	enum tls_method method;
	struct tls_domain* next;
} tls_domain_t;


/*
 * TLS configuration structures
 */
typedef struct tls_cfg {
	tls_domain_t* srv_default; /* Default server domain */
	tls_domain_t* cli_default; /* Default client domain */
	tls_domain_t* srv_list;    /* Server domain list */
	tls_domain_t* cli_list;    /* Client domain list */
	struct tls_cfg* next;      /* Next element in the garbage list */
	int ref_count;             /* How many connections use this configuration */
} tls_cfg_t;


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
tls_cfg_t* tls_new_cfg(void);


/*
 * Add a new configuration domain
 */
int tls_add_domain(tls_cfg_t* cfg, tls_domain_t* d);


/*
 * Fill in missing parameters
 */
int tls_fix_cfg(tls_cfg_t* cfg, tls_domain_t* srv_defaults, tls_domain_t* cli_defaults);


/*
 * Lookup TLS configuration
 */
tls_domain_t* tls_lookup_cfg(tls_cfg_t* cfg, int type, struct ip_addr* ip, unsigned short port);


/*
 * Free TLS configuration data
 */
void tls_free_cfg(tls_cfg_t* cfg);

/*
 * Destroy all the config data
 */
void tls_destroy_cfg(void);

#endif /* _TLS_DOMAIN_H */
