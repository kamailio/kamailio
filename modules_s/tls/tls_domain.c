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

#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
# include <openssl/ui.h>
#endif
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../pt.h"
#include "tls_server.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_init.h"
#include "tls_domain.h"


/*
 * create a new domain 
 */
tls_domain_t* tls_new_domain(int type, struct ip_addr *ip, unsigned short port)
{
	tls_domain_t* d;

	d = shm_malloc(sizeof(tls_domain_t));
	if (d == NULL) {
		ERR("Memory allocation failure\n");
		return 0;
	}
	memset(d, '\0', sizeof(tls_domain_t));

	d->type = type;
	if (ip) memcpy(&d->ip, ip, sizeof(struct ip_addr));
	d->port = port;
	d->verify_cert = -1;
	d->verify_depth = -1;
	d->require_cert = -1;
	return d;
/*
 error:
	shm_free(d);
	return 0; */
}


/*
 * Free all memory used by configuration domain
 */
void tls_free_domain(tls_domain_t* d)
{
	int i;
	int procs_no;
	
	if (!d) return;
	if (d->ctx) {
		procs_no=get_max_procs();
		for(i = 0; i < procs_no; i++) {
			if (d->ctx[i]) SSL_CTX_free(d->ctx[i]);
		}
		shm_free(d->ctx);
	}

	if (d->cipher_list) shm_free(d->cipher_list);
	if (d->ca_file) shm_free(d->ca_file);
	if (d->pkey_file) shm_free(d->pkey_file);
	if (d->cert_file) shm_free(d->cert_file);
	shm_free(d);
}


/*
 * clean up 
 */
void tls_free_cfg(tls_cfg_t* cfg)
{
	tls_domain_t* p;
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
	if (cfg->srv_default) tls_free_domain(cfg->srv_default);
	if (cfg->cli_default) tls_free_domain(cfg->cli_default);
}



void tls_destroy_cfg(void)
{
	tls_cfg_t* ptr;

	if (tls_cfg_lock) {
		lock_destroy(tls_cfg_lock);
		lock_dealloc(tls_cfg_lock);
	}

	if (tls_cfg) {
		while(*tls_cfg) {
			ptr = *tls_cfg;
			*tls_cfg = (*tls_cfg)->next;
			tls_free_cfg(ptr);
		}
		
		shm_free(tls_cfg);
	}
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
	if (d->type & TLS_DOMAIN_DEF) {
		p = strcat(p, "default>");
	} else {
		p = strcat(p, ip_addr2a(&d->ip));
		p = strcat(p, ":");
		p = strcat(p, int2str(d->port, 0));
		p = strcat(p, ">");
	}
	return buf;
}


/*
 * Initialize parameters that have not been configured from
 * parent domain (usualy one of default domains
 */
static int fill_missing(tls_domain_t* d, tls_domain_t* parent)
{
	if (d->method == TLS_METHOD_UNSPEC) d->method = parent->method;
	LOG(L_INFO, "%s: tls_method=%d\n", tls_domain_str(d), d->method);
	
	if (d->method < 1 || d->method >= TLS_METHOD_MAX) {
		ERR("%s: Invalid TLS method value\n", tls_domain_str(d));
		return -1;
	}
	
	if (!d->cert_file && 
	    shm_asciiz_dup(&d->cert_file, parent->cert_file) < 0) return -1;
	LOG(L_INFO, "%s: certificate='%s'\n", tls_domain_str(d), d->cert_file);
	
	if (!d->ca_file &&
	    shm_asciiz_dup(&d->ca_file, parent->ca_file) < 0) return -1;
	LOG(L_INFO, "%s: ca_list='%s'\n", tls_domain_str(d), d->ca_file);
	
	if (d->require_cert == -1) d->require_cert = parent->require_cert;
	LOG(L_INFO, "%s: require_certificate=%d\n", tls_domain_str(d), d->require_cert);
	
	if (!d->cipher_list &&
	    shm_asciiz_dup(&d->cipher_list, parent->cipher_list) < 0) return -1;
	LOG(L_INFO, "%s: cipher_list='%s'\n", tls_domain_str(d), d->cipher_list);
	
	if (!d->pkey_file &&
	    shm_asciiz_dup(&d->pkey_file, parent->pkey_file) < 0) return -1;
	LOG(L_INFO, "%s: private_key='%s'\n", tls_domain_str(d), d->pkey_file);
	
	if (d->verify_cert == -1) d->verify_cert = parent->verify_cert;
	LOG(L_INFO, "%s: verify_certificate=%d\n", tls_domain_str(d), d->verify_cert);
	
	if (d->verify_depth == -1) d->verify_depth = parent->verify_depth;
	LOG(L_INFO, "%s: verify_depth=%d\n", tls_domain_str(d), d->verify_depth);

	return 0;
}


/* 
 * Load certificate from file 
 */
static int load_cert(tls_domain_t* d)
{
	int i;
	int procs_no;

	if (!d->cert_file) {
		DBG("%s: No certificate configured\n", tls_domain_str(d));
		return 0;
	}

	procs_no=get_max_procs();
	for(i = 0; i < procs_no; i++) {
		if (!SSL_CTX_use_certificate_chain_file(d->ctx[i], d->cert_file)) {
			ERR("%s: Unable to load certificate file '%s'\n",
			    tls_domain_str(d), d->cert_file);
			TLS_ERR("load_cert:");
			return -1;
		}
		
	}
	return 0;
}


/* 
 * Load CA list from file 
 */
static int load_ca_list(tls_domain_t* d)
{
	int i;
	int procs_no;

	if (!d->ca_file) {
		DBG("%s: No CA list configured\n", tls_domain_str(d));
		return 0;
	}

	procs_no=get_max_procs();
	for(i = 0; i < procs_no; i++) {
		if (SSL_CTX_load_verify_locations(d->ctx[i], d->ca_file, 0) != 1) {
			ERR("%s: Unable to load CA list '%s'\n", tls_domain_str(d), d->ca_file);
			TLS_ERR("load_ca_list:");
			return -1;
		}
		SSL_CTX_set_client_CA_list(d->ctx[i], SSL_load_client_CA_file(d->ca_file));
		if (SSL_CTX_get_client_CA_list(d->ctx[i]) == 0) {
			ERR("%s: Error while setting client CA list\n", tls_domain_str(d));
			TLS_ERR("load_ca_list:");
			return -1;
		}
	}
	return 0;
}


/* 
 * Configure cipher list 
 */
static int set_cipher_list(tls_domain_t* d)
{
	int i;
	int procs_no;

	if (!d->cipher_list) return 0;
	procs_no=get_max_procs();
	for(i = 0; i < procs_no; i++) {
		if (SSL_CTX_set_cipher_list(d->ctx[i], d->cipher_list) == 0 ) {
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
	int procs_no;

	if (d->require_cert) {
		verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
		LOG(L_INFO, "%s: %s MUST present valid certificate\n", 
		     tls_domain_str(d), d->type & TLS_DOMAIN_SRV ? "Client" : "Server");
	} else {
		if (d->verify_cert) {
			verify_mode = SSL_VERIFY_PEER;
			if (d->type & TLS_DOMAIN_SRV) {
				LOG(L_INFO, "%s: IF client provides certificate then it MUST be valid\n", 
				     tls_domain_str(d));
			} else {
				LOG(L_INFO, "%s: Server MUST present valid certificate\n", 
				     tls_domain_str(d));
			}
		} else {
			verify_mode = SSL_VERIFY_NONE;
			if (d->type & TLS_DOMAIN_SRV) {
				LOG(L_INFO, "%s: No client certificate required and no checks performed\n", 
				     tls_domain_str(d));
			} else {
				LOG(L_INFO, "%s: Server MAY present invalid certificate\n", 
				     tls_domain_str(d));
			}
		}
	}
	
	procs_no=get_max_procs();
	for(i = 0; i < procs_no; i++) {
		SSL_CTX_set_verify(d->ctx[i], verify_mode, 0);
		SSL_CTX_set_verify_depth(d->ctx[i], d->verify_depth);
		
	}
	return 0;
}


/* 
 * Configure generic SSL parameters 
 */
static int set_ssl_options(tls_domain_t* d)
{
	int i;
	int procs_no;
	long options;
#if OPENSSL_VERSION_NUMBER >= 0x00908000L
	long ssl_version;
	STACK_OF(SSL_COMP)* comp_methods;
#endif
	
	procs_no=get_max_procs();
	options=SSL_OP_ALL; /* all the bug workarrounds by default */
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
	options|=SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
				SSL_OP_CIPHER_SERVER_PREFERENCE;
#if		OPENSSL_VERSION_NUMBER >= 0x00908000L
	ssl_version=SSLeay();
	if ((ssl_version >= 0x0090800L) && (ssl_version < 0x0090803fL)){
		/* if 0.9.8 <= openssl version < 0.9.8c and compression support is
		 * enabled disable SSL_OP_TLS_BLOCK_PADDING_BUG (set by SSL_OP_ALL),
		 * see openssl #1204 http://rt.openssl.org/Ticket/Display.html?id=1204
		 */
		
		comp_methods=SSL_COMP_get_compression_methods();
		if (comp_methods && (sk_SSL_COMP_num(comp_methods) > 0)){
			options &= ~SSL_OP_TLS_BLOCK_PADDING_BUG;
			LOG(L_WARN, "tls: set_ssl_options: openssl "
					"SSL_OP_TLS_BLOCK_PADDING bug workaround enabled "
					"(openssl version %lx)\n", ssl_version);
		}else{
			LOG(L_INFO, "tls: set_ssl_options: detected openssl version (%lx) "
					" has the SSL_OP_TLS_BLOCK_PADDING bug, but compression "
					" is disabled so no workaround is needed\n", ssl_version);
		}
	}
#	endif
#endif
	for(i = 0; i < procs_no; i++) {
		SSL_CTX_set_options(d->ctx[i], options);
	}
	return 0;
}


/* 
 * Configure session cache parameters 
 */
static int set_session_cache(tls_domain_t* d)
{
	int i;
	int procs_no;
	
	procs_no=get_max_procs();
	for(i = 0; i < procs_no; i++) {
		     /* janakj: I am not sure if session cache makes sense in ser, session 
		      * cache is stored in SSL_CTX and we have one SSL_CTX per process, thus 
		      * sessions among processes will not be reused
		      */
		SSL_CTX_set_session_cache_mode(d->ctx[i], 
				   tls_session_cache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);
		SSL_CTX_set_session_id_context(d->ctx[i], 
					       (unsigned char*)tls_session_id.s, tls_session_id.len);
	}
	return 0;
}


/*
 * Initialize all domain attributes from default domains
 * if necessary
 */
static int fix_domain(tls_domain_t* d, tls_domain_t* def)
{
	int i;
	int procs_no;

	if (fill_missing(d, def) < 0) return -1;

	procs_no=get_max_procs();
	d->ctx = (SSL_CTX**)shm_malloc(sizeof(SSL_CTX*) * procs_no);
	if (!d->ctx) {
		ERR("%s: Cannot allocate shared memory\n", tls_domain_str(d));
		return -1;
	}
	memset(d->ctx, 0, sizeof(SSL_CTX*) * procs_no);
	for(i = 0; i < procs_no; i++) {
		d->ctx[i] = SSL_CTX_new((SSL_METHOD*)ssl_methods[d->method - 1]);
		if (d->ctx[i] == NULL) {
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

	return 0;
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


#define NUM_RETRIES 3
/*
 * load a private key from a file 
 */
static int load_private_key(tls_domain_t* d)
{
	int idx, ret_pwd, i;
	int procs_no;
	
	if (!d->pkey_file) {
		DBG("%s: No private key specified\n", tls_domain_str(d));
		return 0;
	}

	procs_no=get_max_procs();
	for(i = 0; i < procs_no; i++) {
		SSL_CTX_set_default_passwd_cb(d->ctx[i], passwd_cb);
		SSL_CTX_set_default_passwd_cb_userdata(d->ctx[i], d->pkey_file);
		
		for(idx = 0, ret_pwd = 0; idx < NUM_RETRIES; idx++) {
			ret_pwd = SSL_CTX_use_PrivateKey_file(d->ctx[i], d->pkey_file, SSL_FILETYPE_PEM);
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
		
		if (!SSL_CTX_check_private_key(d->ctx[i])) {
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
 * Initialize attributes of all domains from default domains
 * if necessary
 */
int tls_fix_cfg(tls_cfg_t* cfg, tls_domain_t* srv_defaults, tls_domain_t* cli_defaults)
{
	tls_domain_t* d;

	if (!cfg->cli_default) {
		cfg->cli_default = tls_new_domain(TLS_DOMAIN_DEF | TLS_DOMAIN_CLI, 0, 0);
	}

	if (!cfg->srv_default) {
		cfg->srv_default = tls_new_domain(TLS_DOMAIN_DEF | TLS_DOMAIN_SRV, 0, 0);
	}

	if (fix_domain(cfg->srv_default, srv_defaults) < 0) return -1;
	if (fix_domain(cfg->cli_default, cli_defaults) < 0) return -1;

	d = cfg->srv_list;
	while (d) {
		if (fix_domain(d, srv_defaults) < 0) return -1;
		d = d->next;
	}

	d = cfg->cli_list;
	while (d) {
		if (fix_domain(d, cli_defaults) < 0) return -1;
		d = d->next;
	}

	     /* Ask for passwords as the last step */
	d = cfg->srv_list;
	while(d) {
		if (load_private_key(d) < 0) return -1;
		d = d->next;
	}

	d = cfg->cli_list;
	while(d) {
		if (load_private_key(d) < 0) return -1;
		d = d->next;
	}

	if (load_private_key(cfg->srv_default) < 0) return -1;
	if (load_private_key(cfg->cli_default) < 0) return -1;

	return 0;
}


/*
 * Create new configuration structure
 */
tls_cfg_t* tls_new_cfg(void)
{
	tls_cfg_t* r;

	r = (tls_cfg_t*)shm_malloc(sizeof(tls_cfg_t));
	if (!r) {
		ERR("No memory left\n");
		return 0;
	}
	memset(r, 0, sizeof(tls_cfg_t));
	return r;
}


/*
 * Lookup TLS configuration based on type, ip, and port
 */
tls_domain_t* tls_lookup_cfg(tls_cfg_t* cfg, int type, struct ip_addr* ip, unsigned short port)
{
	tls_domain_t *p;

	if (type & TLS_DOMAIN_DEF) {
		if (type & TLS_DOMAIN_SRV) return cfg->srv_default;
		else return cfg->cli_default;
	} else {
		if (type & TLS_DOMAIN_SRV) p = cfg->srv_list;
		else p = cfg->cli_list;
	}

	while (p) {
		if ((p->port == port) && ip_addr_cmp(&p->ip, ip))
			return p;
		p = p->next;
	}

	     /* No matching domain found, return default */
	if (type & TLS_DOMAIN_SRV) return cfg->srv_default;
	else return cfg->cli_default;
}


/*
 * Check whether configuration domain exists
 */
static int domain_exists(tls_cfg_t* cfg, tls_domain_t* d)
{
	tls_domain_t *p;

	if (d->type & TLS_DOMAIN_DEF) {
		if (d->type & TLS_DOMAIN_SRV) return cfg->srv_default != NULL;
		else return cfg->cli_default != NULL;
	} else {
		if (d->type & TLS_DOMAIN_SRV) p = cfg->srv_list;
		else p = cfg->cli_list;
	}

	while (p) {
		if ((p->port == d->port) && ip_addr_cmp(&p->ip, &d->ip))
			return 1;
		p = p->next;
	}

	return 0;
}


/*
 * Add a domain to the configuration set
 */
int tls_add_domain(tls_cfg_t* cfg, tls_domain_t* d)
{
	if (!cfg) {
		ERR("TLS configuration structure missing\n");
		return -1;
	}

	     /* Make sure the domain does not exist */
	if (domain_exists(cfg, d)) return 1;

	if (d->type & TLS_DOMAIN_DEF) {
		if (d->type & TLS_DOMAIN_CLI) {
			cfg->cli_default = d;
		} else {
			cfg->srv_default = d;
		}
	} else {
		if (d->type & TLS_DOMAIN_SRV) {
			d->next = cfg->srv_list;
			cfg->srv_list = d;
		} else {
			d->next = cfg->cli_list;
			cfg->cli_list = d;
		}
	}
	return 0;
}
