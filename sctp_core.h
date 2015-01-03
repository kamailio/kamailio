/**
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
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
/*!
 * \file
 * \brief Kamailio core :: SCTP support
 * \ingroup core
 * Module: \ref core
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
 */

#ifndef __sctp_core_h__
#define __sctp_core_h__

#include "ip_addr.h"

int sctp_core_init(void);
typedef int (*sctp_srapi_init_f)(void);

void sctp_core_destroy(void);
typedef void (*sctp_srapi_destroy_f)(void);

int sctp_core_init_sock(struct socket_info* sock_info);
typedef int (*sctp_srapi_init_sock_f)(struct socket_info* sock_info);

void sctp_core_init_options(void);
typedef void (*sctp_srapi_init_options_f)(void);

int sctp_core_check_compiled_sockopts(char* buf, int size);
typedef int (*sctp_srapi_check_compiled_sockopts_f)(char* buf, int size);

int sctp_core_check_support(void);
typedef int (*sctp_srapi_check_support_f)(void);

int sctp_core_rcv_loop(void);
typedef int (*sctp_srapi_rcv_loop_f)(void);

int sctp_core_msg_send(struct dest_info* dst, char* buf, unsigned len);
typedef int (*sctp_srapi_msg_send_f)(struct dest_info* dst, char* buf,
		unsigned len);

typedef struct sctp_srapi {
	sctp_srapi_init_f init;
	sctp_srapi_destroy_f destroy;
	sctp_srapi_init_sock_f init_sock;
	sctp_srapi_check_support_f check_support;
	sctp_srapi_rcv_loop_f rcv_loop;
	sctp_srapi_msg_send_f msg_send;
} sctp_srapi_t;

int sctp_core_register_api(sctp_srapi_t *api);

#endif
