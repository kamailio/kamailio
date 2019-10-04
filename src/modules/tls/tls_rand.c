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



#include <stdlib.h>
#include <string.h>

#include "tls_rand.h"

#if OPENSSL_VERSION_NUMBER >= 0x10100000L

#include "../../core/dprint.h"
#include "../../core/rand/kam_rand.h"
#include "../../core/rand/fastrand.h"
#include "fortuna/random.h"

static int ksr_krand_bytes(unsigned char *outdata, int size)
{
	int r;

	if (size < 0) {
		return 0;
	} else if (size == 0) {
		return 1;
	}

/* TODO
	sr_get_pseudo_random_bytes(outdata, size);
	return 1;
*/

	while(size >= sizeof(int)) {
		r = kam_rand();
		memcpy(outdata, &r, sizeof(int));
		size -= sizeof(int);
		outdata += sizeof(int);
	}
	if(size>0) {
		r = kam_rand();
		memcpy(outdata, &r, size);
	}
	return 1;
}

static int ksr_krand_pseudorand(unsigned char *outdata, int size)
{
    return ksr_krand_bytes(outdata, size);
}

static int ksr_krand_status(void)
{
    return 1;
}

const RAND_METHOD _ksr_krand_method = {
    NULL,
    ksr_krand_bytes,
    NULL,
    NULL,
    ksr_krand_pseudorand,
    ksr_krand_status
};

const RAND_METHOD *RAND_ksr_krand_method(void)
{
    return &_ksr_krand_method;
}

static int ksr_fastrand_bytes(unsigned char *outdata, int size)
{
	int r;

	if (size < 0) {
		return 0;
	} else if (size == 0) {
		return 1;
	}

	while(size >= sizeof(int)) {
		r = fastrand();
		memcpy(outdata, &r, sizeof(int));
		size -= sizeof(int);
		outdata += sizeof(int);
	}
	if(size>0) {
		r = fastrand();
		memcpy(outdata, &r, size);
	}
	return 1;
}

static int ksr_fastrand_pseudorand(unsigned char *outdata, int size)
{
    return ksr_fastrand_bytes(outdata, size);
}

static int ksr_fastrand_status(void)
{
    return 1;
}

const RAND_METHOD _ksr_fastrand_method = {
    NULL,
    ksr_fastrand_bytes,
    NULL,
    NULL,
    ksr_fastrand_pseudorand,
    ksr_fastrand_status
};

const RAND_METHOD *RAND_ksr_fastrand_method(void)
{
    return &_ksr_fastrand_method;
}

#endif /* OPENSSL_VERSION_NUMBER >= 0x10100000L */
