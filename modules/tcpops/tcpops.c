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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <errno.h>

#include "../../dprint.h"
#include "../../tcp_options.h"
#include "../../tcp_conn.h"
#include "../../globals.h"
#include "../../pass_fd.h"
#include "../../timer.h"

/**
 * gets the fd of the current message source connection
 *
 * @param conid - connection id
 * @param fd - placeholder to return the fd
 * @return 1 on success, 0 on failure
 *
 */
int tcpops_get_current_fd(int conid, int *fd)
{
	struct tcp_connection *s_con;
	if (unlikely((s_con = tcpconn_get(conid, 0, 0, 0, 0)) == NULL)) {
		LM_ERR("invalid connection id %d, (must be a TCP connid)\n", conid);
		return 0;
	}
	LM_DBG("got fd=%d from id=%d\n", s_con->fd, conid);

	*fd = s_con->fd;
	tcpconn_put(s_con);
	return 1;
}

/**
 * Request the fd corresponding to the given connection id to the TCP main process.
 * You may want to close() the fd after use.
 *
 * @param conid - connection id
 * @param fd - placeholder to return the fd
 * @return 1 on success, 0 on failure
 *
 */
int tcpops_acquire_fd_from_tcpmain(int conid, int *fd)
{
	struct tcp_connection *s_con, *tmp;
	long msg[2];
	int n;

	if (unlikely((s_con = tcpconn_get(conid, 0, 0, 0, 0)) == NULL)) {
		LM_ERR("invalid connection id %d, (must be a TCP connid)\n", conid);
		return 0;
	}

	msg[0] = (long)s_con;
	msg[1] = CONN_GET_FD;

	n = send_all(unix_tcp_sock, msg, sizeof(msg));
	if (unlikely(n <= 0)){
		LM_ERR("failed to send fd request: %s (%d)\n", strerror(errno), errno);
		goto error_release;
	}

	n = receive_fd(unix_tcp_sock, &tmp, sizeof(tmp), fd, MSG_WAITALL);
	if (unlikely(n <= 0)){
		LM_ERR("failed to get fd (receive_fd): %s (%d)\n", strerror(errno), errno);
		goto error_release;
	}
	tcpconn_put(s_con);
	return 1;

error_release:
	tcpconn_put(s_con);
	return 0;
}

#if !defined(HAVE_SO_KEEPALIVE) || !defined(HAVE_TCP_KEEPIDLE) || !defined(HAVE_TCP_KEEPCNT) || !defined(HAVE_TCP_KEEPINTVL)
	#warning "TCP keepalive is not fully supported by your platform"

int tcpops_keepalive_enable(int fd, int idle, int count, int interval, int closefd)
{
	LM_ERR("tcp_keepalive_enable() failed: this module does not support your platform\n");
	return -1;
}

int tcpops_keepalive_disable(int fd, int closefd)
{
	LM_ERR("tcp_keepalive_disable() failed: this module does not support your platform\n");
	return -1;
}
#else

int tcpops_keepalive_enable(int fd, int idle, int count, int interval, int closefd)
{
	static const int enable = 1;
	int ret = -1;

	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable,
					sizeof(enable))<0){
		LM_ERR("failed to enable SO_KEEPALIVE: %s\n", strerror(errno));
		return -1;
	} else {

		if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle,
						sizeof(idle))<0){
			LM_ERR("failed to set keepalive idle interval: %s\n", strerror(errno));
		}

		if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count,
						sizeof(count))<0){
			LM_ERR("failed to set maximum keepalive count: %s\n", strerror(errno));
		}

		if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval,
						sizeof(interval))<0){
			LM_ERR("failed to set keepalive probes interval: %s\n", strerror(errno));
		}

		ret = 1;
		LM_DBG("keepalive enabled for fd=%d, idle=%d, cnt=%d, intvl=%d\n", fd, idle, count, interval);
	}

	if (closefd)
	{
		close(fd);
	}
	return ret;
}

int tcpops_keepalive_disable(int fd, int closefd)
{
	static const int disable = 0;
	int ret = -1;

	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &disable,
					sizeof(disable))<0){
		LM_WARN("failed to disable SO_KEEPALIVE: %s\n", strerror(errno));
	} else {
		ret = 1;
		LM_DBG("keepalive disabled for fd=%d\n", fd);
	}

	if (closefd)
	{
		close(fd);
	}
	return ret;
}

#endif

int tcpops_set_connection_lifetime(struct tcp_connection* con, int time) {
	if (unlikely(con == NULL)) {
		LM_CRIT("BUG: con == NULL");
		return -1;
	}
	if (unlikely(time < 0)) {
		LM_ERR("Invalid timeout value, %d, must be >= 0\n", time);
		return -1;
	}
	con->lifetime = S_TO_TICKS(time);
	con->timeout = get_ticks_raw() + con->lifetime;
	LM_DBG("new connection lifetime for conid=%d: %d\n", con->id, con->timeout);
	return 1;
}
