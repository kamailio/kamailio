/*
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

/** Kamailio core :: tcp internal read functions.
 * @file tcp_read.h
 * @ingroup: core
 * Module: @ref core
 */

#ifndef __tcp_read_h
#define __tcp_read_h

#include "tcp_conn.h"

typedef enum rd_conn_flags
{
	RD_CONN_SHORT_READ = (1 << 0),
	RD_CONN_EOF = (1 << 1),
	RD_CONN_REPEAT_READ = (1 << 2), /* read should be repeated (more data)
	                                 (used so far only by tls) */
	RD_CONN_FORCE_EOF = (1 << 16),
} rd_conn_flags_t;

int tcp_read_data(int fd, struct tcp_connection *c, char *buf, int b_size,
		rd_conn_flags_t *flags);


#endif /*__tcp_read_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
