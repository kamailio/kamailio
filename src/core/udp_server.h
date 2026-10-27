/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the Kamailio software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef udp_server_h
#define udp_server_h

#include <sys/types.h>
#include <sys/socket.h>
#include "ip_addr.h"

#define MAX_RECV_BUFFER_SIZE 256 * 1024
#define MAX_SEND_BUFFER_SIZE 256 * 1024
#define BUFFER_INCREMENT 2048


int udp_main_init(void);
/* udp_rx_mode; the per-worker sockets need Linux 3.9, auto's picker 4.5 */
#define KSR_UDP_RX_SHARED 0 /* one socket read by every worker (default) */
#define KSR_UDP_RX_AUTO 1	/* a socket per worker, a cBPF picker spreads */
#define KSR_UDP_RX_HASH 2	/* a socket per worker, the 4-tuple hash picks */
int ksr_udp_rx_mode_set(const char *mode);

int udp_init(struct socket_info *si);
int udp_send(struct dest_info *dst, char *buf, unsigned len);
int udp_rcv_loop(void);

int ksr_udp_start_mtreceiver(int child_rank, char *agname, int *woneinit);

#endif
