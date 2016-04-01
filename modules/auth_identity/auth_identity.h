/*
 * $Id$
 *
 * Copyright (c) 2007 iptelorg GmbH
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
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
 * \brief SIP-router auth-identity :: Module interface
 * \ingroup auth-identity
 * Module: \ref auth-identity
 */

#ifndef AUTH_IDENT_H
#define AUTH_IDENT_H

#include <openssl/x509.h>
#include <curl/curl.h>

#include "../../locking.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"	/* struct sip_msg */
#include "../../str.h"					/* struct str */
#include "../../parser/parse_identity.h"
#include "../../parser/parse_identityinfo.h"
#include "../../parser/parse_date.h"

#define NEW_RSA_PROC

#define AUTH_DBG_LEVEL L_DBG

#define AUTH_URL_LENGTH 512
#define CERTIFICATE_URL_LENGTH AUTH_URL_LENGTH
#define CERTIFICATE_LENGTH 8*1024
#define DGST_STR_INIT_SIZE 8*1024
#define HASH_STR_SIZE 1024
#define AUTH_TIME_FORMAT "%a, %d %b %Y %H:%M:%S GMT"
#define AUTH_TIME_LENGTH 64
#define AUTH_CONTENTLENGTH_LENGTH AUTH_TIME_LENGTH
#define AUTH_DOMAIN_LENGTH 256
#define IDENTITY_INFO_FIRST_PART "Identity-Info: <"
#define IDENTITY_INFO_LAST_PART ">;alg=rsa-sha1\r\n"

#define IDENTITY_FIRST_PART "Identity: \""
#define IDENTITY_LAST_PART "\"\r\n"

#define ITEM_IN_BUCKET_LIMIT 8

#define CERTIFICATE_TABLE_ENTRIES (2<<10)
#define CERTIFICATE_TABLE_ITEM_LIMIT CERTIFICATE_TABLE_ENTRIES*ITEM_IN_BUCKET_LIMIT*2

/* callid table garbage collector defines */
#define CALLID_GARBAGE_COLLECTOR_INTERVAL 10

#define CALLID_TABLE_ENTRIES (2<<13)
#define CALLID_TABLE_ITEM_LIMIT	CALLID_TABLE_ENTRIES*ITEM_IN_BUCKET_LIMIT*2

#define AUTH_MSG_VALIDITY_TIME 3600
#define AUTH_MSG_TO_AUTH_VALIDITY_TIME 600

#define BEGIN_PEM_CERT "-----BEGIN CERTIFICATE-----"
#define BEGIN_PEM_CERT_LEN (sizeof(BEGIN_PEM_CERT) - 1)

enum msg_part {
	DS_FROM = 1,
	DS_TO,
	DS_CALLID,
	DS_CSEQ,
	DS_DATE,
	DS_CONTACT,
	DS_BODY
};

enum msg_part_flag {
	DS_REQUIRED = 0,
	DS_NOTREQUIRED = 1
};

typedef int (msg_part_proc)(str *, str *, struct sip_msg *);
typedef void (msg_part_free_proc)(void);

typedef struct _dgst_part {
	int itype;
	msg_part_proc *pfunc;
	msg_part_free_proc *pfreefunc;
	int iflag;
} dgst_part;

enum dgststr_asm_flags {
	AUTH_ADD_DATE = 1,
	AUTH_INCOMING_BODY = 1<<1,
	AUTH_OUTGOING_BODY = 1<<2
};

enum proc_ret_val {
	AUTH_OK,
	AUTH_NOTFOUND,
	AUTH_FOUND,
	AUTH_ERROR
};


typedef struct _dstr {
	str	sd;
	int size;
} dynstr;

int app2dynstr(dynstr *sout, str *s2app);
int app2dynchr(dynstr *sout, char capp);
int cpy2dynstr(dynstr *sout, str *s2app);
int initdynstr(dynstr *sout, int isize);
#define free_dynstr(sdyn) if ((sdyn)->sd.s) { pkg_free((sdyn)->sd.s); (sdyn)->size=0; }
#define resetstr_dynstr(sdyn) (sdyn)->sd.len=0
#define getstr_dynstr(sdyn) (sdyn)->sd


/* Table declarations */
/*
fleast(s1, s2) return values:
 1	s2 is less than s1
 0	s1 and s2 are equal
-1  s1 is less than s2
-2	s1 is the least
-3  s2 is the least

fcmp(s1, s2) return values:
 0  s1 and s2 are the same
 any other	s1 and s2 are not the same

fgc(s1) return values:
 1 s1 is garbage
 0 s1 is not garbage
*/
typedef int (table_item_cmp)(const void *, const void *);
typedef void (table_item_free)(const void *);
typedef void (table_item_searchinit)();
typedef int (table_item_gc)(const void *); /* garbage collector function */
typedef struct item {
	void *pdata;
	unsigned int uhash;
	struct item *pnext;
	struct item *pprev;
} titem;
typedef struct bucket {
	titem	*pfirst;
	titem	*plast;
	gen_lock_t lock;
} tbucket;
typedef struct table {
	unsigned int unum;	/* number of items */
	unsigned int ubuckets;	/* number of buckets */
	unsigned int uitemlim;	/* maximum of items */
	gen_lock_t lock;	/* lock for unum modifiing */
	table_item_cmp *fcmp; /* compare function (used by search) */
	table_item_searchinit *fsearchinit; /* init function (used by least item search, garbage collect) */
	table_item_cmp *fleast; /* init function (used by least item search) */
	table_item_free *ffree; /* free function */
	table_item_gc *fgc; /* garbage signer function */
	tbucket *entries;
} ttable;


int init_table(ttable **ptable,
			   unsigned int ubucknum,
			   unsigned int uitemlim,
			   table_item_cmp *fcmp,
			   table_item_searchinit *searchinit,
			   table_item_cmp *fleast,
			   table_item_free *ffree,
			   table_item_gc *fgc);
void free_table(ttable *ptable);
void garbage_collect(ttable *ptable, int ihashstart, int ihashend);

/* Certificate table declarations */
typedef struct cert_item {
	str		surl;
	str 	scertpem;
	time_t	ivalidbefore;	/* expiration time */
	unsigned int uaccessed;
} tcert_item;
int cert_item_cmp(const void *s1, const void *s2);
void cert_item_init();
int cert_item_least(const void *s1, const void *s2);
void cert_item_free(const void *sitem);
int get_cert_from_table(ttable *ptable, str *skey, tcert_item *ptarget);
int addcert2table(ttable *ptable, tcert_item *pcert);

/* Call-ID table declarations */
typedef struct dlg_item {
	str	sftag;	/* tag of the From header */
	unsigned int ucseq; /* number part of the cseq */
	struct dlg_item *pnext; /* next dialog concerned the same call-id */
} tdlg_item;

typedef struct cid_item {
	str	scid; /* call-id of the message */
	time_t ivalidbefore; /* the later expiration time among dialogs concerned this call-id*/
	tdlg_item *pdlgs; /* Cseqs and From tags */
} tcid_item;
int proc_cid(ttable *ptable,
			 str *scid,
			 str *sftag,
			 unsigned int ucseq,
			 time_t ivalidbefore);
int cid_item_cmp(const void *s1, const void *s2);
int cid_item_least(const void *s1, const void *s2);
void cid_item_free(const void *sitem);
void cid_item_init();
int cid_item_gc();

/* cURL functions */
size_t curlmem_cb(void *ptr, size_t size, size_t nmemb, void *data);
int download_cer(str *suri, CURL *hcurl);

/* OpenSSL, Base64 functions */
int retrieve_x509(X509 **pcert, str *scert, int bacceptpem);
int check_x509_subj(X509 *pcert, str* sdom);
int verify_x509(X509 *pcert, X509_STORE *pcacerts);
int rsa_sha1_dec (char *sencedsha, int iencedshalen,
				  char *ssha, int sshasize, int *ishalen,
				  X509 *pcertx509);
int rsa_sha1_enc (dynstr *sdigeststr,
				  dynstr *senc,
				  dynstr *sencb64,
				  RSA *hmyprivkey);
void base64decode(char* src_buf, int src_len, char* tgt_buf, int* tgt_len);
void base64encode(char* src_buf, int src_len, char* tgt_buf, int* tgt_len);
int x509_get_notafter(time_t *tout, X509 *pcert);
int x509_get_notbefore(time_t *tout, X509 *pcert);

/* Common functions */
int digeststr_asm(dynstr *sout, struct sip_msg *msg, str *sdate, int iflags);

int fromhdr_proc(str *sout, str *soutopt, struct sip_msg *msg);
int cseqhdr_proc(str *sout, str *soutopt, struct sip_msg *msg);
int callidhdr_proc(str *sout, str *soutopt, struct sip_msg *msg);
int datehdr_proc(str *sout, str *soutopt, struct sip_msg *msg);
int identityhdr_proc(str *sout, str *soutopt, struct sip_msg *msg);
int identityinfohdr_proc(str *sout, str *soutopt, struct sip_msg *msg);

int append_date(str *sdate, int idatesize, time_t *tout, struct sip_msg *msg);
int append_hf(struct sip_msg* msg, char *str1, enum _hdr_types_t type);

#endif
