/*
 * $Id$
 *
 * TLS module - certificate verification function
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * COpyright (C) 2005 iptelorg GmbH
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../dprint.h"
#include "tls_verify.h"

/*!
 * \file
 * \brief SIP-router TLS support :: Certificate verification
 * \ingroup tls
 * Module: \ref tls
 */


/* FIXME: remove this and use the value in domains instead */
#define VERIFY_DEPTH_S 3

/* This callback is called during each verification process, 
at each step during the chain of certificates (this function
is not the certificate_verification one!). */
int verify_callback(int pre_verify_ok, X509_STORE_CTX *ctx) {
	char buf[256];
	X509 *err_cert;
	int err, depth;

	depth = X509_STORE_CTX_get_error_depth(ctx);
	DBG("verify_callback: depth = %d\n",depth);
	if ( depth > VERIFY_DEPTH_S ) {
		LOG(L_NOTICE, "tls_init: verify_callback: cert chain too long ( depth > VERIFY_DEPTH_S)\n");
		pre_verify_ok=0;
	}
	
	if( pre_verify_ok ) {
		LOG(L_NOTICE, "tls_init: verify_callback: preverify is good: verify return: %d\n", pre_verify_ok);
		return pre_verify_ok;
	}
	
	err_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);	
	X509_NAME_oneline(X509_get_subject_name(err_cert),buf,sizeof buf);
	
	LOG(L_NOTICE, "tls_init: verify_callback: subject = %s\n", buf);
	LOG(L_NOTICE, "tls_init: verify_callback: verify error:num=%d:%s\n", err, X509_verify_cert_error_string(err));	
	LOG(L_NOTICE, "tls_init: verify_callback: error code is %d\n", ctx->error);
	
	switch (ctx->error) {
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
			X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert),buf,sizeof buf);
			LOG(L_NOTICE, "tls_init: verify_callback: issuer= %s\n",buf);
			break;
			
		case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
		case X509_V_ERR_CERT_NOT_YET_VALID:
			LOG(L_NOTICE, "tls_init: verify_callback: notBefore\n");
			break;
		
		case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
		case X509_V_ERR_CERT_HAS_EXPIRED:
			LOG(L_NOTICE, "tls_init: verify_callback: notAfter\n");
			break;
			
		case X509_V_ERR_CERT_SIGNATURE_FAILURE:
		case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
			LOG(L_NOTICE, "tls_init: verify_callback: unable to decrypt cert signature\n");
			break;
			
		case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
			LOG(L_NOTICE, "tls_init: verify_callback: unable to decode issuer public key\n");
			break;
			
		case X509_V_ERR_OUT_OF_MEM:
			ERR("tls_init: verify_callback: Out of memory \n");
			break;
			
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
			LOG(L_NOTICE, "tls_init: verify_callback: Self signed certificate issue\n");
			break;

		case X509_V_ERR_CERT_CHAIN_TOO_LONG:
			LOG(L_NOTICE, "tls_init: verify_callback: certificate chain too long\n");
			break;
		case X509_V_ERR_INVALID_CA:
			LOG(L_NOTICE, "tls_init: verify_callback: invalid CA\n");
			break;
		case X509_V_ERR_PATH_LENGTH_EXCEEDED:
			LOG(L_NOTICE, "tls_init: verify_callback: path length exceeded\n");
			break;
		case X509_V_ERR_INVALID_PURPOSE:
			LOG(L_NOTICE, "tls_init: verify_callback: invalid purpose\n");
			break;
		case X509_V_ERR_CERT_UNTRUSTED:
			LOG(L_NOTICE, "tls_init: verify_callback: certificate untrusted\n");
			break;
		case X509_V_ERR_CERT_REJECTED:
			LOG(L_NOTICE, "tls_init: verify_callback: certificate rejected\n");
			break;
		
		default:
			LOG(L_NOTICE, "tls_init: verify_callback: something wrong with the cert ... error code is %d (check x509_vfy.h)\n", ctx->error);
			break;
	}
	
	LOG(L_NOTICE, "tls_init: verify_callback: verify return:%d\n", pre_verify_ok);
	return(pre_verify_ok);
}
