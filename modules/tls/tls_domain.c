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

#include <stdlib.h>
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../pt.h"
#include "tls_server.h"
#include "tls_domain.h"

tls_domain_t* tls_def_srv = 0;
tls_domain_t* tls_def_cli = 0;
tls_domain_t* tls_srv_list = 0;
tls_domain_t* tls_cli_list = 0;


/*
 * find domain with given ip and port 
 */
tls_domain_t* tls_find_domain(int type, struct ip_addr *ip, unsigned short port)
{
	tls_domain_t *p;

	if (type & TLS_DOMAIN_DEF) {
		if (type & TLS_DOMAIN_SRV) return tls_def_srv;
		else return tls_def_cli;
	} else {
		if (type & TLS_DOMAIN_SRV) p = tls_srv_list;
		else p = tls_cli_list;
	}

	while (p) {
		if ((p->port == port) && ip_addr_cmp(&p->ip, ip))
			return p;
		p = p->next;
	}
	return 0;
}


/*
 * create a new domain 
 */
tls_domain_t* tls_new_domain(int type, struct ip_addr *ip, unsigned short port)
{
	tls_domain_t* d;

	d = pkg_malloc(sizeof(tls_domain_t));
	if (d == NULL) {
		ERR("Memory allocation failure\n");
		return 0;
	}
	memset(d, '\0', sizeof(tls_domain_t));

	d->type = type;
	if (type & TLS_DOMAIN_DEF) {
		if (type & TLS_DOMAIN_SRV) {
			     /* Default server domain */
			d->cert_file = TLS_CERT_FILE;
			d->pkey_file = TLS_PKEY_FILE;
			d->verify_cert = 0;
			d->verify_depth = 3;
			d->ca_file = TLS_CA_FILE;
			d->require_cert = 0;
			d->method = TLS_USE_TLSv1;
			tls_def_srv = d;
		} else {
			     /* Default client domain */
			d->cert_file = 0;
			d->pkey_file = 0;
			d->verify_cert = 0;
			d->verify_depth = 3;
			d->ca_file = TLS_CA_FILE;
			d->require_cert = 1;
			d->method = TLS_USE_TLSv1;
			tls_def_cli = d;
		}		
	} else {
		memcpy(&d->ip, ip, sizeof(struct ip_addr));
		d->port = port;
		d->verify_cert = -1;
		d->verify_depth = -1;
		d->require_cert = -1;

		if (type & TLS_DOMAIN_SRV) {
			d->next = tls_srv_list;
			tls_srv_list = d;
		} else {
			d->next = tls_cli_list;
			tls_cli_list = d;
		}
	}
	return d;
}


static void free_domain(tls_domain_t* d)
{
	int i;
	if (!d) return;
	if (d->ctx) {
		if (*d->ctx) {
			for(i = 0; i < process_count; i++) {
				if ((*d->ctx)[i]) SSL_CTX_free((*d->ctx)[i]);
			}
			shm_free(*d->ctx);
		}
		shm_free(d->ctx);
	}
	pkg_free(d);
}


/*
 * clean up 
 */
void tls_free_domains(void)
{
	tls_domain_t* p;
	while(tls_srv_list) {
		p = tls_srv_list;
		tls_srv_list = tls_srv_list->next;
		free_domain(p);
	}
	while(tls_cli_list) {
		p = tls_srv_list;
		tls_srv_list = tls_srv_list->next;
		free_domain(p);
	}
	if (tls_def_srv) free_domain(tls_def_srv);
	if (tls_def_cli) free_domain(tls_def_cli);
}


/*
 * Print TLS domain identifier
 */
char* tls_domain_str(tls_domain_t* d)
{
	static char buf[1024];
	char* p;

	buf[0] = '\0';
	p = buf;
	p = strcat(p, d->type & TLS_DOMAIN_SRV ? "TLSs<" : "TLSc<");
	if (d->ip.len) {
		p = strcat(p, ip_addr2a(&d->ip));
		p = strcat(p, ":");
		p = strcat(p, int2str(d->port, 0));
		p = strcat(p, ">");
	} else {
		p = strcat(p, "default>");
	}
	return buf;
}


/*
 * Initialize all domain attributes from default domains
 * if necessary
 */
static int fix_domain(tls_domain_t* d, tls_domain_t* def)
{
	d->ctx = (SSL_CTX***)shm_malloc(sizeof(SSL_CTX**));
	if (!d->ctx) {
		ERR("No shared memory left\n");
		return -1;
	}
	*d->ctx = 0;

	if (d->method == TLS_METHOD_UNSPEC) {
		INFO("%s: Method not configured, using default value %d\n",
		     tls_domain_str(d), def->method);
		d->method = def->method;
	} else {
		INFO("%s: using TLS method %d\n",
		     tls_domain_str(d), d->method);
	}
	
	if (d->method < 1 || d->method >= TLS_METHOD_MAX) {
		ERR("%s: Invalid TLS method value\n", tls_domain_str(d));
		return -1;
	}
	
	if (!d->cert_file) {
		INFO("%s: No certificate configured, using default '%s'\n",
		     tls_domain_str(d), def->cert_file);
		d->cert_file = def->cert_file;
	} else {
		INFO("%s: using certificate '%s'\n",
		     tls_domain_str(d), d->cert_file);
	}
	
	if (!d->ca_file) {
		INFO("%s: No CA list configured, using default '%s'\n",
		     tls_domain_str(d), def->ca_file);
		d->ca_file = def->ca_file;
	} else {
		INFO("%s: using CA list '%s'\n",
		     tls_domain_str(d), d->ca_file);
	}
	
	if (d->require_cert == -1) {
		INFO("%s: require_certificate not configured, using default value %d\n",
		     tls_domain_str(d), def->require_cert);
		d->require_cert = def->require_cert;
	} else {
		INFO("%s: require_certificate = %d\n",
		     tls_domain_str(d), d->require_cert);
	}
	
	if (!d->cipher_list) {
		INFO("%s: Cipher list not configured, using default value %s\n",
		     tls_domain_str(d), def->cipher_list);
		d->cipher_list = def->cipher_list;
	} else {
		INFO("%s: using cipher list %s\n",
		     tls_domain_str(d), d->cipher_list);
	}
	
	if (!d->pkey_file) {
		INFO("%s: No private key configured, using default '%s'\n",
		     tls_domain_str(d), def->pkey_file);
		d->pkey_file = def->pkey_file;
	} else {
		INFO("%s: using private key '%s'\n",
		     tls_domain_str(d), d->pkey_file);
	}
	
	if (d->verify_cert == -1) {
		INFO("%s: verify_certificate not configured, using default value %d\n",
		     tls_domain_str(d), def->verify_cert);
		d->verify_cert = def->verify_cert;
	} else {
		INFO("%s: using verify_certificate = %d\n",
		     tls_domain_str(d), d->verify_cert);
	}
	
	if (d->verify_depth == -1) {
		INFO("%s: verify_depth not configured, using default value %d\n",
		     tls_domain_str(d), def->verify_depth);
		d->verify_depth = def->verify_depth;
	} else {
		INFO("%s: using verify_depth = %d\n",
		     tls_domain_str(d), d->verify_depth);
	}
	return 0;
}


/*
 * Initialize attributes of all domains from default domains
 * if necessary
 */
int tls_fix_domains(void)
{
	tls_domain_t* d;

	if (!tls_def_cli) tls_def_cli = tls_new_domain(TLS_DOMAIN_DEF | TLS_DOMAIN_CLI, 0, 0);
	if (!tls_def_srv) tls_def_srv = tls_new_domain(TLS_DOMAIN_DEF | TLS_DOMAIN_SRV, 0, 0);

	d = tls_srv_list;
	while (d) {
		if (fix_domain(d, tls_def_srv) < 0) return -1;
		d = d->next;
	}

	d = tls_cli_list;
	while (d) {
		if (fix_domain(d, tls_def_cli) < 0) return -1;
		d = d->next;
	}
	if (fix_domain(tls_def_srv, tls_def_srv) < 0) return -1;
	if (fix_domain(tls_def_cli, tls_def_cli) < 0) return -1;
	return 0;
}
