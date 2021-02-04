/*
 * TLS module
 *
 * Copyright (C) 2005 iptelorg GmbH
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

#include "../../core/dprint.h"
#include "tls_verify.h"

/*!
 * \file
 * \brief Kamailio TLS support :: Certificate verification
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
	LM_DBG("tls init - depth = %d\n",depth);
	if ( depth > VERIFY_DEPTH_S ) {
		LM_NOTICE("tls init - cert chain too long ( depth > VERIFY_DEPTH_S)\n");
		pre_verify_ok=0;
	}

	if( pre_verify_ok ) {
		LM_NOTICE("tls init - preverify is good: verify return: %d\n",
				pre_verify_ok);
		return pre_verify_ok;
	}

	err_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	X509_NAME_oneline(X509_get_subject_name(err_cert),buf,sizeof buf);

	LM_NOTICE("tls init - subject = %s\n", buf);
	LM_NOTICE("tls init - verify error - num=%d:%s\n", err,
			X509_verify_cert_error_string(err));
	LM_NOTICE("tls init - error code is %d (depth: %d)\n",
			err, depth);

	switch (err) {
		case X509_V_OK:
			LM_NOTICE("tls init - all ok\n");
			break;
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
			X509_NAME_oneline(X509_get_issuer_name(err_cert),buf,sizeof buf);
			LM_NOTICE("tls init - issuer= %s\n",buf);
			break;

		case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
		case X509_V_ERR_CERT_NOT_YET_VALID:
			LM_NOTICE("tls init - notBefore\n");
			break;

		case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
		case X509_V_ERR_CERT_HAS_EXPIRED:
			LM_NOTICE("tls init - notAfter\n");
			break;

		case X509_V_ERR_CERT_SIGNATURE_FAILURE:
		case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
			LM_NOTICE("tls init - unable to decrypt cert signature\n");
			break;

		case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
			LM_NOTICE("tls init - unable to decode issuer public key\n");
			break;

		case X509_V_ERR_OUT_OF_MEM:
			LM_ERR("tls init - Out of memory \n");
			break;

		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
			LM_NOTICE("tls init - Self signed certificate issue\n");
			break;

		case X509_V_ERR_CERT_CHAIN_TOO_LONG:
			LM_NOTICE("tls init - certificate chain too long\n");
			break;
		case X509_V_ERR_INVALID_CA:
			LM_NOTICE("tls init - invalid CA\n");
			break;
		case X509_V_ERR_PATH_LENGTH_EXCEEDED:
			LM_NOTICE("tls init - path length exceeded\n");
			break;
		case X509_V_ERR_INVALID_PURPOSE:
			LM_NOTICE("tls init - invalid purpose\n");
			break;
		case X509_V_ERR_CERT_UNTRUSTED:
			LM_NOTICE("tls init - certificate untrusted\n");
			break;
		case X509_V_ERR_CERT_REJECTED:
			LM_NOTICE("tls init - certificate rejected\n");
			break;

		default:
			LM_NOTICE("tls init - something wrong with the"
					" cert ... error code is %d (check x509_vfy.h)\n",
					err);
			break;
	}

	LM_NOTICE("tls init - verify return: %d\n", pre_verify_ok);
	return(pre_verify_ok);
}


int verify_callback_unconditional_success(int pre_verify_ok, X509_STORE_CTX *ctx) {
	LM_NOTICE("Post-verification callback: unconditional success\n");
	return 1;
}
