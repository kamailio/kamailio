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
#include <stdio.h>
#include <libgen.h>
#include "../../core/mem/shm_mem.h"
#include "../../core/globals.h"
#include "../../core/dprint.h"
#include "../../core/ip_addr.h"
#include "../../core/socket_info.h"
#include "../../core/udp_server.h"
#include "../../core/forward.h"
#include "../../core/resolve.h"

#include "tls_mod.h"
#include "tls_util.h"

/* OpenSSL < 3.0 compatibility shims */
#if OPENSSL_VERSION_NUMBER < 0x30000000L
/* ERR_raise() was introduced in OpenSSL 3.0.
 * ERR_put_error() is the 1.x equivalent; func (middle arg) is unused in 3.x
 * so passing 0 is fine for both error-reporting and backward compat. */
#ifndef ERR_raise
#define ERR_raise(lib, reason) \
	ERR_put_error((lib), 0, (reason), __FILE__, __LINE__)
#endif
/* ERR_R_PASSED_INVALID_ARGUMENT was added in OpenSSL 3.0.
 * Map to the closest 1.x generic reason code. */
#ifndef ERR_R_PASSED_INVALID_ARGUMENT
#define ERR_R_PASSED_INVALID_ARGUMENT ERR_R_PASSED_NULL_PARAMETER
#endif
#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */


extern int *ksr_tls_keylog_mode;
extern str ksr_tls_keylog_file;
extern str ksr_tls_keylog_peer;

static gen_lock_t *ksr_tls_keylog_file_lock = NULL;
static dest_info_t ksr_tls_keylog_peer_dst;

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

/*
 * Get any leftover errors from OpenSSL and print them.
 * ERR_get_error() also removes the error from the OpenSSL error stack.
 * This is useful to call before any SSL_* IO calls to make sure
 * we don't have any leftover errors from previous calls (OpenSSL docs).
 */
void tls_openssl_clear_errors(void)
{
	int i;
	char err[256];
	while((i = ERR_get_error())) {
		ERR_error_string(i, err);
		INFO("clearing leftover error before SSL_* calls: %s\n", err);
	}
}

/**
 *
 */
int ksr_tls_keylog_file_init(void)
{
	if(ksr_tls_keylog_mode == NULL) {
		return 0;
	}
	if(!((*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_INIT)
			   && (*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_FILE))) {
		return 0;
	}
	if(ksr_tls_keylog_file.s == NULL || ksr_tls_keylog_file.len <= 0) {
		return -1;
	}
	if(ksr_tls_keylog_file_lock != NULL) {
		return 0;
	}
	ksr_tls_keylog_file_lock = lock_alloc();
	if(ksr_tls_keylog_file_lock == NULL) {
		return -2;
	}
	if(lock_init(ksr_tls_keylog_file_lock) == NULL) {
		return -3;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static const char *ksr_tls_keylog_vfilters[] = {
	"CLIENT_RANDOM ",
	"CLIENT_HANDSHAKE_TRAFFIC_SECRET ",
	"SERVER_HANDSHAKE_TRAFFIC_SECRET ",
	"EXPORTER_SECRET ",
	"CLIENT_TRAFFIC_SECRET_0 ",
	"SERVER_TRAFFIC_SECRET_0 ",
	NULL
};
/* clang-format on */

/**
 *
 */
int ksr_tls_keylog_vfilter_match(const char *line)
{
	int i;

	for(i = 0; ksr_tls_keylog_vfilters[i] != NULL; i++) {
		if(strcasecmp(ksr_tls_keylog_vfilters[i], line) == 0) {
			return 1;
		}
	}
	return 0;
}

/**
 *
 */
int ksr_tls_keylog_file_write(const SSL *ssl, const char *line)
{
	FILE *lf = NULL;
	int ret = 0;

	if(ksr_tls_keylog_file_lock == NULL) {
		return 0;
	}

	lock_get(ksr_tls_keylog_file_lock);
	lf = fopen(ksr_tls_keylog_file.s, "a");
	if(lf) {
		fprintf(lf, "%s\n", line);
		fclose(lf);
	} else {
		LM_ERR("failed to open keylog file: %s\n", ksr_tls_keylog_file.s);
		ret = -1;
	}
	lock_release(ksr_tls_keylog_file_lock);
	return ret;
}


/**
 *
 */
int ksr_tls_keylog_peer_init(void)
{
	int proto;
	str host;
	int port;

	if(ksr_tls_keylog_mode == NULL) {
		return 0;
	}
	if(!((*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_INIT)
			   && (*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_PEER))) {
		return 0;
	}
	if(ksr_tls_keylog_peer.s == NULL || ksr_tls_keylog_peer.len <= 0) {
		return -1;
	}
	init_dest_info(&ksr_tls_keylog_peer_dst);
	if(parse_phostport(ksr_tls_keylog_peer.s, &host.s, &host.len, &port, &proto)
			!= 0) {
		LM_CRIT("invalid peer addr parameter <%s>\n", ksr_tls_keylog_peer.s);
		return -2;
	}
	if(proto != PROTO_UDP) {
		LM_ERR("only udp supported in peer addr <%s>\n", ksr_tls_keylog_peer.s);
		return -3;
	}
	ksr_tls_keylog_peer_dst.proto = proto;
	if(sip_hostport2su(&ksr_tls_keylog_peer_dst.to, &host, port,
			   &ksr_tls_keylog_peer_dst.proto)
			!= 0) {
		LM_ERR("failed to resolve <%s>\n", ksr_tls_keylog_peer.s);
		return -4;
	}

	return 0;
}

/**
 *
 */
int ksr_tls_keylog_peer_send(const SSL *ssl, const char *line)
{
	if(ksr_tls_keylog_mode == NULL) {
		return 0;
	}
	if(!((*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_INIT)
			   && (*ksr_tls_keylog_mode & KSR_TLS_KEYLOG_MODE_PEER))) {
		return 0;
	}

	if(ksr_tls_keylog_peer_dst.send_sock == NULL) {
		ksr_tls_keylog_peer_dst.send_sock =
				get_send_socket(NULL, &ksr_tls_keylog_peer_dst.to, PROTO_UDP);
		if(ksr_tls_keylog_peer_dst.send_sock == NULL) {
			LM_ERR("no send socket for <%s>\n", ksr_tls_keylog_peer.s);
			return -2;
		}
	}

	if(udp_send(&ksr_tls_keylog_peer_dst, (char *)line, strlen(line)) < 0) {
		LM_ERR("failed to send to <%s>\n", ksr_tls_keylog_peer.s);
		return -1;
	}
	return 0;
}


char *convert_X509_to_DER(X509 *cert, int *len)
{
	char *result = NULL;
	unsigned char *buf;

	if(cert == NULL) {
		*len = 0;
		return NULL;
	}

	*len = i2d_X509(cert, NULL);
	buf = result = shm_malloc(*len);
	i2d_X509(cert, &buf);

	return result;
}

X509 *convert_DER_to_X509(char *der_bytes, int len)
{
	const unsigned char *source = der_bytes;
	if(!der_bytes)
		return NULL;

	X509 *cert = d2i_X509(NULL, &source, len);

	return cert;
}

/**
 * stack_of_x509_to_pkcs7_der - Serialize a certificate stack into a
 *                               DER-encoded PKCS#7 "certs-only" blob.
 *
 * This produces a degenerate SignedData structure (RFC 2315 / CMS) that
 * carries certificates only — no signers, no content, no encryption.
 * It is the standard container used for certificate chains (e.g. the
 * output of "openssl crl2pkcs7 -nocrl").
 *
 * Memory contract
 * ---------------
 * The returned buffer is allocated with shm_malloc().  The caller is
 * responsible for freeing it with the matching shm_free() (or equivalent).
 * On failure NULL is returned and *out_len is set to 0.
 *
 * @param  stack    Non-NULL stack of X509 certificates to encode.
 *                  Ownership is NOT transferred; the stack and its
 *                  certificates are not modified.
 * @param  out_len  Non-NULL output parameter that receives the byte
 *                  length of the returned buffer on success, or 0 on
 *                  failure.
 * @return          Pointer to a shm_malloc'd DER buffer, or NULL on error.
 */
uint8_t *stack_to_pkcs7_DER(STACK_OF(X509) * stack, int *out_len)
{
	PKCS7 *p7 = NULL;
	uint8_t *der_buf = NULL; /* final shm_malloc'd output        */
	uint8_t *tmp_ptr = NULL; /* scratch pointer for i2d (moves!) */
	int der_len = 0;
	int num_certs, i;

	/* ------------------------------------------------------------------ */
	/* 0. Validate inputs                                                  */
	/* ------------------------------------------------------------------ */
	if(!stack || !out_len) {
		ERR_raise(ERR_LIB_USER, ERR_R_PASSED_NULL_PARAMETER);
		goto err;
	}
	*out_len = 0;

	num_certs = sk_X509_num(stack);
	if(num_certs < 0) {
		ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	/* ------------------------------------------------------------------ */
	/* 1. Build an empty PKCS7 SignedData ("degenerate" / certs-only)      */
	/* ------------------------------------------------------------------ */
	p7 = PKCS7_new();
	if(!p7) {
		ERR_raise(ERR_LIB_USER, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if(!PKCS7_set_type(p7, NID_pkcs7_signed)) {
		ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	/*
     * Set the inner content type to Data with a NULL (absent) content body.
     * This is the canonical form for a degenerate SignedData; there are no
     * actual signed octets.
     */
	if(!PKCS7_content_new(p7, NID_pkcs7_data)) {
		ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	/* ------------------------------------------------------------------ */
	/* 2. Attach all certificates from the input stack                     */
	/*    PKCS7_add_certificate bumps each cert's reference count, so the  */
	/*    originals are safe to use after this call.                       */
	/* ------------------------------------------------------------------ */
	for(i = 0; i < num_certs; i++) {
		X509 *cert = sk_X509_value(stack, i);
		if(!cert) {
			ERR_raise(ERR_LIB_USER, ERR_R_PASSED_NULL_PARAMETER);
			goto err;
		}
		if(!PKCS7_add_certificate(p7, cert)) {
			ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
			goto err;
		}
	}

	/* ------------------------------------------------------------------ */
	/* 3. Compute the required DER-encoded size (dry run with NULL dest)   */
	/* ------------------------------------------------------------------ */
	der_len = i2d_PKCS7(p7, NULL);
	if(der_len <= 0) {
		ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	/* ------------------------------------------------------------------ */
	/* 4. Allocate output buffer via shm_malloc                            */
	/* ------------------------------------------------------------------ */
	der_buf = (uint8_t *)shm_malloc((size_t)der_len);
	if(!der_buf) {
		ERR_raise(ERR_LIB_USER, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	/*
     * i2d_PKCS7 advances the pointer it receives, so we hand it a copy
     * and keep the original (der_buf) for the caller.
     */
	tmp_ptr = der_buf;
	if(i2d_PKCS7(p7, &tmp_ptr) != der_len) {
		ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	/* ------------------------------------------------------------------ */
	/* 5. Success — hand the buffer and length back to the caller          */
	/* ------------------------------------------------------------------ */
	*out_len = der_len;
	PKCS7_free(p7);
	return der_buf;

err:
	/* Log the accumulated OpenSSL error queue to stderr for diagnostics. */
	ERR_print_errors_fp(stderr);

	if(der_buf) {
		/* Scrub any partial data before releasing shared memory. */
		memset(der_buf, 0, (size_t)der_len);
		shm_free(der_buf);
	}
	PKCS7_free(p7); /* NULL-safe */
	if(out_len)
		*out_len = 0;
	return NULL;
}


/**
 * pkcs7_der_to_stack_of_x509 - Deserialize a DER-encoded PKCS#7
 *                               "certs-only" blob into a certificate stack.
 *
 * Orthogonality guarantee
 * -----------------------
 * The input buffer (der_buf / der_len) is FULLY orthogonal to the returned
 * stack.  OpenSSL's d2i_PKCS7() deep-copies every ASN.1 field into its own
 * heap allocations during parsing; no pointer into der_buf is retained by
 * either the intermediate PKCS7 object or the final X509 certificates.
 * The caller MAY therefore mutate, shm_free(), or reuse der_buf immediately
 * after this function returns — regardless of success or failure.
 *
 * Ownership of returned stack
 * ---------------------------
 * On success the caller owns the returned STACK_OF(X509) and every X509
 * inside it.  Release with:
 *
 *     sk_X509_pop_free(stack, X509_free);
 *
 * On failure NULL is returned; no resources need to be freed by the caller.
 *
 * @param  der_buf  Pointer to a DER-encoded PKCS#7 SignedData blob.
 *                  May be freed by the caller immediately after return.
 * @param  der_len  Byte length of der_buf.  Must be > 0.
 * @return          A newly allocated STACK_OF(X509), or NULL on error.
 */
STACK_OF(X509) * pkcs7_DER_to_stack(const uint8_t *der_buf, int der_len)
{
	PKCS7 *p7 = NULL;
	STACK_OF(X509) *certs = NULL;  /* certs inside the PKCS7 (owned by p7) */
	STACK_OF(X509) *result = NULL; /* deep-copied output stack             */
	const uint8_t *tmp_ptr = NULL; /* d2i advances this — keep original    */
	int num_certs, i;

	/* ------------------------------------------------------------------ */
	/* 0. Validate inputs                                                  */
	/* ------------------------------------------------------------------ */
	if(!der_buf || der_len <= 0) {
		ERR_raise(ERR_LIB_USER, ERR_R_PASSED_NULL_PARAMETER);
		return NULL;
	}

	/* ------------------------------------------------------------------ */
	/* 1. Parse the DER buffer into a PKCS7 structure                     */
	/*                                                                     */
	/* tmp_ptr is advanced by d2i to point past the consumed bytes.       */
	/* der_buf is untouched and remains safe to free after this call.     */
	/* ------------------------------------------------------------------ */
	tmp_ptr = der_buf;
	p7 = d2i_PKCS7(NULL, &tmp_ptr, (long)der_len);
	if(!p7) {
		ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	/* ------------------------------------------------------------------ */
	/* 2. Verify this is a SignedData (degenerate / certs-only) structure  */
	/* ------------------------------------------------------------------ */
	if(!PKCS7_type_is_signed(p7)) {
		ERR_raise(ERR_LIB_USER, ERR_R_PASSED_INVALID_ARGUMENT);
		goto err;
	}

	/* p7->d.sign->cert is the internal stack — owned by p7, do NOT free. */
	certs = p7->d.sign->cert;

	num_certs = (certs != NULL) ? sk_X509_num(certs) : 0;

	/* ------------------------------------------------------------------ */
	/* 3. Allocate the output stack                                        */
	/* ------------------------------------------------------------------ */
	result = sk_X509_new_null();
	if(!result) {
		ERR_raise(ERR_LIB_USER, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	/* ------------------------------------------------------------------ */
	/* 4. Deep-copy each certificate into the output stack                */
	/*                                                                     */
	/* X509_dup() performs a full encode/decode round-trip, producing a   */
	/* completely independent X509 object with its own heap allocations.  */
	/* The result is independent of p7 — PKCS7_free(p7) below is safe.   */
	/* ------------------------------------------------------------------ */
	for(i = 0; i < num_certs; i++) {
		X509 *src = sk_X509_value(certs, i);
		X509 *copy = NULL;

		if(!src) {
			ERR_raise(ERR_LIB_USER, ERR_R_INTERNAL_ERROR);
			goto err;
		}

		copy = X509_dup(src);
		if(!copy) {
			ERR_raise(ERR_LIB_USER, ERR_R_MALLOC_FAILURE);
			goto err;
		}

		if(!sk_X509_push(result, copy)) {
			X509_free(copy); /* push failed — release this cert only */
			ERR_raise(ERR_LIB_USER, ERR_R_MALLOC_FAILURE);
			goto err;
		}
	}

	/* ------------------------------------------------------------------ */
	/* 5. Release the PKCS7 wrapper — result stack is fully self-contained */
	/* ------------------------------------------------------------------ */
	PKCS7_free(p7);
	return result;

err:
	ERR_print_errors_fp(stderr);
	PKCS7_free(p7); /* NULL-safe */
	sk_X509_pop_free(
			result, X509_free); /* NULL-safe; frees any partial certs */
	return NULL;
}