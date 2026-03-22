/*
 * TLS module
 *
 * Copyright (C) 2005 iptelorg GmbH
 * Copyright (C) 2013 Motorola Solutions, Inc.
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
 * \brief Kamailio TLS support :: Common functions
 * \ingroup tls
 * Module: \ref tls
 */


#define _GNU_SOURCE 1 /* Needed for strndup */

#include <string.h>
#include <libgen.h>
#include "../../core/mem/shm_mem.h"
#include "../../core/globals.h"
#include "../../core/dprint.h"
#include "tls_wolfssl_mod.h"
#include "tls_util.h"

/* compatibility for < v5.5.2-stable */
#if LIBWOLFSSL_VERSION_HEX < 0x05005002
#define wolfSSL_sk_X509_new_null wolfSSL_sk_X509_new
#endif

/*
 * Make a shared memory copy of ASCII zero terminated string
 * Return value: -1 on error
 *                0 on success
 */
int shm_asciiz_dup(char **dest, char *val)
{
	char *ret;
	int len;

	if(!val) {
		*dest = NULL;
		return 0;
	}

	len = strlen(val);
	ret = shm_malloc(len + 1);
	if(!ret) {
		ERR("No memory left\n");
		return -1;
	}
	memcpy(ret, val, len + 1);
	*dest = ret;
	return 0;
}


/*
 * Delete old TLS configuration that is not needed anymore
 */
void collect_garbage(void)
{
	tls_domains_cfg_t *prev, *cur, *next;

	/* Make sure we do not run two garbage collectors
	      * at the same time
	      */
	lock_get(tls_domains_cfg_lock);

	/* Skip the current configuration, garbage starts
	      * with the 2nd element on the list
	      */
	prev = *tls_domains_cfg;
	cur = (*tls_domains_cfg)->next;

	while(cur) {
		next = cur->next;
		if(atomic_get(&cur->ref_count) == 0) {
			/* Not referenced by any existing connection */
			prev->next = cur->next;
			tls_free_cfg(cur);
		} else {
			/* Only update prev if we didn't remove cur */
			prev = cur;
		}
		cur = next;
	}

	lock_release(tls_domains_cfg_lock);
}


/** log the verification failure reason.
 * wolfSSL has a different set of return values
 * than OpenSSL
 */
void tls_dump_verification_failure(long verification_result)
{
	int tls_log;

	tls_log = cfg_get(tls, tls_cfg, log);
	LOG(tls_log, "%s\n", wolfSSL_ERR_reason_error_string(verification_result));
}

/* ------------------------------------------------------------------------- */
/* Serialization Logic                                                       */
/* ------------------------------------------------------------------------- */
/**
 * UTILITY FUNCTION: x509_to_der_shm
 * Converts a WOLFSSL_X509 object to a DER buffer allocated in "shm".
 * Returns the length of the DER buffer, or -1 on error.
 */
unsigned char *cert_to_x509_DER(WOLFSSL_X509 *x509, int *out_sz)
{
	int der_len;
	unsigned char *buf;

	/* 1. Determine required length */
	der_len = wolfSSL_i2d_X509(x509, NULL);
	if(der_len <= 0)
		return NULL;

	/* 2. Allocate using shm_malloc */
	buf = (unsigned char *)shm_malloc(der_len);
	if(!buf)
		return NULL;

	/* 3. Encode into buffer (i2d increments the pointer, so we use a temp) */
	unsigned char *p = buf;
	if(wolfSSL_i2d_X509(x509, &p) <= 0) {
		shm_free(buf);
		return NULL;
	}


	*out_sz = der_len;
	return buf;
}

/**
 * UTILITY FUNCTION: der_to_x509
 * Converts a DER buffer back into a WOLFSSL_X509 object.
 */
WOLFSSL_X509 *x509_DER_to_cert(const unsigned char *der, int len)
{
	const unsigned char *p = der;
	return wolfSSL_d2i_X509(NULL, &p, len);
}

/**
 * serialize_stack:
 * Packs a WOLFSSL_STACK of X509s into a binary blob.
 * Format: [uint32 count] + ([uint32 len][DER data] * count)
 */
unsigned char *stack_to_x509_DER(WOLF_STACK_OF(WOLFSSL_X509) * sk, int *out_sz)
{
	if(!sk || !out_sz)
		return NULL;

	int count = wolfSSL_sk_X509_num(sk);
	if(count <= 0)
		return NULL;

	/* 1. Calculate total buffer size needed */
	size_t total = sizeof(uint32_t);
	for(int i = 0; i < count; i++) {
		WOLFSSL_X509 *x = wolfSSL_sk_X509_value(sk, i);
		int der_sz = wolfSSL_i2d_X509(x, NULL);
		if(der_sz <= 0)
			return NULL;
		total += sizeof(uint32_t) + (size_t)der_sz;
	}

	/* 2. Allocate and Pack in index order (0 to count-1) */
	uint8_t *buf = (uint8_t *)shm_malloc(total);
	if(!buf)
		return NULL;

	uint8_t *p = buf;
	uint32_t u_count = (uint32_t)count;
	memcpy(p, &u_count, sizeof(uint32_t));
	p += sizeof(uint32_t);

	for(int i = 0; i < count; i++) {
		WOLFSSL_X509 *x = wolfSSL_sk_X509_value(sk, i);
		int der_sz = wolfSSL_i2d_X509(x, NULL);
		if(der_sz <= 0) {
			shm_free(buf);
			return NULL;
		}
		uint32_t u_sz = (uint32_t)der_sz;

		/* Write individual cert length */
		memcpy(p, &u_sz, sizeof(uint32_t));
		p += sizeof(uint32_t);

		/* Write DER data (wolfSSL_i2d advances the internal pointer) */
		unsigned char *der_ptr = (unsigned char *)p;
		int written = wolfSSL_i2d_X509(x, &der_ptr);
		if(written != der_sz) {
			LM_ERR("wolfSSL_i2d_X509 wrote %d bytes, expected %d\n", written,
					der_sz);
			shm_free(buf);
			return NULL;
		}
		p += der_sz;
	}

	*out_sz = total;
	return buf;
}

/**
 * deserialize_stack:
 * Unpacks the blob back into a WOLFSSL_STACK, preserving order.
 */
WOLF_STACK_OF(WOLFSSL_X509) * x509_DER_to_stack(unsigned char *buf, int buf_sz)
{
	if(!buf || buf_sz < sizeof(uint32_t))
		return NULL;

	uint8_t *p = buf;
	uint32_t count;
	memcpy(&count, p, sizeof(uint32_t));
	p += sizeof(uint32_t);

	WOLF_STACK_OF(WOLFSSL_X509) *sk = wolfSSL_sk_X509_new_null();
	if(!sk)
		return NULL;

	for(uint32_t i = 0; i < count; i++) {
		/* Safety boundary check */
		if((size_t)(p - buf) + sizeof(uint32_t) > buf_sz)
			break;

		uint32_t der_sz;
		memcpy(&der_sz, p, sizeof(uint32_t));
		p += sizeof(uint32_t);

		if((size_t)(p - buf) + der_sz > buf_sz) {
			LM_ERR("Buffer overrun during deserialization at cert %u\n", i);
			break;
		}

		const unsigned char *der_ptr = (const unsigned char *)p;
		WOLFSSL_X509 *x = wolfSSL_d2i_X509(NULL, &der_ptr, (int)der_sz);
		if(x) {
			wolfSSL_sk_X509_push(sk, x);
		}
		p += der_sz;
	}

	return sk;
}
/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
