/*
 * $Id$
 *
 * TLS module - main server part
 * 
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
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
/*
 * History:
 * --------
 *  2007-01-26  openssl kerberos malloc bug detection/workaround (andrei)
 *  2007-02-23  openssl low memory bugs workaround (andrei)
 */

#include <sys/poll.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "../../dprint.h"
#include "../../ip_addr.h"
#include "../../mem/shm_mem.h"
#include "../../pt.h"
#include "../../timer.h"
#include "../../globals.h"
#include "../../pt.h"

#include "tls_init.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_server.h"

/* low memory treshold for openssl bug #1491 workaround */
#define LOW_MEM_NEW_CONNECTION_TEST() \
	((openssl_mem_threshold1) && (shm_available()<openssl_mem_threshold1))
#define LOW_MEM_CONNECTED_TEST() \
	((openssl_mem_threshold2) && (shm_available()<openssl_mem_threshold2))

/* 
 * finish the ssl init (creates the SSL and set extra_data to it)
 * separated from tls_tcpconn_init to allow delayed ssl context
 * init. (from the "child" process and not from the main one 
 */
static int tls_complete_init(struct tcp_connection* c)
{
	tls_domain_t* dom;
	struct tls_extra_data* data = 0;
	tls_cfg_t* cfg;

	if (LOW_MEM_NEW_CONNECTION_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		goto error2;
	}
	     /* Get current TLS configuration and increate reference
	      * count immediately. There is no need to lock the structure
	      * here, because it does not get deleted immediately. When
	      * SER reloads TLS configuration it will put the old configuration
	      * on a garbage queue and delete it later, so we know here that
	      * the pointer we get from *tls_cfg will be valid for a while, at
	      * least by the time this function finishes
	      */
	cfg = *tls_cfg;

	     /* Increment the reference count in the configuration structure, this
	      * is to ensure that, while on the garbage queue, the configuration does
	      * not get deleted if there are still connection referencing its SSL_CTX
	      */
	cfg->ref_count++;

	if (c->state == S_CONN_ACCEPT) {
		dom = tls_lookup_cfg(cfg, TLS_DOMAIN_SRV, &c->rcv.dst_ip, c->rcv.dst_port);
	} else if (c->state == S_CONN_CONNECT) {
		dom = tls_lookup_cfg(cfg, TLS_DOMAIN_CLI, &c->rcv.dst_ip, c->rcv.dst_port);
	} else {
		BUG("Invalid connection state (bug in TCP code)\n");
		goto error;
	}
	DBG("Using TLS domain %s\n", tls_domain_str(dom));

	data = (struct tls_extra_data*)shm_malloc(sizeof(struct tls_extra_data));
	if (!data) {
		ERR("Not enough shared memory left\n");
		goto error;
	}
	memset(data, '\0', sizeof(struct tls_extra_data));
	data->ssl = SSL_new(dom->ctx[process_no]);
	data->cfg = cfg;

	if (data->ssl == 0) {
		TLS_ERR("Failed to create SSL structure:");
		goto error;
	}
#ifdef TLS_KSSL_WORKARROUND
	 /* if needed apply workaround for openssl bug #1467 */
	if (data->ssl->kssl_ctx && openssl_kssl_malloc_bug){
		kssl_ctx_free(data->ssl->kssl_ctx);
		data->ssl->kssl_ctx=0;
	}
#endif
	c->extra_data = data;
	return 0;

 error:
	cfg->ref_count--;
	if (data) shm_free(data);
 error2:
	c->state = S_CONN_BAD;
	return -1;
}


/*
 * Update ssl structure with new fd 
 */
static int tls_update_fd(struct tcp_connection *c, int fd)
{
	SSL *ssl;
	BIO *rbio;
	BIO *wbio;
	
	if (!c->extra_data && tls_complete_init(c) < 0) {
		ERR("Delayed init failed\n");
		return -1;
	}else if (LOW_MEM_CONNECTED_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		return -1;
	}
	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;
	
	if (((rbio=SSL_get_rbio(ssl))==0) || ((wbio=SSL_get_wbio(ssl))==0)){
		/* no BIO connected */
		if (SSL_set_fd(ssl, fd) != 1) {
			TLS_ERR("tls_update_fd:");
			return -1;
		}
		return 0;
	}
	if ((BIO_set_fd(rbio, fd, BIO_NOCLOSE)!=1) ||
		(BIO_set_fd(wbio, fd, BIO_NOCLOSE)!=1)) {
		/* it should be always 1 */
		TLS_ERR("tls_update_fd:");
		return -1;
	}
	return 0;
}


static void tls_dump_cert_info(char* s, X509* cert)
{
	char* subj;
	char* issuer;
	
	subj=issuer=0;
	subj = X509_NAME_oneline(X509_get_subject_name(cert), 0 , 0);
	issuer = X509_NAME_oneline(X509_get_issuer_name(cert), 0 , 0);
	
	if (subj){
		LOG(tls_log, "%s subject:%s\n", s ? s : "", subj);
		OPENSSL_free(subj);
	}
	if (issuer){
		LOG(tls_log, "%s issuer:%s\n", s ? s : "", issuer);
		OPENSSL_free(issuer);
	}
}


static void tls_dump_verification_failure(long verification_result)
{
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


/*
 * Wrapper around SSL_accept, returns -1 on error, 0 on success 
 */
static int tls_accept(struct tcp_connection *c, int* error)
{
	int ret, err, ssl_err;
	SSL *ssl;
	X509* cert;

	if (c->state != S_CONN_ACCEPT) {
		BUG("Invalid connection state (bug in TLS code)\n");
		     /* Not critical */
		return 0;
	}
	if (LOW_MEM_NEW_CONNECTION_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		goto err;
	}

	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;
	ret = SSL_accept(ssl);
	if (ret == 1) {
		DBG("TLS accept successful\n");
		c->state = S_CONN_OK;
		LOG(tls_log, "tls_accept: new connection from %s:%d using %s %s %d\n", 
		    ip_addr2a(&c->rcv.src_ip), c->rcv.src_port,
		    SSL_get_cipher_version(ssl), SSL_get_cipher_name(ssl), 
		    SSL_get_cipher_bits(ssl, 0)
		    );
		LOG(tls_log, "tls_accept: local socket: %s:%d\n", 
		    ip_addr2a(&c->rcv.dst_ip), c->rcv.dst_port
		    );
		cert = SSL_get_peer_certificate(ssl);
		if (cert != 0) { 
			tls_dump_cert_info("tls_accept: client certificate", cert);
			if (SSL_get_verify_result(ssl) != X509_V_OK) {
				LOG(tls_log, "WARNING: tls_accept: client certificate "
				    "verification failed!!!\n");
				tls_dump_verification_failure(SSL_get_verify_result(ssl));
			}
			X509_free(cert);
		} else {
			LOG(tls_log, "tls_accept: client did not present a certificate\n");
		}
	} else {
		err = SSL_get_error(ssl, ret);
		if (error) *error = err;
		switch (err) {
		case SSL_ERROR_ZERO_RETURN:
			DBG("TLS handshake failed cleanly\n");
			goto err;
			
		case SSL_ERROR_WANT_READ:
			DBG("Need to get more data to finish TLS accept\n");
			break;
			
		case SSL_ERROR_WANT_WRITE:
			DBG("Need to send more data to finish TLS accept\n");
			break;
			
#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*0.9.7*/
		case SSL_ERROR_WANT_CONNECT:
			DBG("Need to retry connect\n");
			break;
			
		case SSL_ERROR_WANT_ACCEPT:
			DBG("Need to retry accept\n");
			break;
#endif
		case SSL_ERROR_WANT_X509_LOOKUP:
			DBG("Application callback asked to be called again\n");
			break;
			
		case SSL_ERROR_SYSCALL:
			TLS_ERR_RET(ssl_err, "TLS accept:");
			if (!ssl_err) {
				if (ret == 0) {
					WARN("Unexpected EOF occurred while performing TLS accept\n");
				} else {
					ERR("IO error: (%d) %s\n", errno, strerror(errno));
				}
			}
			goto err;
			
		default:
			TLS_ERR("SSL error:");
			goto err;
		}
	}
	return 0;
 err:
	c->state = S_CONN_BAD;
	return -1;
}


/*
 * wrapper around SSL_connect, returns 0 on success, -1 on error 
 */
static int tls_connect(struct tcp_connection *c, int* error)
{
	SSL *ssl;
	int ret, err, ssl_err;
	X509* cert;

	if (c->state != S_CONN_CONNECT) {
		BUG("Invalid connection state\n");
		     /* Not critical */
		return 0;
	}
	if (LOW_MEM_NEW_CONNECTION_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		goto err;
	}

	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;
	ret = SSL_connect(ssl);
	if (ret == 1) {
		DBG("TLS connect successuful\n");
		c->state = S_CONN_OK;
		LOG(tls_log, "tls_connect: new connection to %s:%d using %s %s %d\n", 
		    ip_addr2a(&c->rcv.src_ip), c->rcv.src_port,
		    SSL_get_cipher_version(ssl), SSL_get_cipher_name(ssl),
		    SSL_get_cipher_bits(ssl, 0)
		    );
		LOG(tls_log, "tls_connect: sending socket: %s:%d \n", 
		    ip_addr2a(&c->rcv.dst_ip), c->rcv.dst_port
		    );
		cert = SSL_get_peer_certificate(ssl);
		if (cert != 0) { 
			tls_dump_cert_info("tls_connect: server certificate", cert);
			if (SSL_get_verify_result(ssl) != X509_V_OK) {
				LOG(tls_log, "WARNING: tls_connect: server certificate "
				    "verification failed!!!\n");
				tls_dump_verification_failure(SSL_get_verify_result(ssl));
			}
			X509_free(cert);
		} else {
			     /* this should not happen, servers always present a cert */
			LOG(tls_log, "tls_connect: server did not present a certificate\n");
		}
	} else {
		err = SSL_get_error(ssl, ret);
		if (error) *error = err;
		switch (err) {
		case SSL_ERROR_ZERO_RETURN:
			DBG("TLS handshake failed cleanly\n");
			goto err;
			
		case SSL_ERROR_WANT_READ:
			DBG("Need to get more data to finish TLS connect\n");
			break;
			
		case SSL_ERROR_WANT_WRITE:
			DBG("Need to send more data to finish TLS connect\n");
			break;
			
#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*0.9.7*/
		case SSL_ERROR_WANT_CONNECT:
			DBG("Need to retry connect\n");
			break;
			
		case SSL_ERROR_WANT_ACCEPT:
			DBG("Need to retry accept\n");
			break;
#endif
		case SSL_ERROR_WANT_X509_LOOKUP:
			DBG("Application callback asked to be called again\n");
			break;
			
		case SSL_ERROR_SYSCALL:
			TLS_ERR_RET(ssl_err, "TLS connect:");
			if (!ssl_err) {
				if (ret == 0) {
					WARN("Unexpected EOF occurred while performing TLS connect\n");
				} else {
					ERR("IO error: (%d) %s\n", errno, strerror(errno));
				}
			}
			goto err;
			
		default:
			TLS_ERR("SSL error:");
			goto err;
		}
	}
	return 0;
 err:
	c->state = S_CONN_BAD;
	return -1;
}


/*
 * wrapper around SSL_shutdown, returns -1 on error, 0 on success 
 */
static int tls_shutdown(struct tcp_connection *c)
{
	int ret, err, ssl_err;
	SSL *ssl;

	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;
	if (ssl == 0) {
		ERR("No SSL data to perform tls_shutdown\n");
		return -1;
	}
	if (LOW_MEM_CONNECTED_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		goto err;
	}
	
	ret = SSL_shutdown(ssl);
	if (ret == 1) {
		DBG("TLS shutdown successful\n");
		return 0;
	} else if (ret == 0) {
		DBG("First phase of 2-way handshake completed succesfuly\n");
		return 0;
	} else {
		err = SSL_get_error(ssl, ret);
		switch (err) {
		case SSL_ERROR_ZERO_RETURN:
			DBG("TLS shutdown failed cleanly\n");
			goto err;
			
		case SSL_ERROR_WANT_READ:
			DBG("Need to get more data to finish TLS shutdown\n");
			break;
			
		case SSL_ERROR_WANT_WRITE:
			DBG("Need to send more data to finish TLS shutdown\n");
			break;
			
#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*0.9.7*/
		case SSL_ERROR_WANT_CONNECT:
			DBG("Need to retry connect\n");
			break;
			
		case SSL_ERROR_WANT_ACCEPT:
			DBG("Need to retry accept\n");
			break;
#endif
		case SSL_ERROR_WANT_X509_LOOKUP:
			DBG("Application callback asked to be called again\n");
			break;
			
		case SSL_ERROR_SYSCALL:
			TLS_ERR_RET(ssl_err, "TLS shutdown");
			if (!ssl_err) {
				if (ret == 0) {
					WARN("Unexpected EOF occurred while performing TLS shutdown\n");
				} else {
					ERR("IO error: (%d) %s\n", errno, strerror(errno));
				}
			}
			goto err;
			
		default:
			TLS_ERR("SSL error:");
			goto err;
		}
	}
	
	return 0;
 err:
	return -1;
}


/* "normal", return number of bytes written,  -1 on error/EOF & sets error
 * & c->state on EOF; 0 on want READ/WRITE
 * 
 * expects a set fd */
static int tls_write(struct tcp_connection *c, const void *buf, size_t len, int* error)
{
	int ret, err, ssl_err;
	SSL *ssl;
	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;

	err = 0;
	if (LOW_MEM_CONNECTED_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		ret=-1;
		goto err;
	}
	ret = SSL_write(ssl, buf, len);
	if (ret <= 0) {
		err = SSL_get_error(ssl, ret);
		switch (err) {
		case SSL_ERROR_ZERO_RETURN:
			DBG("TLS connection has been closed\n");
			c->state = S_CONN_EOF;
			ret = -1;
			break;
			
		case SSL_ERROR_WANT_READ:
			DBG("Need to get more data to finish TLS write\n");
			ret = 0;
			break;

		case SSL_ERROR_WANT_WRITE:
			DBG("Need to send more data to finish TLS write\n");
			ret = 0;
			break;
			
#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*0.9.7*/
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			DBG("TLS not connected\n");
			ret = -1;
			break;
#endif
		case SSL_ERROR_WANT_X509_LOOKUP:
			DBG("Application callback asked to be called again\n");
			ret = 0;
			break;
			
		case SSL_ERROR_SYSCALL:
			TLS_ERR_RET(ssl_err, "tls_write:");
			if (!ssl_err) {
				if (ret == 0) {
					WARN("Unexpected EOF occurred while performing TLS shutdown\n");
					c->state = S_CONN_EOF;
					ret = -1;
				} else {
					ERR("IO error: (%d) %s\n", errno, strerror(errno));
				}
			}
			break;
			
		default:
			TLS_ERR("SSL error:");
			break;
		}
	}

err:
	if (error) *error = err;
	return ret;
}


/*
 * Called when new tcp connection is accepted or connected, create ssl
 * data structures here, there is no need to acquire any lock, because the 
 * connection is being created by a new process and on other process has
 * access to it yet, this is called before adding the tcp_connection
 * structure into the hash 
 */
int tls_h_tcpconn_init(struct tcp_connection *c, int sock)
{
	c->type = PROTO_TLS;
	c->rcv.proto = PROTO_TLS;
	c->timeout = get_ticks_raw() + tls_con_lifetime;
	c->extra_data = 0;
	return 0;
}


/*
 * clean the extra data upon connection shut down 
 */
void tls_h_tcpconn_clean(struct tcp_connection *c)
{
	struct tls_extra_data* extra;
	/*
	* runs within global tcp lock 
	*/
	if (c->type != PROTO_TLS) {
		BUG("Bad connection structure\n");
		abort();
	}
	if (c->extra_data) {
		extra = (struct tls_extra_data*)c->extra_data;
		SSL_free(extra->ssl);
		extra->cfg->ref_count--;
		shm_free(c->extra_data);
		c->extra_data = 0;
	}
}


/*
 * perform one-way shutdown, do not wait for notify from the remote peer 
 */
void tls_h_close(struct tcp_connection *c, int fd)
{
	     /*
	      * runs within global tcp lock 
	      */
	DBG("Closing SSL connection\n");
	if (c->extra_data) {
		if (tls_update_fd(c, fd)==0)
			tls_shutdown(c); /* shudown only on succesfull set fd */
	}
}



/*
 * This is shamelessly stolen tsend_stream from tsend.c 
 */
/*
 * fixme: probably does not work correctly 
 */
int tls_h_blocking_write(struct tcp_connection *c, int fd, const char *buf,
			  unsigned int len)
{
	int err, n, ticks, tout;
	fd_set sel_set;
	struct timeval timeout;
	
	n = 0;
	if (tls_update_fd(c, fd) < 0) goto error;
again:
	err = 0;
	     /* first try  a "fast" write -- avoid the extra select call,
	      * we might get lucky and not need it */
	if (c->state == S_CONN_CONNECT) {
		if (tls_connect(c, &err) < 0) goto error;
		tout = tls_handshake_timeout;
	} else if (c->state == S_CONN_ACCEPT) {
		if (tls_accept(c, &err) < 0) goto error;
		tout = tls_handshake_timeout;
	} else {
		n = tls_write(c, buf, len, &err);
		if (n < 0) {
			DBG("tls_write error %d (ssl %d)\n", n, err);
			goto error;
		} else if (n < len) {
			     /* not all the contents was written => try again w/ the rest
			      * (possible when SSL_MODE_ENABLE_PARTIAL_WRITE is set)
			      */
			DBG("%ld bytes still need to be written\n", 
				(long)(len - n));
			buf += n; 
			len -= n;
		} else {
			     /* succesfull write */
			DBG("write finished, %d bytes written\n", n);
			goto end;
		}
		tout = tls_send_timeout;
	}

	while(1) {
		FD_ZERO(&sel_set);
		FD_SET(fd, &sel_set);
		timeout.tv_sec = tout;
		timeout.tv_usec = 0;
		ticks = get_ticks();

		     /* blocking part, wait until we can write again on the fd */
		switch(err){
			case 0:
			case SSL_ERROR_WANT_WRITE:
				n = select(fd + 1, 0, &sel_set ,0 , &timeout);
				break;

			case SSL_ERROR_WANT_READ:
				n = select(fd + 1, &sel_set, 0 ,0 , &timeout);
				break;

#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*0.9.7*/
			case SSL_ERROR_WANT_ACCEPT:
#endif
			case SSL_ERROR_WANT_CONNECT:
				DBG("re-trying accept/connect\n");
				goto again;

			default:
				BUG("Unhandled SSL error %d\n", err);
				goto error;
		}
		if (n < 0) {
			if (errno == EINTR) continue;/* just a signal */
			ERR("Select failed:"
			    " (%d) %s\n", errno, strerror(errno));
			goto error;
		}
		if (n == 0) {
			     /* timeout, make sure the interval really expired */
			if ((get_ticks() - ticks) >= tout) {
				ERR("Peer not "
				    " responding after %d s=> timeout (state %d) \n",
				    tout, c->state);
				goto error;
			}
		}
		if (FD_ISSET(fd, &sel_set)) {
			     /* we can write again */
			DBG("Ready to read/write again\n");
			goto again;
		}
	}
	
 error:
	return -1;
 end:
	return n;
}


/*
 * called only when a connection is in S_CONN_OK, we do not have to care
 * about accepting or connecting here, each modification of ssl data
 * structures has to be protected, another process might ask for the same
 * connection and attempt write to it which would result in updating the
 * ssl structures 
 */
int tls_h_read(struct tcp_connection * c)
{
	struct tcp_req* r;
	int bytes_free, bytes_read, err, ssl_err;
	SSL* ssl;

	r = &c->req;
	bytes_free = TCP_BUF_SIZE - (int)(r->pos - r->buf);
	
	if (bytes_free == 0) {
		ERR("Buffer overrun, dropping\n");
		r->error = TCP_REQ_OVERRUN;
		return -1;
	}
	if (LOW_MEM_CONNECTED_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		return -1;
	}
	     /* we have to avoid to run in the same time 
	      * with a tls_write because of the 
	      * update_fd stuff  (we don't want a write
	      * stealing the fd under us or vice versa)
	      * => lock on con->write_lock (ugly hack) */
	lock_get(&c->write_lock);
	if (tls_update_fd(c, c->fd) != 0) {
		     /* error */
		lock_release(&c->write_lock);
		return -1;
	}
	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;
	bytes_read = SSL_read(ssl, r->pos, bytes_free);
	lock_release(&c->write_lock);
	
	if (bytes_read <= 0) {
		err = SSL_get_error(ssl, bytes_read);
		switch(err){
		case SSL_ERROR_ZERO_RETURN:
			     /* tls connection has been closed */
			DBG("tls_read: eof\n");
			c->state = S_CONN_EOF;
			return 0;

		case SSL_ERROR_WANT_READ:
			DBG("tls_read: Need to read more data\n");
			return 0;
			
		case SSL_ERROR_WANT_WRITE:
			     /* retry later */
			DBG("tls_read: Need to write more data\n");
			return 0;

		case SSL_ERROR_SYSCALL:
			TLS_ERR_RET(ssl_err, "tls_read:");
			if (!ssl_err) {
				if (bytes_read == 0) {
					LOG(tls_log, "WARNING: tls_read: improper EOF on tls"
					    " (harmless)\n");
					c->state = S_CONN_EOF;
					return 0;
				} else {
					ERR("Error reading: syscall"
					    " (%d) %s\n", errno, strerror(errno));
				}
			} 
			     /* error return */
			r->error = TCP_READ_ERROR;
			return -1;
		default:
			TLS_ERR("tls_read:");
			r->error = TCP_READ_ERROR;
			return -1;
		}
	}

	r->pos += bytes_read;
	return bytes_read;
}


/*
 * called before tls_read, the this function should attempt tls_accept or
 * tls_connect depending on the state of the connection, if this function
 * does not transit a connection into S_CONN_OK then tcp layer would not
 * call tcp_read 
 */
int tls_h_fix_read_conn(struct tcp_connection *c)
{
	int ret;
	ret = 0;

	switch (c->state) {
	case S_CONN_ACCEPT:
		lock_get(&c->write_lock);
		     /* It might have changed meanwhile */
		if (c->state == S_CONN_ACCEPT) {
			ret = tls_update_fd(c, c->fd);
			if (ret == 0) ret = tls_accept(c, 0);
			else ret = -1;
		}
		lock_release(&c->write_lock);
		break;
		
	case S_CONN_CONNECT:
		lock_get(&c->write_lock);
		     /* It might have changed meanwhile */
		if (c->state == S_CONN_CONNECT) {
			ret = tls_update_fd(c, c->fd);
			if (ret == 0) ret = tls_connect(c, 0);
			else ret = -1;
		}
		lock_release(&c->write_lock);
		break;
		
	default: /* fall through */
		break;
	}
	return ret;
}
