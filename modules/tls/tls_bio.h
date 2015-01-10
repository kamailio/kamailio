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
 
/** openssl BIOs for reading/writing via a fixed memory buffer.
 * @file modules/tls/tls_bio.h
 * @ingroup tls
 */
 
#ifndef __tls_bio_h
#define __tls_bio_h

#include <openssl/bio.h>

/* memory buffer used for tls I/O */
struct tls_mbuf {
	unsigned char* buf;
	int pos;  /**< current position in the buffer while reading or writing*/
	int used; /**< how much it's used  (read or write)*/
	int size; /**< total buffer size (fixed) */
};

struct tls_bio_mbuf_data {
	struct tls_mbuf* rd;
	struct tls_mbuf* wr;
};


BIO_METHOD* tls_BIO_mbuf(void);
BIO* tls_BIO_new_mbuf(struct tls_mbuf* rd, struct tls_mbuf* wr);
int tls_BIO_mbuf_set(BIO* b, struct tls_mbuf* rd, struct tls_mbuf* wr);



/** intialize an mbuf structure.
 * @param mb - struct tls_mbuf pointer that will be intialized.
 * @param b  - buffer (unsigned char*).
 * @param sz - suze of the buffer (int).
 * WARNING: the buffer will not be copied, but referenced.
 */
#define tls_mbuf_init(mb, b, sz) \
	do { \
		(mb)->buf = (b); \
		(mb)->size = (sz); \
		(mb)->pos = 0; \
		(mb)->used = 0; \
	} while(0)



#endif /*__tls_bio_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
