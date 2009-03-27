/*
 * $Id$
 *
 * TLS module - select interface
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * COpyright (C) 2005 iptelorg GmbH
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

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include "../../globals.h"
#include "../../tcp_server.h"
#include "../../tcp_conn.h"
#include "../../ut.h"
#include "tls_server.h"
#include "tls_select.h"
#include "tls_mod.h"

enum {
	CERT_LOCAL = 1,   /* Select local certificate */
	CERT_PEER,        /* Select peer certificate */
	CERT_SUBJECT,     /* Select subject part of certificate */
	CERT_ISSUER,      /* Select issuer part of certificate */
	CERT_VERIFIED,    /* Test for verified certificate */
	CERT_REVOKED,     /* Test for revoked certificate */
	CERT_EXPIRED,     /* Expiration certificate test */
	CERT_SELFSIGNED,  /* self-signed certificate test */
	CERT_NOTBEFORE,   /* Select validity end from certificate */
	CERT_NOTAFTER,    /* Select validity start from certificate */
	COMP_CN,          /* Common name */
	COMP_O,           /* Organization name */
	COMP_OU,          /* Organization unit */
	COMP_C,           /* Country name */
	COMP_ST,          /* State */
	COMP_L,           /* Locality/town */
	COMP_HOST,        /* hostname from subject/alternative */
	COMP_URI,         /* URI from subject/alternative */
	COMP_E,           /* Email address */
	COMP_IP           /* IP from subject/alternative */
};


struct tcp_connection* get_cur_connection(struct sip_msg* msg)
{
	struct tcp_connection* c;
	if (msg->rcv.proto != PROTO_TLS) {
		ERR("Transport protocol is not TLS (bug in config)\n");
		return 0;
	}

	c = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, tls_con_lifetime);
	if (c && c->type != PROTO_TLS) {
		ERR("Connection found but is not TLS\n");
		tcpconn_put(c);
		return 0;
	}
	return c;
}


static SSL* get_ssl(struct tcp_connection* c)
{
	struct tls_extra_data* extra;

	if (!c || !c->extra_data) {
		ERR("Unable to extract SSL data from TLS connection\n");
		return 0;
	}
	extra = (struct tls_extra_data*)c->extra_data;
	return extra->ssl;
}


static int get_cert(X509** cert, struct tcp_connection** c, struct sip_msg* msg, int my)
{
	SSL* ssl;

	*cert = 0;
	*c = get_cur_connection(msg);
	if (!(*c)) {
		INFO("TLS connection not found\n");
		return -1;
	}
	ssl = get_ssl(*c);
	if (!ssl) goto err;
	*cert = my ? SSL_get_certificate(ssl) : SSL_get_peer_certificate(ssl);
	if (!*cert) {
		ERR("Unable to retrieve TLS certificate from SSL structure\n");
		goto err;
	}
	
	return 0;
	
 err:
	tcpconn_put(*c);
	return -1;
}


static int get_cipher(str* res, sip_msg_t* msg) 
{
	str cipher;
	static char buf[1024];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		INFO("TLS connection not found in select_cipher\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	cipher.s = (char*)SSL_CIPHER_get_name(SSL_get_current_cipher(ssl));
	cipher.len = cipher.s ? strlen(cipher.s) : 0;
	if (cipher.len >= 1024) {
		ERR("Cipher name too long\n");
		goto err;
	}
	memcpy(buf, cipher.s, cipher.len);
	res->s = buf;
	res->len = cipher.len;
	tcpconn_put(c);
	return 0;

 err:
	if (c) tcpconn_put(c);
	return -1;
}

static int sel_cipher(str* res, select_t* s, sip_msg_t* msg)
{
	return get_cipher(res, msg);
}





static int get_bits(str* res, int* i, sip_msg_t* msg) 
{
	str bits;
	int b;
	static char buf[1024];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		INFO("TLS connection not found in select_bits\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	b = SSL_CIPHER_get_bits(SSL_get_current_cipher(ssl), 0);
	bits.s = int2str(b, &bits.len);
	if (bits.len >= 1024) {
		ERR("Bits string too long\n");
		goto err;
	}
	memcpy(buf, bits.s, bits.len);
	res->s = buf;
	res->len = bits.len;
	if (i) *i = b;
	tcpconn_put(c);
	return 0;

 err:
	if (c) tcpconn_put(c);
	return -1;
}


static int sel_bits(str* res, select_t* s, sip_msg_t* msg) 
{
	return get_bits(res, NULL, msg);
}



static int get_version(str* res, sip_msg_t* msg)
{
	str version;
	static char buf[1024];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		INFO("TLS connection not found in select_version\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	version.s = (char*)SSL_get_version(ssl);
	version.len = version.s ? strlen(version.s) : 0;
	if (version.len >= 1024) {
		ERR("Version string too long\n");
		goto err;
	}
	memcpy(buf, version.s, version.len);
	res->s = buf;
	res->len = version.len;
	tcpconn_put(c);
        return 0;

 err:
	if (c) tcpconn_put(c);
	return -1;
}


static int sel_version(str* res, select_t* s, sip_msg_t* msg)
{
	return get_version(res, msg);
}


static int get_desc(str* res, sip_msg_t* msg)
{
	static char buf[128];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		INFO("TLS connection not found in select_desc\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	buf[0] = '\0';
	SSL_CIPHER_description(SSL_get_current_cipher(ssl), buf, 128);
	res->s = buf;
	res->len = strlen(buf);
	tcpconn_put(c);
	return 0;

 err:
	if (c) tcpconn_put(c);
	return -1;	
}


static int sel_desc(str* res, select_t* s, sip_msg_t* msg)
{
	return get_desc(res, msg);
}


static int get_cert_version(str* res, int local, sip_msg_t* msg)
{
	static char buf[INT2STR_MAX_LEN];
	X509* cert;
	struct tcp_connection* c;
	char* version;

	if (get_cert(&cert, &c, msg, local) < 0) return -1;
	version = int2str(X509_get_version(cert), &res->len);
	memcpy(buf, version, res->len);
	res->s = buf;
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return 0;
}

static int sel_cert_version(str* res, select_t* s, sip_msg_t* msg)
{
	int local;
	
	switch(s->params[s->n - 2].v.i) {
	case CERT_PEER: local = 0; break;
	case CERT_LOCAL: local = 1; break;
	default:
		BUG("Bug in call to sel_cert_version\n");
		return -1;
	}

	return get_cert_version(res, local, msg);
}



/*
 * Check whether peer certificate exists and verify the result
 * of certificate verification
 */
static int check_cert(str* res, int* ires, int local, int err, sip_msg_t* msg)
{
	static str succ = STR_STATIC_INIT("1");
	static str fail = STR_STATIC_INIT("0");

	struct tcp_connection* c;
	SSL* ssl;
	X509* cert = 0;

	c = get_cur_connection(msg);
	if (!c) return -1;

	ssl = get_ssl(c);
	if (!ssl) goto error;

	if (local) {
		DBG("Verification of local certificates not supported\n");
		goto error;
	} else {
		if ((cert = SSL_get_peer_certificate(ssl)) && SSL_get_verify_result(ssl) == err) {
			*res = succ;
			if (ires) *ires = 1;
		} else {
			*res = fail;
			if (ires) *ires = 0;
		}
	}

	if (cert) X509_free(cert);
	tcpconn_put(c);
	return 0;

 error:
	if (cert) X509_free(cert);
	if (c) tcpconn_put(c);
	return -1;
}


static int sel_check_cert(str* res, select_t* s, sip_msg_t* msg)
{
	int local, err;
	
	switch(s->params[s->n - 2].v.i) {
	case CERT_PEER: local = 0; break;
	case CERT_LOCAL: local = 1; break;
	default:
		BUG("Bug in call to sel_cert_version\n");
		return -1;
	}

	switch (s->params[s->n - 1].v.i) {
	case CERT_VERIFIED:   err = X509_V_OK;                              break;
	case CERT_REVOKED:    err = X509_V_ERR_CERT_REVOKED;                break;
	case CERT_EXPIRED:    err = X509_V_ERR_CERT_HAS_EXPIRED;            break;
	case CERT_SELFSIGNED: err = X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT; break;
	default:
		BUG("Unexpected parameter value \"%d\"\n", s->params[s->n - 1].v.i);
		return -1;
	}   

	return check_cert(res, NULL, local, err, msg);
}



static int get_validity(str* res, int local, int bound, sip_msg_t* msg)
{
#define NOT_BEFORE 0
#define NOT_AFTER 1
	static char buf[1024];
	X509* cert;
	struct tcp_connection* c;
	BUF_MEM* p;
	BIO* mem = 0;
	ASN1_TIME* date;

	if (get_cert(&cert, &c, msg, local) < 0) return -1;

	switch (bound) {
	case NOT_BEFORE: date = X509_get_notBefore(cert); break;
	case NOT_AFTER:  date = X509_get_notAfter(cert);  break;
	default:
		BUG("Unexpected parameter value \"%d\"\n", bound);
		goto err;
	}

	mem = BIO_new(BIO_s_mem());
	if (!mem) {
		ERR("Error while creating memory BIO\n");
		goto err;
	}

	if (!ASN1_TIME_print(mem, date)) {
		ERR("Error while printing certificate date/time\n");
		goto err;
	}
	
	BIO_get_mem_ptr(mem, &p);
	if (p->length >= 1024) {
		ERR("Date/time too long\n");
		goto err;
	}
	memcpy(buf, p->data, p->length);
	res->s = buf;
	res->len = p->length;

	BIO_free(mem);
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return 0;
 err:
	if (mem) BIO_free(mem);
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return -1;
}

static int sel_validity(str* res, select_t* s, sip_msg_t* msg)
{
	int local, bound;
	
	switch(s->params[s->n - 2].v.i) {
	case CERT_PEER:  local = 0; break;
	case CERT_LOCAL: local = 1; break;
	default:
		BUG("Could not determine certificate\n");
		return -1;
	}

	switch (s->params[s->n - 1].v.i) {
	case CERT_NOTBEFORE: bound = NOT_BEFORE; break;
	case CERT_NOTAFTER:  bound = NOT_AFTER; break;
	default:
		BUG("Unexpected parameter value \"%d\"\n", s->params[s->n - 1].v.i);
		return -1;
	}

	return get_validity(res, local, bound, msg);
}



static int get_sn(str* res, int* ires, int local, sip_msg_t* msg)
{
	static char buf[INT2STR_MAX_LEN];
	X509* cert;
	struct tcp_connection* c;
	char* sn;
	int num;

	if (get_cert(&cert, &c, msg, local) < 0) return -1;

	num = ASN1_INTEGER_get(X509_get_serialNumber(cert));
	sn = int2str(num, &res->len);
	memcpy(buf, sn, res->len);
	res->s = buf;
	if (ires) *ires = num;
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return 0;
}

static int sel_sn(str* res, select_t* s, sip_msg_t* msg)
{
	int local;

	switch(s->params[s->n - 2].v.i) {
	case CERT_PEER:  local = 0; break;
	case CERT_LOCAL: local = 1; break;
	default:
		BUG("Could not determine certificate\n");
		return -1;
	}

	return get_sn(res, NULL, local, msg);
}


static int get_comp(str* res, int local, int issuer, int nid, sip_msg_t* msg)
{
	static char buf[1024];
	X509* cert;
	struct tcp_connection* c;
	X509_NAME* name;
	X509_NAME_ENTRY* e;
	ASN1_STRING* asn1;
	int index, text_len;
	char* elem;
	unsigned char* text_s;
	       
	text_s = 0;

	if (get_cert(&cert, &c, msg, local) < 0) return -1;

	name = issuer ? X509_get_issuer_name(cert) : X509_get_subject_name(cert);
	if (!name) {
		ERR("Cannot extract subject or issuer name from peer certificate\n");
		goto err;
	}

	index = X509_NAME_get_index_by_NID(name, nid, -1);
	if (index == -1) {
		switch(nid) {
		case NID_commonName:             elem = "CommonName";              break;
		case NID_organizationName:       elem = "OrganizationName";        break;
		case NID_organizationalUnitName: elem = "OrganizationalUnitUname"; break;
		case NID_countryName:            elem = "CountryName";             break;
		case NID_stateOrProvinceName:    elem = "StateOrProvinceName";     break;
		case NID_localityName:           elem = "LocalityName";            break;
		default:                         elem = "Unknown";                 break;
		}
		DBG("Element %s not found in certificate subject/issuer\n", elem);
		goto err;
	}

	e = X509_NAME_get_entry(name, index);
	asn1 = X509_NAME_ENTRY_get_data(e);
	text_len = ASN1_STRING_to_UTF8(&text_s, asn1);
	if (text_len < 0 || text_len >= 1024) {
		ERR("Error converting ASN1 string\n");
		goto err;
	}
	memcpy(buf, text_s, text_len);
	res->s = buf;
	res->len = text_len;

	OPENSSL_free(text_s);
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return 0;

 err:
	if (text_s) OPENSSL_free(text_s);
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return -1;
}


static int sel_comp(str* res, select_t* s, sip_msg_t* msg)
{
	int i, local = 0, issuer = 0;
	int nid = NID_commonName;

	for(i = 1; i <= s->n - 1; i++) {
		switch(s->params[i].v.i) {
		case CERT_LOCAL:   local = 1;                        break;
		case CERT_PEER:    local = 0;                        break;
		case CERT_SUBJECT: issuer = 0;                       break;
		case CERT_ISSUER:  issuer = 1;                       break;
		case COMP_CN:      nid = NID_commonName;             break;
		case COMP_O:       nid = NID_organizationName;       break;
		case COMP_OU:      nid = NID_organizationalUnitName; break;
		case COMP_C:       nid = NID_countryName;            break;
		case COMP_ST:      nid = NID_stateOrProvinceName;    break;
		case COMP_L:       nid = NID_localityName;           break;
		default:
			BUG("Bug in sel_comp: %d\n", s->params[s->n - 1].v.i);
			return -1;
		}
	}

	return get_comp(res, local, issuer, nid, msg);
}



static int get_alt(str* res, int local, int type, sip_msg_t* msg)
{
	static char buf[1024];
	int n, found = 0;
	STACK_OF(GENERAL_NAME)* names = 0;
	GENERAL_NAME* nm;
	X509* cert;
	struct tcp_connection* c;
	str text;
	struct ip_addr ip;

	if (get_cert(&cert, &c, msg, local) < 0) return -1;

	names = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (!names) {
		DBG("Cannot get certificate alternative subject\n");
		goto err;

	}

	for (n = 0; n < sk_GENERAL_NAME_num(names); n++) {
		nm = sk_GENERAL_NAME_value(names, n);
		if (nm->type != type) continue;
		switch(type) {
		case GEN_EMAIL:
		case GEN_DNS:
		case GEN_URI:
			text.s = (char*)nm->d.ia5->data;
			text.len = nm->d.ia5->length;
			if (text.len >= 1024) {
				ERR("Alternative subject text too long\n");
				goto err;
			}
			memcpy(buf, text.s, text.len);
			res->s = buf;
			res->len = text.len;
			found = 1;
			break;

		case GEN_IPADD:
			ip.len = nm->d.iPAddress->length;
			ip.af = (ip.len == 16) ? AF_INET6 : AF_INET;
			memcpy(ip.u.addr, nm->d.iPAddress->data, ip.len);
			text.s = ip_addr2a(&ip);
			text.len = strlen(text.s);
			memcpy(buf, text.s, text.len);
			res->s = buf;
			res->len = text.len;
			found = 1;
			break;
		}
		break;
	}
	if (!found) goto err;

	if (names) sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return 0;
 err:
	if (names) sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
	if (!local) X509_free(cert);
	tcpconn_put(c);
	return -1;
}

static int sel_alt(str* res, select_t* s, sip_msg_t* msg)
{
	int type = GEN_URI, local = 0, i;

	for(i = 1; i <= s->n - 1; i++) {
		switch(s->params[i].v.i) {
		case CERT_LOCAL: local = 1; break;
		case CERT_PEER:  local = 0; break;
		case COMP_E:     type = GEN_EMAIL; break;
		case COMP_HOST:  type = GEN_DNS;   break;
		case COMP_URI:   type = GEN_URI;   break;
		case COMP_IP:    type = GEN_IPADD; break;
		default:
			BUG("Bug in sel_alt: %d\n", s->params[s->n - 1].v.i);
			return -1;
		}
	}
	
	return get_alt(res, local, type, msg);
}



static int sel_tls(str* res, select_t* s, struct sip_msg* msg)
{
	return sel_desc(res, s, msg);
}


static int sel_name(str* res, select_t* s, struct sip_msg* msg)
{
	return sel_comp(res, s, msg);
}


static int sel_cert(str* res, select_t* s, struct sip_msg* msg)
{
	return sel_comp(res, s, msg);
}


select_row_t tls_sel[] = {
	/* Current cipher parameters */
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("tls"), sel_tls, 0},
	
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("version"),     sel_version, 0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("desc"),        sel_desc,    0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("description"), sel_desc,    0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("cipher"),      sel_cipher,  0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("peer"),        sel_cert,    DIVERSION | CERT_PEER},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("my"),          sel_cert,    DIVERSION | CERT_LOCAL},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("me"),          sel_cert,    DIVERSION | CERT_LOCAL},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("myself"),      sel_cert,    DIVERSION | CERT_LOCAL},
	
	{ sel_cipher, SEL_PARAM_STR, STR_STATIC_INIT("bits"), sel_bits, 0},
	
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("subject"), sel_name, DIVERSION | CERT_SUBJECT},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("subj"),    sel_name, DIVERSION | CERT_SUBJECT},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("issuer"),  sel_name, DIVERSION | CERT_ISSUER},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("verified"),    sel_check_cert, DIVERSION | CERT_VERIFIED},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("revoked"),     sel_check_cert, DIVERSION | CERT_REVOKED},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("expired"),     sel_check_cert, DIVERSION | CERT_EXPIRED},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("self_signed"), sel_check_cert, DIVERSION | CERT_SELFSIGNED},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("version"), sel_cert_version, 0},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("sn"),            sel_sn, 0},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("serialNumber"),  sel_sn, 0},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("serial_number"), sel_sn, 0},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("notBefore"),  sel_validity, DIVERSION | CERT_NOTBEFORE},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("not_before"), sel_validity, DIVERSION | CERT_NOTBEFORE},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("notAfter"),   sel_validity, DIVERSION | CERT_NOTAFTER},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("not_after"),  sel_validity, DIVERSION | CERT_NOTAFTER},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("email"),         sel_alt, DIVERSION | COMP_E},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("emailAddress"),  sel_alt, DIVERSION | COMP_E},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("email_address"), sel_alt, DIVERSION | COMP_E},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("host"),     sel_alt, DIVERSION | COMP_HOST},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("hostname"), sel_alt, DIVERSION | COMP_HOST},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("dns"),      sel_alt, DIVERSION | COMP_HOST},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("uri"), sel_alt, DIVERSION | COMP_URI},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("url"), sel_alt, DIVERSION | COMP_URI},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("urn"), sel_alt, DIVERSION | COMP_URI},

	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("ip"),         sel_alt, DIVERSION | COMP_IP},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("IPAddress"),  sel_alt, DIVERSION | COMP_IP},
	{ sel_cert, SEL_PARAM_STR, STR_STATIC_INIT("ip_address"), sel_alt, DIVERSION | COMP_IP},

	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("cn"),          sel_comp, DIVERSION | COMP_CN},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("commonName"),  sel_comp, DIVERSION | COMP_CN},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("common_name"), sel_comp, DIVERSION | COMP_CN},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("name"),        sel_comp, DIVERSION | COMP_CN},

	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("l"),             sel_comp, DIVERSION | COMP_L},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("localityName"),  sel_comp, DIVERSION | COMP_L},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("locality_name"), sel_comp, DIVERSION | COMP_L},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("locality"),      sel_comp, DIVERSION | COMP_L},

	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("c"),            sel_comp, DIVERSION | COMP_C},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("countryName"),  sel_comp, DIVERSION | COMP_C},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("country_name"), sel_comp, DIVERSION | COMP_C},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("country"),      sel_comp, DIVERSION | COMP_C},

	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("st"),                     sel_comp, DIVERSION | COMP_ST},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("stateOrProvinceName"),    sel_comp, DIVERSION | COMP_ST},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("state_or_province_name"), sel_comp, DIVERSION | COMP_ST},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("state"),                  sel_comp, DIVERSION | COMP_ST},

	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("o"),                 sel_comp, DIVERSION | COMP_O},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("organizationName"),  sel_comp, DIVERSION | COMP_O},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("organization_name"), sel_comp, DIVERSION | COMP_O},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("organization"),      sel_comp, DIVERSION | COMP_O},

	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("ou"),                       sel_comp, DIVERSION | COMP_OU},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("organizationalUnitName"),   sel_comp, DIVERSION | COMP_OU},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("organizational_unit_name"), sel_comp, DIVERSION | COMP_OU},
	{ sel_name, SEL_PARAM_STR, STR_STATIC_INIT("unit"),                     sel_comp, DIVERSION | COMP_OU},

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};
