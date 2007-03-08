#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#include "../../mem/mem.h"

#include "auth_identity.h"



int retrieve_x509(X509 **pcert, str *scert)
{
	BIO *bcer=NULL;
	char serr[160];
	int iRet=0;


	if (!(bcer=BIO_new(BIO_s_mem()))) {
		LOG(L_ERR, "AUTH_INDENTITY:retrieve_x509: Unable to create BIO\n");

		return -1;
	}

	do {
		if (BIO_write(bcer, scert->s, scert->len)!=scert->len) {
			LOG(L_ERR, "AUTH_INDENTITY:retrieve_x509: Unable to write BIO\n");
			iRet=-2;
			break;
		}

		if (!(*pcert = PEM_read_bio_X509(bcer, NULL, NULL, NULL))) {
			ERR_error_string_n(ERR_get_error(), serr, sizeof(serr));
			LOG(L_ERR, "AUTH_INDENTITY:retrieve_x509: Certificate %s\n", serr);
			iRet=-3;
		}
	} while (0);

	BIO_free(bcer);

	return iRet;
}

int verify_x509(X509 *pcert, X509_STORE *pcacerts, str *sdom)
{
	X509_STORE_CTX ca_ctx;
	char *strerr;
	char scname[AUTH_DOMAIN_LENGTH];
	int ilen;

	if (X509_STORE_CTX_init(&ca_ctx, pcacerts, pcert, NULL) != 1) {
		LOG(L_ERR, "AUTH_INDENTITY:verify_x509: Unable to init X509 store ctx\n");
		return -1;
	}

	if (X509_verify_cert(&ca_ctx) != 1) {
		strerr = (char *) X509_verify_cert_error_string(ca_ctx.error);
		LOG(L_ERR, "AUTH_INDENTITY VERIFIER: Certificate verification error: %s\n", strerr);
		X509_STORE_CTX_cleanup(&ca_ctx);
		return -2;
	}
	X509_STORE_CTX_cleanup(&ca_ctx);

	/* certificate supplier host and certificate subject match check */
	ilen=X509_NAME_get_text_by_NID (X509_get_subject_name (pcert),
							   		NID_commonName, scname, sizeof (scname));
	if (sdom->len != ilen || strncasecmp(scname, sdom->s, sdom->len)) {
		LOG(L_ERR, "AUTH_INDENTITY VERIFIER: certificate common name doesn't match host name\n");
		return -3;
	}

	LOG(AUTH_DBG_LEVEL, "AUTH_INDENTITY VERIFIER: Certificate is valid\n");

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

	ires=RSA_private_encrypt(sizeof sstrcrypted, sstrcrypted,
							 (unsigned char*)getstr_dynstr(senc).s, hmyprivkey, RSA_PKCS1_PADDING );
	if (ires<0)
	{
		ERR_error_string_n(ERR_get_error(), serr, sizeof serr);
		LOG(L_ERR, "AUTH_INDENT:rsa_sha1_enc: '%s'\n", serr);
		return -1;
	}

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
		LOG(L_ERR, "AUTH_INDENTITY:decrypt_identity: Pubkey %s\n", serr);
		return -1;
	}

	X509_free(pcertx509);

	hpubkey = EVP_PKEY_get1_RSA(pkey);
	EVP_PKEY_free(pkey);
	if (hpubkey == NULL) {
		LOG(L_ERR, "AUTH_INDENTITY:decrypt_identity: Error getting RSA key\n");
		return -2;
	}

	/* it is bigger than the output buffer */
	if (RSA_size(hpubkey) > sshasize) {
		LOG(L_ERR, "AUTH_INDENTITY:decrypt_identity: Unexpected Identity hash length (%d > %d)\n", RSA_size(hpubkey), sshasize);
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
		LOG(L_ERR, "AUTH_INDENTITY:decrypt_identity: RSA operation error %s\n", serr);
		RSA_free(hpubkey);
		return -4;
	}

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
