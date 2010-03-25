/* 
 * $Id$
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
/** openssl BIOs for reading/writing via a fixed memory buffer.
 * @file /home/andrei/sr.git/modules/tls/tls_bio.c
 * @ingroup tls
 * @Module: @ref tls
 */
/*
 * History:
 * --------
 *  2010-03-25  initial version (andrei)
*/

#include "tls_bio.h"
#include "../../compiler_opt.h"
#include "../../dprint.h"
#include "../../ut.h"

/* 0xf2 should be unused (as of openssl 1.0.0 max.
   internal defined BIO is 23) */
#define BIO_TYPE_TLS_MBUF	(BIO_TYPE_SOURCE_SINK | 0xf2)


static int tls_bio_mbuf_new(BIO* b);
static int tls_bio_mbuf_free(BIO* b);
static int tls_bio_mbuf_write(BIO* b, const char* buf, int num);
static int tls_bio_mbuf_read(BIO* b, char* buf, int num);
static int tls_bio_mbuf_puts(BIO* b, const char* s);
static long tls_bio_mbuf_ctrl(BIO* b, int cmd, long arg1, void* arg2);


static BIO_METHOD tls_mbuf_method = {
	BIO_TYPE_TLS_MBUF,	/* type */
	"sr_tls_mbuf",		/* name */
	tls_bio_mbuf_write,	/* write function */
	tls_bio_mbuf_read,	/* read function */
	tls_bio_mbuf_puts,	/* puts function */
	0,					/* gets function */
	tls_bio_mbuf_ctrl,	/* ctrl function */
	tls_bio_mbuf_new,	/* create(new) function */
	tls_bio_mbuf_free,	/* destroy(free) function */
	0					/* ctrl callback */
};


/** returns a custom tls_mbuf BIO. */
BIO_METHOD* tls_BIO_mbuf(void)
{
	return &tls_mbuf_method;
}



/** create an initialize a new tls_BIO_mbuf.
 * @return new BIO on success (!=0), 0 on error.
 */
BIO* tls_BIO_new_mbuf(struct tls_mbuf* rd, struct tls_mbuf* wr)
{
	BIO* ret;
	
	ret = BIO_new(tls_BIO_mbuf());
	if (unlikely(ret == 0))
		return 0;
	if (unlikely(tls_BIO_mbuf_set(ret, rd, wr) == 0)) {
		BIO_free(ret);
		return 0;
	}
	return ret;
}



/** sets the read and write mbuf for an  mbuf BIO.
 * @return 1 on success, 0 on error (openssl BIO convention).
 */
int tls_BIO_mbuf_set(BIO* b, struct tls_mbuf* rd, struct tls_mbuf* wr)
{
	struct tls_bio_mbuf_data* d;
	
	if (unlikely(b->ptr == 0)){
		BUG("null BIO ptr\n");
		return 0;
	}
	d = b->ptr;
	d->rd = rd;
	d->wr = wr;
	b->init = 1;
	return 1;
}



/** create a new BIO.
 * (internal openssl use via the tls_mbuf method)
 * @return 1 on success, 0 on error.
 */
static int tls_bio_mbuf_new(BIO* b)
{
	struct tls_bio_mbuf_data* d;
	
	b->init = 0; /* not initialized yet */
	b->num = 0;
	b->ptr = 0;
	b->flags = 0;
	d = OPENSSL_malloc(sizeof(*d));
	if (unlikely(d == 0))
		return 0;
	d->rd = 0;
	d->wr = 0;
	b->ptr = d;
	return 1;
}



/** destroy a tls mbuf BIO.
 * (internal openssl use via the tls_mbuf method)
 * @return 1 on success, 0 on error.
 */
static int tls_bio_mbuf_free(BIO* b)
{
	if (unlikely( b == 0))
			return 0;
	if (likely(b->ptr)){
		OPENSSL_free(b->ptr);
		b->ptr = 0;
		b->init = 0;
	}
	return 1;
}



/** read from a mbuf.
 * (internal openssl use via the tls_mbuf method)
 * @return bytes read on success (0< ret <=dst_len), -1 on empty buffer & sets
 *  should_retry_read, -1 on some other errors (w/o should_retry_read set).
 */
static int tls_bio_mbuf_read(BIO* b, char* dst, int dst_len)
{
	struct tls_bio_mbuf_data* d;
	struct tls_mbuf* rd;
	int ret;
	
	ret = 0;
	if (likely(dst)) {
		d= b->ptr;
		BIO_clear_retry_flags(b);
		if (unlikely(d == 0 || d->rd->buf == 0)) {
			if (d == 0)
				BUG("tls_BIO_mbuf %p: read called with null b->ptr\n", b);
			else
				BUG("tls_BIO_mbuf %p: read called with null read buffer\n", b);
			return -1;
		}
		rd = d->rd;
		if (unlikely(rd->used == rd->pos && dst_len)) {
			/* mimic non-blocking read behaviour */
			BIO_set_retry_read(b);
			return -1;
		}
		ret = MIN_int(rd->used - rd->pos, dst_len);
		/* copy data from rd.buf into dst */
		memcpy(rd->buf+rd->pos, dst, ret);
		rd->pos += ret;
/*		if (unlikely(rd->pos < rd->used))
			BIO_set_retry_read(b);
*/
	}
	return ret;
}



/** write to a mbuf.
 * (internal openssl use via the tls_mbuf method)
 * @return bytes written on success (0<= ret <=src_len), -1 on error or buffer
 * full (in this case sets should_retry_write).
 */
static int tls_bio_mbuf_write(BIO* b, const char* src, int src_len)
{
	struct tls_bio_mbuf_data* d;
	struct tls_mbuf* wr;
	int ret;
	
	ret = 0;
	d= b->ptr;
	BIO_clear_retry_flags(b);
	if (unlikely(d == 0 || d->wr->buf == 0)) {
		if (d == 0)
			BUG("tls_BIO_mbuf %p: write called with null b->ptr\n", b);
		else
			BUG("tls_BIO_mbuf %p: write called with null read buffer\n", b);
		return -1;
	}
	wr = d->wr;
	if (unlikely(wr->size == wr->used && src_len)) {
		/* mimic non-blocking socket behaviour */
		BIO_set_retry_write(b);
		return -1;
	}
	ret = MIN_int(wr->size - wr->used, src_len);
	memcpy(wr->buf + wr->used, src, ret);
	wr->used += ret;
/*	if (unlikely(ret < src_len))
		BIO_set_retry_write();
*/
	return ret;
}



static long tls_bio_mbuf_ctrl(BIO* b, int cmd, long arg1, void* arg2)
{
	/* no cmd supported */
	return 0;
}



static int tls_bio_mbuf_puts(BIO* b, const char* s)
{
	int len;
	
	len=strlen(s);
	return tls_bio_mbuf_write(b, s, len);
}



/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
