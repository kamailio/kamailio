/*
 * TLS module
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
/*!
 * \file
 * \brief Kamailio TLS support :: Common functions
 * \ingroup tls
 * Module: \ref tls
 */


#ifndef _TLS_UTIL_H
#define _TLS_UTIL_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../../core/dprint.h"
#include "../../core/str.h"
#include "tls_domain.h"

#define KSR_TLS_KEYLOG_MODE_INIT (1)
#define KSR_TLS_KEYLOG_MODE_ACTIVE (1 << 1)
#define KSR_TLS_KEYLOG_MODE_MLOG (1 << 2)
#define KSR_TLS_KEYLOG_MODE_FILE (1 << 3)
#define KSR_TLS_KEYLOG_MODE_PEER (1 << 4)

static inline int tls_err_ret(
		char *s, SSL *ssl, tls_domains_cfg_t **tls_domains_cfg)
{
	long err;
	int ret = 0;
	const char *sn = NULL;

	if((*tls_domains_cfg)->srv_default->ctx
			&& (*tls_domains_cfg)->srv_default->ctx[0]) {
		if(ssl) {
			sn = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
		}
		while((err = ERR_get_error())) {
			ret = 1;
			ERR("%s%s (sni: %s)\n", s ? s : "", ERR_error_string(err, 0),
					(sn) ? sn : "unknown");
		}
	}
	return ret;
}

#define TLS_ERR_RET(r, s)                              \
	do {                                               \
		(r) = tls_err_ret((s), NULL, tls_domains_cfg); \
	} while(0)


#define TLS_ERR(s)                               \
	do {                                         \
		tls_err_ret((s), NULL, tls_domains_cfg); \
	} while(0)

#define TLS_ERR_SSL(s, ssl)                       \
	do {                                          \
		tls_err_ret((s), (ssl), tls_domains_cfg); \
	} while(0)

/*
 * Make a shared memory copy of ASCII zero terminated string
 * Return value: -1 on error
 *                0 on success
 */
int shm_asciiz_dup(char **dest, char *val);


/*
 * Delete old TLS configuration that is not needed anymore
 */
void collect_garbage(void);

void tls_openssl_clear_errors(void);

int ksr_tls_keylog_file_init(void);
int ksr_tls_keylog_file_write(const SSL *ssl, const char *line);
int ksr_tls_keylog_peer_init(void);
int ksr_tls_keylog_peer_send(const SSL *ssl, const char *line);

#endif /* _TLS_UTIL_H */
