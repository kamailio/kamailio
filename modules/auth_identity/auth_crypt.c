/*
 * $Id$
 *
 * Copyright (c) 2007 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief SIP-router auth-identity :: Crypt
 * \ingroup auth-identity
 * Module: \ref auth-identity
 */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"

#include "auth_identity.h"



int retrieve_x509(X509 **pcert, str *scert, int bacceptpem)
{
	BIO *bcer=NULL;
	char serr[160];
	int iRet=0;


	if (!(bcer=BIO_new(BIO_s_mem()))) {
		LOG(L_ERR, "AUTH_IDENTITY:retrieve_x509: Unable to create BIO\n");

		return -1;
	}

	do {
		if (BIO_write(bcer, scert->s, scert->len)!=scert->len) {
			LOG(L_ERR, "AUTH_IDENTITY:retrieve_x509: Unable to write BIO\n");
			iRet=-2;
			break;
		}

		/* RFC 4474 only accepts certs in the DER form but it can not harm
		 * to be a little bit more flexible and accept PEM as well. */
		if (bacceptpem
		  	&& scert->len > BEGIN_PEM_CERT_LEN
			&& memmem(scert->s,
					  scert->len,
					  BEGIN_PEM_CERT,
					  BEGIN_PEM_CERT_LEN)) {
			if (!(*pcert = PEM_read_bio_X509(bcer, NULL, NULL, NULL))) {
				ERR_error_string_n(ERR_get_error(), serr, sizeof(serr));
				LOG(L_ERR, "AUTH_IDENTITY:retrieve_x509: PEM Certificate %s\n", serr);
				iRet=-4;
			}
		} else {
			if (!(*pcert = d2i_X509_bio(bcer, NULL))) {
				ERR_error_string_n(ERR_get_error(), serr, sizeof(serr));
				LOG(L_ERR, "AUTH_IDENTITY:retrieve_x509: DER Certificate %s\n", serr);
				iRet=-3;
			}
		}
	} while (0);

	BIO_free(bcer);

	return iRet;
}

int check_x509_subj(X509 *pcert, str* sdom)
{
	STACK_OF(GENERAL_NAME) *altnames;
	int ialts, i1, ilen, altlen;
	const GENERAL_NAME *actname;
	char scname[AUTH_DOMAIN_LENGTH];
	char *altptr;
	struct sip_uri suri;
	int ret = 0;


	/* we're looking for subjectAltName for the first time */
	altnames = X509_get_ext_d2i(pcert, NID_subject_alt_name, NULL, NULL);

	if (altnames) {
		ialts = sk_GENERAL_NAME_num(altnames);

		for (i1=0; i1 < ialts; i1++) {
			actname = sk_GENERAL_NAME_value(altnames, i1);

			if (actname->type == GEN_DNS || actname->type == GEN_URI) {
				/* we've found one */
				altptr = (char *)ASN1_STRING_data(actname->d.ia5);
				if (actname->type == GEN_URI) {
					if (parse_uri(altptr, strlen(altptr), &suri) != 0) {
						continue;
					}
					if (!(suri.type == SIP_URI_T || suri.type == SIPS_URI_T)) {
						continue;
					}
					if (suri.user.len != 0 || suri.passwd.len != 0) {
						continue;
					}
					altptr = suri.host.s;
					altlen = suri.host.len;
				} else {
					altlen = strlen(altptr);
				}
				if (sdom->len != altlen 
					|| strncasecmp(altptr, sdom->s, sdom->len)) {
					LOG(L_INFO, "AUTH_IDENTITY VERIFIER: subAltName of certificate doesn't match host name\n");
					ret = -1;
				} else {
					ret = 1;
					break;
				}
			}
		}
		GENERAL_NAMES_free(altnames);
	}

	if (ret != 0) {
		return ret == 1 ? 0 : ret;
 	}

	/* certificate supplier host and certificate subject match check */
	ilen=X509_NAME_get_text_by_NID (X509_get_subject_name (pcert),
									NID_commonName,
									scname,
									sizeof (scname));
	if (sdom->len != ilen || strncasecmp(scname, sdom->s, sdom->len)) {
		LOG(L_INFO, "AUTH_IDENTITY VERIFIER: common name of certificate doesn't match host name\n");
		return -2;
	}

	return 0;
}

int verify_x509(X509 *pcert, X509_STORE *pcacerts)
{
	X509_STORE_CTX ca_ctx;
	char *strerr;


	if (X509_STORE_CTX_init(&ca_ctx, pcacerts, pcert, NULL) != 1) {
		LOG(L_ERR, "AUTH_IDENTITY:verify_x509: Unable to init X509 store ctx\n");
		return -1;
	}

	if (X509_verify_cert(&ca_ctx) != 1) {
		strerr = (char *) X509_verify_cert_error_string(ca_ctx.error);
		LOG(L_ERR, "AUTH_IDENTITY VERIFIER: Certificate verification error: %s\n", strerr);
		X509_STORE_CTX_cleanup(&ca_ctx);
		return -2;
	}
	X509_STORE_CTX_cleanup(&ca_ctx);

	LOG(AUTH_DBG_LEVEL, "AUTH_IDENTITY VERIFIER: Certificate is valid\n");

	return 0;
}

int rsa_sha1_enc (dynstr *sdigeststr,
				  dynstr *senc,
				  dynstr *sencb64,
				  RSA *hmyprivkey)
{
	unsigned char sstrcrypted[SHA_DIGEST_LENGTH];
	int ires;
	char serr[160];


	SHA1((unsigned char*)getstr_dynstr(sdigeststr).s,
		 getstr_dynstr(sdigeststr).len,
		 sstrcrypted);

#ifdef NEW_RSA_PROC
	ires = senc->size;
	if (RSA_sign(NID_sha1,
			 	 sstrcrypted,
			 	 sizeof sstrcrypted,
				 (unsigned char*)getstr_dynstr(senc).s,
				 (unsigned int*)&ires,
			 	 hmyprivkey) != 1) {
		ERR_error_string_n(ERR_get_error(), serr, sizeof serr);
		LOG(L_ERR, "AUTH_IDENTITY:rsa_sha1_enc: '%s'\n", serr);
		return -2;
	}
#else
	ires=RSA_private_encrypt(sizeof sstrcrypted, sstrcrypted,
							 (unsigned char*)getstr_dynstr(senc).s, hmyprivkey,
							 RSA_PKCS1_PADDING );
	if (ires<0)
	{
		ERR_error_string_n(ERR_get_error(), serr, sizeof serr);
		LOG(L_ERR, "AUTH_IDENTITY:rsa_sha1_enc: '%s'\n", serr);
		return -1;
	}
#endif

	base64encode(getstr_dynstr(senc).s, senc->size, getstr_dynstr(sencb64).s, &getstr_dynstr(sencb64).len );

	return 0;
}

int rsa_sha1_dec (char *sencedsha, int iencedshalen,
				  char *ssha, int sshasize, int *ishalen,
				  X509 *pcertx509)
{
	EVP_PKEY *pkey;
	RSA* hpubkey;
	unsigned long lerr;
	char serr[160];


	pkey=X509_get_pubkey(pcertx509);
	if (pkey == NULL) {
		lerr=ERR_get_error(); ERR_error_string_n(lerr, serr, sizeof(serr));
		LOG(L_ERR, "AUTH_IDENTITY:decrypt_identity: Pubkey %s\n", serr);
		return -1;
	}

	X509_free(pcertx509);

	hpubkey = EVP_PKEY_get1_RSA(pkey);
	EVP_PKEY_free(pkey);
	if (hpubkey == NULL) {
		LOG(L_ERR, "AUTH_IDENTITY:decrypt_identity: Error getting RSA key\n");
		return -2;
	}

#ifdef NEW_RSA_PROC
	if (RSA_verify(NID_sha1,
		 			(unsigned char*)ssha, sshasize,
					(unsigned char*)sencedsha, iencedshalen,
					hpubkey) != 1) {
		LOG(L_INFO, "AUTH_IDENTITY VERIFIER: RSA verify returned: '%s'\n", ERR_error_string(ERR_get_error(), NULL));
		LOG(L_INFO, "AUTH_IDENTITY VERIFIER: RSA verify failed -> Invalid Identity Header\n");
		RSA_free(hpubkey);
		return -5;
	}
#else
	/* it is bigger than the output buffer */
	if (RSA_size(hpubkey) > sshasize) {
		LOG(L_ERR, "AUTH_IDENTITY:decrypt_identity: Unexpected Identity hash length (%d > %d)\n", RSA_size(hpubkey), sshasize);
		RSA_free(hpubkey);
		return -3;
	}
	*ishalen=RSA_public_decrypt(iencedshalen,
								(unsigned char*)sencedsha,
								(unsigned char*)ssha,
								hpubkey,
								RSA_PKCS1_PADDING);
	if (*ishalen<=0) {
		lerr=ERR_get_error(); ERR_error_string_n(lerr, serr, sizeof(serr));
		LOG(L_ERR, "AUTH_IDENTITY:decrypt_identity: RSA operation error %s\n", serr);
		RSA_free(hpubkey);
		return -4;
	}
#endif

	RSA_free(hpubkey);

	return 0;
}

/* copypasted from ser/modules/rr/avp_cookie.c + this adds '=' sign! ) */
void base64encode(char* src_buf, int src_len, char* tgt_buf, int* tgt_len) {
	static char code64[64+1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int pos;
	for (pos=0, *tgt_len=0; pos < src_len; pos+=3,*tgt_len+=4) {
		tgt_buf[*tgt_len+0] = code64[(unsigned char)src_buf[pos+0] >> 2];
		tgt_buf[*tgt_len+1] = code64[(((unsigned char)src_buf[pos+0] & 0x03) << 4) | ((pos+1 < src_len)?((unsigned char)src_buf[pos+1] >> 4):0)];
		if (pos+1 < src_len)
			tgt_buf[*tgt_len+2] = code64[(((unsigned char)src_buf[pos+1] & 0x0F) << 2) | ((pos+2 < src_len)?((unsigned char)src_buf[pos+2] >> 6):0)];
		else
			tgt_buf[*tgt_len+2] = '=';
		if (pos+2 < src_len)
			tgt_buf[*tgt_len+3] = code64[(unsigned char)src_buf[pos+2] & 0x3F];
		else
			tgt_buf[*tgt_len+3] = '=';
	}
}


/* copypasted from ser/modules/rr/avp_cookie.c */
void base64decode(char* src_buf, int src_len, char* tgt_buf, int* tgt_len) {
	int pos, i, n;
	unsigned char c[4];
	for (pos=0, i=0, *tgt_len=0; pos < src_len; pos++) {
		if (src_buf[pos] >= 'A' && src_buf[pos] <= 'Z')
			c[i] = src_buf[pos] - 65;   /* <65..90>  --> <0..25> */
		else if (src_buf[pos] >= 'a' && src_buf[pos] <= 'z')
			c[i] = src_buf[pos] - 71;   /* <97..122>  --> <26..51> */
		else if (src_buf[pos] >= '0' && src_buf[pos] <= '9')
			c[i] = src_buf[pos] + 4;    /* <48..56>  --> <52..61> */
		else if (src_buf[pos] == '+')
			c[i] = 62;
		else if (src_buf[pos] == '/')
			c[i] = 63;
		else  /* '=' */
			c[i] = 64;
		i++;
		if (pos == src_len-1) {
			while (i < 4) {
				c[i] = 64;
				i++;
			}
		}
		if (i==4) {
			if (c[0] == 64)
				n = 0;
			else if (c[2] == 64)
				n = 1;
			else if (c[3] == 64)
				n = 2;
			else
				n = 3;
			switch (n) {
				case 3:
					tgt_buf[*tgt_len+2] = (char) (((c[2] & 0x03) << 6) | c[3]);
					/* no break */
				case 2:
					tgt_buf[*tgt_len+1] = (char) (((c[1] & 0x0F) << 4) | (c[2] >> 2));
					/* no break */
				case 1:
					tgt_buf[*tgt_len+0] = (char) ((c[0] << 2) | (c[1] >> 4));
					break;
			}
			i=0;
			*tgt_len+= n;
		}
	}
}

int x509_get_validitytime(time_t *tout, ASN1_UTCTIME *tin)
{
	char *sasn1;
	int i1;
	struct tm tmptm;


	memset(&tmptm, 0, sizeof(tmptm));
	i1=tin->length;
	sasn1=(char *)tin->data;

	if (i1 < 10)
		return -1;
/*	if (sasn1[i1-1]!='Z')
		return -1;*/
	for (i1=0; i1<10; i1++)
		if((sasn1[i1] > '9') || (sasn1[i1] < '0'))
			return -2;

	tmptm.tm_year=(sasn1[0]-'0')*10+(sasn1[1]-'0');
	if(tmptm.tm_year < 50)
		tmptm.tm_year+=100;

	tmptm.tm_mon=(sasn1[2]-'0')*10+(sasn1[3]-'0')-1;
	if((tmptm.tm_mon > 11) || (tmptm.tm_mon < 0))
		return -3;

	tmptm.tm_mday=(sasn1[4]-'0')*10+(sasn1[5]-'0');
	tmptm.tm_hour= (sasn1[6]-'0')*10+(sasn1[7]-'0');
	tmptm.tm_min=(sasn1[8]-'0')*10+(sasn1[9]-'0');

	if ((sasn1[10] >= '0') && (sasn1[10] <= '9') &&
		   (sasn1[11] >= '0') && (sasn1[11] <= '9'))
		tmptm.tm_sec=(sasn1[10]-'0')*10+(sasn1[11]-'0');

#ifdef HAVE_TIMEGM
	*tout=timegm(&tmptm);
#else
	*tout=_timegm(&tmptm);
#endif

	return 0;
}

int x509_get_notbefore(time_t *tout, X509 *pcert)
{
	return (x509_get_validitytime(tout, X509_get_notBefore(pcert)));
}

int x509_get_notafter(time_t *tout, X509 *pcert)
{
	return (x509_get_validitytime(tout, X509_get_notAfter(pcert)));
}
