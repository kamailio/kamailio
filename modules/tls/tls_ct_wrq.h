/* 
 * $Id$
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
/** .
 * @file /home/andrei/sr.git/modules/tls/tls_ct_wrq.h
 * @ingroup: 
 * Module: 
 */
/*
 * History:
 * --------
 *  2010-03-31  initial version (andrei)
*/

#ifndef __tls_ct_wrq_h
#define __tls_ct_wrq_h

#include "tls_ct_q.h"
#include "tls_server.h"
#include "../../tcp_conn.h"



int tls_ct_wq_init();
void tls_ct_wq_destroy();
unsigned int tls_ct_wq_total_bytes();

#define tls_ct_wq_empty(tc_q) (*(tc_q)==0 || (*(tc_q))->first==0)
#define tls_ct_wq_non_empty(bq) (*(tc_q) && (*(tc_q))->first!=0)


int tls_ct_wq_flush(struct tcp_connection* c, tls_ct_q** tc_q,
					int* flags, int* ssl_err);
int tls_ct_wq_add(tls_ct_q** ct_q, const void* data, unsigned int size);
unsigned int tls_ct_wq_free(tls_ct_q** ct_q);

#endif /*__tls_ct_wrq_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
