/*
 * TLS module - OpenSSL capability detection
 *
 * Centralised derivation of KSR_SSL_COMMON / KSR_SSL_ENGINE /
 * KSR_SSL_PROVIDER and the associated KEY_PREFIX macros.
 *
 * Include this header instead of repeating the detection block in every
 * translation unit.  Guards prevent multiple inclusion.
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

#ifndef _TLS_OPENSSL_H
#define _TLS_OPENSSL_H

#include <openssl/opensslv.h>

/* OpenSSL ENGINE API (deprecated in OpenSSL 3) */
#if !defined(OPENSSL_NO_ENGINE) && OPENSSL_VERSION_NUMBER < 0x030000000L
#define KSR_SSL_COMMON
#define KSR_SSL_ENGINE
#define KEY_PREFIX "/engine:"
#define KEY_PREFIX_LEN (strlen(KEY_PREFIX))
#endif /* KSR_SSL_ENGINE */

/* OpenSSL PROVIDER API (OpenSSL 3+) */
#if !defined(OPENSSL_NO_PROVIDER) && OPENSSL_VERSION_NUMBER >= 0x030000000L
#define KSR_SSL_COMMON
#define KSR_SSL_PROVIDER
#include <openssl/store.h>
#define KEY_PREFIX "/uri:"
#define KEY_PREFIX_LEN (strlen(KEY_PREFIX))
#endif /* KSR_SSL_PROVIDER */

#endif /* _TLS_OPENSSL_H */
