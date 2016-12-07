/* 
 * TLS module
 *
 * Copyright (C) 2005,2006 iptelorg GmbH
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
 * \brief Kamailio TLS support :: OpenSSL initialization funtions
 * \ingroup tls
 * Module: \ref tls
 */


#ifndef _TLS_INIT_H
#define _TLS_INIT_H

#include <openssl/ssl.h>
#include "../../ip_addr.h"
#include "tls_domain.h"

/* openssl < 1. 0 */
#if OPENSSL_VERSION_NUMBER < 0x01000000L
/* alternative: check ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME */
#define OPENSSL_NO_TLSEXT
#endif /* OPENSSL_VERION < 1.0 */
#ifndef OPENSSL_NO_KRB5
/* enable workarround for openssl kerberos wrong malloc bug
 * (kssl code uses libc malloc/free/calloc instead of OPENSSL_malloc & 
 * friends)*/
#define TLS_KSSL_WORKARROUND
extern int openssl_kssl_malloc_bug; /* is openssl bug #1467 present ? */
#endif


extern const SSL_METHOD* ssl_methods[];


/*
 * just once, pre-initialize the tls subsystem
 */
int tls_pre_init(void);

/**
 * just once, prepare for init of all modules
 */
int tls_mod_pre_init_h(void);

/*
 * just once, initialize the tls subsystem after all mod inits
 */
int init_tls_h(void);


/*
 * just once before cleanup 
 */
void destroy_tls_h(void);


/*
 * for each socket 
 */
int tls_h_init_si(struct socket_info *si);

/*
 * Make sure that all server domains in the configuration have corresponding
 * listening socket in SER
 */
int tls_check_sockets(tls_domains_cfg_t* cfg);

#endif /* _TLS_INIT_H */
