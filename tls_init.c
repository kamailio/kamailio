/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
/*
 * tls initialization & cleanup functions
 * 
 * History:
 * --------
 *  2003-06-29  created by andrei
 */
#ifdef USE_TLS



#include <openssl/ssl.h>
#include <openssl/err.h>

#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "tcp_init.h"
#include "dprint.h"



#if OPENSSL_VERSION_NUMBER < 0x00906000L  /* 0.9.6*/
#error "OpenSSL 0.9.6 or greater required"
/* it might work ok with older versions (I think
 *  >= 0.9.4 should be ok), but I didn't test them
 *  so try them at your own risk :-) -- andrei
 */
#endif


/* global tls related data */
SSL_CTX* default_ctx=0 ; /* global ssl context */

int tls_log=L_INFO; /* tls log level */
int tls_require_cert=0; /* require client certificate */
char* tls_pkey_file=0; /* private key file name */
char* tls_cert_file=0; /* certificate file name */
char* tls_ca_file=0;   /* CA list file name */


/* inits a sock_info structure with tls data
 * (calls tcp_init for the tcp part)
 * returns 0 on success, -1 on error */
int tls_init(struct socket_info* sock_info)
{
	int ret;
	if ((ret=tcp_init(sock_info))!=0){
		LOG(L_ERR, "ERROR: tls_init: tcp_init failed on"
			"%.*s:%d\n", sock_info->address_str.len,
			sock_info->address_str.s, sock_info->port_no);
		return ret;
	}
	sock_info->proto=PROTO_TLS;
	/* tls specific stuff */
	return 0;
}


/* malloc & friends functions that will be used
 * by libssl (we need most ssl info in shared mem.)*/

void* tls_malloc(size_t size)
{
	return shm_malloc(size);
}


void tls_free(void* ptr)
{
	shm_free(ptr);
}


void* tls_realloc(void* ptr, size_t size)
{
	return shm_realloc(ptr, size);
}


/* print the ssl error stack */
void tls_dump_errors(char* s)
{
	long err;
	if ( 1 /*default_ctx */) /* only if ssl was initialized */
		while((err=ERR_get_error()))
			LOG(L_ERR, "%s%s\n", (s)?s:"", ERR_error_string(err,0));
}



/* inits ser tls support
 * returns 0 on success, <0 on error */
int init_tls()
{

	
	if (tls_pkey_file==0)
		tls_pkey_file=TLS_PKEY_FILE;
	if (tls_cert_file==0)
		tls_cert_file=TLS_CERT_FILE;
	if (tls_ca_file==0)
		tls_ca_file=TLS_CA_FILE;
	
	DBG("initializing openssl...\n");
	SSL_library_init();  /* don't use shm_ for SSL_library_init() */
	/* init mem. alloc. for libcrypt & openssl */
	CRYPTO_set_mem_functions(tls_malloc, tls_realloc,
								tls_free);
	
	/* init the openssl library */
	SSL_load_error_strings(); /* readable error messages*/
	/* seed the PRNG, nothing on linux because openssl should automatically
	   use /dev/urandom, see RAND_seed, RAND_add */
	
	/* create the ssl context */
	DBG("creating the ssl context...\n");
	default_ctx=SSL_CTX_new(TLSv1_method());
	if (default_ctx==0){
		LOG(L_ERR, "init_tls: failed to create ssl context\n");
		goto error;
	}
	/* no passwd: */
	 /* SSL_CTX_set_default_passwd_cb(ctx, callback); */
	
	/* set options, e.g SSL_OP_NO_SSLv2, 
	 * SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
	 */
	/* SSL_CTX_set_options(ctx, options); */
	
	/* mode, e.g. SSL_MODE_ENABLE_PARTIAL_WRITE,
	 * SSL_MODE_AUTO_RETRY */
	/* SSL_CTX_set_mode(ctx, mode); */
	
	/* using certificates (we don't allow anonymous ciphers => at least
	 * the server must have a cert)*/
	/* private key */
	if (SSL_CTX_use_PrivateKey_file(default_ctx, tls_pkey_file,
				SSL_FILETYPE_PEM)!=1){
		LOG(L_ERR, "init_tls: failed to load private key from \"%s\"\n",
				tls_pkey_file);
		goto error_certs;
	}
	if (SSL_CTX_use_certificate_chain_file(default_ctx, tls_cert_file)!=1){
		/* better than *_use_certificate_file 
		 * see SSL_CTX_use_certificate(3)/Notes */
		LOG(L_ERR, "init_tls: failed to load certificate from \"%s\"\n",
					tls_cert_file);
		goto error_certs;
	}
	/* check if private key corresponds to the loaded ceritficate */
	if (SSL_CTX_check_private_key(default_ctx)!=1){
		LOG(L_CRIT, "init_tls: private key \"%s\" does not match the"
				" certificate file \"%s\"\n", tls_pkey_file, tls_cert_file);
		goto error_certs;
	}
	
	/* set session id context, usefull for reusing stored sessions */
	/*
	if (SSL_CTX_set_session_id_context(ctx, version, version_len)!=1){
		LOG(L_CRIT, "init_tls: failed to set session id\n");
		goto error;
	}
	*/
	
	/* set cert. verifications options */
	/* verify peer if it has a cert (to fail for no cert. add 
	 *  | SSL_VERIFY_FAIL_IF_NO_PEER_CERT ); forces the server to send
	 *  a client certificate request */
	SSL_CTX_set_verify(default_ctx, SSL_VERIFY_PEER | ( (tls_require_cert)?
			SSL_VERIFY_FAIL_IF_NO_PEER_CERT:0 ), 0);
	/* SSL_CTX_set_verify_depth(ctx, 2);  -- default 9 */
	/* CA locations, list */
	if (tls_ca_file){
		if (SSL_CTX_load_verify_locations(default_ctx, tls_ca_file, 0 )!=1){
			/* we don't support ca path, we load them only from files */
			LOG(L_CRIT, "init_tls: error while processing CA locations\n");
			goto error_certs;
		}
		SSL_CTX_set_client_CA_list(default_ctx, 
									SSL_load_client_CA_file(tls_ca_file));
		if (SSL_CTX_get_client_CA_list(default_ctx)==0){
			LOG(L_CRIT, "init_tls: error setting client CA list from <%s>\n",
						tls_ca_file);
			goto error_certs;
		}
	}
	
	/* DH tmp key generation -- see DSA_generate_parameters,
	 * SSL_CTX_set_tmp_dh, SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE */
	
	/* RSA tmp key generation => we don't care, we won't accept 
	 * connection to export restricted applications and tls does not
	 * allow a tmp key in another sitaution */
	
	return 0;
error_certs:
	/*
	SSL_CTX_free(ctx);
	ctx=0;
	*/
error:
	tls_dump_errors("tls_init:");
	return -1;
}



void destroy_tls()
{
	if(default_ctx){
		DBG("destroy_tls...\n");
		SSL_CTX_free(default_ctx);
		ERR_free_strings();
		default_ctx=0; 
	}
}

#endif
