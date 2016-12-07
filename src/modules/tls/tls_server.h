/*
 * TLS module - main server part
 * 
 * Copyright (C) 2005-2010 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
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

/** main tls part (implements the tls hooks that are called from the tcp code).
 * @file tls_server.h
 * @ingroup tls
 * Module: @ref tls
 */


#ifndef _TLS_SERVER_H
#define _TLS_SERVER_H

#include <stdio.h>
#include "../../tcp_conn.h"
#include "tls_domain.h"
#include "tls_ct_wrq.h"

enum tls_conn_states {
						S_TLS_NONE = 0,
						S_TLS_ACCEPTING,
						S_TLS_CONNECTING,
						S_TLS_ESTABLISHED
					};

struct tls_rd_buf {
	unsigned int pos; /* current position */
	unsigned int size; /* total size (buf) */
	unsigned char buf[1];
};

/* tls conn flags */
#define F_TLS_CON_WR_WANTS_RD    1 /* write wants read */
#define F_TLS_CON_HANDSHAKED     2 /* connection is handshaked */
#define F_TLS_CON_RENEGOTIATION  4 /* renegotiation by clinet */

struct tls_extra_data {
	tls_domains_cfg_t* cfg; /* Configuration used for this connection */
	SSL* ssl;               /* SSL context used for the connection */
	BIO* rwbio;             /* bio used for read/write
							   (openssl code might add buffering BIOs so
							    it's better to remember our original BIO) */
	tls_ct_q* ct_wq;
	struct tls_rd_buf* enc_rd_buf;
	unsigned int flags;
	enum  tls_conn_states state;
};


/* return true if write wants read */
#define tls_write_wants_read(tls_ed) (tls_ed->flags & F_TLS_CON_WR_WANTS_RD)


/*
 * Called when new tcp connection is accepted 
 */
int tls_h_tcpconn_init(struct tcp_connection *c, int sock);

/*
 * clean the extra data upon connection shut down 
 */
void tls_h_tcpconn_clean(struct tcp_connection *c);

/*
 * shut down the TLS connection 
 */
void tls_h_close(struct tcp_connection *c, int fd);

int tls_encode_f(struct tcp_connection *c,
					const char ** pbuf, unsigned int* plen,
						const char** rest_buf, unsigned int* rest_len,
						snd_flags_t* send_flags) ;

int tls_read_f(struct tcp_connection *c, int* flags);

int tls_h_fix_read_conn(struct tcp_connection *c);

int tls_connect(struct tcp_connection *c, int* error);
int tls_accept(struct tcp_connection *c, int* error);

void tls_lookup_event_routes(void);
#endif /* _TLS_SERVER_H */
