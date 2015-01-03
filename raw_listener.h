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
/** Kamailio core :: raw socket udp listen functions.
 *  @file raw_listener.h
 *  @ingroup core
 *  @author andrei
 *  Module: @ref core
 */

#ifndef _raw_listener_h
#define _raw_listener_h

#include "ip_addr.h"


/** default raw socket used for sending on udp ipv4 */
struct socket_info* raw_udp_sendipv4;

int raw_listener_init(struct socket_info* si, str* iface, int iphdr_incl);
int raw_udp4_rcv_loop(int rsock, int port1, int port2);

#endif /* _raw_listener_h */
