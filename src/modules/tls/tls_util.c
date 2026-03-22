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


unsigned char *cert_to_x509_DER(X509 *cert, int *len)
{
	unsigned char *result = NULL;
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


X509 *x509_DER_to_cert(const unsigned char *der_bytes, int len)
{
	const unsigned char *source = der_bytes;
	if(!der_bytes)
		return NULL;

	X509 *cert = d2i_X509(NULL, &source, len);

	return cert;
}

/*
 * stack_to_x509_DER:
 *   Packs a STACK_OF(X509) into a heap-allocated binary blob.
 *   Caller must shm_free() the returned pointer.
 *   *out_sz receives the total byte count.
 *   Returns NULL on error.
 */
unsigned char *stack_to_x509_DER(STACK_OF(X509) * sk, int *out_sz)
{
	if(!sk || !out_sz)
		return NULL;

	int count = sk_X509_num(sk);
	if(count <= 0)
		return NULL;

	/* ---- pass 1: calculate required buffer size ---- */
	size_t total = sizeof(uint32_t); /* leading count field */

	for(int i = 0; i < count; i++) {
		X509 *x = sk_X509_value(sk, i);
		int der_sz = i2d_X509(x, NULL); /* returns length only */
		if(der_sz <= 0) {
			LM_ERR("i2d_X509 probe failed for cert %d\n", i);
			return NULL;
		}
		total += sizeof(uint32_t) + (size_t)der_sz;
	}

	/* ---- pass 2: allocate and pack ---- */
	unsigned char *buf = shm_malloc(total);
	if(!buf)
		return NULL;

	uint8_t *p = buf;

	uint32_t u_count = (uint32_t)count;
	memcpy(p, &u_count, sizeof(uint32_t));
	p += sizeof(uint32_t);

	for(int i = 0; i < count; i++) {
		X509 *x = sk_X509_value(sk, i);

		/* Probe length again (cheap; avoids storing an array) */
		int der_sz = i2d_X509(x, NULL);
		if(der_sz <= 0) {
			shm_free(buf);
			return NULL;
		}

		uint32_t u_sz = (uint32_t)der_sz;
		memcpy(p, &u_sz, sizeof(uint32_t));
		p += sizeof(uint32_t);

		/*
         * i2d_X509 with a non-NULL **pp writes DER and advances *pp
         * by der_sz bytes — identical behaviour to wolfSSL_i2d_X509.
         */
		unsigned char *der_ptr = p;
		int written = i2d_X509(x, &der_ptr);
		if(written != der_sz) {
			LM_ERR("i2d_X509 wrote %d bytes, expected %d\n", written, der_sz);
			shm_free(buf);
			return NULL;
		}
		/* der_ptr was advanced by i2d_X509; p advances manually */
		p += der_sz;
	}

	*out_sz = (int)total;
	return buf;
}

/* ---------- deserialise ------------------------------------------ */

/*
 * x509_DER_to_stack:
 *   Reconstructs a STACK_OF(X509) from the blob produced above.
 *   Caller owns the returned stack and must free it with
 *   sk_X509_pop_free(sk, X509_free).
 *   Returns NULL on hard error.
 */
STACK_OF(X509) * x509_DER_to_stack(const unsigned char *buf, int buf_sz)
{
	if(!buf || buf_sz < (int)sizeof(uint32_t))
		return NULL;

	const uint8_t *p = buf;
	const uint8_t *end = buf + buf_sz;

	uint32_t count;
	memcpy(&count, p, sizeof(uint32_t));
	p += sizeof(uint32_t);

	STACK_OF(X509) *sk = sk_X509_new_null();
	if(!sk)
		return NULL;

	for(uint32_t i = 0; i < count; i++) {
		/* Need at least 4 bytes for the length field */
		if(end - p < (ptrdiff_t)sizeof(uint32_t)) {
			LM_ERR("Truncated buffer at cert %u (no length field)\n", i);
			break;
		}

		uint32_t der_sz;
		memcpy(&der_sz, p, sizeof(uint32_t));
		p += sizeof(uint32_t);

		if((size_t)(end - p) < (size_t)der_sz) {
			LM_ERR("Buffer overrun during deserialisation at cert %u "
				   "(claims %u bytes, only %td remain)\n",
					i, der_sz, end - p);
			break;
		}

		/*
         * d2i_X509 advances the const-pointer it is given; keep a
         * local copy so we can advance p by the original der_sz.
         */
		const unsigned char *der_ptr = p;
		X509 *x = d2i_X509(NULL, &der_ptr, (long)der_sz);
		if(x) {
			sk_X509_push(sk, x);
		} else {
			LM_ERR("d2i_X509 failed for cert %u\n", i);
			ERR_print_errors_fp(stderr);
		}

		p += der_sz;
	}

	return sk;
}
