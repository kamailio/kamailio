/*
 * TLS module
 *
 * Copyright (C) 2006 enum.at
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 */

/** log the verification failure reason.
 * @file tls_dump_vf.c
 * @ingroup: tls
 * Module: @ref tls
 */

#include "tls_dump_vf.h"

#include <openssl/ssl.h>
#include "../../dprint.h"
#include "tls_mod.h"
#include "tls_cfg.h"

/** log the verification failure reason.
 */
void tls_dump_verification_failure(long verification_result)
{
	int tls_log;
	
	tls_log = cfg_get(tls, tls_cfg, log);
	switch(verification_result) {
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		LOG(tls_log, "verification failure: unable to get issuer certificate\n");
		break;
	case X509_V_ERR_UNABLE_TO_GET_CRL:
		LOG(tls_log, "verification failure: unable to get certificate CRL\n");
		break;
	case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
		LOG(tls_log, "verification failure: unable to decrypt certificate's signature\n");
		break;
	case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
		LOG(tls_log, "verification failure: unable to decrypt CRL's signature\n");
		break;
	case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
		LOG(tls_log, "verification failure: unable to decode issuer public key\n");
		break;
	case X509_V_ERR_CERT_SIGNATURE_FAILURE:
		LOG(tls_log, "verification failure: certificate signature failure\n");
		break;
	case X509_V_ERR_CRL_SIGNATURE_FAILURE:
		LOG(tls_log, "verification failure: CRL signature failure\n");
		break;
	case X509_V_ERR_CERT_NOT_YET_VALID:
		LOG(tls_log, "verification failure: certificate is not yet valid\n");
		break;
	case X509_V_ERR_CERT_HAS_EXPIRED:
		LOG(tls_log, "verification failure: certificate has expired\n");
		break;
	case X509_V_ERR_CRL_NOT_YET_VALID:
		LOG(tls_log, "verification failure: CRL is not yet valid\n");
		break;
	case X509_V_ERR_CRL_HAS_EXPIRED:
		LOG(tls_log, "verification failure: CRL has expired\n");
		break;
	case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
		LOG(tls_log, "verification failure: format error in certificate's notBefore field\n");
		break;
	case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
		LOG(tls_log, "verification failure: format error in certificate's notAfter field\n");
		break;
	case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
		LOG(tls_log, "verification failure: format error in CRL's lastUpdate field\n");
		break;
	case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
		LOG(tls_log, "verification failure: format error in CRL's nextUpdate field\n");
		break;
	case X509_V_ERR_OUT_OF_MEM:
		LOG(tls_log, "verification failure: out of memory\n");
		break;
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
		LOG(tls_log, "verification failure: self signed certificate\n");
		break;
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
		LOG(tls_log, "verification failure: self signed certificate in certificate chain\n");
		break;
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
		LOG(tls_log, "verification failure: unable to get local issuer certificate\n");
		break;
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
		LOG(tls_log, "verification failure: unable to verify the first certificate\n");
		break;
	case X509_V_ERR_CERT_CHAIN_TOO_LONG:
		LOG(tls_log, "verification failure: certificate chain too long\n");
		break;
	case X509_V_ERR_CERT_REVOKED:
		LOG(tls_log, "verification failure: certificate revoked\n");
		break;
	case X509_V_ERR_INVALID_CA:
		LOG(tls_log, "verification failure: invalid CA certificate\n");
		break;
	case X509_V_ERR_PATH_LENGTH_EXCEEDED:
		LOG(tls_log, "verification failure: path length constraint exceeded\n");
		break;
	case X509_V_ERR_INVALID_PURPOSE:
		LOG(tls_log, "verification failure: unsupported certificate purpose\n");
		break;
	case X509_V_ERR_CERT_UNTRUSTED:
		LOG(tls_log, "verification failure: certificate not trusted\n");
		break;
	case X509_V_ERR_CERT_REJECTED:
		LOG(tls_log, "verification failure: certificate rejected\n");
		break;
	case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
		LOG(tls_log, "verification failure: subject issuer mismatch\n");
		break;
	case X509_V_ERR_AKID_SKID_MISMATCH:
		LOG(tls_log, "verification failure: authority and subject key identifier mismatch\n");
		break;
	case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
		LOG(tls_log, "verification failure: authority and issuer serial number mismatch\n");
		break;
	case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
		LOG(tls_log, "verification failure: key usage does not include certificate signing\n");
		break;
	case X509_V_ERR_APPLICATION_VERIFICATION:
		LOG(tls_log, "verification failure: application verification failure\n");
		break;
	}
}


/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
