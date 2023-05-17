/*
 * TLS module
 *
 * Copyright (C) 2007 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio TLS support :: Locking
 * \ingroup tls
 * Module: \ref tls
 */


#include <stdlib.h> /* abort() */
#include <openssl/crypto.h>
#include "../../core/dprint.h"
#include "../../core/locking.h"

static int n_static_locks = 0;
static gen_lock_set_t *static_locks = 0;

/* OpenSSL is thread-safe since 1.1.0 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

/* "dynamic" locks */

struct CRYPTO_dynlock_value
{
	gen_lock_t lock;
};


static struct CRYPTO_dynlock_value *dyn_create_f(const char *file, int line)
{
	struct CRYPTO_dynlock_value *l;

	l = shm_malloc(sizeof(struct CRYPTO_dynlock_value));
	if(l == 0) {
		LM_CRIT("dyn create locking callback out of shm."
				" memory (called from %s:%d)\n",
				file, line);
		goto error;
	}
	if(lock_init(&l->lock) == 0) {
		LM_CRIT("dyn create locking callback: lock "
				"initialization failed (called from %s:%d)\n",
				file, line);
		shm_free(l);
		goto error;
	}
	return l;
error:
	return 0;
}


static void dyn_lock_f(
		int mode, struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	if(l == 0) {
		LM_CRIT("dyn lock locking callback: null lock"
				" (called from %s:%d)\n",
				file, line);
		/* try to continue */
		return;
	}
	if(mode & CRYPTO_LOCK) {
		lock_get(&l->lock);
	} else {
		lock_release(&l->lock);
	}
}


static void dyn_destroy_f(
		struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	if(l == 0) {
		LM_CRIT("dyn destroy locking callback: null lock"
				" (called from %s:%d)\n",
				file, line);
		return;
	}
	lock_destroy(&l->lock);
	shm_free(l);
}


/* normal locking callback */
static void locking_f(int mode, int n, const char *file, int line)
{
	if(n < 0 || n >= n_static_locks) {
		LM_CRIT("locking (callback): invalid lock number: "
				" %d (range 0 - %d), called from %s:%d\n",
				n, n_static_locks, file, line);
		abort(); /* quick crash :-) */
	}
	if(mode & CRYPTO_LOCK) {
#ifdef EXTRA_DEBUG
		LM_DBG("lock get (%d): %d (%s:%d)\n", mode, n, file, line);
#endif
		lock_set_get(static_locks, n);
	} else {
		lock_set_release(static_locks, n);
#ifdef EXTRA_DEBUG
		LM_DBG("lock release (%d): %d (%s:%d)\n", mode, n, file, line);
#endif
	}
}

#endif /* openssl < 0x10100000L (1.1.0) or LibreSSL */


void tls_destroy_locks()
{
	if(static_locks) {
		lock_set_destroy(static_locks);
		lock_set_dealloc(static_locks);
		static_locks = 0;
		n_static_locks = 0;
	}
}


unsigned long sr_ssl_id_f()
{
	return my_pid();
}

/* returns -1 on error, 0 on success */
int tls_init_locks()
{
/* OpenSSL is no longer supporting to set locking callbacks since 1.1.0 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	/* init "static" tls locks */
	n_static_locks = CRYPTO_num_locks();
	if(n_static_locks < 0) {
		LM_CRIT("bad CRYPTO_num_locks %d\n", n_static_locks);
		n_static_locks = 0;
	}
	if(n_static_locks) {
		if(CRYPTO_get_locking_callback() != NULL) {
			LM_CRIT("ssl locking callback already set\n");
			return -1;
		}
		static_locks = lock_set_alloc(n_static_locks);
		if(static_locks == 0) {
			LM_CRIT("could not allocate lockset with %d locks\n",
					n_static_locks);
			goto error;
		}
		if(lock_set_init(static_locks) == 0) {
			LM_CRIT("lock set init failed (%d locks)\n", n_static_locks);
			lock_set_dealloc(static_locks);
			static_locks = 0;
			n_static_locks = 0;
			goto error;
		}
		CRYPTO_set_locking_callback(locking_f);
	}

	/* set "dynamic" locks callbacks */
	CRYPTO_set_dynlock_create_callback(dyn_create_f);
	CRYPTO_set_dynlock_lock_callback(dyn_lock_f);
	CRYPTO_set_dynlock_destroy_callback(dyn_destroy_f);

	/* starting with v1.0.0 openssl does not use anymore getpid(), but address
	 * of errno which can point to same virtual address in a multi-process
	 * application
	 * - for refrence http://www.openssl.org/docs/crypto/threads.html
	 */
	CRYPTO_set_id_callback(sr_ssl_id_f);

	/* atomic add -- since for now we don't have atomic_add
	 *  (only atomic_inc), fallback to the default use-locks mode
	 * CRYPTO_set_add_lock_callback(atomic_add_f);
	 */

	return 0;
error:
	tls_destroy_locks();
	return -1;

#else
	return 0;
#endif
}
