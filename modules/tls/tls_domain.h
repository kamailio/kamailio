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

#ifndef _TLS_DOMAIN_H
#define _TLS_DOMAIN_H

#include "../../str.h"
#include "../../ip_addr.h"
#include <openssl/ssl.h>


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
	SSL_CTX*** ctx;     /* Pointer to the array is stored in shm mem */
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

extern tls_domain_t* tls_def_srv; /* Default server domain */
extern tls_domain_t* tls_def_cli; /* Default client domain */
extern tls_domain_t* tls_srv_list;
extern tls_domain_t* tls_cli_list;

/*
 * find domain with given ip and port, if ip == NULL then the
 * default domain will be returned
 */
tls_domain_t *tls_find_domain(int type, struct ip_addr *ip,
			      unsigned short port);

/*
 * create a new domain 
 */
tls_domain_t *tls_new_domain(int type, struct ip_addr *ip, 
			     unsigned short port);

/*
 * clean up 
 */
void tls_free_domains(void);


/*
 * Generate tls domain string identifier
 */
char* tls_domain_str(tls_domain_t* d);

/*
 * Fill in missing parameters
 */
int tls_fix_domains(void);

#endif /* _TLS_DOMAIN_H */
