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

/*
 * OpenSSL docs:
 * https://www.openssl.org/docs/man1.1.1/man7/RAND.html
 * https://www.openssl.org/docs/man1.1.1/man3/RAND_set_rand_method.html
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "tls_rand.h"

#if OPENSSL_VERSION_NUMBER >= 0x10100000L

#include "../../core/dprint.h"
#include "../../core/locking.h"
#include "../../core/mem/shm.h"
#include "../../core/rand/kam_rand.h"
#include "../../core/rand/fastrand.h"
#include "../../core/rand/fortuna/random.h"

/*
 * Implementation for tests with system library PNRG,
 * do not use this in production.
 */
static int ksr_krand_bytes(unsigned char *outdata, int size)
{
	int r;

	if (size < 0) {
		return 0;
	} else if (size == 0) {
		return 1;
	}

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

/*
 * Implementation for tests with fastrand implementation,
 * better as system library but still not secure enough.
 * Do not use this in production.y
 */
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

/*
 * Implementation with Fortuna cryptographic PRNG.
 * We are not strictly implementing the OpenSSL API here - we will
 * not return an error if the PRNG has not been seeded with enough
 * randomness to ensure an unpredictable byte sequence.
 */
static int ksr_cryptorand_bytes(unsigned char *outdata, int size)
{
	if (size < 0) {
		return 0;
	} else if (size == 0) {
		return 1;
	}

	sr_get_pseudo_random_bytes(outdata, size);
	return 1;
}

static int ksr_cryptorand_status(void)
{
    return 1;
}

/*
 * We don't have a dedicated function for pseudo-random
 * bytes, just use the secure version as well for it.
 */
const RAND_METHOD _ksr_cryptorand_method = {
    NULL,
    ksr_cryptorand_bytes,
    NULL,
    NULL,
    ksr_cryptorand_bytes,
    ksr_cryptorand_status
};

const RAND_METHOD *RAND_ksr_cryptorand_method(void)
{
    return &_ksr_cryptorand_method;
}

/**
 * PRNG by using default libssl random engine protected by lock
 */
static int _ksr_kxlibssl_local_pid = 0;
gen_lock_t *_ksr_kxlibssl_local_lock = 0;
const RAND_METHOD *_ksr_kxlibssl_local_method = 0;

void ksr_kxlibssl_init(void)
{
	int mypid;

	if(_ksr_kxlibssl_local_method == NULL) {
		_ksr_kxlibssl_local_method = RAND_get_rand_method();
	}
	mypid = getpid();
	if(_ksr_kxlibssl_local_lock==NULL || _ksr_kxlibssl_local_pid!=mypid) {
		_ksr_kxlibssl_local_lock = lock_alloc();
		if(_ksr_kxlibssl_local_lock == 0) {
			LM_ERR("failed to allocate the lock\n");
			return;
		}
		lock_init(_ksr_kxlibssl_local_lock);
		_ksr_kxlibssl_local_pid = mypid;
		LM_DBG("lock initialized for pid: %d\n", mypid);
	}
}

#ifdef KSRTLSVOID
void ksr_kxlibssl_seed(const void *buf, int num)
{
	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return;
	}
	if(_ksr_kxlibssl_local_method->seed == NULL) {
		return;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	_ksr_kxlibssl_local_method->seed(buf, num);
	lock_release(_ksr_kxlibssl_local_lock);
}
#else
int ksr_kxlibssl_seed(const void *buf, int num)
{
	int ret;

	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return 0;
	}
	if(_ksr_kxlibssl_local_method->seed == NULL) {
		return 0;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	ret = _ksr_kxlibssl_local_method->seed(buf, num);
	lock_release(_ksr_kxlibssl_local_lock);
	return ret;
}
#endif

int ksr_kxlibssl_bytes(unsigned char *buf, int num)
{
	int ret;

	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return 0;
	}
	if(_ksr_kxlibssl_local_method->bytes == NULL) {
		return 0;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	ret = _ksr_kxlibssl_local_method->bytes(buf, num);
	lock_release(_ksr_kxlibssl_local_lock);
	return ret;
}

void ksr_kxlibssl_cleanup(void)
{
	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return;
	}
	if(_ksr_kxlibssl_local_method->cleanup == NULL) {
		return;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	_ksr_kxlibssl_local_method->cleanup();
	lock_release(_ksr_kxlibssl_local_lock);
}

#ifdef KSRTLSVOID
void ksr_kxlibssl_add(const void *buf, int num, int randomness)
{
	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return;
	}
	if(_ksr_kxlibssl_local_method->add == NULL) {
		return;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	_ksr_kxlibssl_local_method->add(buf, num, randomness);
	lock_release(_ksr_kxlibssl_local_lock);
}
#else
int ksr_kxlibssl_add(const void *buf, int num, double randomness)
{
	int ret;

	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return 0;
	}
	if(_ksr_kxlibssl_local_method->add == NULL) {
		return 0;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	ret = _ksr_kxlibssl_local_method->add(buf, num, randomness);
	lock_release(_ksr_kxlibssl_local_lock);

	return ret;
}
#endif

int ksr_kxlibssl_pseudorand(unsigned char *buf, int num)
{
	int ret;

	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return 0;
	}
	if(_ksr_kxlibssl_local_method->pseudorand == NULL) {
		return 0;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	ret = _ksr_kxlibssl_local_method->pseudorand(buf, num);
	lock_release(_ksr_kxlibssl_local_lock);
	return ret;
}

int ksr_kxlibssl_status(void)
{
	int ret;

	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return 0;
	}
	if(_ksr_kxlibssl_local_method->status == NULL) {
		return 0;
	}
	lock_get(_ksr_kxlibssl_local_lock);
	ret = _ksr_kxlibssl_local_method->status();
	lock_release(_ksr_kxlibssl_local_lock);
	return ret;
}

static RAND_METHOD _ksr_kxlibssl_method = {0};

const RAND_METHOD *RAND_ksr_kxlibssl_method(void)
{
	ksr_kxlibssl_init();
	if(_ksr_kxlibssl_local_lock == 0 || _ksr_kxlibssl_local_method == 0) {
		return 0;
	}

	if(_ksr_kxlibssl_local_method->seed != NULL) {
		_ksr_kxlibssl_method.seed = ksr_kxlibssl_seed;
	}
	if(_ksr_kxlibssl_local_method->bytes != NULL) {
		_ksr_kxlibssl_method.bytes = ksr_kxlibssl_bytes;
	}
	if(_ksr_kxlibssl_local_method->cleanup != NULL) {
		_ksr_kxlibssl_method.cleanup = ksr_kxlibssl_cleanup;
	}
	if(_ksr_kxlibssl_local_method->add != NULL) {
		_ksr_kxlibssl_method.add = ksr_kxlibssl_add;
	}
	if(_ksr_kxlibssl_local_method->pseudorand != NULL) {
		_ksr_kxlibssl_method.pseudorand = ksr_kxlibssl_pseudorand;
	}
	if(_ksr_kxlibssl_local_method->status != NULL) {
		_ksr_kxlibssl_method.status = ksr_kxlibssl_status;
	}

    return &_ksr_kxlibssl_method;
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x10100000L */
