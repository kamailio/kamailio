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
 * \brief SIP-router TLS support :: Common functions
 * \ingroup tls
 * Module: \ref tls
 */


#ifndef _TLS_UTIL_H
#define _TLS_UTIL_H

#include <openssl/err.h>
#include "../../dprint.h"
#include "../../str.h"
#include "tls_domain.h"


#define TLS_ERR_RET(r, s)                               \
do {                                                    \
	long err;                                       \
        (r) = 0;                                        \
	if ((*tls_domains_cfg)->srv_default->ctx &&         \
	    (*tls_domains_cfg)->srv_default->ctx[0]) {      \
		while((err = ERR_get_error())) {        \
			(r) = 1;                        \
			ERR("%s%s\n", ((s)) ? (s) : "", \
			    ERR_error_string(err, 0));  \
		}                                       \
	}                                               \
} while(0)


#define TLS_ERR(s)           \
do {                         \
	int ret;             \
	TLS_ERR_RET(ret, s); \
} while(0)


/*
 * Make a shared memory copy of ASCII zero terminated string
 * Return value: -1 on error
 *                0 on success
 */
int shm_asciiz_dup(char** dest, char* val);


/*
 * Delete old TLS configuration that is not needed anymore
 */
void collect_garbage(void);

#endif /* _TLS_UTIL_H */
