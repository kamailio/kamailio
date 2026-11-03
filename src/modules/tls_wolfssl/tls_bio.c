/*
 * Kamailio TLS module
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

/** wolfSSL I/O callbacks reading/writing via fixed memory buffers.
 * The native wolfSSL I/O callbacks (CallbackIORecv/CallbackIOSend) take the
 * place of the tls module's custom BIO: wolfSSL reads ciphertext straight
 * from the tcp read buffer and writes it straight into the output buffer.
 * The callback contract is the same from wolfSSL 4.6.0 on.
 * @file modules/tls_wolfssl/tls_bio.c
 * @ingroup tls
 */

#include <string.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfio.h>

#include "tls_bio.h"
#include "../../core/compiler_opt.h"
#include "../../core/ut.h"

/** read from the current read mbuf.
 * @return bytes read (0 < ret <= sz), WOLFSSL_CBIO_ERR_WANT_READ when no
 *  read buffer is set or it is empty (mimics a non-blocking socket)
 */
static int tls_bio_mbuf_recv(WOLFSSL *ssl, char *buf, int sz, void *ctx)
{
	struct tls_bio_mbuf_data *d = (struct tls_bio_mbuf_data *)ctx;
	struct tls_mbuf *rd;
	int ret;

	(void)ssl;
	if(unlikely(d == NULL || d->rd == NULL || d->rd->buf == NULL))
		return WOLFSSL_CBIO_ERR_WANT_READ;
	rd = d->rd;
	if(unlikely(rd->used == rd->pos || sz <= 0))
		return WOLFSSL_CBIO_ERR_WANT_READ;
	ret = MIN_int(rd->used - rd->pos, sz);
	memcpy(buf, rd->buf + rd->pos, ret);
	rd->pos += ret;
	return ret;
}

/** write to the current write mbuf.
 * @return bytes written (0 < ret <= sz), WOLFSSL_CBIO_ERR_WANT_WRITE when no
 *  write buffer is set or it is full (mimics a non-blocking socket)
 */
static int tls_bio_mbuf_send(WOLFSSL *ssl, char *buf, int sz, void *ctx)
{
	struct tls_bio_mbuf_data *d = (struct tls_bio_mbuf_data *)ctx;
	struct tls_mbuf *wr;
	int ret;

	(void)ssl;
	if(unlikely(d == NULL || d->wr == NULL || d->wr->buf == NULL))
		return WOLFSSL_CBIO_ERR_WANT_WRITE;
	wr = d->wr;
	if(unlikely(wr->size == wr->used || sz <= 0))
		return WOLFSSL_CBIO_ERR_WANT_WRITE;
	ret = MIN_int(wr->size - wr->used, sz);
	memcpy(wr->buf + wr->used, buf, ret);
	wr->used += ret;
	return ret;
}

void tls_bio_mbuf_attach(WOLFSSL *ssl, struct tls_bio_mbuf_data *d)
{
	wolfSSL_SSLSetIORecv(ssl, tls_bio_mbuf_recv);
	wolfSSL_SSLSetIOSend(ssl, tls_bio_mbuf_send);
	wolfSSL_SetIOReadCtx(ssl, d);
	wolfSSL_SetIOWriteCtx(ssl, d);
}
