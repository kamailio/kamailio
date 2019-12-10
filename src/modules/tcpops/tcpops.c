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

#include "../../core/dprint.h"
#include "../../core/tcp_options.h"
#include "../../core/tcp_conn.h"
#include "../../core/globals.h"
#include "../../core/pass_fd.h"
#include "../../core/timer.h"
#include "../../core/route.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../core/sr_module.h"

#include "tcpops.h"


extern str tcpops_event_callback;

static str tcpops_evrt_closed = str_init("tcp:closed");
static str tcpops_evrt_timeout = str_init("tcp:timeout");
static str tcpops_evrt_reset = str_init("tcp:reset");

/* globally enabled by default */
int tcp_closed_event = 1;

int tcp_closed_routes[_TCP_CLOSED_REASON_MAX] =
	{[0 ... sizeof(tcp_closed_routes)/sizeof(*tcp_closed_routes)-1] = -1};

/**
 *
 */
void tcpops_init_evroutes(void)
{
	if(tcpops_event_callback.len > 0) {
		return;
	}

	/* get event routes */
	tcp_closed_routes[TCP_CLOSED_EOF] = route_get(&event_rt, tcpops_evrt_closed.s);
	tcp_closed_routes[TCP_CLOSED_TIMEOUT] = route_get(&event_rt, tcpops_evrt_timeout.s);
	tcp_closed_routes[TCP_CLOSED_RESET] = route_get(&event_rt, tcpops_evrt_reset.s);
}

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

#if !defined(HAVE_SO_KEEPALIVE) || !defined(HAVE_TCP_KEEPCNT) || !defined(HAVE_TCP_KEEPINTVL)
	#define KSR_TCPOPS_NOKEEPALIVE
#endif

#ifdef KSR_TCPOPS_NOKEEPALIVE

#warning "TCP keepalive options not supported by this platform"

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
#ifdef HAVE_TCP_KEEPIDLE
		if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle,
						sizeof(idle))<0){
			LM_ERR("failed to set keepalive idle interval: %s\n", strerror(errno));
		}
#else
		#warning "TCP_KEEPIDLE option not supported by this platform"
		LM_DBG("TCP_KEEPIDLE option not available - ignoring\n");
#endif

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

static void tcpops_tcp_closed_run_route(tcp_closed_event_info_t *tev)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	str *evname;

	if(tcpops_event_callback.len > 0) {
		rt = -1;
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_DBG("even callback set, but no kemi engine\n");
			return;
		}
	} else {
		rt = tcp_closed_routes[tev->reason];
		LM_DBG("event reason id: %d rt: %d\n", tev->reason, rt);
		if (rt == -1) return;
	}

	if (faked_msg_init() < 0) {
		LM_ERR("faked_msg_init() failed\n");
		return;
	}
	fmsg = faked_msg_next();
	fmsg->rcv = tev->con->rcv;

	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if(keng == NULL) {
		if(rt>=0) {
			run_top_route(event_rt.rlist[rt], fmsg, 0);
		} else {
			LM_DBG("no event route block to execute\n");
		}
	} else {
		if(tev->reason==TCP_CLOSED_TIMEOUT) {
			evname = &tcpops_evrt_timeout;
		} else if(tev->reason==TCP_CLOSED_RESET) {
			evname = &tcpops_evrt_reset;
		} else {
			evname = &tcpops_evrt_closed;
		}
		if(sr_kemi_route(keng, fmsg, EVENT_ROUTE, &tcpops_event_callback,
					evname)<0) {
			LM_ERR("error running event route kemi callback [%.*s - %.*s]\n",
						tcpops_event_callback.len, tcpops_event_callback.s,
						evname->len, evname->s);
		}
	}
	set_route_type(backup_rt);
}

int tcpops_handle_tcp_closed(sr_event_param_t *evp)
{
	tcp_closed_event_info_t *tev = (tcp_closed_event_info_t *)evp->data;

	if (tev == NULL || tev->con == NULL) {
		LM_WARN("received bad TCP closed event\n");
		return -1;
	}
	LM_DBG("received TCP closed event\n");

	/* run event route if tcp_closed_event == 1 or if the
	 * F_CONN_CLOSE_EV flag is explicitly set */
	if (tcp_closed_event == 1 || (tev->con->flags & F_CONN_CLOSE_EV)) {
		tcpops_tcp_closed_run_route(tev);
	}

	return 0;
}
