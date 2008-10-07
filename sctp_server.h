/* 
 * $Id$
 * 
 * Copyright (C) 2008 iptelorg GmbH
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
/* 
 * sctp one to many 
 */
/*
 * History:
 * --------
 *  2008-08-07  initial version (andrei)
 */

#ifndef _sctp_server_h
#define _sctp_server_h

#include "ip_addr.h"

int sctp_check_compiled_sockopts(char* buf, int size);
int sctp_check_support();
int sctp_init_sock(struct socket_info* sock_info);
int sctp_rcv_loop();
int sctp_msg_send(struct dest_info* dst, char* buf, unsigned len);

void destroy_sctp();

#endif /* _sctp_server_h */
