/* 
 * TLS module
 *
 * Copyright (C) 2010 iptelorg GmbH
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

/**
 * tls clear text write queue.
 * (queue clear text when SSL_write() cannot write it immediately due to
 * re-keying).
 * @file
 * @ingroup tls
 * Module: @ref tls
 */

#include "tls_ct_wrq.h"
#include "tls_cfg.h"
#include "tls_server.h"
#include "../../atomic_ops.h"
#include "../../mem/shm_mem.h"
#include <openssl/err.h>
#include <openssl/ssl.h>


atomic_t* tls_total_ct_wq; /* total clear text bytes queued for a future
							  SSL_write() (due to renegotiations/
							  SSL_WRITE_WANTS_READ ).*/



/**
 * @brief Init clear text write queues support
 * @return 0 on success, < 0 on error.
 */
int tls_ct_wq_init()
{
	tls_total_ct_wq = shm_malloc(sizeof(*tls_total_ct_wq));
	if (unlikely(tls_total_ct_wq == 0))
		return -1;
	atomic_set(tls_total_ct_wq, 0);
	return 0;
}



/**
 * @brief Destroy clear text write queues support
 */
void tls_ct_wq_destroy()
{
	if (tls_total_ct_wq) {
		shm_free(tls_total_ct_wq);
		tls_total_ct_wq = 0;
	}
}



/**
 * @brief Total number of written queued bytes in all the SSL connections
 * @return total number of written queued bytes in all SSL connections
 */
unsigned int tls_ct_wq_total_bytes()
{
	return (unsigned)atomic_get(tls_total_ct_wq);
}



/**
 * @brief Callback for tls_ct_q_flush()
 *
 * @param tcp_c TCP connection containing the SSL context
 * @param error error reason (set on exit)
 * @param buf buffer
 * @param size buffer size
 * @return >0 on success (bytes written), <=0 on ssl error (should be
 * handled outside)
 * @warning the SSL context must have the wbio and rbio previously set!
 */
static int ssl_flush(void* tcp_c, void* error, const void* buf, unsigned size)
{
	int n;
	int ssl_error;
	struct tls_extra_data* tls_c;
	SSL* ssl;
	
	tls_c = ((struct tcp_connection*)tcp_c)->extra_data;
	ssl = tls_c->ssl;
	ssl_error = SSL_ERROR_NONE;
	if (unlikely(tls_c->state == S_TLS_CONNECTING)) {
		n = tls_connect(tcp_c, &ssl_error);
		if (unlikely(n>=1)) {
			n = SSL_write(ssl, buf, size);
			if (unlikely(n <= 0))
				ssl_error = SSL_get_error(ssl, n);
		}
	} else if (unlikely(tls_c->state == S_TLS_ACCEPTING)) {
		n = tls_accept(tcp_c, &ssl_error);
		if (unlikely(n>=1)) {
			n = SSL_write(ssl, buf, size);
			if (unlikely(n <= 0))
				ssl_error = SSL_get_error(ssl, n);
		}
	} else {
		n = SSL_write(ssl, buf, size);
		if (unlikely(n <= 0))
			ssl_error = SSL_get_error(ssl, n);
	}
	
	*(long*)error = ssl_error;
	return n;
}



/**
 * @brief Wrapper over tls_ct_q_flush()
 * 
 * Wrapper over tls_ct_q_flush(), besides doing a tls_ct_q_add it
 * also keeps track of queue size and total queued bytes.
 * @param c TCP connection
 * @param ct_q clear text queue
 * @param flags filled, @see tls_ct_q_add() for more details.
 * @param ssl_err set to the SSL err (SSL_ERROR_NONE on full success).
 * @return -1 on internal error, or the number of bytes flushed on success
 *         (>=0).
 */
int tls_ct_wq_flush(struct tcp_connection* c, tls_ct_q** ct_q,
					int* flags, int* ssl_err)
{
	int ret;
	long error;
	
	error = SSL_ERROR_NONE;
	ret = tls_ct_q_flush(ct_q,  flags, ssl_flush, c, &error);
	*ssl_err = (int)error;
	if (likely(ret > 0))
		atomic_add(tls_total_ct_wq, -ret);
	return ret;
}



/**
 * @brief Wrapper over tls_ct_q_add()
 * 
 * Wrapper over tls_ct_q_add(), besides doing a tls_ct_q_add it
 * also keeps track of queue size and total queued bytes.
 * If the maximum queue size is exceeded => error.
 * @param ct_q clear text queue
 * @param data data
 * @param size data size
 * @return 0 on success, < 0 on error (-1 memory allocation, -2 queue size
 *         too big).
 */
int tls_ct_wq_add(tls_ct_q** ct_q, const void* data, unsigned int size)
{
	int ret;
	
	if (unlikely( (*ct_q && (((*ct_q)->queued + size) >
						cfg_get(tls, tls_cfg, con_ct_wq_max))) ||
				(atomic_get(tls_total_ct_wq) + size) >
						cfg_get(tls, tls_cfg, ct_wq_max))) {
		return -2;
	}
	ret = tls_ct_q_add(ct_q, data, size,
						cfg_get(tls, tls_cfg, ct_wq_blk_size));
	if (likely(ret >= 0))
		atomic_add(tls_total_ct_wq, size);
	return ret;
}



/**
 * @brief Wrapper over tls_ct_q_destroy()
 * Wrapper over tls_ct_q_destroy(), besides doing a tls_ct_q_destroy it
 * also keeps track of the total queued bytes.
 * @param ct_q clear text queue
 * @return number of bytes that used to be queued (>=0),
 */
unsigned int tls_ct_wq_free(tls_ct_q** ct_q)
{
	unsigned int ret;
	
	if (likely((ret = tls_ct_q_destroy(ct_q)) > 0))
		atomic_add(tls_total_ct_wq, -ret);
	return ret;
}


/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
