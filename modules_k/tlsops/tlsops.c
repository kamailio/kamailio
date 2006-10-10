/*$Id$
 *
 * Copyright (C) 2006 nic.at
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * History:
 * -------
 *  2006-01-26  initial version
 *  
 * tls module, it implements the following commands:
 * is_peer_verified(): returns 1 if the message is received via TLS
 *     and the peer was verified during TLS connection handshake,
 *     otherwise it returns -1
 *  
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/ssl.h>

#include "tlsops.h"
#include "tls_select.h"
#include "../../tcp_conn.h"    /* struct tcp_connection */
#include "../../tcp_server.h"  /* tcpconn_get() */
#include "../../sr_module.h"

MODULE_VERSION

int tcp_con_lifetime=DEFAULT_TCP_CONNECTION_LIFETIME;

/* definition of exported functions */
static int is_peer_verified(struct sip_msg*, char*, char*);

/* definition of internal functions */
static int mod_init(void);
static void mod_destroy(void);

/*
 * Module parameter variables
 */

/*
 * Exported functions
 */
static cmd_export_t cmds[]={
	{"is_peer_verified", is_peer_verified,   0, 0, 
			REQUEST_ROUTE},
	{0,0,0,0,0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0,0,0}
}; 

/*
 *  pseudo variables
 */
static pseudo_export_t pseudos[] = {
	/* TLS session parameters */
	{"tls_version",      tlsops_version  , 0},
	{"tls_description",  tlsops_desc     , 0},
	{"tls_cipher_info",  tlsops_cipher   , 0},
	{"tls_cipher_bits",  tlsops_bits     , 0},
	/* general certificate parameters for peer and local */
	{"tls_peer_version", tlsops_cert_version, CERT_PEER  },
	{"tls_my_version",   tlsops_cert_version, CERT_LOCAL },
	{"tls_peer_serial",  tlsops_sn, CERT_PEER  },
	{"tls_my_serial",    tlsops_sn, CERT_LOCAL },
	/* certificate parameters for peer and local, for subject and issuer*/	
	{"tls_peer_subject", tlsops_comp     , CERT_PEER  | CERT_SUBJECT },
	{"tls_peer_issuer",  tlsops_comp     , CERT_PEER  | CERT_ISSUER  },
	{"tls_my_subject",   tlsops_comp     , CERT_LOCAL | CERT_SUBJECT },
	{"tls_my_issuer",    tlsops_comp     , CERT_LOCAL | CERT_ISSUER  },
	{"tls_peer_subject_cn", tlsops_comp     , CERT_PEER  | CERT_SUBJECT | COMP_CN },
	{"tls_peer_issuer_cn",  tlsops_comp     , CERT_PEER  | CERT_ISSUER  | COMP_CN },
	{"tls_my_subject_cn",   tlsops_comp     , CERT_LOCAL | CERT_SUBJECT | COMP_CN },
	{"tls_my_issuer_cn",    tlsops_comp     , CERT_LOCAL | CERT_ISSUER  | COMP_CN },
	{"tls_peer_subject_locality", tlsops_comp     , CERT_PEER  | CERT_SUBJECT | COMP_L },
	{"tls_peer_issuer_locality",  tlsops_comp     , CERT_PEER  | CERT_ISSUER  | COMP_L },
	{"tls_my_subject_locality",   tlsops_comp     , CERT_LOCAL | CERT_SUBJECT | COMP_L },
	{"tls_my_issuer_locality",    tlsops_comp     , CERT_LOCAL | CERT_ISSUER  | COMP_L },
	{"tls_peer_subject_country", tlsops_comp     , CERT_PEER  | CERT_SUBJECT | COMP_C },
	{"tls_peer_issuer_country",  tlsops_comp     , CERT_PEER  | CERT_ISSUER  | COMP_C },
	{"tls_my_subject_country",   tlsops_comp     , CERT_LOCAL | CERT_SUBJECT | COMP_C },
	{"tls_my_issuer_country",    tlsops_comp     , CERT_LOCAL | CERT_ISSUER  | COMP_C },
	{"tls_peer_subject_state", tlsops_comp     , CERT_PEER  | CERT_SUBJECT | COMP_ST },
	{"tls_peer_issuer_state",  tlsops_comp     , CERT_PEER  | CERT_ISSUER  | COMP_ST },
	{"tls_my_subject_state",   tlsops_comp     , CERT_LOCAL | CERT_SUBJECT | COMP_ST },
	{"tls_my_issuer_state",    tlsops_comp     , CERT_LOCAL | CERT_ISSUER  | COMP_ST },
	{"tls_peer_subject_organization", tlsops_comp     , CERT_PEER  | CERT_SUBJECT | COMP_O },
	{"tls_peer_issuer_organization",  tlsops_comp     , CERT_PEER  | CERT_ISSUER  | COMP_O },
	{"tls_my_subject_organization",   tlsops_comp     , CERT_LOCAL | CERT_SUBJECT | COMP_O },
	{"tls_my_issuer_organization",    tlsops_comp     , CERT_LOCAL | CERT_ISSUER  | COMP_O },
	{"tls_peer_subject_unit", tlsops_comp     , CERT_PEER  | CERT_SUBJECT | COMP_OU },
	{"tls_peer_issuer_unit",  tlsops_comp     , CERT_PEER  | CERT_ISSUER  | COMP_OU },
	{"tls_my_subject_unit",   tlsops_comp     , CERT_LOCAL | CERT_SUBJECT | COMP_OU },
	{"tls_my_issuer_unit",    tlsops_comp     , CERT_LOCAL | CERT_ISSUER  | COMP_OU },
	/* subject alternative name parameters for peer and local */	
	{"tls_peer_san_email",    tlsops_alt, CERT_PEER  | COMP_E },
	{"tls_my_san_email",      tlsops_alt, CERT_LOCAL | COMP_E },
	{"tls_peer_san_hostname", tlsops_alt, CERT_PEER  | COMP_HOST },
	{"tls_my_san_hostname",   tlsops_alt, CERT_LOCAL | COMP_HOST },
	{"tls_peer_san_uri",      tlsops_alt, CERT_PEER  | COMP_URI },
	{"tls_my_san_uri",        tlsops_alt, CERT_LOCAL | COMP_URI },
	{"tls_peer_san_ip",       tlsops_alt, CERT_PEER  | COMP_IP },
	{"tls_my_san_ip",         tlsops_alt, CERT_LOCAL | COMP_IP },
	/* peer certificate validation parameters */		
	{"tls_peer_verified",   tlsops_check_cert, CERT_VERIFIED },
	{"tls_peer_revoked",    tlsops_check_cert, CERT_REVOKED },
	{"tls_peer_expired",    tlsops_check_cert, CERT_EXPIRED },
	{"tls_peer_selfsigned", tlsops_check_cert, CERT_SELFSIGNED },
	{"tls_peer_notBefore", tlsops_validity, CERT_NOTBEFORE },
	{"tls_peer_notAfter",  tlsops_validity, CERT_NOTAFTER },

	{0,0,0}
}; 

/*
 * Module interface
 */
struct module_exports exports = {
	"tls", 
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	0,           /* exported statistics */
	0,           /* exported MI functions */
	mod_init,    /* module initialization function */
	0,           /* response function */
	mod_destroy, /* destroy function */
	0            /* child initialization function */
};

static int mod_init(void)
{
	pseudo_export_t *pseudo;
	DBG("%s module - initializing...\n", exports.name);
	
	pseudo = &pseudos[0];
	while (pseudo->name) {
		xl_param_t param;
		param.ind = pseudo->flags;
		if(xl_add_extra(pseudo->name, pseudo->function, 0, &param)!=0) {
			LOG(L_ERR,"ERROR:tlsops:mod_init: failed to register pvar '%s'\n",
					pseudo->name);
			return -1;
		}
		pseudo++;
	}
	
	DBG("%s module - initializing...done\n", exports.name);
	return 0;
}


static void mod_destroy(void)
{
	DBG("%s module - shutting down...\n", exports.name);
	DBG("%s module - shutting down...done\n", exports.name);
}


static int is_peer_verified(struct sip_msg* msg, char* foo, char* foo2)
{
	struct tcp_connection *c;
	SSL *ssl;
	long ssl_verify;
	X509 *x509_cert;

	LOG(L_DBG, "tlsops:is_peer_verified: is_peer_verified() started...\n");
	if (msg->rcv.proto != PROTO_TLS) {
		LOG(L_ERR, "tlsops:is_peer_verified: ERROR: proto != TLS -->"
			" peer can't be verified, return -1\n");
		return -1;
	}

	LOG(L_DBG, "tlsops:is_peer_verified: trying to find TCP connection "
		"of received message...\n");
	/* what if we have multiple connections to the same remote socket? e.g. we can have 
	     connection 1: localIP1:localPort1 <--> remoteIP:remotePort
	     connection 2: localIP2:localPort2 <--> remoteIP:remotePort
	   but I think the is very unrealistic */
	c=tcpconn_get(0, &(msg->rcv.src_ip), msg->rcv.src_port, tcp_con_lifetime);
	if (!c) {
		LOG(L_ERR, "tlsops:is_peer_verified: ERROR: no corresponding TLS/TCP "
			"connection found. This should not happen... return -1\n");
		return -1;
	}
	LOG(L_DBG, "tlsops:is_peer_verified: corresponding TLS/TCP connection "
		"found. s=%d, fd=%d, id=%d\n", c->s, c->fd, c->id);

	if (!c->extra_data) {
		LOG(L_ERR, "tlsops:is_peer_verified: ERROR: no extra_data specified "
			"in TLS/TCP connection found. This should not happen... "
			"return -1\n");
		tcpconn_put(c);
		return -1;
	}

	ssl = (SSL *) c->extra_data;		

	ssl_verify = SSL_get_verify_result(ssl);
	if ( ssl_verify != X509_V_OK ) {
		LOG(L_WARN, "tlsops:is_peer_verified: WARNING: verification of "
				"presented certificate failed... return -1\n");
		tcpconn_put(c);
		return -1;
	}
	
	/* now, we have only valid peer certificates or peers without certificates.
	 * Thus we have to check for the existence of a peer certificate
	 */
	x509_cert = SSL_get_peer_certificate(ssl);
	if ( x509_cert == NULL ) {
		LOG(L_WARN, "tlsops:is_peer_verified: WARNING: peer did not presented "
			"a certificate. Thus it could not be verified... return -1\n");
		tcpconn_put(c);
		return -1;
	}
	
	X509_free(x509_cert);
	
	tcpconn_put(c);
	
	LOG(L_DBG, "tlsops:is_peer_verified: peer is successfuly verified"
		"...done\n");
	return 1;
}
