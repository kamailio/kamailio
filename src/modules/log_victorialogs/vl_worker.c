/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 * Copyright (C) 2025-2026 Michael Furmur (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

/**
 * Sender process: receives packed log entries from the SIP workers over a
 * socket pair, serializes them into a queue and ships the queue to
 * VictoriaLogs with curl multi driven by epoll.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/time.h>
#include <time.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm.h"

#include "vl_worker.h"
#include "vl_async.h"
#include "log_victorialogs.h"
#include "curl_defs.h"

/* timerfds + pair socket + curl sockets */
#define VL_MAX_EPOLL_EVENTS 64
/* minimal batch delay in milliseconds */
#define VL_MIN_BATCH_DELAY 100

static int vl_pair_sockets[2] = {-1, -1};
static int vl_epollfd = -1;
static int vl_mcurl_timerfd = -1;
static int vl_batch_delay_timerfd = -1;

static CURLM *vl_curl_multi = 0;

static int vl_sock_cb(
		CURL *easy, curl_socket_t s, int what, void *user_data, void *socketp);
static int vl_multi_timer_cb(CURLM *multi, long timeout_ms, void *user_data);
static void vl_curl_check_multi_info(void);

static inline void vl_set_timespec_ms(struct timespec *tmr, long mstime)
{
	tmr->tv_sec = (time_t)(mstime / 1000L);
	tmr->tv_nsec = 1000000L * (mstime % 1000L);
}

static int vl_epoll_link(int fd, uint32_t events)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	return epoll_ctl(vl_epollfd, EPOLL_CTL_ADD, fd, &ev);
}

static int vl_worker_init(void)
{
	struct itimerspec tmr;
	long batch_delay = vl_batch_delay;

	memset(&tmr, 0, sizeof(tmr));

	vl_epollfd = epoll_create1(0);
	if(vl_epollfd < 0) {
		LM_ERR("couldn't create epoll fd: %s\n", strerror(errno));
		return -1;
	}

	vl_mcurl_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(vl_mcurl_timerfd < 0) {
		LM_ERR("couldn't create curl timerfd: %s\n", strerror(errno));
		return -1;
	}
	if(vl_epoll_link(vl_mcurl_timerfd, EPOLLIN) == -1) {
		LM_ERR("couldn't add curl timerfd to epoll: %s\n", strerror(errno));
		return -1;
	}

	if(batch_delay < VL_MIN_BATCH_DELAY)
		batch_delay = VL_MIN_BATCH_DELAY;

	vl_batch_delay_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(vl_batch_delay_timerfd < 0) {
		LM_ERR("couldn't create batch timerfd: %s\n", strerror(errno));
		return -1;
	}
	if(vl_epoll_link(vl_batch_delay_timerfd, EPOLLIN) == -1) {
		LM_ERR("couldn't add batch timerfd to epoll: %s\n", strerror(errno));
		return -1;
	}

	vl_set_timespec_ms(&tmr.it_value, batch_delay);
	vl_set_timespec_ms(&tmr.it_interval, batch_delay);
	if(timerfd_settime(vl_batch_delay_timerfd, 0, &tmr, NULL) == -1) {
		LM_ERR("couldn't arm batch timerfd: %s\n", strerror(errno));
		return -1;
	}

	if(vl_epoll_link(vl_pair_sockets[0], EPOLLIN) == -1) {
		LM_ERR("couldn't add pair socket to epoll: %s\n", strerror(errno));
		return -1;
	}

	vl_curl_multi = curl_multi_init();
	if(!vl_curl_multi) {
		LM_ERR("curl_multi_init() failed\n");
		return -1;
	}
	multi_setopt(vl_curl_multi, CURLMOPT_SOCKETFUNCTION, vl_sock_cb);
	multi_setopt(vl_curl_multi, CURLMOPT_TIMERFUNCTION, vl_multi_timer_cb);
	return 0;
}

void vl_worker_destroy(void)
{
	struct itimerspec tmr;

	memset(&tmr, 0, sizeof(tmr));

	if(vl_curl_multi) {
		curl_multi_cleanup(vl_curl_multi);
		vl_curl_multi = 0;
	}

	if(vl_mcurl_timerfd >= 0) {
		timerfd_settime(vl_mcurl_timerfd, 0, &tmr, NULL);
		close(vl_mcurl_timerfd);
		vl_mcurl_timerfd = -1;
	}
	if(vl_batch_delay_timerfd >= 0) {
		timerfd_settime(vl_batch_delay_timerfd, 0, &tmr, NULL);
		close(vl_batch_delay_timerfd);
		vl_batch_delay_timerfd = -1;
	}
	if(vl_epollfd >= 0) {
		close(vl_epollfd);
		vl_epollfd = -1;
	}
}

static bool vl_worker_on_batch_timer(void)
{
	uint64_t ticks = 0;

	if(read(vl_batch_delay_timerfd, &ticks, sizeof(ticks)) != sizeof(ticks)
			&& errno != EAGAIN)
		LM_ERR("read(batch timerfd): %s\n", strerror(errno));

	return true;
}

static bool vl_worker_on_sockpeer(void)
{
	bool ret = false;
	async_log_entry_t *r;

	for(;;) {
		ssize_t bytes = recvfrom(vl_pair_sockets[0], &r,
				sizeof(async_log_entry_t *), 0, NULL, 0);

		if(unlikely(bytes != sizeof(async_log_entry_t *)))
			break;

		if(unlikely(!r))
			continue;

		ret |= vl_add_message_to_queue(r);
		shm_free(r);
	}
	return ret;
}

static void vl_worker_on_curl_timer(void)
{
	int curl_running_handles;
	uint64_t ticks;

	if(read(vl_mcurl_timerfd, &ticks, sizeof(ticks)) != sizeof(ticks)
			&& errno != EAGAIN)
		LM_ERR("read(curl timerfd): %s\n", strerror(errno));

	curl_multi_socket_action(
			vl_curl_multi, CURL_SOCKET_TIMEOUT, 0, &curl_running_handles);
	vl_curl_check_multi_info();
}

static void vl_worker_on_curl_socket(int socket, uint32_t events)
{
	int action = 0, curl_running_handles = 0;

	if(events & EPOLLIN)
		action |= CURL_CSELECT_IN;
	if(events & EPOLLOUT)
		action |= CURL_CSELECT_OUT;
	if(events & EPOLLERR)
		action |= CURL_CSELECT_ERR;

	curl_multi_socket_action(
			vl_curl_multi, socket, action, &curl_running_handles);
	vl_curl_check_multi_info();
}

void vl_worker_run(void)
{
	int n, i;
	bool send_events;
	struct epoll_event events[VL_MAX_EPOLL_EVENTS];

	if(vl_worker_init()) {
		LM_ERR("failed to initialize sender process\n");
		return;
	}

	while(1) {
		n = epoll_wait(vl_epollfd, events, VL_MAX_EPOLL_EVENTS, -1);

		if(n == -1) {
			if(errno != EINTR)
				LM_ERR("epoll_wait(): %s\n", strerror(errno));
			continue;
		}

		send_events = false;
		for(i = 0; i < n; ++i) {
			if(vl_mcurl_timerfd == events[i].data.fd) {
				vl_worker_on_curl_timer();
			} else if(vl_batch_delay_timerfd == events[i].data.fd) {
				send_events |= vl_worker_on_batch_timer();
			} else if(vl_pair_sockets[0] == events[i].data.fd) {
				send_events |= vl_worker_on_sockpeer();
			} else {
				vl_worker_on_curl_socket(events[i].data.fd, events[i].events);
			}
		}
		if(send_events) {
			vl_send_queue_messages();
		}
	}
}

int vl_worker_init_pair_sockets(void)
{
	if(socketpair(PF_LOCAL, SOCK_DGRAM | SOCK_NONBLOCK, 0, vl_pair_sockets)
			< 0) {
		LM_ERR("opening dgram socket pair: %s\n", strerror(errno));
		return -1;
	}

	LM_DBG("inter-process notification sockets initialized\n");
	return 0;
}

void vl_worker_close_sockets_child(void)
{
	LM_DBG("closing the notification socket used by children\n");
	close(vl_pair_sockets[1]);
	vl_pair_sockets[1] = -1;
}

void vl_worker_close_sockets_parent(void)
{
	LM_DBG("closing the notification socket used by parent\n");
	close(vl_pair_sockets[0]);
	vl_pair_sockets[0] = -1;
}

bool vl_worker_send(void *entry)
{
	ssize_t ret;

	ret = sendto(vl_pair_sockets[1], &entry, sizeof(async_log_entry_t *), 0,
			NULL, 0);
	if(ret != sizeof(async_log_entry_t *)) {
		LM_ERR("sendto ret: %ld, errno: %d (%s)\n", (long)ret, errno,
				strerror(errno));
		return false;
	}
	return true;
}

int vl_conn_add(struct curl_conn_handle *conn)
{
	LM_DBG("add connection %p\n", conn);
	if(CURLM_OK != curl_multi_add_handle(vl_curl_multi, conn->curl)) {
		LM_ERR("curl_multi_add_handle() failed\n");
		return -1;
	}
	return 0;
}

void vl_conn_remove(struct curl_conn_handle *conn)
{
	if(conn->headers)
		curl_slist_free_all(conn->headers);
	if(conn->curl) {
		curl_multi_remove_handle(vl_curl_multi, conn->curl);
		curl_easy_cleanup(conn->curl);
	}
	if(conn->body.s)
		pkg_free(conn->body.s);
	pkg_free(conn);
}

static int vl_sock_cb(
		CURL *easy, curl_socket_t s, int what, void *user_data, void *socketp)
{
	struct epoll_event ev;

	(void)easy;
	(void)user_data;

	if(CURL_POLL_NONE == what)
		return 0;
	if(CURL_POLL_REMOVE == what) {
		if(CURLM_OK != curl_multi_assign(vl_curl_multi, s, 0)) {
			LM_ERR("curl_multi_assign(%d, 0) failed\n", s);
		}
		LM_DBG("delete socket %d from epoll %d\n", s, vl_epollfd);
		if(-1 == epoll_ctl(vl_epollfd, EPOLL_CTL_DEL, s, NULL)) {
			LM_DBG("couldn't delete curl fd from epoll: %s\n", strerror(errno));
		}
		return 0;
	}

	memset(&ev, 0, sizeof(ev));
	ev.data.fd = s;
	switch(what) {
		case CURL_POLL_IN:
			ev.events = EPOLLIN | EPOLLERR;
			break;
		case CURL_POLL_OUT:
			ev.events = EPOLLOUT | EPOLLERR;
			break;
		case CURL_POLL_INOUT:
			ev.events = EPOLLOUT | EPOLLIN | EPOLLERR;
			break;
	}

	if(socketp) {
		LM_DBG("update socket %d in epoll\n", s);
		if(-1 == epoll_ctl(vl_epollfd, EPOLL_CTL_MOD, s, &ev)) {
			LM_ERR("couldn't change curl fd in epoll: %s\n", strerror(errno));
			return -1;
		}
	} else {
		LM_DBG("add socket %d to epoll\n", s);
		if(CURLM_OK != curl_multi_assign(vl_curl_multi, s, (void *)1)) {
			LM_ERR("curl_multi_assign(%d, 1) failed\n", s);
		}
		if(-1 == epoll_ctl(vl_epollfd, EPOLL_CTL_ADD, s, &ev)) {
			LM_ERR("couldn't add curl fd to epoll: %s\n", strerror(errno));
			return -1;
		}
	}

	return 0;
}

static int vl_multi_timer_cb(CURLM *multi, long timeout_ms, void *user_data)
{
	struct itimerspec tmr;

	(void)multi;
	(void)user_data;

	memset(&tmr, 0, sizeof(tmr));

	if(timeout_ms > 0) {
		vl_set_timespec_ms(&tmr.it_value, timeout_ms);
	} else if(timeout_ms == 0) {
		/* call socket_action as soon as possible */
		tmr.it_value.tv_nsec = 1;
	}
	/* timeout_ms == -1: disarm the timer */

	timerfd_settime(vl_mcurl_timerfd, 0, &tmr, NULL);
	return 0;
}

static void vl_finish_curl_conn(struct curl_conn_handle *conn, CURLcode code)
{
	long http_response_code = 0;
	bool success;
	struct vl_target *next;
	struct vl_target *failover_target;

	curl_easy_getinfo(conn->curl, CURLINFO_RESPONSE_CODE, &http_response_code);

	success = (code == CURLE_OK && http_response_code >= 200
			   && http_response_code < 300);

	vl_send_queue_messages_fin(success);

	if(success) {
		/* continue to the same destination */
		vl_curl_send_message(conn->target, conn->failover_target);
		return;
	}

	LM_ERR("sending to %s failed: curl code=%d (%s), http code=%ld\n",
			conn->target->url.s, code, curl_easy_strerror(code),
			http_response_code);

	update_stat(vl_stat_err_total, 1);

	/* try the next target, stop when the full round of targets failed */
	failover_target =
			conn->failover_target ? conn->failover_target : conn->target;

	next = SLIST_NEXT(conn->target, next);
	if(!next)
		next = SLIST_FIRST(&vl_targets);

	if(next != failover_target)
		vl_curl_send_message(next, failover_target);
}

static void vl_curl_check_multi_info(void)
{
	CURLMsg *msg;
	int msgs_left;
	struct curl_conn_handle *conn;

	while((msg = curl_multi_info_read(vl_curl_multi, &msgs_left))) {
		if(msg->msg == CURLMSG_DONE) {
			LM_DBG("easy=%p result=%d\n", msg->easy_handle, msg->data.result);
			conn = 0;
			curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &conn);
			if(!conn)
				continue;
			vl_finish_curl_conn(conn, msg->data.result);
			vl_conn_remove(conn);
		}
	}
}
