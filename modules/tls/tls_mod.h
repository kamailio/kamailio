/* 
 * TLS module - module interface
 *
 * Copyright (C) 2005 iptelorg GmbH
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
/** Kamailio TLS support :: module interface.
 * @file
 * @ingroup tls
 * Module: @ref tls
 */



#ifndef _TLS_MOD_H
#define _TLS_MOD_H

#include "../../str.h"
#include "../../locking.h"
#include "tls_domain.h"


/* Current TLS configuration */
extern tls_domains_cfg_t** tls_domains_cfg;
extern gen_lock_t* tls_domains_cfg_lock;

extern tls_domain_t cli_defaults;
extern tls_domain_t srv_defaults;

extern str tls_domains_cfg_file;

extern int sr_tls_renegotiation;

#endif /* _TLS_MOD_H */
