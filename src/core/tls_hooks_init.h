/*
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

/**
 * @file
 * @brief Kamailio TLS support :: TLS hooks init
 * @ingroup tls
 * Module: @ref tls
 */


#ifndef _tls_hooks_init_h
#define _tls_hooks_init_h

#ifdef TLS_HOOKS

#include "ip_addr.h" /* socket_info */

#ifndef USE_TLS
#error "USE_TLS required and not defined (please compile with make \
	TLS_HOOKS=1)"
#endif

#ifdef CORE_TLS
#error "Conflict: CORE_TLS and TLS_HOOKS cannot be defined in the same time"
#endif


int tls_loaded(void);
int tls_has_init_si(void); /*returns true if a handle for tls_init is registered*/
int tls_init(struct socket_info* si);
int init_tls(void);
void destroy_tls(void);
int pre_init_tls(void);

#endif /* TLS_HOOKS */
#endif
