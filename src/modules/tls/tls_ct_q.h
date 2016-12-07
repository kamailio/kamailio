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
 * TLS clear text queue (wrappers over sbufq)
 * (e.g. queue clear text when SSL_write() cannot write it immediately due to
 * re-keying).
 * @file
 * @ingroup tls
 * Module: @ref tls
 */

#ifndef __tls_ct_q_h
#define __tls_ct_q_h

#include "sbufq.h"
#include "../../compiler_opt.h"

typedef struct sbuffer_queue tls_ct_q;


#define tls_ct_q_empty(bq) ((bq)==0 || (bq)->first==0)
#define tls_ct_q_non_empty(bq) ((bq) && (bq)->first!=0)


/**
 * @brief Adds/appends data to a tls clear text buffer queue
 * @warning it does no attempt to synchronize access/lock. If needed it should
 * be called under lock.
 * @param **ct_q - double pointer to the buffer queue
 * @param data
 * @param size
 * @param min_buf_size - min size to allocate for new buffer elements
 * @return 0 on success, -1 on error (mem. allocation)
 */
inline static int tls_ct_q_add(tls_ct_q** ct_q, const void* data,
								unsigned int size, unsigned int min_buf_size)
{
	tls_ct_q* q;
	
	q = *ct_q;
	if (likely(q == 0)){
		q=shm_malloc(sizeof(tls_ct_q));
		if (unlikely(q==0))
			goto error;
		memset(q, 0, sizeof(tls_ct_q));
		*ct_q = q;
	}
	return sbufq_add(q, data, size, min_buf_size);
error:
	return -1;
}



/**
 * @brief Destroy a buffer queue
 * 
 * Everything is destroyed from a buffer queue (shm_free()'d), included the queue head.
 * @warning it does no attempt to synchronize access/lock. If needed it should
 * be called under lock.
 * @param **ct_q - double pointer to the queue
 * @return - number of bytes that used to be queued (>=0).
 */
inline static unsigned int tls_ct_q_destroy(tls_ct_q** ct_q)
{
	unsigned int ret;
	
	ret = 0;
	if (likely(ct_q && *ct_q)) {
		ret = sbufq_destroy(*ct_q);
		shm_free(*ct_q);
		*ct_q = 0;
	}
	return ret;
}



/**
 * @brief Tries to flush the tls clear text queue
 * 
 * Tries to flush as much as possible from the given queue, using the 
 * given callback.
 * @warning it does no attempt to synchronize access/lock. If needed it should
 * be called under lock.
 * @param tc_q - buffer queue
 * @param *flags - set to:
 *                   F_BUFQ_EMPTY if the queued is completely flushed
 *                   F_BUFQ_FLUSH_ERR if the flush_f callback returned error.
 * @param flush_f - flush function (callback). modeled after write():
 *                    flush_f(flush_p, const void* buf, unsigned size).
 *                    It should return the number of bytes "flushed" on
 *                    success, or <0 on error. If the number of bytes
 *                    "flushed" is smaller then the requested size, it
 *                    would be assumed that no more bytes can be flushed
 *                    and sbufq_flush will exit.
 * @param flush_p1 - parameter for the flush function callback.
 * @param flush_p2 - parameter for the flush function callback.
 * @return -1 on internal error, or the number of bytes flushed on
 *            success (>=0). Note that the flags param is
 *            always set and it should be used to check for errors, since
 *            a flush_f() failure will not result in a negative return.
 */
inline static int tls_ct_q_flush(tls_ct_q** tc_q, int* flags,
								int (*flush_f)(void* p1, void* p2,
												const void* buf,
												unsigned size),
								void* flush_p1, void* flush_p2)
{
	return *tc_q?sbufq_flush(*tc_q, flags, flush_f, flush_p1, flush_p2):0;
}



#endif /*__tls_ct_q_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
