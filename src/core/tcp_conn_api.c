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

/* TCP connection API for modules (tcp_conn_api.h)
 *  - tcp_main_threads 0/1: the change runs in the calling process; a socket
 *    option needs the fd: the reader's own, or a copy from tcp main
 *  - tcp_main_threads 2: tcp main owns the socket, the flags and the timer;
 *    the change runs there, on the io_wait thread (a KSR_TCPX_F_IOWAIT task,
 *    the caller waits for the result) or as a CONN_EOF command */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "tcp_conn_api.h" /* tcp_conn.h: before tcp_reactor.h */
#include "dprint.h"
#include "globals.h"
#include "pt.h"
#include "pass_fd.h"
#include "timer_ticks.h"
#include "mem/shm_mem.h"
#include "tcp_options.h"
#include "tcp_mtops.h"
#include "tcp_reactor.h"

/* flags TCPCONN_CTRL_SET_FLAGS accepts */
#define TCPCONN_API_FLAGS F_CONN_CLOSE_EV

enum tcpconn_api_op
{
	TCPCONN_OP_KA_ON = 1, /* p1 idle, p2 cnt, p3 intvl */
	TCPCONN_OP_KA_OFF,
	TCPCONN_OP_FLAGS,	/* p1 flags */
	TCPCONN_OP_LIFETIME /* p1 seconds */
};

/* keepalive options on fd: 1 success, -1 failure */
static int tcpconn_api_keepalive(int fd, int on, int idle, int cnt, int intvl)
{
#if !defined(HAVE_SO_KEEPALIVE) || !defined(HAVE_TCP_KEEPCNT) \
		|| !defined(HAVE_TCP_KEEPINTVL)
	LM_ERR("tcp keepalive options are not supported on this platform\n");
	return -1;
#else
	int optval;

	if(!on) {
		optval = 0;
		if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))
				< 0) {
			LM_WARN("failed to disable SO_KEEPALIVE: %s\n", strerror(errno));
			return -1;
		}
		LM_DBG("keepalive disabled for fd=%d\n", fd);
		return 1;
	}
	optval = 1;
	if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
		LM_ERR("failed to enable SO_KEEPALIVE: %s\n", strerror(errno));
		return -1;
	}
#ifdef HAVE_TCP_KEEPIDLE
	if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0) {
		LM_ERR("failed to set keepalive idle interval: %s\n", strerror(errno));
	}
#else
	LM_DBG("TCP_KEEPIDLE option not available - ignoring\n");
#endif
	if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) < 0) {
		LM_ERR("failed to set maximum keepalive count: %s\n", strerror(errno));
	}
	if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) < 0) {
		LM_ERR("failed to set keepalive probes interval: %s\n",
				strerror(errno));
	}
	LM_DBG("keepalive enabled for fd=%d, idle=%d, cnt=%d, intvl=%d\n", fd, idle,
			cnt, intvl);
	return 1;
#endif
}

/* modes 0/1: a copy of c's fd from tcp main; the caller holds a ref on c and
 * closes the fd. Returns the fd or -1 */
static int tcpconn_api_get_fd(struct tcp_connection *c)
{
	struct tcp_connection *tmp;
	long msg[2];
	int fd = -1;

	msg[0] = (long)c;
	msg[1] = CONN_GET_FD;
	if(unlikely(send_all(unix_tcp_sock, msg, sizeof(msg)) <= 0)) {
		LM_ERR("failed to send fd request: %s (%d)\n", strerror(errno), errno);
		return -1;
	}
	if(unlikely(receive_fd(unix_tcp_sock, &tmp, sizeof(tmp), &fd, MSG_WAITALL)
				<= 0)) {
		LM_ERR("failed to get fd (receive_fd): %s (%d)\n", strerror(errno),
				errno);
		return -1;
	}
	return fd;
}

/* mode 2: a request run by tcp main on its io_wait thread */
typedef struct tcpconn_api_task
{
	struct tcp_connection *c; /* the requester holds a ref */
	int op;					  /* TCPCONN_OP_* */
	int p1;
	int p2;
	int p3;
} tcpconn_api_task_t;

/* mode 2 [io_wait]: apply the request to c, result code back to pidx */
static void tcpconn_api_task_exec(void *p, int pidx)
{
	tcpconn_api_task_t *t = (tcpconn_api_task_t *)p;
	struct tcp_connection *c = t->c;
	tcpx_task_result_t *rtask;
	int rc = -1;

	switch(t->op) {
		case TCPCONN_OP_KA_ON:
		case TCPCONN_OP_KA_OFF:
			if(c->state == S_CONN_BAD || (c->flags & F_CONN_FD_CLOSED)
					|| c->s == -1)
				break;
			rc = tcpconn_api_keepalive(
					c->s, t->op == TCPCONN_OP_KA_ON, t->p1, t->p2, t->p3);
			break;
		case TCPCONN_OP_FLAGS:
			c->flags |= (unsigned int)t->p1;
			rc = 1;
			break;
		case TCPCONN_OP_LIFETIME:
			tcp_reactor_set_lifetime(c, S_TO_TICKS(t->p1));
			rc = 1;
			break;
		default:
			LM_BUG("unknown request %d\n", t->op);
	}
	rtask = (tcpx_task_result_t *)shm_mallocxz(sizeof(tcpx_task_result_t));
	if(rtask == NULL)
		SHM_MEM_ERROR;
	else
		rtask->code = rc;
	ksr_tcpx_thread_eresult(rtask, pidx);
}

/* mode 2: run the request on c in tcp main and wait for its result; the
 * caller holds a ref on c. Returns the result (1 success) or -1 */
static int tcpconn_api_run(
		struct tcp_connection *c, int op, int p1, int p2, int p3)
{
	tcpx_task_t *ptask;
	tcpx_task_result_t *rtask = NULL;
	tcpconn_api_task_t *t;
	int rc = -1;

	ptask = (tcpx_task_t *)shm_mallocxz(
			sizeof(tcpx_task_t) + sizeof(tcpconn_api_task_t));
	if(ptask == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	ptask->exec = tcpconn_api_task_exec;
	ptask->param = (void *)((char *)ptask + sizeof(tcpx_task_t));
	ptask->flags = KSR_TCPX_F_IOWAIT;
	t = (tcpconn_api_task_t *)ptask->param;
	t->c = c;
	t->op = op;
	t->p1 = p1;
	t->p2 = p2;
	t->p3 = p3;

	if(ksr_tcpx_task_send(ptask, process_no) < 0) {
		LM_ERR("failed to send the request to tcp main\n");
		shm_free(ptask);
		return -1;
	}
	if(ksr_tcpx_task_result_recv(&rtask, process_no) < 0) {
		/* tcp main may still use ptask: leave it */
		LM_ERR("failed to receive the result from tcp main\n");
		return -1;
	}
	if(rtask != NULL) {
		rc = rtask->code;
		shm_free(rtask);
	}
	shm_free(ptask);
	return rc;
}

static int tcpconn_api_close(int conid, enum tcp_closed_reason reason)
{
	struct tcp_connection *c;
	long mcmd[2];

	if((c = tcpconn_get(conid, 0, 0, 0, 0)) == NULL)
		return TCPCONN_API_NOCONN;
	mcmd[0] = (long)c;
	mcmd[1] = CONN_EOF;
	if(reason != 0)
		c->event = reason;
	/* mode 2: the flag words belong to the io_wait thread; CONN_EOF closes c
	 * there */
	if(ksr_tcp_main_threads != 2) {
		c->send_flags.f |= SND_F_CON_CLOSE;
		c->flags |= F_CONN_FORCE_EOF;
	}
	if(unlikely(send_all(unix_tcp_sock, mcmd, sizeof(mcmd)) <= 0)) {
		LM_ERR("failed to send close request: %s (%d)\n", strerror(errno),
				errno);
		tcpconn_chld_put(c);
		return -1;
	}
	/* the ref travels with CONN_EOF */
	return 1;
}

static int tcpconn_api_set_lifetime(int conid, unsigned int secs)
{
	struct tcp_connection *c;
	int rc;

	if((c = tcpconn_get(conid, 0, 0, 0, 0)) == NULL)
		return TCPCONN_API_NOCONN;
	if(ksr_tcp_main_threads == 2) {
		rc = tcpconn_api_run(c, TCPCONN_OP_LIFETIME, (int)secs, 0, 0);
	} else {
		c->lifetime = S_TO_TICKS(secs);
		c->timeout = get_ticks_raw() + c->lifetime;
		LM_DBG("new connection lifetime for conid=%d: %d\n", c->id, c->timeout);
		rc = 1;
	}
	tcpconn_chld_put(c);
	return rc;
}

static int tcpconn_api_set_keepalive(
		int conid, int on, int idle, int cnt, int intvl)
{
	struct tcp_connection *c;
	int fd;
	int rc;

	if((c = tcpconn_get(conid, 0, 0, 0, 0)) == NULL)
		return TCPCONN_API_NOCONN;
	if(ksr_tcp_main_threads == 2) {
		rc = tcpconn_api_run(
				c, on ? TCPCONN_OP_KA_ON : TCPCONN_OP_KA_OFF, idle, cnt, intvl);
	} else if(c->reader_pid == my_pid() && c->fd >= 0) {
		/* this reader holds c: its own fd */
		rc = tcpconn_api_keepalive(c->fd, on, idle, cnt, intvl);
	} else {
		fd = tcpconn_api_get_fd(c);
		rc = (fd >= 0) ? tcpconn_api_keepalive(fd, on, idle, cnt, intvl) : -1;
		if(fd >= 0)
			close(fd);
	}
	tcpconn_chld_put(c);
	return rc;
}

static int tcpconn_api_set_flags(int conid, unsigned int flags)
{
	struct tcp_connection *c;
	int rc;

	if(flags & ~TCPCONN_API_FLAGS) {
		LM_ERR("flags 0x%x outside the allowed set 0x%x\n", flags,
				TCPCONN_API_FLAGS);
		return -1;
	}
	if((c = tcpconn_get(conid, 0, 0, 0, 0)) == NULL)
		return TCPCONN_API_NOCONN;
	if(ksr_tcp_main_threads == 2) {
		rc = tcpconn_api_run(c, TCPCONN_OP_FLAGS, (int)flags, 0, 0);
	} else {
		c->flags |= flags;
		rc = 1;
	}
	tcpconn_chld_put(c);
	return rc;
}

int tcpconn_ctrl(int conid, int cmd, ...)
{
	struct tcpconn_keepalive *ka;
	va_list ap;
	int rc;

	va_start(ap, cmd);
	switch(cmd) {
		case TCPCONN_CTRL_CLOSE:
			rc = tcpconn_api_close(
					conid, (enum tcp_closed_reason)va_arg(ap, int));
			break;
		case TCPCONN_CTRL_SET_LIFETIME:
			rc = tcpconn_api_set_lifetime(conid, va_arg(ap, unsigned int));
			break;
		case TCPCONN_CTRL_SET_KEEPALIVE:
			ka = va_arg(ap, struct tcpconn_keepalive *);
			rc = (ka == NULL) ? -1
							  : tcpconn_api_set_keepalive(conid, ka->on,
										ka->idle, ka->cnt, ka->intvl);
			break;
		case TCPCONN_CTRL_SET_FLAGS:
			rc = tcpconn_api_set_flags(conid, va_arg(ap, unsigned int));
			break;
		default:
			LM_ERR("unknown tcpconn_ctrl() command %d\n", cmd);
			rc = -1;
	}
	va_end(ap);
	return rc;
}
