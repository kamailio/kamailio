/*
 * TLS module - select interface
 *
 * Copyright (C) 2005 iptelorg GmbH
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

/** Kamailio TLS support :: Select interface.
 * @file
 * @ingroup tls
 * Module: @ref tls
 */


#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include "../../globals.h"
#include "../../tcp_server.h"
#include "../../tcp_conn.h"
#include "../../ut.h"
#include "../../cfg/cfg.h"
#include "../../dprint.h"
#include "tls_server.h"
#include "tls_select.h"
#include "tls_mod.h"
#include "tls_init.h" /* features macros */
#include "tls_cfg.h"

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
	COMP_IP,          /* IP from subject/alternative */
	TLSEXT_SN         /* Server name of the peer */
};


enum {
	PV_CERT_LOCAL      = 1<<0,   /* Select local certificate */
	PV_CERT_PEER       = 1<<1,   /* Select peer certificate */
	PV_CERT_SUBJECT    = 1<<2,   /* Select subject part of certificate */
	PV_CERT_ISSUER     = 1<<3,   /* Select issuer part of certificate */

	PV_CERT_VERIFIED   = 1<<4,   /* Test for verified certificate */
	PV_CERT_REVOKED    = 1<<5,   /* Test for revoked certificate */
	PV_CERT_EXPIRED    = 1<<6,   /* Expiration certificate test */
	PV_CERT_SELFSIGNED = 1<<7,   /* self-signed certificate test */
	PV_CERT_NOTBEFORE  = 1<<8,   /* Select validity end from certificate */
	PV_CERT_NOTAFTER   = 1<<9,   /* Select validity start from certificate */

	PV_COMP_CN = 1<<10,          /* Common name */
	PV_COMP_O  = 1<<11,          /* Organization name */
	PV_COMP_OU = 1<<12,          /* Organization unit */
	PV_COMP_C  = 1<<13,          /* Country name */
	PV_COMP_ST = 1<<14,          /* State */
	PV_COMP_L  = 1<<15,          /* Locality/town */

	PV_COMP_HOST = 1<<16,        /* hostname from subject/alternative */
	PV_COMP_URI  = 1<<17,        /* URI from subject/alternative */
	PV_COMP_E    = 1<<18,        /* Email address */
	PV_COMP_IP   = 1<<19,        /* IP from subject/alternative */

	PV_TLSEXT_SNI = 1<<20,       /* Peer's server name (TLS extension) */
};



static struct tcp_connection* _tls_pv_con = 0;


void tls_set_pv_con(struct tcp_connection *c)
{
	_tls_pv_con = c;
}

struct tcp_connection* get_cur_connection(struct sip_msg* msg)
{
	struct tcp_connection* c;

	if(_tls_pv_con != 0)
		return _tls_pv_con;

	if (msg->rcv.proto != PROTO_TLS) {
		ERR("Transport protocol is not TLS (bug in config)\n");
		return 0;
	}

	c = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0,
					cfg_get(tls, tls_cfg, con_lifetime));
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


static int pv_cipher(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	if (get_cipher(&res->rs, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	res->flags = PV_VAL_STR;
	return 0;
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

static int pv_bits(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	if (get_bits(&res->rs, &res->ri, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	res->flags = PV_VAL_STR | PV_VAL_INT;
	return 0;
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


static int pv_version(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	if (get_version(&res->rs, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	res->flags = PV_VAL_STR;
	return 0;
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

static int pv_desc(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	if (get_desc(&res->rs, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	res->flags = PV_VAL_STR;
	return 0;
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

static int pv_cert_version(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	int local;
	
	if (param->pvn.u.isname.name.n & PV_CERT_PEER) {
		local = 0;
	} else if (param->pvn.u.isname.name.n & PV_CERT_LOCAL) {
		local = 1;
	} else {
		BUG("bug in call to pv_cert_version\n");
		return pv_get_null(msg, param, res);
	}

	if (get_cert_version(&res->rs, local, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	res->flags = PV_VAL_STR;
	return 0;
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

static int pv_check_cert(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	int err;
	
	switch (param->pvn.u.isname.name.n) {
	case PV_CERT_VERIFIED:   err = X509_V_OK;                              break;
	case PV_CERT_REVOKED:    err = X509_V_ERR_CERT_REVOKED;                break;
	case PV_CERT_EXPIRED:    err = X509_V_ERR_CERT_HAS_EXPIRED;            break;
	case PV_CERT_SELFSIGNED: err = X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT; break;
	default:
		BUG("unexpected parameter value \"%d\"\n", param->pvn.u.isname.name.n);
		return pv_get_null(msg, param, res);
	}
	

	if (check_cert(&res->rs, &res->ri, 0, err, msg) < 0) {
		return pv_get_null(msg, param, res);
	}

	res->flags = PV_VAL_STR | PV_VAL_INT;
	return 0;
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


static int pv_validity(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	int bound;
	
	switch (param->pvn.u.isname.name.n) {
	case PV_CERT_NOTBEFORE: bound = NOT_BEFORE; break;
	case PV_CERT_NOTAFTER:  bound = NOT_AFTER;  break;
	default:
		BUG("unexpected parameter value \"%d\"\n", param->pvn.u.isname.name.n);
		return pv_get_null(msg, param, res);
	}

	if (get_validity(&res->rs, 0, bound, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	
	res->flags = PV_VAL_STR;
	return 0;
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


static int pv_sn(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	int local;
	
	if (param->pvn.u.isname.name.n & PV_CERT_PEER) {
		local = 0;
	} else if (param->pvn.u.isname.name.n & PV_CERT_LOCAL) {
		local = 1;
	} else {
		BUG("could not determine certificate\n");
		return pv_get_null(msg, param, res);
	}
	
	if (get_sn(&res->rs, &res->ri, local, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	
	res->flags = PV_VAL_STR | PV_VAL_INT;
	return 0;
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


static int pv_comp(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	int ind_local, local = 0, issuer = 0, nid = NID_commonName;

	/* copy callback value as we modify it */
	ind_local = param->pvn.u.isname.name.n;	
	DBG("ind_local = %x", ind_local);

	if (ind_local & PV_CERT_PEER) {
		local = 0;
		ind_local = ind_local ^ PV_CERT_PEER;
	} else if (ind_local & PV_CERT_LOCAL) {
		local = 1;
		ind_local = ind_local ^ PV_CERT_LOCAL;
	} else {
		BUG("could not determine certificate\n");
		return pv_get_null(msg, param, res);
	}

	if (ind_local & PV_CERT_SUBJECT) {
		issuer = 0;
		ind_local = ind_local ^ PV_CERT_SUBJECT;
	} else if (ind_local & PV_CERT_ISSUER) {
		issuer = 1;
		ind_local = ind_local ^ PV_CERT_ISSUER;
	} else {
		BUG("could not determine subject or issuer\n");
		return pv_get_null(msg, param, res);
	}

	switch(ind_local) {
		case PV_COMP_CN: nid = NID_commonName;             break;
		case PV_COMP_O:  nid = NID_organizationName;       break;
		case PV_COMP_OU: nid = NID_organizationalUnitName; break;
		case PV_COMP_C:  nid = NID_countryName;            break;
		case PV_COMP_ST: nid = NID_stateOrProvinceName;    break;
		case PV_COMP_L:  nid = NID_localityName;           break;
		default:      nid = NID_undef;
	}

	if (get_comp(&res->rs, local, issuer, nid, msg) < 0) {
		return pv_get_null(msg, param, res);
	}

	res->flags = PV_VAL_STR;
	return 0;
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


static int pv_alt(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	int ind_local, local = 0, type = GEN_URI;
	
	ind_local = param->pvn.u.isname.name.n;

	if (ind_local & PV_CERT_PEER) {
		local = 0;
		ind_local = ind_local ^ PV_CERT_PEER;
	} else if (ind_local & PV_CERT_LOCAL) {
		local = 1;
		ind_local = ind_local ^ PV_CERT_LOCAL;
	} else {
		BUG("could not determine certificate\n");
		return pv_get_null(msg, param, res);
	}

	switch(ind_local) {
		case PV_COMP_E:    type = GEN_EMAIL; break;
		case PV_COMP_HOST: type = GEN_DNS;   break;
		case PV_COMP_URI:  type = GEN_URI;   break;
		case PV_COMP_IP:   type = GEN_IPADD; break;
		default:
			BUG("ind_local=%d\n", ind_local);
			return pv_get_null(msg, param, res);
	}

	if (get_alt(&res->rs, local, type, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	
	res->flags = PV_VAL_STR;
	return 0;
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


#ifdef OPENSSL_NO_TLSEXT
static int get_tlsext_sn(str* res, sip_msg_t* msg)
{
	ERR("TLS extension 'server name' is not available! "
		"please install openssl with TLS extension support and recompile "
		"the server\n");
	return -1;
}
#else
static int get_tlsext_sn(str* res, sip_msg_t* msg)
{
	static char buf[1024];
	struct tcp_connection* c;
	str server_name;	
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		INFO("TLS connection not found in select_desc\n");
		goto error;
	}
	ssl = get_ssl(c);
	if (!ssl) goto error;

	buf[0] = '\0';

	server_name.s = (char*)SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (server_name.s) {
		server_name.len = strlen(server_name.s);
		DBG("received server_name (TLS extension): '%.*s'\n", 
			STR_FMT(&server_name));
	} else {
		DBG("SSL_get_servername returned NULL\n");
		goto error;
	}
	
	/* copy server_name into the buffer. If the buffer is too small copy only
	 * the last bytes as these are the more important ones and prefix with
	 * '+' */
	if (server_name.len > sizeof(buf)) {
		ERR("server_name to big for buffer\n");
		buf[0] = '+';
		memcpy(buf + 1, server_name.s + 1 + server_name.len - sizeof(buf), 
			   sizeof(buf) - 1);
		res->len = sizeof(buf);
	} else {
		memcpy(buf, server_name.s, server_name.len);
		res->len = server_name.len;
	}
	res->s = buf;
	
	tcpconn_put(c);
	return 0;
	
error:
	if (c) tcpconn_put(c);
	return -1;
}
#endif


static int sel_tlsext_sn(str* res, select_t* s, sip_msg_t* msg)
{
	return get_tlsext_sn(res, msg);
}


static int pv_tlsext_sn(sip_msg_t* msg, pv_param_t* param, pv_value_t* res)
{
	if (param->pvn.u.isname.name.n != PV_TLSEXT_SNI) {
		BUG("unexpected parameter value \"%d\"\n",
			param->pvn.u.isname.name.n);
		return pv_get_null(msg, param, res);
	}
	
	if (get_tlsext_sn(&res->rs, msg) < 0) {
		return pv_get_null(msg, param, res);
	}
	
	res->flags = PV_VAL_STR;
	return 0;
}





select_row_t tls_sel[] = {
	/* Current cipher parameters */
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("tls"), sel_tls, 0},
	
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("version"),     sel_version, 0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("desc"),        sel_desc,    0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("description"), sel_desc,    0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("cipher"),      sel_cipher,  0},

	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("serverName"), sel_tlsext_sn,  0},
	{ sel_tls, SEL_PARAM_STR, STR_STATIC_INIT("server_name"), sel_tlsext_sn,  0},

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


/*
 *  pseudo variables
 */
pv_export_t tls_pv[] = {
	/* TLS session parameters */
	{{"tls_version", sizeof("tls_version")-1},
		PVT_OTHER, pv_version, 0,
		0, 0, 0, 0 },
	{{"tls_description", sizeof("tls_description")-1},
		PVT_OTHER, pv_desc, 0,
		0, 0, 0, 0 },
	{{"tls_cipher_info", sizeof("tls_cipher_info")-1},
		PVT_OTHER, pv_cipher, 0,
		0, 0, 0, 0 },
	{{"tls_cipher_bits", sizeof("tls_cipher_bits")-1},
		PVT_OTHER,  pv_bits, 0,
		0, 0, 0, 0 },
	/* general certificate parameters for peer and local */
	{{"tls_peer_version", sizeof("tls_peer_version")-1},
		PVT_OTHER, pv_cert_version, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  },
	{{"tls_my_version", sizeof("tls_my_version")-1},
		PVT_OTHER, pv_cert_version, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL },
	{{"tls_peer_serial", sizeof("tls_peer_serial")-1},
		PVT_OTHER, pv_sn, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  },
	{{"tls_my_serial", sizeof("tls_my_serial")-1},
		PVT_OTHER, pv_sn,0,
		0, 0, pv_init_iname, PV_CERT_LOCAL },
	/* certificate parameters for peer and local, for subject and issuer*/	
	{{"tls_peer_subject", sizeof("tls_peer_subject")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_SUBJECT },
	{{"tls_peer_issuer", sizeof("tls_peer_issuer")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_ISSUER  },
	{{"tls_my_subject", sizeof("tls_my_subject")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_SUBJECT },
	{{"tls_my_issuer", sizeof("tls_my_issuer")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_ISSUER  },
	{{"tls_peer_subject_cn", sizeof("tls_peer_subject_cn")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_SUBJECT | PV_COMP_CN },
	{{"tls_peer_issuer_cn", sizeof("tls_peer_issuer_cn")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_ISSUER  | PV_COMP_CN },
	{{"tls_my_subject_cn", sizeof("tls_my_subject_cn")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_SUBJECT | PV_COMP_CN },
	{{"tls_my_issuer_cn", sizeof("tls_my_issuer_cn")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_ISSUER  | PV_COMP_CN },
	{{"tls_peer_subject_locality", sizeof("tls_peer_subject_locality")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_SUBJECT | PV_COMP_L },
	{{"tls_peer_issuer_locality", sizeof("tls_peer_issuer_locality")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_ISSUER  | PV_COMP_L },
	{{"tls_my_subject_locality", sizeof("tls_my_subject_locality")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_SUBJECT | PV_COMP_L },
	{{"tls_my_issuer_locality", sizeof("tls_my_issuer_locality")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_ISSUER  | PV_COMP_L },
	{{"tls_peer_subject_country", sizeof("tls_peer_subject_country")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_SUBJECT | PV_COMP_C },
	{{"tls_peer_issuer_country", sizeof("tls_peer_issuer_country")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_ISSUER  | PV_COMP_C },
	{{"tls_my_subject_country", sizeof("tls_my_subject_country")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_SUBJECT | PV_COMP_C },
	{{"tls_my_issuer_country", sizeof("tls_my_issuer_country")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_ISSUER  | PV_COMP_C },
	{{"tls_peer_subject_state", sizeof("tls_peer_subject_state")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_SUBJECT | PV_COMP_ST },
	{{"tls_peer_issuer_state", sizeof("tls_peer_issuer_state")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_ISSUER  | PV_COMP_ST },
	{{"tls_my_subject_state", sizeof("tls_my_subject_state")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_SUBJECT | PV_COMP_ST },
	{{"tls_my_issuer_state", sizeof("tls_my_issuer_state")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_ISSUER  | PV_COMP_ST },
	{{"tls_peer_subject_organization", sizeof("tls_peer_subject_organization")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_SUBJECT | PV_COMP_O },
	{{"tls_peer_issuer_organization", sizeof("tls_peer_issuer_organization")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_ISSUER  | PV_COMP_O },
	{{"tls_my_subject_organization", sizeof("tls_my_subject_organization")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_SUBJECT | PV_COMP_O },
	{{"tls_my_issuer_organization", sizeof("tls_my_issuer_organization")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_ISSUER  | PV_COMP_O },
	{{"tls_peer_subject_unit", sizeof("tls_peer_subject_unit")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_SUBJECT | PV_COMP_OU },
	{{"tls_peer_issuer_unit", sizeof("tls_peer_issuer_unit")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_CERT_ISSUER  | PV_COMP_OU },
	{{"tls_my_subject_unit", sizeof("tls_my_subject_unit")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_SUBJECT | PV_COMP_OU },
	{{"tls_my_issuer_unit", sizeof("tls_my_issuer_unit")-1},
		PVT_OTHER, pv_comp, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_CERT_ISSUER  | PV_COMP_OU },
	/* subject alternative name parameters for peer and local */	
	{{"tls_peer_san_email", sizeof("tls_peer_san_email")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_COMP_E },
	{{"tls_my_san_email", sizeof("tls_my_san_email")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_COMP_E },
	{{"tls_peer_san_hostname", sizeof("tls_peer_san_hostname")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_COMP_HOST },
	{{"tls_my_san_hostname", sizeof("tls_my_san_hostname")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_COMP_HOST },
	{{"tls_peer_san_uri", sizeof("tls_peer_san_uri")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_COMP_URI },
	{{"tls_my_san_uri", sizeof("tls_my_san_uri")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_COMP_URI },
	{{"tls_peer_san_ip", sizeof("tls_peer_san_ip")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_PEER  | PV_COMP_IP },
	{{"tls_my_san_ip", sizeof("tls_my_san_ip")-1},
		PVT_OTHER, pv_alt, 0,
		0, 0, pv_init_iname, PV_CERT_LOCAL | PV_COMP_IP },
	/* peer certificate validation parameters */		
	{{"tls_peer_verified", sizeof("tls_peer_verified")-1},
		PVT_OTHER, pv_check_cert, 0,
		0, 0, pv_init_iname, PV_CERT_VERIFIED },
	{{"tls_peer_revoked", sizeof("tls_peer_revoked")-1},
		PVT_OTHER, pv_check_cert, 0,
		0, 0, pv_init_iname, PV_CERT_REVOKED },
	{{"tls_peer_expired", sizeof("tls_peer_expired")-1},
		PVT_OTHER, pv_check_cert, 0,
		0, 0, pv_init_iname, PV_CERT_EXPIRED },
	{{"tls_peer_selfsigned", sizeof("tls_peer_selfsigned")-1},
		PVT_OTHER, pv_check_cert, 0,
		0, 0, pv_init_iname, PV_CERT_SELFSIGNED },
	{{"tls_peer_notBefore", sizeof("tls_peer_notBefore")-1},
		PVT_OTHER, pv_validity, 0,
		0, 0, pv_init_iname, PV_CERT_NOTBEFORE },
	{{"tls_peer_notAfter", sizeof("tls_peer_notAfter")-1},
		PVT_OTHER, pv_validity, 0,
		0, 0, pv_init_iname, PV_CERT_NOTAFTER },
	/* peer certificate validation parameters */		
	{{"tls_peer_server_name", sizeof("tls_peer_server_name")-1},
		PVT_OTHER, pv_tlsext_sn, 0,
		0, 0, pv_init_iname, PV_TLSEXT_SNI },

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }

}; 
