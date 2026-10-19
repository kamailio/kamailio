/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
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
 * the log server with curl multi driven by epoll.
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

#include "lh_worker.h"
#include "lh_async.h"
#include "log_http.h"
#include "curl_defs.h"

/* timerfds + pair socket + curl sockets */
#define LH_MAX_EPOLL_EVENTS 64
/* minimal batch delay in milliseconds */
#define LH_MIN_BATCH_DELAY 100

static int lh_pair_sockets[2] = {-1, -1};
static int lh_epollfd = -1;
static int lh_mcurl_timerfd = -1;
static int lh_batch_delay_timerfd = -1;

static CURLM *lh_curl_multi = 0;

static int lh_sock_cb(
		CURL *easy, curl_socket_t s, int what, void *user_data, void *socketp);
static int lh_multi_timer_cb(CURLM *multi, long timeout_ms, void *user_data);
static void lh_curl_check_multi_info(void);

static inline void lh_set_timespec_ms(struct timespec *tmr, long mstime)
{
	tmr->tv_sec = (time_t)(mstime / 1000L);
	tmr->tv_nsec = 1000000L * (mstime % 1000L);
}

static int lh_epoll_link(int fd, uint32_t events)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	return epoll_ctl(lh_epollfd, EPOLL_CTL_ADD, fd, &ev);
}

static int lh_worker_init(void)
{
	struct itimerspec tmr;
	long batch_delay = lh_batch_delay;

	memset(&tmr, 0, sizeof(tmr));

	lh_epollfd = epoll_create1(0);
	if(lh_epollfd < 0) {
		LM_ERR("couldn't create epoll fd: %s\n", strerror(errno));
		return -1;
	}

	lh_mcurl_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(lh_mcurl_timerfd < 0) {
		LM_ERR("couldn't create curl timerfd: %s\n", strerror(errno));
		return -1;
	}
	if(lh_epoll_link(lh_mcurl_timerfd, EPOLLIN) == -1) {
		LM_ERR("couldn't add curl timerfd to epoll: %s\n", strerror(errno));
		return -1;
	}

	if(batch_delay < LH_MIN_BATCH_DELAY)
		batch_delay = LH_MIN_BATCH_DELAY;

	lh_batch_delay_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(lh_batch_delay_timerfd < 0) {
		LM_ERR("couldn't create batch timerfd: %s\n", strerror(errno));
		return -1;
	}
	if(lh_epoll_link(lh_batch_delay_timerfd, EPOLLIN) == -1) {
		LM_ERR("couldn't add batch timerfd to epoll: %s\n", strerror(errno));
		return -1;
	}

	lh_set_timespec_ms(&tmr.it_value, batch_delay);
	lh_set_timespec_ms(&tmr.it_interval, batch_delay);
	if(timerfd_settime(lh_batch_delay_timerfd, 0, &tmr, NULL) == -1) {
		LM_ERR("couldn't arm batch timerfd: %s\n", strerror(errno));
		return -1;
	}

	if(lh_epoll_link(lh_pair_sockets[0], EPOLLIN) == -1) {
		LM_ERR("couldn't add pair socket to epoll: %s\n", strerror(errno));
		return -1;
	}

	lh_curl_multi = curl_multi_init();
	if(!lh_curl_multi) {
		LM_ERR("curl_multi_init() failed\n");
		return -1;
	}
	multi_setopt(lh_curl_multi, CURLMOPT_SOCKETFUNCTION, lh_sock_cb);
	multi_setopt(lh_curl_multi, CURLMOPT_TIMERFUNCTION, lh_multi_timer_cb);
	return 0;
}

void lh_worker_destroy(void)
{
	struct itimerspec tmr;

	memset(&tmr, 0, sizeof(tmr));

	if(lh_curl_multi) {
		curl_multi_cleanup(lh_curl_multi);
		lh_curl_multi = 0;
	}

	if(lh_mcurl_timerfd >= 0) {
		timerfd_settime(lh_mcurl_timerfd, 0, &tmr, NULL);
		close(lh_mcurl_timerfd);
		lh_mcurl_timerfd = -1;
	}
	if(lh_batch_delay_timerfd >= 0) {
		timerfd_settime(lh_batch_delay_timerfd, 0, &tmr, NULL);
		close(lh_batch_delay_timerfd);
		lh_batch_delay_timerfd = -1;
	}
	if(lh_epollfd >= 0) {
		close(lh_epollfd);
		lh_epollfd = -1;
	}
}

static bool lh_worker_on_batch_timer(void)
{
	uint64_t ticks = 0;

	if(read(lh_batch_delay_timerfd, &ticks, sizeof(ticks)) != sizeof(ticks)
			&& errno != EAGAIN)
		LM_ERR("read(batch timerfd): %s\n", strerror(errno));

	return true;
}

static bool lh_worker_on_sockpeer(void)
{
	bool ret = false;
	async_log_entry_t *r;

	for(;;) {
		ssize_t bytes = recvfrom(lh_pair_sockets[0], &r,
				sizeof(async_log_entry_t *), 0, NULL, 0);

		if(unlikely(bytes != sizeof(async_log_entry_t *)))
			break;

		if(unlikely(!r))
			continue;

		ret |= lh_add_message_to_queue(r);
		shm_free(r);
	}
	return ret;
}

static void lh_worker_on_curl_timer(void)
{
	int curl_running_handles;
	uint64_t ticks;

	if(read(lh_mcurl_timerfd, &ticks, sizeof(ticks)) != sizeof(ticks)
			&& errno != EAGAIN)
		LM_ERR("read(curl timerfd): %s\n", strerror(errno));

	curl_multi_socket_action(
			lh_curl_multi, CURL_SOCKET_TIMEOUT, 0, &curl_running_handles);
	lh_curl_check_multi_info();
}

static void lh_worker_on_curl_socket(int socket, uint32_t events)
{
	int action = 0, curl_running_handles = 0;

	if(events & EPOLLIN)
		action |= CURL_CSELECT_IN;
	if(events & EPOLLOUT)
		action |= CURL_CSELECT_OUT;
	if(events & EPOLLERR)
		action |= CURL_CSELECT_ERR;

	curl_multi_socket_action(
			lh_curl_multi, socket, action, &curl_running_handles);
	lh_curl_check_multi_info();
}

void lh_worker_run(void)
{
	int n, i;
	bool send_events;
	struct epoll_event events[LH_MAX_EPOLL_EVENTS];

	if(lh_worker_init()) {
		LM_ERR("failed to initialize sender process\n");
		return;
	}

	while(1) {
		n = epoll_wait(lh_epollfd, events, LH_MAX_EPOLL_EVENTS, -1);

		if(n == -1) {
			if(errno != EINTR)
				LM_ERR("epoll_wait(): %s\n", strerror(errno));
			continue;
		}

		send_events = false;
		for(i = 0; i < n; ++i) {
			if(lh_mcurl_timerfd == events[i].data.fd) {
				lh_worker_on_curl_timer();
			} else if(lh_batch_delay_timerfd == events[i].data.fd) {
				send_events |= lh_worker_on_batch_timer();
			} else if(lh_pair_sockets[0] == events[i].data.fd) {
				send_events |= lh_worker_on_sockpeer();
			} else {
				lh_worker_on_curl_socket(events[i].data.fd, events[i].events);
			}
		}
		if(send_events) {
			lh_send_queue_messages();
		}
	}
}

int lh_worker_init_pair_sockets(void)
{
	if(socketpair(PF_LOCAL, SOCK_DGRAM | SOCK_NONBLOCK, 0, lh_pair_sockets)
			< 0) {
		LM_ERR("opening dgram socket pair: %s\n", strerror(errno));
		return -1;
	}

	LM_DBG("inter-process notification sockets initialized\n");
	return 0;
}

void lh_worker_close_sockets_child(void)
{
	LM_DBG("closing the notification socket used by children\n");
	close(lh_pair_sockets[1]);
	lh_pair_sockets[1] = -1;
}

void lh_worker_close_sockets_parent(void)
{
	LM_DBG("closing the notification socket used by parent\n");
	close(lh_pair_sockets[0]);
	lh_pair_sockets[0] = -1;
}

bool lh_worker_send(void *entry)
{
	ssize_t ret;

	ret = sendto(lh_pair_sockets[1], &entry, sizeof(async_log_entry_t *), 0,
			NULL, 0);
	if(ret != sizeof(async_log_entry_t *)) {
		LM_ERR("sendto ret: %ld, errno: %d (%s)\n", (long)ret, errno,
				strerror(errno));
		return false;
	}
	return true;
}

int lh_conn_add(struct curl_conn_handle *conn)
{
	LM_DBG("add connection %p\n", conn);
	if(CURLM_OK != curl_multi_add_handle(lh_curl_multi, conn->curl)) {
		LM_ERR("curl_multi_add_handle() failed\n");
		return -1;
	}
	return 0;
}

void lh_conn_remove(struct curl_conn_handle *conn)
{
	if(conn->headers)
		curl_slist_free_all(conn->headers);
	if(conn->curl) {
		curl_multi_remove_handle(lh_curl_multi, conn->curl);
		curl_easy_cleanup(conn->curl);
	}
	if(conn->body.s)
		pkg_free(conn->body.s);
	pkg_free(conn);
}

static int lh_sock_cb(
		CURL *easy, curl_socket_t s, int what, void *user_data, void *socketp)
{
	struct epoll_event ev;

	(void)easy;
	(void)user_data;

	if(CURL_POLL_NONE == what)
		return 0;
	if(CURL_POLL_REMOVE == what) {
		if(CURLM_OK != curl_multi_assign(lh_curl_multi, s, 0)) {
			LM_ERR("curl_multi_assign(%d, 0) failed\n", s);
		}
		LM_DBG("delete socket %d from epoll %d\n", s, lh_epollfd);
		if(-1 == epoll_ctl(lh_epollfd, EPOLL_CTL_DEL, s, NULL)) {
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
		if(-1 == epoll_ctl(lh_epollfd, EPOLL_CTL_MOD, s, &ev)) {
			LM_ERR("couldn't change curl fd in epoll: %s\n", strerror(errno));
			return -1;
		}
	} else {
		LM_DBG("add socket %d to epoll\n", s);
		if(CURLM_OK != curl_multi_assign(lh_curl_multi, s, (void *)1)) {
			LM_ERR("curl_multi_assign(%d, 1) failed\n", s);
		}
		if(-1 == epoll_ctl(lh_epollfd, EPOLL_CTL_ADD, s, &ev)) {
			LM_ERR("couldn't add curl fd to epoll: %s\n", strerror(errno));
			return -1;
		}
	}

	return 0;
}

static int lh_multi_timer_cb(CURLM *multi, long timeout_ms, void *user_data)
{
	struct itimerspec tmr;

	(void)multi;
	(void)user_data;

	memset(&tmr, 0, sizeof(tmr));

	if(timeout_ms > 0) {
		lh_set_timespec_ms(&tmr.it_value, timeout_ms);
	} else if(timeout_ms == 0) {
		/* call socket_action as soon as possible */
		tmr.it_value.tv_nsec = 1;
	}
	/* timeout_ms == -1: disarm the timer */

	timerfd_settime(lh_mcurl_timerfd, 0, &tmr, NULL);
	return 0;
}

static void lh_finish_curl_conn(struct curl_conn_handle *conn, CURLcode code)
{
	long http_response_code = 0;
	bool success;
	struct lh_target *next;
	struct lh_target *failover_target;

	curl_easy_getinfo(conn->curl, CURLINFO_RESPONSE_CODE, &http_response_code);

	success = (code == CURLE_OK && http_response_code >= 200
			   && http_response_code < 300);

	lh_send_queue_messages_fin(success);

	if(success) {
		/* continue to the same destination */
		lh_curl_send_message(conn->target, conn->failover_target);
		return;
	}

	LM_ERR("sending to %s failed: curl code=%d (%s), http code=%ld\n",
			conn->target->url.s, code, curl_easy_strerror(code),
			http_response_code);

	update_stat(lh_stat_err_total, 1);

	/* try the next target, stop when the full round of targets failed */
	failover_target =
			conn->failover_target ? conn->failover_target : conn->target;

	next = SLIST_NEXT(conn->target, next);
	if(!next)
		next = SLIST_FIRST(&lh_targets);

	if(next != failover_target)
		lh_curl_send_message(next, failover_target);
}

static void lh_curl_check_multi_info(void)
{
	CURLMsg *msg;
	int msgs_left;
	struct curl_conn_handle *conn;

	while((msg = curl_multi_info_read(lh_curl_multi, &msgs_left))) {
		if(msg->msg == CURLMSG_DONE) {
			LM_DBG("easy=%p result=%d\n", msg->easy_handle, msg->data.result);
			conn = 0;
			curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &conn);
			if(!conn)
				continue;
			lh_finish_curl_conn(conn, msg->data.result);
			lh_conn_remove(conn);
		}
	}
}
