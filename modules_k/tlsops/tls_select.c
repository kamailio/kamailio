/*
 * $Id$
 *
 * TLS module - select interface
 *
 * Copyright (C)  2001-2003 FhG FOKUS
 * Copyright (C)  2004,2005 Free Software Foundation, Inc.
 * Copyright (C)  2005 iptelorg GmbH
 * Copyright (C)  2006 enum.at
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include "../../globals.h"
#include "../../tcp_server.h"
#include "../../tcp_conn.h"
#include "../../ut.h"
#include "tls_select.h"


struct tcp_connection* get_cur_connection(struct sip_msg* msg)
{
	struct tcp_connection* c;

	if (msg->rcv.proto != PROTO_TLS) {
		LM_ERR("transport protocol is not TLS (bug in config)\n");
		return 0;
	}

	c = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, tcp_con_lifetime);
	if (c && c->type != PROTO_TLS) {
		LM_ERR("connection found but is not TLS (bug in config)\n");
		tcpconn_put(c);
		return 0;
	}
	return c;
}


static inline SSL* get_ssl(struct tcp_connection* c)
{
	if (!c || !c->extra_data) {
		LM_ERR("failed to extract SSL data from TLS connection\n");
		return 0;
	}
	return c->extra_data;
}


static inline int get_cert(X509** cert, struct tcp_connection** c,
												struct sip_msg* msg, int my)
{
	SSL* ssl;

	*cert = 0;
	*c = get_cur_connection(msg);
	if (!(*c)) {
		LM_INFO("TLS connection not found\n");
		return -1;
	}
	ssl = get_ssl(*c);
	if (!ssl) goto err;
	*cert = my ? SSL_get_certificate(ssl) : SSL_get_peer_certificate(ssl);
	if (!*cert) {
		LM_ERR("failed to get certificate from SSL structure\n");
		goto err;
	}

	return 0;
err:
	tcpconn_put(*c);
	return -1;
}


int tlsops_cipher(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str cipher;
	static char buf[1024];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		LM_INFO("TLS connection not found in select_cipher\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	cipher.s = (char*)SSL_CIPHER_get_name(SSL_get_current_cipher(ssl));
	cipher.len = cipher.s ? strlen(cipher.s) : 0;
	if (cipher.len >= 1024) {
		LM_ERR("cipher name too long\n");
		goto err;
	}
	memcpy(buf, cipher.s, cipher.len);
	res->rs.s = buf;
	res->rs.len = cipher.len;
	res->flags = PV_VAL_STR;
	tcpconn_put(c);

	return 0;
err:
	if (c) tcpconn_put(c);
	return pv_get_null(msg, param, res);
}


int tlsops_bits(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str bits;
	int b;
	static char buf[1024];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		LM_INFO("TLS connection not found in select_bits\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	b = SSL_CIPHER_get_bits(SSL_get_current_cipher(ssl), 0);
	bits.s = int2str(b, &bits.len);
	if (bits.len >= 1024) {
		LM_ERR("bits string too long\n");
		goto err;
	}
	memcpy(buf, bits.s, bits.len);
	res->rs.s = buf;
	res->rs.len = bits.len;
	res->ri = b;
	res->flags = PV_VAL_STR | PV_VAL_INT;
	tcpconn_put(c);

	return 0;
err:
	if (c) tcpconn_put(c);
	return pv_get_null(msg, param, res);
}


int tlsops_version(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	str version;
	static char buf[1024];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		LM_INFO("TLS connection not found in select_version\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	version.s = (char*)SSL_get_version(ssl);
	version.len = version.s ? strlen(version.s) : 0;
	if (version.len >= 1024) {
		LM_ERR("version string too long\n");
		goto err;
	}
	memcpy(buf, version.s, version.len);

	res->rs.s = buf;
	res->rs.len = version.len;
	res->flags = PV_VAL_STR;

	tcpconn_put(c);

	return 0;
err:
	if (c) tcpconn_put(c);
	return pv_get_null(msg, param, res);
}


int tlsops_desc(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char buf[128];

	struct tcp_connection* c;
	SSL* ssl;

	c = get_cur_connection(msg);
	if (!c) {
		LM_INFO("TLS connection not found in select_desc\n");
		goto err;
	}
	ssl = get_ssl(c);
	if (!ssl) goto err;

	buf[0] = '\0';
	SSL_CIPHER_description(SSL_get_current_cipher(ssl), buf, 128);
	res->rs.s = buf;
	res->rs.len = strlen(buf);
	res->flags = PV_VAL_STR;

	tcpconn_put(c);

	return 0;
err:
	if (c) tcpconn_put(c);
	return pv_get_null(msg, param, res);
}


int tlsops_cert_version(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char buf[INT2STR_MAX_LEN];
	X509* cert;
	struct tcp_connection* c;
	char* version;
	int my;

	if (param->pvn.u.isname.name.n & CERT_PEER) {
		my = 0;
	} else if (param->pvn.u.isname.name.n & CERT_LOCAL) {
		my = 1;
	} else {
		LM_CRIT("bug in call to tlsops_cert_version\n");
		return pv_get_null(msg, param, res);
	}

	if (get_cert(&cert, &c, msg, my) < 0) return -1;
	version = int2str(X509_get_version(cert), &res->rs.len);
	memcpy(buf, version, res->rs.len);
	res->rs.s = buf;
	res->flags = PV_VAL_STR;
	if (!my) X509_free(cert);
	tcpconn_put(c);
	return 0;
}


/*
 * Check whether peer certificate exists and verify the result
 * of certificate verification
 */
int tlsops_check_cert(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static str succ = str_init("1");
	static str fail = str_init("0");

	int err;
	struct tcp_connection* c;
	SSL* ssl;
	X509* cert = 0;

	switch (param->pvn.u.isname.name.n) {
	case CERT_VERIFIED:   err = X509_V_OK;                              break;
	case CERT_REVOKED:    err = X509_V_ERR_CERT_REVOKED;                break;
	case CERT_EXPIRED:    err = X509_V_ERR_CERT_HAS_EXPIRED;            break;
	case CERT_SELFSIGNED: err = X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT; break;
	default:
		LM_CRIT("unexpected parameter value \"%d\"\n",
				param->pvn.u.isname.name.n);
		return pv_get_null(msg, param, res);
	}   

	c = get_cur_connection(msg);
	if (!c) return -1;

	ssl = get_ssl(c);
	if (!ssl) goto err;

	if ((cert = SSL_get_peer_certificate(ssl)) && SSL_get_verify_result(ssl) == err) {
		res->rs.s = succ.s;
		res->rs.len = succ.len;
		res->ri   = 1;
	} else {
		res->rs.s = fail.s;
		res->rs.len = fail.len;
		res->ri   = 0;
	}
	res->flags = PV_VAL_STR | PV_VAL_INT;

	if (cert) X509_free(cert);
	tcpconn_put(c);

	return 0;
err:
	if (cert) X509_free(cert);
	if (c) tcpconn_put(c);
	return pv_get_null(msg, param, res);
}


int tlsops_validity(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char buf[1024];
	X509* cert;
	struct tcp_connection* c;
	BUF_MEM* p;
	BIO* mem = 0;
	ASN1_TIME* date;
	int my = 0;

	if (get_cert(&cert, &c, msg, my) < 0) return -1;

	switch (param->pvn.u.isname.name.n) {
	case CERT_NOTBEFORE: date = X509_get_notBefore(cert); break;
	case CERT_NOTAFTER:  date = X509_get_notAfter(cert);  break;
	default:
		LM_CRIT("unexpected parameter value \"%d\"\n", param->pvn.u.isname.name.n);
		goto err;
	}

	mem = BIO_new(BIO_s_mem());
	if (!mem) {
		LM_ERR("failed to create memory BIO\n");
		goto err;
	}

	if (!ASN1_TIME_print(mem, date)) {
		LM_ERR("failed to print certificate date/time\n");
		goto err;
	}
	
	BIO_get_mem_ptr(mem, &p);
	if (p->length >= 1024) {
		LM_ERR("Date/time too long\n");
		goto err;
	}
	memcpy(buf, p->data, p->length);
	res->rs.s = buf;
	res->rs.len = p->length;
	res->flags = PV_VAL_STR ;

	BIO_free(mem);
	if (!my) X509_free(cert);
	tcpconn_put(c);

	return 0;
err:
	if (mem) BIO_free(mem);
	if (!my) X509_free(cert);
	tcpconn_put(c);
	return pv_get_null(msg, param, res);
}


int tlsops_sn(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char buf[INT2STR_MAX_LEN];
	X509* cert;
	struct tcp_connection* c;
	int my, serial;
	char* sn;

	if (param->pvn.u.isname.name.n & CERT_PEER) {
		my = 0;
	} else if (param->pvn.u.isname.name.n & CERT_LOCAL) {
		my = 1;
	} else {
		LM_CRIT("could not determine certificate\n");
		return pv_get_null(msg, param, res);
	}
	
	if (get_cert(&cert, &c, msg, my) < 0)
		return pv_get_null(msg, param, res);
	
	serial = ASN1_INTEGER_get(X509_get_serialNumber(cert));
	sn = int2str( serial, &res->rs.len);
	memcpy(buf, sn, res->rs.len);
	res->rs.s = buf;
	res->ri = serial;
	res->flags = PV_VAL_STR | PV_VAL_INT;	
	
	if (!my) X509_free(cert);
	tcpconn_put(c);
	return 0;
}

int tlsops_comp(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char buf[1024];
	X509* cert;
	struct tcp_connection* c;
	X509_NAME* name;
	X509_NAME_ENTRY* e;
	ASN1_STRING* asn1;
	int nid = NID_commonName, index, my = 0, issuer = 0, ind_local;
	char* elem;
	str text;

	text.s = 0;
	/* copy callback value as we modify it */
	ind_local = param->pvn.u.isname.name.n;

	LM_DBG("ind_local = %x", ind_local);
	if (ind_local & CERT_PEER) {
		my = 0;
		ind_local = ind_local ^ CERT_PEER;
	} else if (ind_local & CERT_LOCAL) {
		my = 1;
		ind_local = ind_local ^ CERT_LOCAL;
	} else {
		LM_CRIT("could not determine certificate\n");
		return pv_get_null(msg, param, res);
	}

	if (ind_local & CERT_SUBJECT) {
		issuer = 0;
		ind_local = ind_local ^ CERT_SUBJECT;
	} else if (ind_local & CERT_ISSUER) {
		issuer = 1;
		ind_local = ind_local ^ CERT_ISSUER;
	} else {
		LM_CRIT("could not determine subject or issuer\n");
		return pv_get_null(msg, param, res);
	}

	switch(ind_local) {
		case COMP_CN: nid = NID_commonName;             break;
		case COMP_O:  nid = NID_organizationName;       break;
		case COMP_OU: nid = NID_organizationalUnitName; break;
		case COMP_C:  nid = NID_countryName;            break;
		case COMP_ST: nid = NID_stateOrProvinceName;    break;
		case COMP_L:  nid = NID_localityName;           break;
		default:      nid = NID_undef;
	}

	if (get_cert(&cert, &c, msg, my) < 0) return -1;

	name = issuer ? X509_get_issuer_name(cert) : X509_get_subject_name(cert);
	if (!name) {
		LM_ERR("cannot extract subject or issuer name from peer"
				" certificate\n");
		goto err;
	}

	if (nid == NID_undef) { /* dump the whole cert info into buf */
		X509_NAME_oneline(name, buf, sizeof(buf));
		res->rs.s = buf;
		res->rs.len = strlen(buf);
		res->flags = PV_VAL_STR;
	} else {
		index = X509_NAME_get_index_by_NID(name, nid, -1);
		if (index == -1) {
			switch(ind_local) {
			case COMP_CN: elem = "CommonName";              break;
			case COMP_O:  elem = "OrganizationName";        break;
			case COMP_OU: elem = "OrganizationalUnitUname"; break;
			case COMP_C:  elem = "CountryName";             break;
			case COMP_ST: elem = "StateOrProvinceName";     break;
			case COMP_L:  elem = "LocalityName";            break;
			default:      elem = "Unknown";                 break;
			}
			LM_DBG("element %s not found in "
				"certificate subject/issuer\n", elem);
			goto err;
		}
	
		e = X509_NAME_get_entry(name, index);
		asn1 = X509_NAME_ENTRY_get_data(e);
		text.len = ASN1_STRING_to_UTF8((unsigned char**)(void*)&text.s, asn1);
		if (text.len < 0 || text.len >= 1024) {
			LM_ERR("failed to convert ASN1 string\n");
			goto err;
		}
		memcpy(buf, text.s, text.len);
		res->rs.s = buf;
		res->rs.len = text.len;
		res->flags = PV_VAL_STR;
	
		OPENSSL_free(text.s);
	}
	if (!my) X509_free(cert);
	tcpconn_put(c);
	return 0;

 err:
	if (text.s) OPENSSL_free(text.s);
	if (!my) X509_free(cert);
	tcpconn_put(c);
	return pv_get_null(msg, param, res);
}

int tlsops_alt(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	static char buf[1024];
	int type = GEN_URI, my = 0, n, found = 0, ind_local;
	STACK_OF(GENERAL_NAME)* names = 0;
	GENERAL_NAME* nm;
	X509* cert;
	struct tcp_connection* c;
	str text;
	struct ip_addr ip;

	ind_local = param->pvn.u.isname.name.n;

	if (ind_local & CERT_PEER) {
		my = 0;
		ind_local = ind_local ^ CERT_PEER;
	} else if (ind_local & CERT_LOCAL) {
		my = 1;
		ind_local = ind_local ^ CERT_LOCAL;
	} else {
		LM_CRIT("could not determine certificate\n");
		return pv_get_null(msg, param, res);
	}

	switch(ind_local) {
		case COMP_E:    type = GEN_EMAIL; break;
		case COMP_HOST: type = GEN_DNS;   break;
		case COMP_URI:  type = GEN_URI;   break;
		case COMP_IP:   type = GEN_IPADD; break;
		default:
			LM_CRIT("ind_local=%d\n", ind_local);
			return pv_get_null(msg, param, res);
	}

	if (get_cert(&cert, &c, msg, my) < 0) return -1;

	names = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (!names) {
		LM_ERR("cannot get certificate alternative subject\n");
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
				LM_ERR("alternative subject text too long\n");
				goto err;
			}
			memcpy(buf, text.s, text.len);
			res->rs.s = buf;
			res->rs.len = text.len;
			res->flags = PV_VAL_STR;
			found = 1;
			break;

		case GEN_IPADD:
			ip.len = nm->d.iPAddress->length;
			ip.af = (ip.len == 16) ? AF_INET6 : AF_INET;
			memcpy(ip.u.addr, nm->d.iPAddress->data, ip.len);
			text.s = ip_addr2a(&ip);
			text.len = strlen(text.s);
			memcpy(buf, text.s, text.len);
			res->rs.s = buf;
			res->rs.len = text.len;
			res->flags = PV_VAL_STR;
			found = 1;
			break;
		}
		break;
	}
	if (!found) goto err;

	if (names) sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
	if (!my) X509_free(cert);
	tcpconn_put(c);
	return 0;
 err:
	if (names) sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
	if (!my) X509_free(cert);
	tcpconn_put(c);
	return pv_get_null(msg, param, res);
}

