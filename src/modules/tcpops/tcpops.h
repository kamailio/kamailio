/**
 * Copyright 2015 (C) Orange
 * <camille.oudot@orange.com>
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

#ifndef _TCPOPS_H_
#define _TCPOPS_H_

#include "../../core/tcp_conn.h"
#include "../../core/events.h"

extern int tcp_closed_event;
extern int tcp_closed_routes[_TCP_CLOSED_REASON_MAX];

int tcpops_get_current_fd(int conid, int *fd);
int tcpops_acquire_fd_from_tcpmain(int conid, int *fd);
int tcpops_keepalive_enable(int fd, int idle, int count, int interval, int closefd);
int tcpops_keepalive_disable(int fd, int closefd);
int tcpops_set_connection_lifetime(struct tcp_connection* con, int time);
int tcpops_handle_tcp_closed(sr_event_param_t *evp);

#endif /* _TCPOPS_H_ */
