/*
 * $Id$
 *
 * TLS module - main server part
 * 
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2005-2010 iptelorg GmbH
 *
 * This file is part of SIP-router, a free SIP server.
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
/*
 * History:
 * --------
 *  2007-01-26  openssl kerberos malloc bug detection/workaround (andrei)
 *  2007-02-23  openssl low memory bugs workaround (andrei)
 *  2009-09-21  tls connection state is now kept in c->extra_data (no
 *               longer shared with tcp state) (andrei)
 */
/** main tls part (implements the tls hooks that are called from the tcp code).
 * @file tls_server.c
 * @ingroup tls
 * Module: @ref tls
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
#include "../../tcp_int_send.h"
#include "../../tcp_read.h"

#include "tls_init.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_server.h"
#include "tls_bio.h"
#include "tls_dump_vf.h"

/* low memory treshold for openssl bug #1491 workaround */
#define LOW_MEM_NEW_CONNECTION_TEST() \
	((openssl_mem_threshold1) && (shm_available()<openssl_mem_threshold1))
#define LOW_MEM_CONNECTED_TEST() \
	((openssl_mem_threshold2) && (shm_available()<openssl_mem_threshold2))

#define TLS_RD_MBUF_SZ	65536
#define TLS_WR_MBUF_SZ	65536

/** finish the ssl init.
 * Creates the SSL and set extra_data to it.
 * Separated from tls_tcpconn_init to allow delayed ssl context
 * init. (from the "child" process and not from the main one 
 * WARNING: the connection should be already locked.
 * @return 0 on success, -1 on errror.
 */
static int tls_complete_init(struct tcp_connection* c)
{
	tls_domain_t* dom;
	struct tls_extra_data* data = 0;
	tls_cfg_t* cfg;
	enum tls_conn_states state;

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

	if (c->flags & F_CONN_PASSIVE) {
		state=S_TLS_ACCEPTING;
		dom = tls_lookup_cfg(cfg, TLS_DOMAIN_SRV,
								&c->rcv.dst_ip, c->rcv.dst_port);
	} else {
		state=S_TLS_CONNECTING;
		dom = tls_lookup_cfg(cfg, TLS_DOMAIN_CLI,
								&c->rcv.dst_ip, c->rcv.dst_port);
	}
	if (unlikely(c->state<0)) {
		BUG("Invalid connection (state %d)\n", c->state);
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
	data->state = state;

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
	return -1;
}



/** completes tls init if needed and checks if tls can be used.
 *  It will check for low memory.
 *  WARNING: must be called with c->write_lock held.
 *  @return 0 on success, < 0 on error (complete init failed or out of memory).
 */
static int tls_fix_connection(struct tcp_connection* c)
{
	if (unlikely(!c->extra_data)) {
		if (unlikely(tls_complete_init(c) < 0)) {
			ERR("Delayed init failed\n");
			return -1;
		}
	}else if (unlikely(LOW_MEM_CONNECTED_TEST())){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		return -1;
	}
	return 0;
}



/** sets an mbuf pair for the bio used by the tls connection.
 * WARNING: must be called with c->write_lock held.
 * @return 0 on success, -1 on error.
 */
static int tls_set_mbufs(struct tcp_connection *c,
							struct tls_mbuf* rd,
							struct tls_mbuf* wr)
{
	SSL *ssl;
	BIO *rwbio;
	
	/* if (unlikely(tls_fix_connection(c) < 0))
		return -1;
	*/
	
	ssl = ((struct tls_extra_data*)c->extra_data)->ssl;
	if (unlikely(((rwbio=SSL_get_rbio(ssl))==0) ||
					((rwbio=SSL_get_wbio(ssl))==0))) {
		rwbio = tls_BIO_new_mbuf(rd, wr);
		if (unlikely(rwbio == 0)) {
			ERR("new mbuf BIO creation failure\n");
			return -1;
		}
		/* use the same bio for both read & write */
		SSL_set_bio(ssl, rwbio, rwbio);
		return 0;
	}
	if (unlikely(tls_BIO_mbuf_set(rwbio, rd, wr)<=0)) {
		/* it should be always 1 */
		ERR("failed to set mbufs");
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



/** wrapper around SSL_accept, usin SSL return convention.
 * It will also log critical errors and certificate debugging info.
 * @param c - tcp connection with tls (extra_data must be a filled
 *            tcp_extra_data structure). The state must be S_TLS_ACCEPTING.
 * @param error  set to the error reason (SSL_ERROR_*).
 * @return >=1 on success, 0 and <0 on error. 0 means the underlying SSL
 *           connection was closed/shutdown. < 0 is also returned for
 *           WANT_READ or WANT_WRITE 
 *
 */
static int tls_accept(struct tcp_connection *c, int* error)
{
	int ret, ssl_err;
	SSL *ssl;
	X509* cert;
	struct tls_extra_data* tls_c;

	if (LOW_MEM_NEW_CONNECTION_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		goto err;
	}
	
	tls_c=(struct tls_extra_data*)c->extra_data;
	ssl=tls_c->ssl;
	
	if (unlikely(tls_c->state != S_TLS_ACCEPTING)) {
		BUG("Invalid connection state %d (bug in TLS code)\n", tls_c->state);
		/* Not critical */
		goto err;
	}
	ret = SSL_accept(ssl);
	if (unlikely(ret == 1)) {
		DBG("TLS accept successful\n");
		tls_c->state = S_TLS_ESTABLISHED;
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
	} else { /* ret == 0 or < 0 */
		*error = SSL_get_error(ssl, ret);
		switch (*error) {
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
					WARN("Unexpected EOF occurred while performing"
							" TLS accept\n");
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
	return ret;
err:
	return -1;
}


/** wrapper around SSL_connect, usin SSL return convention.
 * It will also log critical errors and certificate debugging info.
 * @param c - tcp connection with tls (extra_data must be a filled
 *            tcp_extra_data structure). The state must be S_TLS_CONNECTING.
 * @param error  set to the error reason (SSL_ERROR_*).
 * @return >=1 on success, 0 and <0 on error. 0 means the underlying SSL
 *           connection was closed/shutdown. < 0 is also returned for
 *           WANT_READ or WANT_WRITE 
 *
 */
static int tls_connect(struct tcp_connection *c, int* error)
{
	SSL *ssl;
	int ret, ssl_err;
	X509* cert;
	struct tls_extra_data* tls_c;

	if (LOW_MEM_NEW_CONNECTION_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		goto err;
	}

	tls_c=(struct tls_extra_data*)c->extra_data;
	ssl=tls_c->ssl;
	
	*error = SSL_ERROR_NONE;
	if (unlikely(tls_c->state != S_TLS_CONNECTING)) {
		BUG("Invalid connection state %d (bug in TLS code)\n", tls_c->state);
		/* Not critical */
		goto err;
	}
	ret = SSL_connect(ssl);
	if (unlikely(ret == 1)) {
		DBG("TLS connect successful\n");
		tls_c->state = S_TLS_ESTABLISHED;
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
			LOG(tls_log, "tls_connect: server did not "
							"present a certificate\n");
		}
	} else { /* 0 or < 0 */
		*error = SSL_get_error(ssl, ret);
		switch (*error) {
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
	return ret;
err:
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



/*
 * Called when new tcp connection is accepted or connected. It creates the ssl
 * data structures. There is no need to acquire any lock, because when the
 * connection is being created no other process has access to it yet
 * (this is called before adding the tcp_connection structure into the hash) 
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
		if (extra->ct_wq)
			tls_ct_wq_free(&extra->ct_wq);
		if (extra->enc_rd_buf) {
			shm_free(extra->enc_rd_buf);
			extra->enc_rd_buf = 0;
		}
		shm_free(c->extra_data);
		c->extra_data = 0;
	}
}


/*
 * perform one-way shutdown, do not wait for notify from the remote peer 
 */
void tls_h_close(struct tcp_connection *c, int fd)
{
	unsigned char rd_buf[TLS_RD_MBUF_SZ];
	unsigned char wr_buf[TLS_WR_MBUF_SZ];
	struct tls_mbuf rd, wr;
	
	/*
	 * runs either within global tcp lock or after the connection has 
	 * been "detached" and is unreachable from any other process.
	 * Unfortunately when called via
	 * tcpconn_put_destroy()+tcpconn_close_main_fd() the connection might
	 * still be in a writer, so in this case locking is needed.
	 */
	DBG("Closing SSL connection %p\n", c->extra_data);
	if (likely(c->extra_data)) {
		lock_get(&c->write_lock);
			if (unlikely(c->extra_data == 0)) {
				/* changed in the meanwhile */
				lock_release(&c->write_lock);
				return;
			}
			tls_mbuf_init(&rd, rd_buf, sizeof(rd_buf));
			tls_mbuf_init(&wr, wr_buf, sizeof(wr_buf));
			if (tls_set_mbufs(c, &rd, &wr)==0) {
				tls_shutdown(c); /* shudown only on succesfull set fd */
				/* write as much as possible and update wr.
				 * Since this is a close, we don't want to queue the write
				 * (if it can't write immediately, just fail silently)
				 */
				if (wr.used)
					_tcpconn_write_nb(fd, c, (char*)wr.buf, wr.used);
				/* we don't bother reading anything (we don't want to wait
				on close) */
			}
		lock_release(&c->write_lock);
	}
}



/* generic tcpconn_{do,1st}_send() function pointer type */
typedef int (*tcp_low_level_send_t)(int fd, struct tcp_connection *c,
									char* buf, unsigned len,
									snd_flags_t send_flags,
									long* resp, int locked);



/** tls generic send function.
 * It is used by tls_do_send_f and tls_1st_send_f (which are wrappers
 * arround it).
 * WARNING: it must be called with c->write_lock held!
 * @param c - tcp connection
 * @param fd - valid file descriptor for the tcp connection
 * @param buf - data
 * @param len - data size
 * @param send_flags
 * @param resp - filled with a cmd. for tcp_main (@see tcpconn_do_send() for
 *               more details)
 * 
 * @return >=0 on success, < 0 on error && * resp == CON_ERROR.
 */
static int tls_generic_send(int fd, struct tcp_connection *c,
						const char *buf, unsigned int len,
						snd_flags_t send_flags, long* resp,
						tcp_low_level_send_t tcp_do_send_f)
{
	int n, offs;
	SSL* ssl;
	struct tls_extra_data* tls_c;
	unsigned char wr_buf[TLS_WR_MBUF_SZ];
	struct tls_mbuf rd, wr;
	int ssl_error;
	
	*resp = CONN_NOP;
	n = 0;
	offs = 0;
	ssl_error = SSL_ERROR_NONE;
	lock_get(&c->write_lock);
	if (unlikely(tls_fix_connection(c) < 0))
		goto error;
	tls_c = (struct tls_extra_data*)c->extra_data;
	ssl = tls_c->ssl;
	/* clear text already queued (WANTS_READ) queue directly*/
	if (unlikely(tls_write_wants_read(tls_c))) {
		if (unlikely(tls_ct_wq_add(&tls_c->ct_wq, buf+offs, len -offs) < 0))
				goto error_wq_full;
		goto end;
	}
	tls_mbuf_init(&rd, 0, 0); /* no read */
redo_wr:
	tls_mbuf_init(&wr, wr_buf, sizeof(wr_buf));
	if (tls_set_mbufs(c, &rd, &wr) < 0)
		goto error;
	if (unlikely(tls_c->state == S_TLS_CONNECTING)) {
		n = tls_connect(c, &ssl_error);
		if (unlikely(n>=1)) {
			n = SSL_write(ssl, buf + offs, len - offs);
			if (unlikely(n <= 0))
				ssl_error = SSL_get_error(ssl, n);
		}
	} else if (unlikely(tls_c->state == S_TLS_ACCEPTING)) {
		n = tls_accept(c, &ssl_error);
		if (unlikely(n>=1)) {
			n = SSL_write(ssl, buf + offs, len - offs);
			if (unlikely(n <= 0))
				ssl_error = SSL_get_error(ssl, n);
		}
	} else {
		n = SSL_write(ssl, buf + offs, len - offs);
		if (unlikely(n <= 0))
			ssl_error = SSL_get_error(ssl, n);
	}
	if (wr.used ) {
		/* something was written */
		if (unlikely( n < (len -offs)  && n >= 0)) {
			/* if partial tls write, don't force close the tcp connection */
			tcpconn_set_send_flags(c, send_flags); /* set the original flags */
			send_flags.f &= ~SND_F_CON_CLOSE;
		}
		if (unlikely(tcp_do_send_f(fd, c, (char*)wr.buf, wr.used,
											send_flags, resp, 1) < 0)){
			tls_set_mbufs(c, 0, 0);
			goto error_send;
		}
	}
	/* check for possible ssl errors */
	if (unlikely(n <= 0)){
		switch(ssl_error) {
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				/* SSL EOF */
				goto ssl_eof;
			case SSL_ERROR_WANT_READ:
				/* queue write buffer */
				if (unlikely(tls_ct_wq_add(&tls_c->ct_wq, buf+offs, len -offs)
								< 0))
					goto error_wq_full;
				tls_c->flags |= F_TLS_CON_WR_WANTS_RD;
				break; /* or goto end */
			case SSL_ERROR_WANT_WRITE:
				/*  error, no record fits in the buffer */
				BUG("write buffer too small (%d/%d bytes)\n",
						wr.used, wr.size);
				goto bug;
			case SSL_ERROR_SYSCALL:
			default:
				BUG("unexpected SSL error %d\n", ssl_error);
				goto bug;
		}
	} else if (unlikely(n < (len - offs))) {
		/* partial ssl write => retry with the rest */
		offs += n;
		goto redo_wr;
	}
	tls_set_mbufs(c, 0, 0);
end:
	lock_release(&c->write_lock);
	return len;
error:
error_send:
error_wq_full:
bug:
	tls_set_mbufs(c, 0, 0);
	lock_release(&c->write_lock);
	*resp = CONN_ERROR;
	return -1;
ssl_eof:
	c->state = S_CONN_EOF;
	lock_release(&c->write_lock);
	DBG("TLS connection has been closed\n");
	*resp = CONN_EOF;
	return -1;
}



/** tls do_send callback.
 * It is called for all sends (by the tcp send code), except the first send
 * on an async connection (@see tls_1st_send).
 * WARNING: it must be called with c->write_lock held!
 * @param c - tcp connection
 * @param fd - valid file descriptor for the tcp connection
 * @param buf - data
 * @param len - data size
 * @param send_flags
 * @param resp - filled with a cmd. for tcp_main (@see tcpconn_do_send() for
 *               more details)
 * 
 * @return >=0 on success, < 0 on error && * resp == CON_ERROR.
 */
int tls_do_send_f(int fd, struct tcp_connection *c,
						const char *buf, unsigned int len,
						snd_flags_t send_flags, long* resp)
{
	return tls_generic_send(fd, c, buf, len, send_flags, resp,
							tcpconn_do_send);
}



/** tls 1st_send callback.
 * It is called for the first send on an async tcp connection
 * (should be non-blocking).
 * WARNING: it must be called with c->write_lock held!
 * @param c - tcp connection
 * @param fd - valid file descriptor for the tcp connection
 * @param buf - data
 * @param len - data size
 * @param send_flags
 * @param resp - filled with a cmd. for tcp_main (@see tcpconn_1st_send() for
 *               more details)
 * 
 * @return >=0 on success, < 0 on error && * resp == CON_ERROR.
 */
int tls_1st_send_f(int fd, struct tcp_connection *c,
						const char *buf, unsigned int len,
						snd_flags_t send_flags, long* resp)
{
	return tls_generic_send(fd, c, buf, len, send_flags, resp,
							tcpconn_1st_send);
}



/** tls read.
 * Each modification of ssl data structures has to be protected, another process * might ask for the same connection and attempt write to it which would
 * result in updating the ssl structures 
 * WARNING: must be called whic c->write_lock _unlocked_.
 * @param c - tcp connection pointer. The following flags might be set:
 * @param flags - value/result:
 *                     input: RD_CONN_FORCE_EOF  - force EOF after the first
 *                            successful read (bytes_read >=0 )
 *                     output: RD_CONN_SHORT_READ if the read exhausted
 *                              all the bytes in the socket read buffer.
 *                             RD_CONN_EOF if EOF detected (0 bytes read)
 *                              or forced via RD_CONN_FORCE_EOF.
 *                             RD_CONN_RETRY_READ  if this function should
 *                              be called again (e.g. has some data
 *                              buffered internally that didn't fit in
 *                              tcp_req).
 *                     Note: RD_CONN_SHORT_READ & RD_CONN_EOF must be cleared
 *                           before calling this function.
 * used to signal a seen or "forced" EOF on the
 *     connection (when it is known that no more data will come after the
 *     current socket buffer is emptied )=> return/signal EOF on the first
 *     short read (=> don't use it on POLLPRI, as OOB data will cause short
 *      reads even if there are still remaining bytes in the socket buffer)
 * return number of bytes read, 0 on EOF or -1 on error,
 * on EOF it also sets c->state to S_CONN_EOF.
 * sets also r->error.
 * @return bytes decrypted on success, -1 on error (it also sets some
 *         tcp connection flags)
 */
int tls_read_f(struct tcp_connection* c, int* flags)
{
	struct tcp_req* r;
	int bytes_free, bytes_read, read_size, ssl_error, ssl_read;
	SSL* ssl;
	unsigned char rd_buf[TLS_RD_MBUF_SZ];
	unsigned char wr_buf[TLS_WR_MBUF_SZ];
	struct tls_mbuf rd, wr;
	struct tls_extra_data* tls_c;
	struct tls_rd_buf* enc_rd_buf;
	int n, flush_flags;
	
	ssl_read = 0;
	r = &c->req;
	enc_rd_buf = 0;
	*flags &= ~RD_CONN_REPEAT_READ;
	if (unlikely(c->extra_data == 0)) {
		/* not yet fully init => lock & intialize */
		lock_get(&c->write_lock);
			if (tls_fix_connection(c) < 0) {
				lock_release(&c->write_lock);
				return -1;
			}
		lock_release(&c->write_lock);
	} else if (unlikely(LOW_MEM_CONNECTED_TEST())){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
			" operation: %lu\n", shm_available());
		return -1;
	}
	/* here it's safe to use c->extra_data in read-only mode.
	   If it's != 0 is changed only on destroy. It's not possible to have
	   parallel reads.*/
	tls_c = c->extra_data;
redo_read:
	bytes_free = c->req.b_size - (int)(r->pos - r->buf);
	if (unlikely(bytes_free == 0)) {
		ERR("Buffer overrun, dropping\n");
		r->error = TCP_REQ_OVERRUN;
		return -1;
	}
	/* if data queued from a previous read(), use it (don't perform
	 * a real read()).
	*/
	if (unlikely(tls_c->enc_rd_buf)) {
		/* use queued data */
		/* safe to use without locks, because only read changes it and 
		   there can't be parallel reads on the same connection */
		enc_rd_buf = tls_c->enc_rd_buf;
		tls_c->enc_rd_buf = 0;
		tls_mbuf_init(&rd, enc_rd_buf->buf + enc_rd_buf->pos,
						enc_rd_buf->size - enc_rd_buf->pos);
		rd.used = enc_rd_buf->size - enc_rd_buf->pos;
	} else {
		/* if we were using using queued data before, free & reset the
			the queued read data before performing the real read() */
		if (unlikely(enc_rd_buf)) {
			shm_free(enc_rd_buf);
			enc_rd_buf = 0;
		}
		/* real read() */
		tls_mbuf_init(&rd, rd_buf, sizeof(rd_buf));
		/* read() only if no SSL_PENDING (bytes available for immediate
		   read inside the SSL context */
		if (likely(!(tls_c->flags & F_TLS_CON_SSL_PENDING))) {
			/* don't read more then the free bytes in the tcp req buffer */
			read_size = MIN_unsigned(rd.size, bytes_free);
			bytes_read = tcp_read_data(c->fd, c, (char*)rd.buf, read_size,
										flags);
			if (unlikely(bytes_read <= 0)) {
				if (likely(bytes_read == 0))
					goto end;
				else
					goto error;
			}
			rd.used = bytes_read;
		}
	}
	
	tls_mbuf_init(&wr, wr_buf, sizeof(wr_buf));
	ssl_error = SSL_ERROR_NONE;
	
	/* we have to avoid to run in the same time 
	 * with a tls_write because of the
	 * update bio stuff  (we don't want a write
	 * stealing the wbio or rbio under us or vice versa)
	 * => lock on con->write_lock (ugly hack) */
	lock_get(&c->write_lock);
		tls_set_mbufs(c, &rd, &wr);
		ssl = tls_c->ssl;
		n = 0;
		if (unlikely(tls_write_wants_read(tls_c) &&
						!(*flags & RD_CONN_EOF))) {
			DBG("tls write on read (WRITE_WANTS_READ)\n");
			n = tls_ct_wq_flush(ssl, &tls_c->ct_wq, &flush_flags,
								&ssl_error);
			if (unlikely(n < 0 )) {
				tls_set_mbufs(c, 0, 0);
				lock_release(&c->write_lock);
				ERR("write flush error (%d)\n", n);
				goto error;
			}
			if (likely(flush_flags & F_BUFQ_EMPTY))
				tls_c->flags &= ~F_TLS_CON_WR_WANTS_RD;
		}
		if (likely((rd.pos != rd.used ||
						(tls_c->flags & F_TLS_CON_SSL_PENDING)) &&
					ssl_error == SSL_ERROR_NONE)) {
			/* reset the SSL_PENDING flag */
			tls_c->flags &= ~F_TLS_CON_SSL_PENDING;
			if (unlikely(tls_c->state == S_TLS_CONNECTING)) {
				n = tls_connect(c, &ssl_error);
				if (unlikely(n>=1)) {
					n = SSL_read(ssl, r->pos, bytes_free);
				} else
					goto ssl_read_skipped;
			} else if (unlikely(tls_c->state == S_TLS_ACCEPTING)) {
				n = tls_accept(c, &ssl_error);
				if (unlikely(n>=1)) {
					n = SSL_read(ssl, r->pos, bytes_free);
				} else
					goto ssl_read_skipped;
			} else {
				/* if bytes in then decrypt read buffer into tcpconn req.
				   buffer */
				n = SSL_read(ssl, r->pos, bytes_free);
			}
			if (unlikely(n <= 0)) {
				ssl_error = SSL_get_error(ssl, n);
				/*  errors handled below, outside the lock */
			} else {
				ssl_error = SSL_ERROR_NONE;
				r->pos += n;
				ssl_read += n;
				if (unlikely(SSL_pending(ssl)>0)) {
					tls_c->flags |= F_TLS_CON_SSL_PENDING;
					*flags |= RD_CONN_REPEAT_READ;
				}
			}
ssl_read_skipped:
			;
		}
		if (unlikely(wr.used != 0 && ssl_error != SSL_ERROR_ZERO_RETURN)) {
			/* something was written and it's not ssl EOF*/
			if (unlikely(tcpconn_send_unsafe(c->fd, c, (char*)wr.buf,
											wr.used, c->send_flags) < 0)) {
				tls_set_mbufs(c, 0, 0);
				lock_release(&c->write_lock);
				goto error_send;
			}
		}
		/* quickly catch bugs: segfault if accessed and not set */
		tls_set_mbufs(c, 0, 0);
	lock_release(&c->write_lock);
	switch(ssl_error) {
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_ZERO_RETURN:
			/* SSL EOF */
			DBG("tls_read: SSL EOF on %p, FD %d\n", c, c->fd);
			goto ssl_eof;
		case SSL_ERROR_WANT_READ:
			/* reset the SSL_PENDING flag (in case we end here due to
			   a failed write buffer flush) */
			tls_c->flags &= ~F_TLS_CON_SSL_PENDING;
			/* needs to read more data */
			if (unlikely(rd.pos != rd.used)) {
				/* data still in the read buffer */
				BUG("SSL_ERROR_WANT_READ but data still in"
						" the rbio (%d bytes)\n", rd.used - rd.pos);
				goto bug;
			}
			if (unlikely((*flags & (RD_CONN_EOF | RD_CONN_SHORT_READ)) == 0))
				/* there might still be data to read and there is space
				   to decrypt it in tcp_req (no byte has been written into
				    tcp_req in this case) */
				goto redo_read;
			goto end; /* no more data to read */
		case SSL_ERROR_WANT_WRITE:
			/* write buffer too small, nothing written */
			BUG("write buffer too small (%d/%d bytes)\n",
					wr.used, wr.size);
			goto bug;
		case SSL_ERROR_SYSCALL:
		default:
			BUG("unexpected SSL error %d\n", ssl_error);
			goto bug;
	}
	if (unlikely(rd.pos != rd.used)) {
		/* encrypted data still in the read buffer (SSL_read() did not
		   consume all of it) */
		/* if (n< bytes_free) then not a full record read yet to get all
		        the requested bytes (unlikely, since openssl should buffer
		        it internally in this case).
		   else more data, but no space to store it => queue read data? */
		if (unlikely(n < 0))
			/* here n should always be >= 0 */
			BUG("unexpected value (n = %d)\n", n);
		else if (unlikely(n < bytes_free))
			BUG("read buffer not exhausted (rbio still has %d bytes,"
					"last SSL_read %d / %d)\n",
					rd.used - rd.pos, n, bytes_free);
		else if (n == bytes_free) {
			/*  queue read data if not fully consumed by SSL_read()
			 * (very unlikely situation)
			 */
			if (likely(!enc_rd_buf)) {
				enc_rd_buf = shm_malloc(sizeof(*enc_rd_buf) -
										sizeof(enc_rd_buf->buf) +
										rd.used - rd.pos);
				if (unlikely(enc_rd_buf == 0)) {
					ERR("memory allocation error (%d bytes requested)\n",
						sizeof(*enc_rd_buf) + sizeof(enc_rd_buf->buf) +
										rd.used - rd.pos);
					goto error;
				}
				enc_rd_buf->pos = 0;
				enc_rd_buf->size = rd.used - rd.pos;
				memcpy(enc_rd_buf->buf, rd.buf + rd.pos,
										enc_rd_buf->size);
			} else if ((enc_rd_buf->buf + enc_rd_buf->pos) == rd.buf)
				enc_rd_buf->pos += rd.pos;
			else {
				BUG("enc_rd_buf->buf = %p, pos = %d, rd_buf.buf = %p\n",
						enc_rd_buf->buf, enc_rd_buf->pos, rd.buf);
				goto bug;
			}
			if (unlikely(tls_c->enc_rd_buf))
				BUG("tls_c->enc_rd_buf!=0 (%p)\n", tls_c->enc_rd_buf);
			/* there can't be 2 reads in parallel, so no locking is needed
			   here */
			tls_c->enc_rd_buf = enc_rd_buf;
			enc_rd_buf = 0;
			*flags |= RD_CONN_REPEAT_READ;
		}
	} else if (n < bytes_free && n > 0 &&
				((*flags & (RD_CONN_EOF|RD_CONN_SHORT_READ)) == 0)) {
		/* still space in the tcp unenc. req. buffer, no SSL_read error,
		   not a short read and not an EOF (possible more data in
		   the socket buffer) => redo read*/
		goto redo_read;
	}
	
end:
	if (enc_rd_buf)
		shm_free(enc_rd_buf);
	return ssl_read;
ssl_eof:
	if (enc_rd_buf)
		shm_free(enc_rd_buf);
	c->state = S_CONN_EOF;
	*flags |= RD_CONN_EOF;
	return ssl_read;
error_send:
error:
bug:
	if (enc_rd_buf)
		shm_free(enc_rd_buf);
	r->error=TCP_READ_ERROR;
	return -1;
}
