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

#ifndef __tls_ct_wrq_h
#define __tls_ct_wrq_h

#include "tls_ct_q.h"
#include "tls_server.h"
#include "../../tcp_conn.h"



/**
 * @brief Init clear text write queues support
 * @return 0 on success, < 0 on error.
 */
int tls_ct_wq_init();

/**
 * @brief Destroy clear text write queues support
 */
void tls_ct_wq_destroy();


/**
 * @brief Total number of written queued bytes in all the SSL connections
 * @return total number of written queued bytes in all SSL connections
 */
unsigned int tls_ct_wq_total_bytes();

#define tls_ct_wq_empty(tc_q) (*(tc_q)==0 || (*(tc_q))->first==0)
#define tls_ct_wq_non_empty(bq) (*(tc_q) && (*(tc_q))->first!=0)

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
int tls_ct_wq_flush(struct tcp_connection* c, tls_ct_q** tc_q,
					int* flags, int* ssl_err);

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
int tls_ct_wq_add(tls_ct_q** ct_q, const void* data, unsigned int size);

/**
 * @brief Wrapper over tls_ct_q_destroy()
 * Wrapper over tls_ct_q_destroy(), besides doing a tls_ct_q_destroy it
 * also keeps track of the total queued bytes.
 * @param ct_q clear text queue
 * @return number of bytes that used to be queued (>=0),
 */
unsigned int tls_ct_wq_free(tls_ct_q** ct_q);

#endif /*__tls_ct_wrq_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
