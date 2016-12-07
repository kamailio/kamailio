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
/** internal Kamailio core :: tcp send functions (use with care).
 * @file tcp_int_send.h
 * @ingroup core
 */
/*
 * History:
 * --------
 *  2010-03-23  initial version (andrei)
*/

#ifndef __tcp_int_send_h
#define __tcp_int_send_h

#include "tcp_conn.h"

int tcpconn_send_unsafe(int fd, struct tcp_connection *c,
						const char* buf, unsigned len, snd_flags_t send_flags);

/* direct non-blocking, unsafe (assumes locked) send on a tcp connection */
int _tcpconn_write_nb(int fd, struct tcp_connection* c,
									const char* buf, int len);


#endif /*__tcp_int_send_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
