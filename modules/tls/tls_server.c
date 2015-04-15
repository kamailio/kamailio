/*
 * TLS module - main server part
 *
 * Copyright (C) 2005-2010 iptelorg GmbH
 * Copyright (C) 2013 Motorola Solutions, Inc.
 *
 * This file is part of Kamailio, a free SIP server.
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
#include "../../cfg/cfg.h"
#include "../../route.h"
#include "../../forward.h"
#include "../../onsend.h"
#include "../../xavp.h"

#include "tls_init.h"
#include "tls_domain.h"
#include "tls_util.h"
#include "tls_mod.h"
#include "tls_server.h"
#include "tls_select.h"
#include "tls_bio.h"
#include "tls_dump_vf.h"
#include "tls_cfg.h"

int tls_run_event_routes(struct tcp_connection *c);

/* low memory treshold for openssl bug #1491 workaround */
#define LOW_MEM_NEW_CONNECTION_TEST() \
	(cfg_get(tls, tls_cfg, low_mem_threshold1) && \
	  (shm_available_safe() < cfg_get(tls, tls_cfg, low_mem_threshold1)))
#define LOW_MEM_CONNECTED_TEST() \
	(cfg_get(tls, tls_cfg, low_mem_threshold2) && \
	  (shm_available_safe() <  cfg_get(tls, tls_cfg, low_mem_threshold2)))

#define TLS_RD_MBUF_SZ	65536
#define TLS_WR_MBUF_SZ	65536


/* debugging */
#ifdef NO_TLS_RD_DEBUG
#undef TLS_RD_DEBUG
#endif

#ifdef NO_TLS_WR_DEBUG
#undef TLS_WR_DEBUG
#endif
#if defined TLS_RD_DEBUG || defined TLS_WR_DEBUG
#define TLS_F_DEBUG
#endif

/* if NO_TLS_F_DEBUG or NO_TLS_DEBUG => no debug code */
#if defined NO_TLS_F_DEBUG || defined NO_TLS_DEBUG
#undef TLS_F_DEBUG
#endif

#ifdef TLS_F_DEBUG
	#ifdef __SUNPRO_C
		#define TLS_F_TRACE(fmt, ...) \
			LOG_(DEFAULT_FACILITY, cfg_get(tls, tls_cfg, debug),\
					"TLS_TRACE: " LOC_INFO, " %s" fmt,\
					_FUNC_NAME_,  __VA_ARGS__)
	#else
		#define TLS_F_TRACE(fmt, args...) \
			LOG_(DEFAULT_FACILITY, cfg_get(tls, tls_cfg, debug),\
					"TLS_TRACE: " LOC_INFO, " %s" fmt,\
					_FUNC_NAME_, ## args)
	#endif /* __SUNPRO_c */
#else /* TLS_F_DEBUG */
	#ifdef __SUNPRO_C
		#define TLS_F_TRACE(...)
	#else
		#define TLS_F_TRACE(fmt, args...)
	#endif /* __SUNPRO_c */
#endif /* TLS_F_DEBUG */

/* tls_read debugging */
#ifdef TLS_RD_DEBUG
	#define TLS_RD_TRACE TLS_F_TRACE
#else /* TLS_RD_DEBUG */
	#ifdef __SUNPRO_C
		#define TLS_RD_TRACE(...)
	#else
		#define TLS_RD_TRACE(fmt, args...)
	#endif /* __SUNPRO_c */
#endif /* TLS_RD_DEBUG */

/* tls_write debugging */
#ifdef TLS_WR_DEBUG
	#define TLS_WR_TRACE TLS_F_TRACE
#else /* TLS_RD_DEBUG */
	#ifdef __SUNPRO_C
		#define TLS_WR_TRACE(...)
	#else
		#define TLS_WR_TRACE(fmt, args...)
	#endif /* __SUNPRO_c */
#endif /* TLS_RD_DEBUG */


extern str sr_tls_xavp_cfg;

/**
 * get the server name (sni) for outbound connections from xavp
 */
static str *tls_get_connect_server_name(void)
{
#ifndef OPENSSL_NO_TLSEXT
	sr_xavp_t *vavp = NULL;
	str sname = {"server_name", 11};

	if(sr_tls_xavp_cfg.s!=NULL)
		vavp = xavp_get_child_with_sval(&sr_tls_xavp_cfg, &sname);
	if(vavp==NULL || vavp->val.v.s.len<=0) {
		LM_DBG("xavp with outbound server name not found\n");
		return NULL;
	}
	LM_DBG("found xavp with outbound server name: %s\n", vavp->val.v.s.s);
	return &vavp->val.v.s;
#else
	return NULL;
#endif
}

/** finish the ssl init.
 * Creates the SSL context + internal tls_extra_data and sets
 * extra_data to it.
 * Separated from tls_tcpconn_init to allow delayed ssl context
 * init (from the "child" process and not from the main one).
 * WARNING: the connection should be already locked.
 * @return 0 on success, -1 on errror.
 */
static int tls_complete_init(struct tcp_connection* c)
{
	tls_domain_t* dom;
	struct tls_extra_data* data = 0;
	tls_domains_cfg_t* cfg;
	enum tls_conn_states state;
	str *sname = NULL;

	if (LOW_MEM_NEW_CONNECTION_TEST()){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		goto error2;
	}
	     /* Get current TLS configuration and increase reference
	      * count immediately.
	      */

	LM_DBG("completing tls connection initialization\n");

	lock_get(tls_domains_cfg_lock);
	cfg = *tls_domains_cfg;

	     /* Increment the reference count in the configuration structure, this
	      * is to ensure that, while on the garbage queue, the configuration does
	      * not get deleted if there are still connection referencing its SSL_CTX
	      */
	atomic_inc(&cfg->ref_count);
	lock_release(tls_domains_cfg_lock);

	if (c->flags & F_CONN_PASSIVE) {
		state=S_TLS_ACCEPTING;
		dom = tls_lookup_cfg(cfg, TLS_DOMAIN_SRV,
								&c->rcv.dst_ip, c->rcv.dst_port, 0);
	} else {
		state=S_TLS_CONNECTING;
		sname = tls_get_connect_server_name();
		dom = tls_lookup_cfg(cfg, TLS_DOMAIN_CLI,
						&c->rcv.dst_ip, c->rcv.dst_port, sname);
	}
	if (unlikely(c->state<0)) {
		BUG("Invalid connection (state %d)\n", c->state);
		goto error;
	}
	DBG("Using initial TLS domain %s (dom %p ctx %p sn [%s])\n",
			tls_domain_str(dom), dom, dom->ctx[process_no],
			ZSW(dom->server_name.s));

	data = (struct tls_extra_data*)shm_malloc(sizeof(struct tls_extra_data));
	if (!data) {
		ERR("Not enough shared memory left\n");
		goto error;
	}
	memset(data, '\0', sizeof(struct tls_extra_data));
	data->ssl = SSL_new(dom->ctx[process_no]);
	data->rwbio = tls_BIO_new_mbuf(0, 0);
	data->cfg = cfg;
	data->state = state;

	if (unlikely(data->ssl == 0 || data->rwbio == 0)) {
		TLS_ERR("Failed to create SSL or BIO structure:");
		if (data->ssl)
			SSL_free(data->ssl);
		if (data->rwbio)
			BIO_free(data->rwbio);
		goto error;
	}

#ifndef OPENSSL_NO_TLSEXT
	if (sname!=NULL) {
		if(!SSL_set_tlsext_host_name(data->ssl, sname->s)) {
			if (data->ssl)
				SSL_free(data->ssl);
			if (data->rwbio)
				BIO_free(data->rwbio);
			goto error;
		}
		LM_DBG("outbound TLS server name set to: %s\n", sname->s);
	}
#endif

#ifdef TLS_KSSL_WORKARROUND
	 /* if needed apply workaround for openssl bug #1467 */
	if (data->ssl->kssl_ctx && openssl_kssl_malloc_bug){
		kssl_ctx_free(data->ssl->kssl_ctx);
		data->ssl->kssl_ctx=0;
	}
#endif
	SSL_set_bio(data->ssl, data->rwbio, data->rwbio);
	c->extra_data = data;

	/* link the extra data struct inside ssl connection*/
	SSL_set_app_data(data->ssl, data);
	return 0;

 error:
	atomic_dec(&cfg->ref_count);
	if (data) shm_free(data);
 error2:
	return -1;
}



/** completes tls init if needed and checks if tls can be used (unsafe).
 *  It will check for low memory.
 *  If it returns success, c->extra_data is guaranteed to be !=0.
 *  WARNING: must be called with c->write_lock held.
 *  @return 0 on success, < 0 on error (complete init failed or out of memory).
 */
static int tls_fix_connection_unsafe(struct tcp_connection* c)
{
	if (unlikely(!c->extra_data)) {
		if (unlikely(tls_complete_init(c) < 0)) {
			return -1;
		}
	}else if (unlikely(LOW_MEM_CONNECTED_TEST())){
		ERR("tls: ssl bug #1491 workaround: not enough memory for safe"
				" operation: %lu\n", shm_available());
		return -1;
	}
	return 0;
}



/** completes tls init if needed and checks if tls can be used, (safe).
 *  It will check for low memory.
 *  If it returns success, c->extra_data is guaranteed to be !=0.
 *  WARNING: must _not_ be called with c->write_lock held (it will
 *   lock/unlock internally), see also tls_fix_connection_unsafe().
 *  @return 0 on success, < 0 on error (complete init failed or out of memory).
 */
static int tls_fix_connection(struct tcp_connection* c)
{
	int ret;
	
	if (unlikely(c->extra_data == 0)) {
		lock_get(&c->write_lock);
			if (unlikely(c->extra_data == 0)) {
				ret = tls_complete_init(c);
				lock_release(&c->write_lock);
				return ret;
			}
		lock_release(&c->write_lock);
	}
	if (unlikely(LOW_MEM_CONNECTED_TEST())){
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
	BIO *rwbio;
	
	rwbio = ((struct tls_extra_data*)c->extra_data)->rwbio;
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
		LOG(cfg_get(tls, tls_cfg, log), "%s subject:%s\n", s ? s : "", subj);
		OPENSSL_free(subj);
	}
	if (issuer){
		LOG(cfg_get(tls, tls_cfg, log), "%s issuer:%s\n", s ? s : "", issuer);
		OPENSSL_free(issuer);
	}
}



/** wrapper around SSL_accept, usin SSL return convention.
 * It will also log critical errors and certificate debugging info.
 * @param c - tcp connection with tls (extra_data must be a filled
 *            tcp_extra_data structure). The state must be S_TLS_ACCEPTING.
 * @param error  set to the error reason (SSL_ERROR_*).
 *            Note that it can be SSL_ERROR_NONE while the return is < 0
 *            ("internal" error, not at the SSL level, see below).
 * @return >=1 on success, 0 and <0 on error. 0 means the underlying SSL
 *           connection was closed/shutdown.  < 0 is returned for any
 *           SSL_ERROR (including  WANT_READ or WANT_WRITE), but also
 *           for internal non SSL related errors (in this case -2 is
 *           returned and error==SSL_ERROR_NONE).
 *
 */
int tls_accept(struct tcp_connection *c, int* error)
{
	int ret;
	SSL *ssl;
	X509* cert;
	struct tls_extra_data* tls_c;
	int tls_log;

	*error = SSL_ERROR_NONE;
	tls_c=(struct tls_extra_data*)c->extra_data;
	ssl=tls_c->ssl;
	
	if (unlikely(tls_c->state != S_TLS_ACCEPTING)) {
		BUG("Invalid connection state %d (bug in TLS code)\n", tls_c->state);
		goto err;
	}
	ret = SSL_accept(ssl);
	if (unlikely(ret == 1)) {
		DBG("TLS accept successful\n");
		tls_c->state = S_TLS_ESTABLISHED;
		tls_log = cfg_get(tls, tls_cfg, log);
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
	}
	return ret;
err:
	/* internal non openssl related errors */
	return -2;
}


/** wrapper around SSL_connect, using SSL return convention.
 * It will also log critical errors and certificate debugging info.
 * @param c - tcp connection with tls (extra_data must be a filled
 *            tcp_extra_data structure). The state must be S_TLS_CONNECTING.
 * @param error  set to the error reason (SSL_ERROR_*).
 *            Note that it can be SSL_ERROR_NONE while the return is < 0
 *            ("internal" error, not at the SSL level, see below).
 * @return >=1 on success, 0 and <0 on error. 0 means the underlying SSL
 *           connection was closed/shutdown. < 0 is returned for any
 *           SSL_ERROR (including  WANT_READ or WANT_WRITE), but also
 *           for internal non SSL related errors (in this case -2 is
 *           returned and error==SSL_ERROR_NONE).
 *
 */
int tls_connect(struct tcp_connection *c, int* error)
{
	SSL *ssl;
	int ret;
	X509* cert;
	struct tls_extra_data* tls_c;
	int tls_log;

	*error = SSL_ERROR_NONE;
	tls_c=(struct tls_extra_data*)c->extra_data;
	ssl=tls_c->ssl;
	
	if (unlikely(tls_c->state != S_TLS_CONNECTING)) {
		BUG("Invalid connection state %d (bug in TLS code)\n", tls_c->state);
		goto err;
	}
	ret = SSL_connect(ssl);
	if (unlikely(ret == 1)) {
		DBG("TLS connect successful\n");
		tls_c->state = S_TLS_ESTABLISHED;
		tls_log = cfg_get(tls, tls_cfg, log);
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
		tls_run_event_routes(c);
	} else { /* 0 or < 0 */
		*error = SSL_get_error(ssl, ret);
	}
	return ret;
err:
	/* internal non openssl related errors */
	return -2;
}


/*
 * wrapper around SSL_shutdown, returns -1 on error, 0 on success.
 */
static int tls_shutdown(struct tcp_connection *c)
{
	int ret, err, ssl_err;
	struct tls_extra_data* tls_c;
	SSL *ssl;

	tls_c=(struct tls_extra_data*)c->extra_data;
	if (unlikely(tls_c == 0 || tls_c->ssl == 0)) {
		ERR("No SSL data to perform tls_shutdown\n");
		return -1;
	}
	ssl = tls_c->ssl;
	/* it doesn't make sense to try a TLS level shutdown
	   if the connection is not fully initialized */
	if (unlikely(tls_c->state != S_TLS_ESTABLISHED))
		return 0;
	if (unlikely(LOW_MEM_CONNECTED_TEST())){
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
			
		case SSL_ERROR_SSL:
		default:
			TLS_ERR("SSL error:");
			goto err;
		}
	}
	
	return 0;
 err:
	return -1;
}



/** init tls specific data in a tcp connection.
 * Called when a new tcp connection is accepted or connected.
 * It completes the tcp connection initialisation by setting the tls
 * specific parts.
 * Note that ssl context creation and other expensive operation are left
 * out (they are delayed until the first read/write).
 * No locking is needed (when the connection is created no other process
 * can access it).
 * @param c - tcp connection.
 * @param sock - socket (unused for now).
 * @return  0 on success, < 0 on error.
 */
int tls_h_tcpconn_init(struct tcp_connection *c, int sock)
{
	c->type = PROTO_TLS;
	c->rcv.proto = PROTO_TLS;
	c->timeout = get_ticks_raw() + cfg_get(tls, tls_cfg, con_lifetime);
	c->lifetime = cfg_get(tls, tls_cfg, con_lifetime);
	c->extra_data = 0;
	return 0;
}


/** clean the extra data upon connection shut down.
 */
void tls_h_tcpconn_clean(struct tcp_connection *c)
{
	struct tls_extra_data* extra;
	/*
	* runs within global tcp lock
	*/
	if ((c->type != PROTO_TLS) && (c->type != PROTO_WSS)) {
		BUG("Bad connection structure\n");
		abort();
	}
	if (c->extra_data) {
		extra = (struct tls_extra_data*)c->extra_data;
		SSL_free(extra->ssl);
		atomic_dec(&extra->cfg->ref_count);
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


/** perform one-way shutdown, do not wait for notify from the remote peer.
 */
void tls_h_close(struct tcp_connection *c, int fd)
{
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
	if (unlikely(cfg_get(tls, tls_cfg, send_close_notify) && c->extra_data)) {
		lock_get(&c->write_lock);
			if (unlikely(c->extra_data == 0)) {
				/* changed in the meanwhile */
				lock_release(&c->write_lock);
				return;
			}
			tls_mbuf_init(&rd, 0, 0); /* no read */
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



/** tls encrypt before sending function.
 * It is a callback that will be called by the tcp code, before a send
 * on TLS would be attempted. It should replace the input buffer with a
 * new static buffer containing the TLS processed data.
 * If the input buffer could not be fully encoded (e.g. run out of space
 * in the internal static buffer), it should set rest_buf and rest_len to
 * the remaining part, so that it could be called again once the output has
 * been used (sent). The send_flags used are also passed and they can be
 * changed (e.g. to disallow a close() after a partial encode).
 * WARNING: it must always be called with c->write_lock held!
 * @param c - tcp connection
 * @param pbuf - pointer to buffer (value/result, on success it will be
 *               replaced with a static buffer).
 * @param plen - pointer to buffer size (value/result, on success it will be
 *               replaced with the size of the replacement buffer.
 * @param rest_buf - (result) should be filled with a pointer to the
 *                remaining unencoded part of the original buffer if any,
 *                0 otherwise.
 * @param rest_len - (result) should be filled with the length of the
 *                 remaining unencoded part of the original buffer (0 if
 *                 the original buffer was fully encoded).
 * @param send_flags - pointer to the send_flags that will be used for sending
 *                     the message.
 * @return *plen on success (>=0), < 0 on error.
 */
int tls_encode_f(struct tcp_connection *c,
						const char** pbuf, unsigned int* plen,
						const char** rest_buf, unsigned int* rest_len,
						snd_flags_t* send_flags)
{
	int n, offs;
	SSL* ssl;
	struct tls_extra_data* tls_c;
	static unsigned char wr_buf[TLS_WR_MBUF_SZ];
	struct tls_mbuf rd, wr;
	int ssl_error;
	char* err_src;
	const char* buf;
	unsigned int len;
	int x;
	
	buf = *pbuf;
	len = *plen;
	*rest_buf = 0;
	*rest_len = 0;
	TLS_WR_TRACE("(%p, %p, %d, ... 0x%0x) start (%s:%d* -> %s)\n",
					c, buf, len, send_flags->f,
					ip_addr2a(&c->rcv.dst_ip), c->rcv.dst_port,
					su2a(&c->rcv.src_su, sizeof(c->rcv.src_su)));
	n = 0;
	offs = 0;
	ssl_error = SSL_ERROR_NONE;
	err_src = "TLS write:";
	if (unlikely(tls_fix_connection_unsafe(c) < 0)) {
		/* c->extra_data might be null => exit immediately */
		TLS_WR_TRACE("(%p) end: tls_fix_connection_unsafe failed =>"
						" immediate error exit\n", c);
		return -1;
	}
	tls_c = (struct tls_extra_data*)c->extra_data;
	ssl = tls_c->ssl;
	tls_mbuf_init(&rd, 0, 0); /* no read */
	tls_mbuf_init(&wr, wr_buf, sizeof(wr_buf));
	/* clear text already queued (WANTS_READ) queue directly*/
	if (unlikely(tls_write_wants_read(tls_c))) {
		TLS_WR_TRACE("(%p) WANTS_READ queue present => queueing"
						" (%d bytes,  %p + %d)\n", c, len - offs, buf, offs);
		if (unlikely(tls_ct_wq_add(&tls_c->ct_wq, buf+offs, len -offs) < 0)) {
				ERR("ct write buffer full for %p (%d bytes)\n",
						c, tls_c->ct_wq?tls_c->ct_wq->queued:0);
				goto error_wq_full;
		}
		/* buffer queued for a future send attempt, after first reading
		   some data (key exchange) => don't allow immediate closing of
		   the connection */
		send_flags->f &= ~SND_F_CON_CLOSE;
		goto end;
	}
	if (unlikely(tls_set_mbufs(c, &rd, &wr) < 0)) {
		ERR("tls_set_mbufs failed\n");
		goto error;
	}
redo_wr:
	if (unlikely(tls_c->state == S_TLS_CONNECTING)) {
		n = tls_connect(c, &ssl_error);
		TLS_WR_TRACE("(%p) tls_connect() => %d (err=%d)\n", c, n, ssl_error);
		if (unlikely(n>=1)) {
			n = SSL_write(ssl, buf + offs, len - offs);
			if (unlikely(n <= 0))
				ssl_error = SSL_get_error(ssl, n);
		} else {
			/* tls_connect failed/needs more IO */
			if (unlikely(n < 0 && ssl_error == SSL_ERROR_NONE))
				goto error;
			err_src = "TLS connect:";
		}
	} else if (unlikely(tls_c->state == S_TLS_ACCEPTING)) {
		n = tls_accept(c, &ssl_error);
		TLS_WR_TRACE("(%p) tls_accept() => %d (err=%d)\n", c, n, ssl_error);
		if (unlikely(n>=1)) {
			n = SSL_write(ssl, buf + offs, len - offs);
			if (unlikely(n <= 0))
				ssl_error = SSL_get_error(ssl, n);
		} else {
			/* tls_accept failed/needs more IO */
			if (unlikely(n < 0 && ssl_error == SSL_ERROR_NONE))
				goto error;
			err_src = "TLS accept:";
		}
	} else {
		n = SSL_write(ssl, buf + offs, len - offs);
		if (unlikely(n <= 0))
			ssl_error = SSL_get_error(ssl, n);
	}
	TLS_WR_TRACE("(%p) SSL_write(%p + %d, %d) => %d (err=%d)\n",
					c, buf, offs, len - offs, n, ssl_error);
	/* check for possible ssl errors */
	if (unlikely(n <= 0)){
		switch(ssl_error) {
			case SSL_ERROR_NONE:
				BUG("unexpected SSL_ERROR_NONE for n=%d\n", n);
				goto error;
				break;
			case SSL_ERROR_ZERO_RETURN:
				/* SSL EOF */
				ERR("ssl level EOF\n");
				goto ssl_eof;
			case SSL_ERROR_WANT_READ:
				/* queue write buffer */
				TLS_WR_TRACE("(%p) SSL_ERROR_WANT_READ => queueing for read"
								" (%p + %d, %d)\n", c, buf, offs, len -offs);
				if (unlikely(tls_ct_wq_add(&tls_c->ct_wq, buf+offs, len -offs)
								< 0)) {
					ERR("ct write buffer full (%d bytes)\n",
							tls_c->ct_wq?tls_c->ct_wq->queued:0);
					goto error_wq_full;
				}
				tls_c->flags |= F_TLS_CON_WR_WANTS_RD;
				/* buffer queued for a future send attempt, after first
				   reading some data (key exchange) => don't allow immediate
				   closing of the connection */
				send_flags->f &= ~SND_F_CON_CLOSE;
				break; /* or goto end */
			case SSL_ERROR_WANT_WRITE:
				if (unlikely(offs == 0)) {
					/*  error, no record fits in the buffer or
					  no partial write enabled and buffer to small to fit
					  all the records */
					BUG("write buffer too small (%d/%d bytes)\n",
							wr.used, wr.size);
					goto bug;
				} else {
					/* offs != 0 => something was "written"  */
					*rest_buf = buf + offs;
					*rest_len = len - offs;
					/* this function should be called again => disallow
					   immediate closing of the connection */
					send_flags->f &= ~SND_F_CON_CLOSE;
					TLS_WR_TRACE("(%p) SSL_ERROR_WANT_WRITE partial write"
								" (written %p , %d, rest_buf=%p"
								" rest_len=%d))\n", c, buf, offs,
								*rest_buf, *rest_len);
				}
				break; /* or goto end */
			case SSL_ERROR_SSL:
				/* protocol level error */
				TLS_ERR(err_src);
				goto error;
#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*0.9.7*/
			case SSL_ERROR_WANT_CONNECT:
				/* only if the underlying BIO is not yet connected
				   and the call would block in connect().
				   (not possible in our case) */
				BUG("unexpected SSL_ERROR_WANT_CONNECT\n");
				break;
			case SSL_ERROR_WANT_ACCEPT:
				/* only if the underlying BIO is not yet connected
				   and call would block in accept()
				   (not possible in our case) */
				BUG("unexpected SSL_ERROR_WANT_ACCEPT\n");
				break;
#endif
			case SSL_ERROR_WANT_X509_LOOKUP:
				/* can only appear on client application and it indicates that
				   an installed client cert. callback should be called again
				   (it returned < 0 indicated that it wants to be called
				   later). Not possible in our case */
				BUG("unsupported SSL_ERROR_WANT_X509_LOOKUP");
				goto bug;
			case SSL_ERROR_SYSCALL:
				TLS_ERR_RET(x, err_src);
				if (!x) {
					if (n == 0) {
						WARN("Unexpected EOF\n");
					} else
						/* should never happen */
						BUG("IO error (%d) %s\n", errno, strerror(errno));
				}
				goto error;
			default:
				TLS_ERR(err_src);
				BUG("unexpected SSL error %d\n", ssl_error);
				goto bug;
		}
	} else if (unlikely(n < (len - offs))) {
		/* partial ssl write (possible if SSL_MODE_ENABLE_PARTIAL_WRITE) =>
		   retry with the rest */
		TLS_WR_TRACE("(%p) partial write (%d < %d, offset %d), retry\n",
						c, n, len - offs, offs);
		offs += n;
		goto redo_wr;
	}
	tls_set_mbufs(c, 0, 0);
end:
	*pbuf = (const char*)wr.buf;
	*plen = wr.used;
	TLS_WR_TRACE("(%p) end (offs %d, rest_buf=%p rest_len=%d 0x%0x) => %d \n",
					c, offs, *rest_buf, *rest_len, send_flags->f, *plen);
	return *plen;
error:
/*error_send:*/
error_wq_full:
bug:
	tls_set_mbufs(c, 0, 0);
	TLS_WR_TRACE("(%p) end error (offs %d, %d encoded) => -1\n",
					c, offs, wr.used);
	return -1;
ssl_eof:
	c->state = S_CONN_EOF;
	c->flags |= F_CONN_FORCE_EOF;
	*pbuf = (const char*)wr.buf;
	*plen = wr.used;
	DBG("TLS connection has been closed\n");
	TLS_WR_TRACE("(%p) end EOF (offs %d) => (%d\n",
					c, offs, *plen);
	return *plen;
}



/** tls read.
 * Each modification of ssl data structures has to be protected, another process * might ask for the same connection and attempt write to it which would
 * result in updating the ssl structures.
 * WARNING: must be called whic c->write_lock _unlocked_.
 * @param c - tcp connection pointer. The following flags might be set:
 * @param flags - value/result:
 *                     input: RD_CONN_FORCE_EOF  - force EOF after the first
 *                            successful read (bytes_read >=0 )
 *                     output: RD_CONN_SHORT_READ if the read exhausted
 *                              all the bytes in the socket read buffer.
 *                             RD_CONN_EOF if EOF detected (0 bytes read)
 *                              or forced via RD_CONN_FORCE_EOF.
 *                             RD_CONN_REPEAT_READ  if this function should
 *                              be called again (e.g. has some data
 *                              buffered internally that didn't fit in
 *                              tcp_req).
 *                     Note: RD_CONN_SHORT_READ & RD_CONN_EOF should be cleared
 *                           before calling this function when there is new
 *                           data (e.g. POLLIN), but not if the called is
 *                           retried because of RD_CONN_REPEAT_READ and there
 *                           is no information about the socket having more
 *                           read data available.
 * @return bytes decrypted on success, -1 on error (it also sets some
 *         tcp connection flags and might set c->state and r->error on
 *         EOF or error).
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
	char* err_src;
	int x;
	int tls_dbg;
	
	TLS_RD_TRACE("(%p, %p (%d)) start (%s -> %s:%d*)\n",
					c, flags, *flags,
					su2a(&c->rcv.src_su, sizeof(c->rcv.src_su)),
					ip_addr2a(&c->rcv.dst_ip), c->rcv.dst_port);
	ssl_read = 0;
	r = &c->req;
	enc_rd_buf = 0;
	*flags &= ~RD_CONN_REPEAT_READ;
	if (unlikely(tls_fix_connection(c) < 0)) {
		TLS_RD_TRACE("(%p, %p) end: tls_fix_connection failed =>"
						" immediate error exit\n", c, flags);
		return -1;
	}
	/* here it's safe to use c->extra_data in read-only mode.
	   If it's != 0 is changed only on destroy. It's not possible to have
	   parallel reads.*/
	tls_c = c->extra_data;
	bytes_free = c->req.b_size - (int)(r->pos - r->buf);
	if (unlikely(bytes_free == 0)) {
		ERR("Buffer overrun, dropping\n");
		r->error = TCP_REQ_OVERRUN;
		return -1;
	}
redo_read:
	/* if data queued from a previous read(), use it (don't perform
	 * a real read()).
	*/
	if (unlikely(tls_c->enc_rd_buf)) {
		/* use queued data */
		/* safe to use without locks, because only read changes it and
		   there can't be parallel reads on the same connection */
		enc_rd_buf = tls_c->enc_rd_buf;
		tls_c->enc_rd_buf = 0;
		TLS_RD_TRACE("(%p, %p) using queued data (%p: %p %d bytes)\n", c,
					flags, enc_rd_buf, enc_rd_buf->buf + enc_rd_buf->pos,
					enc_rd_buf->size - enc_rd_buf->pos);
		tls_mbuf_init(&rd, enc_rd_buf->buf + enc_rd_buf->pos,
						enc_rd_buf->size - enc_rd_buf->pos);
		rd.used = enc_rd_buf->size - enc_rd_buf->pos;
	} else {
		/* if we were using using queued data before, free & reset the
			the queued read data before performing the real read() */
		if (unlikely(enc_rd_buf)) {
			TLS_RD_TRACE("(%p, %p) reset prev. used enc_rd_buf (%p)\n", c,
							flags, enc_rd_buf);
			shm_free(enc_rd_buf);
			enc_rd_buf = 0;
		}
		/* real read() */
		tls_mbuf_init(&rd, rd_buf, sizeof(rd_buf));
		/* read() only if no previously detected EOF, or previous
		   short read (which means the socket buffer was emptied) */
		if (likely(!(*flags & (RD_CONN_EOF|RD_CONN_SHORT_READ)))) {
			/* don't read more then the free bytes in the tcp req buffer */
			read_size = MIN_unsigned(rd.size, bytes_free);
			bytes_read = tcp_read_data(c->fd, c, (char*)rd.buf, read_size,
										flags);
			TLS_RD_TRACE("(%p, %p) tcp_read_data(..., %d, *%d) => %d bytes\n",
						c, flags, read_size, *flags, bytes_read);
			/* try SSL_read even on 0 bytes read, it might have
			   internally buffered data */
			if (unlikely(bytes_read < 0)) {
					goto error;
			}
			rd.used = bytes_read;
		}
	}
	
continue_ssl_read:
	tls_mbuf_init(&wr, wr_buf, sizeof(wr_buf));
	ssl_error = SSL_ERROR_NONE;
	err_src = "TLS read:";
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
			n = tls_ct_wq_flush(c, &tls_c->ct_wq, &flush_flags,
								&ssl_error);
			TLS_RD_TRACE("(%p, %p) tls write on read (WRITE_WANTS_READ):"
							" ct_wq_flush()=> %d (ff=%d ssl_error=%d))\n",
							c, flags, n, flush_flags, ssl_error);
			if (unlikely(n < 0 )) {
				tls_set_mbufs(c, 0, 0);
				lock_release(&c->write_lock);
				ERR("write flush error (%d)\n", n);
				goto error;
			}
			if (likely(flush_flags & F_BUFQ_EMPTY))
				tls_c->flags &= ~F_TLS_CON_WR_WANTS_RD;
			if (unlikely(flush_flags & F_BUFQ_ERROR_FLUSH))
				err_src = "TLS write:";
		}
		if (likely(ssl_error == SSL_ERROR_NONE)) {
			if (unlikely(tls_c->state == S_TLS_CONNECTING)) {
				n = tls_connect(c, &ssl_error);
				TLS_RD_TRACE("(%p, %p) tls_connect() => %d (err=%d)\n",
								c, flags, n, ssl_error);
				if (unlikely(n>=1)) {
					n = SSL_read(ssl, r->pos, bytes_free);
				} else {
					/* tls_connect failed/needs more IO */
					if (unlikely(n < 0 && ssl_error == SSL_ERROR_NONE)) {
						lock_release(&c->write_lock);
						goto error;
					}
					err_src = "TLS connect:";
					goto ssl_read_skipped;
				}
			} else if (unlikely(tls_c->state == S_TLS_ACCEPTING)) {
				n = tls_accept(c, &ssl_error);
				TLS_RD_TRACE("(%p, %p) tls_accept() => %d (err=%d)\n",
								c, flags, n, ssl_error);
				if (unlikely(n>=1)) {
					n = SSL_read(ssl, r->pos, bytes_free);
				} else {
					/* tls_accept failed/needs more IO */
					if (unlikely(n < 0 && ssl_error == SSL_ERROR_NONE)) {
						lock_release(&c->write_lock);
						goto error;
					}
					err_src = "TLS accept:";
					goto ssl_read_skipped;
				}
			} else {
				/* if bytes in then decrypt read buffer into tcpconn req.
				   buffer */
				n = SSL_read(ssl, r->pos, bytes_free);
			}
			/** handle SSL_read() return.
			 *  There are 3 main cases, each with several sub-cases, depending
			 *  on whether or not the output buffer was filled, if there
			 *  is still unconsumed input data in the input buffer (rd)
			 *  and if there is "cached" data in the internal openssl
			 *  buffers.
			 *  0. error (n<=0):
			 *     SSL_ERROR_WANT_READ - input data fully
			 *       consumed, no more returnable cached data inside openssl
			 *       => exit.
			 *     SSL_ERROR_WANT_WRITE - should never happen (the write
			 *       buffer is big enough to handle any re-negociation).
			 *     SSL_ERROR_ZERO_RETURN - ssl level shutdown => exit.
			 *    other errors are unexpected.
			 * 1. output buffer filled (n == bytes_free):
			 *    1i.  - still unconsumed input, nothing buffered by openssl
			 *    1ip. - unconsumed input + buffered data by openssl (pending
			             on the next SSL_read).
			 *    1p.  - completely consumed input, buffered data internally
			 *            by openssl (pending).
			 *           Likely to happen, about the only case when
			 *           SSL_pending() could be used (but only if readahead=0).
			 *    1f.  - consumed input, no buffered data.
			 * 2. output buffer not fully filled (n < bytes_free):
			 *     2i. - still unconsumed input, nothing buffered by openssl.
			 *           This can appear if SSL readahead is 0 (SSL_read()
			 *           tries to get only 1 record from the input).
			 *     2ip. - unconsumed input and buffered data by openssl.
			 *            Unlikely to happen (e.g. readahead is 1, more
			 *            records are buffered internally by openssl, but
			 *            there was not enough space for buffering the whole
			 *            input).
			 *     2p  - consumed input, but buffered data by openssl.
			 *            It happens especially when readahead is 1.
			 *     2f.  - consumed input, no buffered data.
			 *
			 * One should repeat SSL_read() until and error is detected
			 *  (0*) or the input and internal ssl buffers are fully consumed
			 *  (1f or 2f). However in general is not possible to see if
			 *  SSL_read() could return more data. SSL_pending() has very
			 *  limited usability (basically it would return !=0 only if there
			 *  was no enough space in the output buffer and only if this did
			 *  not happen at a record boundary).
			 * The solution is to repeat SSL_read() until error or until
			 *  the output buffer is filled (0* or 1*).
			 *  In the later case, this whole function should be called again
			 *  once there is more output space (set RD_CONN_REPEAT_READ).
			 */

			if (unlikely(tls_c->flags & F_TLS_CON_RENEGOTIATION)) {
				/* Fix CVE-2009-3555 - disable renegotiation if started by client
				 * - simulate SSL EOF to force close connection*/
				tls_dbg = cfg_get(tls, tls_cfg, debug);
				LOG(tls_dbg, "Reading on a renegotiation of connection (n:%d) (%d)\n",
						n, SSL_get_error(ssl, n));
				err_src = "TLS R-N read:";
				ssl_error = SSL_ERROR_ZERO_RETURN;
			} else {
				if (unlikely(n <= 0)) {
					ssl_error = SSL_get_error(ssl, n);
					err_src = "TLS read:";
					/*  errors handled below, outside the lock */
				} else {
					ssl_error = SSL_ERROR_NONE;
					r->pos += n;
					ssl_read += n;
					bytes_free -=n;
				}
			}
			TLS_RD_TRACE("(%p, %p) SSL_read() => %d (err=%d) ssl_read=%d"
							" *flags=%d tls_c->flags=%d\n",
							c, flags, n, ssl_error, ssl_read, *flags,
							tls_c->flags);
ssl_read_skipped:
			;
		}
		if (unlikely(wr.used != 0 && ssl_error != SSL_ERROR_ZERO_RETURN)) {
			TLS_RD_TRACE("(%p, %p) tcpconn_send_unsafe %d bytes\n",
							c, flags, wr.used);
			/* something was written and it's not ssl EOF*/
			if (unlikely(tcpconn_send_unsafe(c->fd, c, (char*)wr.buf,
											wr.used, c->send_flags) < 0)) {
				tls_set_mbufs(c, 0, 0);
				lock_release(&c->write_lock);
				TLS_RD_TRACE("(%p, %p) tcpconn_send_unsafe error\n", c, flags);
				goto error_send;
			}
		}
	/* quickly catch bugs: segfault if accessed and not set */
	tls_set_mbufs(c, 0, 0);
	lock_release(&c->write_lock);
	switch(ssl_error) {
		case SSL_ERROR_NONE:
			if (unlikely(n < 0)) {
				BUG("unexpected SSL_ERROR_NONE for n=%d\n", n);
				goto error;
			}
			break;
		case SSL_ERROR_ZERO_RETURN:
			/* SSL EOF */
			TLS_RD_TRACE("(%p, %p) SSL EOF (fd=%d)\n", c, flags, c->fd);
			goto ssl_eof;
		case SSL_ERROR_WANT_READ:
			TLS_RD_TRACE("(%p, %p) SSL_ERROR_WANT_READ *flags=%d\n",
							c, flags, *flags);
			/* needs to read more data */
			if (unlikely(rd.pos != rd.used)) {
				/* data still in the read buffer */
				BUG("SSL_ERROR_WANT_READ but data still in"
						" the rbio (%p, %d bytes at %d)\n", rd.buf,
						rd.used - rd.pos, rd.pos);
				goto bug;
			}
			if (unlikely((*flags & (RD_CONN_EOF | RD_CONN_SHORT_READ)) == 0) &&
							bytes_free){
				/* there might still be data to read and there is space
				   to decrypt it in tcp_req (no byte has been written into
				    tcp_req in this case) */
				TLS_RD_TRACE("(%p, %p) redo read *flags=%d bytes_free=%d\n",
								c, flags, *flags, bytes_free);
				goto redo_read;
			}
			goto end; /* no more data to read */
		case SSL_ERROR_WANT_WRITE:
			if (wr.used) {
				/* something was written => buffer not big enough to hold
				   everything => reset buffer & retry (the tcp_write already
				   happened if we are here) */
				TLS_RD_TRACE("(%p) SSL_ERROR_WANT_WRITE partial write"
							" (written  %d), retrying\n", c, wr.used);
				goto continue_ssl_read;
			}
			/* else write buffer too small, nothing written */
			BUG("write buffer too small (%d/%d bytes)\n",
						wr.used, wr.size);
			goto bug;
		case SSL_ERROR_SSL:
			/* protocol level error */
			TLS_ERR(err_src);
			goto error;
#if OPENSSL_VERSION_NUMBER >= 0x00907000L /*0.9.7*/
		case SSL_ERROR_WANT_CONNECT:
			/* only if the underlying BIO is not yet connected
			   and the call would block in connect().
			   (not possible in our case) */
			BUG("unexpected SSL_ERROR_WANT_CONNECT\n");
			goto bug;
		case SSL_ERROR_WANT_ACCEPT:
			/* only if the underlying BIO is not yet connected
			   and call would block in accept()
			   (not possible in our case) */
			BUG("unexpected SSL_ERROR_WANT_ACCEPT\n");
			goto bug;
#endif
		case SSL_ERROR_WANT_X509_LOOKUP:
			/* can only appear on client application and it indicates that
			   an installed client cert. callback should be called again
			   (it returned < 0 indicated that it wants to be called
			   later). Not possible in our case */
			BUG("unsupported SSL_ERROR_WANT_X509_LOOKUP");
			goto bug;
		case SSL_ERROR_SYSCALL:
			TLS_ERR_RET(x, err_src);
			if (!x) {
				if (n == 0) {
					WARN("Unexpected EOF\n");
				} else
					/* should never happen */
					BUG("IO error (%d) %s\n", errno, strerror(errno));
			}
			goto error;
		default:
			TLS_ERR(err_src);
			BUG("unexpected SSL error %d\n", ssl_error);
			goto bug;
	}
	if (unlikely(n < 0)) {
		/* here n should always be >= 0 */
		BUG("unexpected value (n = %d)\n", n);
		goto bug;
	}
	if (unlikely(rd.pos != rd.used)) {
		/* encrypted data still in the read buffer (SSL_read() did not
		   consume all of it) */
		if (unlikely(n < 0))
			/* here n should always be >= 0 */
			BUG("unexpected value (n = %d)\n", n);
		else {
			if (unlikely(bytes_free != 0)) {
				/* 2i or 2ip: unconsumed input and output buffer not filled =>
				  retry ssl read (SSL_read() will read will stop at
				  record boundaries, unless readahead==1).
				  No tcp_read() is attempted, since that would reset the
				  current no-yet-consumed input data.
				 */
				TLS_RD_TRACE("(%p, %p) input not fully consumed =>"
								" retry SSL_read"
								" (pos: %d, remaining %d, output free %d)\n",
								c, flags, rd.pos, rd.used-rd.pos, bytes_free);
				goto continue_ssl_read;
			}
			/* 1i or 1ip: bytes_free == 0
			   (unconsumed input, but filled output  buffer) =>
			    queue read data, and exit asking for repeating the call
			    once there is some space in the output buffer.
			 */
			if (likely(!enc_rd_buf)) {
				TLS_RD_TRACE("(%p, %p) creating enc_rd_buf (for %d bytes)\n",
								c, flags, rd.used - rd.pos);
				enc_rd_buf = shm_malloc(sizeof(*enc_rd_buf) -
										sizeof(enc_rd_buf->buf) +
										rd.used - rd.pos);
				if (unlikely(enc_rd_buf == 0)) {
					ERR("memory allocation error (%d bytes requested)\n",
						(int)(sizeof(*enc_rd_buf) + sizeof(enc_rd_buf->buf) +
										rd.used - rd.pos));
					goto error;
				}
				enc_rd_buf->pos = 0;
				enc_rd_buf->size = rd.used - rd.pos;
				memcpy(enc_rd_buf->buf, rd.buf + rd.pos,
										enc_rd_buf->size);
			} else if ((enc_rd_buf->buf + enc_rd_buf->pos) == rd.buf) {
				TLS_RD_TRACE("(%p, %p) enc_rd_buf already in use,"
								" updating pos %d\n",
								c, flags, enc_rd_buf->pos);
				enc_rd_buf->pos += rd.pos;
			} else {
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
	} else if (bytes_free != 0) {
		/*  2f or 2p: input fully consumed (rd.pos == rd.used),
		    output buffer not filled, still possible to have pending
		    data buffered by openssl */
		if (unlikely((*flags & (RD_CONN_EOF|RD_CONN_SHORT_READ)) == 0)) {
			/* still space in the tcp unenc. req. buffer, no SSL_read error,
			   not a short read and not an EOF (possible more data in
			   the socket buffer) => try a new tcp read too */
			TLS_RD_TRACE("(%p, %p) retry read (still space and no short"
							" tcp read: %d)\n", c, flags, *flags);
			goto redo_read;
		} else {
			/* don't tcp_read() anymore, but there might still be data
			   buffered internally by openssl (e.g. if readahead==1) =>
			   retry SSL_read() with the current full input buffer
			   (if no more internally SSL buffered data => WANT_READ => exit).
			 */
			TLS_RD_TRACE("(%p, %p) retry SSL_read only (*flags =%d)\n",
							c, flags, *flags);
			goto continue_ssl_read;
		}
	} else {
		/*   1p or 1f: rd.pos == rd.used && bytes_free == 0
			 (input fully consumed && output buffer filled) */
		/* ask for a repeat when there is more buffer space
		   (there is no definitive way to know if ssl doesn't still have
		    some internal buffered data until we get WANT_READ, see
			SSL_read() comment above) */
		*flags |= RD_CONN_REPEAT_READ;
		TLS_RD_TRACE("(%p, %p) output filled, exit asking to be called again"
						" (*flags =%d)\n", c, flags, *flags);
	}
	
end:
	if (enc_rd_buf)
		shm_free(enc_rd_buf);
	TLS_RD_TRACE("(%p, %p) end => %d (*flags=%d)\n",
					c, flags, ssl_read, *flags);
	return ssl_read;
ssl_eof:
	/* behave as an EOF would have been received at the tcp level */
	if (enc_rd_buf)
		shm_free(enc_rd_buf);
	c->state = S_CONN_EOF;
	*flags |= RD_CONN_EOF;
	TLS_RD_TRACE("(%p, %p) end EOF => %d (*flags=%d)\n",
					c, flags, ssl_read, *flags);
	return ssl_read;
error_send:
error:
bug:
	if (enc_rd_buf)
		shm_free(enc_rd_buf);
	r->error=TCP_READ_ERROR;
	TLS_RD_TRACE("(%p, %p) end error => %d (*flags=%d)\n",
					c, flags, ssl_read, *flags);
	return -1;
}


static int _tls_evrt_connection_out = -1; /* default disabled */

/*!
 * lookup tls event routes
 */
void tls_lookup_event_routes(void)
{
	_tls_evrt_connection_out=route_lookup(&event_rt, "tls:connection-out");
	if (_tls_evrt_connection_out>=0 && event_rt.rlist[_tls_evrt_connection_out]==0)
		_tls_evrt_connection_out=-1; /* disable */
	if(_tls_evrt_connection_out!=-1)
		forward_set_send_info(1);
}

/**
 *
 */
int tls_run_event_routes(struct tcp_connection *c)
{
	int backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t tmsg;

	if(_tls_evrt_connection_out<0)
		return 0;
	if(p_onsend==0 || p_onsend->msg==0)
		return 0;

	backup_rt = get_route_type();
	set_route_type(LOCAL_ROUTE);
	init_run_actions_ctx(&ctx);
	tls_set_pv_con(c);
	run_top_route(event_rt.rlist[_tls_evrt_connection_out], &tmsg, 0);
	tls_set_pv_con(0);
	set_route_type(backup_rt);
	return 0;
}
