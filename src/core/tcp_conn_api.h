/*
 * Copyright 2015 (C) Orange
 * <camille.oudot@orange.com>
 * Copyright (C) 2026 S-P Chan
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _TCP_CONN_API_H_
#define _TCP_CONN_API_H_

#include "tcp_conn.h"

/* TCP connection API for modules: change a connection by its id from any
 * process; the core applies the change where the connection's owner is
 *  - tcp_main_threads 0/1: in the calling process, as modules did before
 *  - tcp_main_threads 2: in PROC_TCP_MAIN (a command or an io_wait task)
 * Reads (state, id, rcv, type) stay direct. */

/* return codes: 1 success, -1 failure, TCPCONN_API_NOCONN unknown conid */
#define TCPCONN_API_NOCONN (-2)

/* tcpconn_ctrl() commands, with the argument after cmd
 *  - TCPCONN_CTRL_CLOSE (int, enum tcp_closed_reason, 0 = none): close conid
 *    after its pending writes; the tcp:closed/timeout/reset events report
 *    the reason
 *  - TCPCONN_CTRL_SET_LIFETIME (unsigned int): set the idle lifetime of conid
 *    (seconds)
 *  - TCPCONN_CTRL_SET_KEEPALIVE (struct tcpconn_keepalive *): enable (on = 1:
 *    idle, cnt, intvl) or disable TCP keepalive on conid
 *  - TCPCONN_CTRL_SET_FLAGS (unsigned int): set connection flags on conid;
 *    allowed: F_CONN_CLOSE_EV */
enum tcpconn_ctrl_cmd
{
	TCPCONN_CTRL_CLOSE = 1,
	TCPCONN_CTRL_SET_LIFETIME,
	TCPCONN_CTRL_SET_KEEPALIVE,
	TCPCONN_CTRL_SET_FLAGS
};

/* TCPCONN_CTRL_SET_KEEPALIVE argument */
struct tcpconn_keepalive
{
	int on;
	int idle;
	int cnt;
	int intvl;
};

/* change conid, fcntl-style: tcpconn_ctrl(conid, TCPCONN_CTRL_..., arg) */
int tcpconn_ctrl(int conid, int cmd, ...);

#endif /* _TCP_CONN_API_H_ */
