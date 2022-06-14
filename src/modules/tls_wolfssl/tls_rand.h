/*
 * TLS module
 *
 * Copyright (C) 2019 Asipto GmbH
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


#ifndef _TLS_RAND_H_
#define _TLS_RAND_H_
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

const WOLFSSL_RAND_METHOD *RAND_ksr_krand_method(void);
const WOLFSSL_RAND_METHOD *RAND_ksr_fastrand_method(void);
const WOLFSSL_RAND_METHOD *RAND_ksr_cryptorand_method(void);
// WOLFFIX const WOLFSSL_RAND_METHOD *RAND_ksr_kxlibssl_method(void);

#endif
