/**
 * $Id$
 *
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdlib.h>
#include <stdio.h> //temp
#include <sys/socket.h>
#include <event2/event.h>
#include <event2/buffer.h> //temp
#include <event2/bufferevent.h>
#include "test.h"
#include "../netstring.h"


int fd[2];
const int writer = 0;
const int reader = 1;
struct event_base *evbase;
struct bufferevent *bev;
netstring_t* ns_buffer;
char* ns;
char* next;


/* ***********************************************************************
 *                  tests for netstring_read_evbuffer                    *
 * ***********************************************************************/

//
// Test normal operation of netstring_read_fd, with data received in three chunks.
//
void ev_init(void (*cb)(struct bufferevent*, void*))
{
	ns_buffer = NULL;
	evbase = event_base_new();
	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
	evutil_make_socket_nonblocking(fd[reader]);
	bev = bufferevent_socket_new(evbase, fd[reader], BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, cb, NULL, NULL, NULL);
    bufferevent_enable(bev, EV_READ);
}

void read_evb_cb3(struct bufferevent *bev, void *ptr)
{
	int r = netstring_read_evbuffer(bev, &ns_buffer);
	assert_int_equal(0, r);
	if (r == 0)
		assert_string_equal("foobar-bizbaz", ns_buffer->string);

	int res = event_base_loopbreak(evbase);
	return;
}

void read_evb_cb2(struct bufferevent *bev, void *ptr)
{
	assert_int_equal(NETSTRING_INCOMPLETE, netstring_read_evbuffer(bev, &ns_buffer));
	send(fd[writer], next, 10, 0);
	bufferevent_setcb(bev, read_evb_cb3, NULL, NULL, NULL);
	return;
}

void read_evb_cb1(struct bufferevent *bev, void *ptr)
{
	assert_int_equal(NETSTRING_INCOMPLETE, netstring_read_evbuffer(bev, &ns_buffer));
	send(fd[writer], next, 5, 0);
	next = next+5;
	bufferevent_setcb(bev, read_evb_cb2, NULL, NULL, NULL);
	return;
}

void test_read_evbuffer()
{
	ns = "13:foobar-bizbaz,";
	ev_init(read_evb_cb1);
	send(fd[writer], ns, 2, 0);
	next = ns+2;
	event_base_dispatch(evbase);
}

void test_read_evbuffer_one_chunk()
{
	ns = "13:foobar-bizbaz,";
	ev_init(read_evb_cb3);
	send(fd[writer], ns, strlen(ns), 0);
	event_base_dispatch(evbase);
}

//
// Abnormal test scenarios for netstring_read_evbuffer
//

void read_evb_leading_zero_cb()
{
	assert_int_equal(NETSTRING_ERROR_LEADING_ZERO, netstring_read_evbuffer(bev, &ns_buffer));
	int res = event_base_loopbreak(evbase);
}

void test_read_evbuffer_leading_zero()
{
	ns = "0001:abbbbbbb,";
	ev_init(read_evb_leading_zero_cb);
	send(fd[writer], ns, strlen(ns), 0);
	event_base_dispatch(evbase);
}

void read_evb_no_length_cb()
{
	assert_int_equal(NETSTRING_ERROR_NO_LENGTH, netstring_read_evbuffer(bev, &ns_buffer));
	int res = event_base_loopbreak(evbase);
}

void test_read_evbuffer_no_length()
{
	ns = "a:......................b,";
	ev_init(read_evb_no_length_cb);
	send(fd[writer], ns, strlen(ns), 0);
	event_base_dispatch(evbase);
}

void read_evb_too_long_cb()
{
	assert_int_equal(NETSTRING_ERROR_TOO_LONG, netstring_read_evbuffer(bev, &ns_buffer));
	int res = event_base_loopbreak(evbase);
}

void test_read_evbuffer_too_long()
{
	ns = "999999999999999999999:...,";
	ev_init(read_evb_too_long_cb);
	send(fd[writer], ns, strlen(ns), 0);
	event_base_dispatch(evbase);
}

void read_evb_no_colon_cb()
{
	assert_int_equal(NETSTRING_ERROR_NO_COLON, netstring_read_evbuffer(bev, &ns_buffer));
	int res = event_base_loopbreak(evbase);
}

void test_read_evbuffer_no_colon()
{
	ns = "999abcc,";
	ev_init(read_evb_no_colon_cb);
	send(fd[writer], ns, strlen(ns), 0);
	event_base_dispatch(evbase);
}

void read_evb_no_comma_cb()
{
	assert_int_equal(NETSTRING_ERROR_NO_COMMA, netstring_read_evbuffer(bev, &ns_buffer));
	int res = event_base_loopbreak(evbase);
}

void test_read_evbuffer_no_comma()
{
	ns = "2:ab.";
	ev_init(read_evb_no_comma_cb);
	send(fd[writer], ns, strlen(ns), 0);
	event_base_dispatch(evbase);
}

/* ***********************************************************************
 *                    tests for netstring_read_fd                        *
 * ***********************************************************************/

//
// Test normal operation of netstring_read_fd, with data received in three chunks.
//
void test_read_fd()
{
	char* ns = "13:foobar-bizbaz,";
	char* temp;
	int fd[2];
	const int writer = 0;
	const int reader = 1;
	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    netstring_t* buffer = NULL;

	send(fd[writer], ns, 4, 0);
	assert_int_equal(NETSTRING_INCOMPLETE, netstring_read_fd(fd[reader], &buffer));
	temp = ns+4;
	send(fd[writer], temp, 3, 0);
	assert_int_equal(NETSTRING_INCOMPLETE, netstring_read_fd(fd[reader], &buffer));
	temp = temp+3;
	send(fd[writer], temp, 10, 0);
	int r = netstring_read_fd(fd[reader], &buffer);
	assert_int_equal(0, r);
	if (r == 0)
		assert_string_equal("foobar-bizbaz", buffer->string);
}


//
// Abnormal test scenarios for netstring_read_fd
//
void test_read_fd_leading_zero()
{
	char *ns = "0001:a,";
	int fd[2];
	const int writer = 0;
	const int reader = 1;
	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    netstring_t* buffer = NULL;

	send(fd[writer], ns, 7, 0);
	assert_int_equal(NETSTRING_ERROR_LEADING_ZERO, netstring_read_fd(fd[reader], &buffer));
}

void test_read_fd_no_length()
{
	char *ns = "ab,";
	int fd[2];
	const int writer = 0;
	const int reader = 1;
	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    netstring_t* buffer = NULL;

	send(fd[writer], ns, 3, 0);
	assert_int_equal(NETSTRING_ERROR_NO_LENGTH, netstring_read_fd(fd[reader], &buffer));
}

void test_read_fd_too_long()
{
	char *ns = "999999999999999999999:...,";
	int fd[2];
	const int writer = 0;
	const int reader = 1;
	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    netstring_t* buffer = NULL;

	send(fd[writer], ns, strlen(ns), 0);
	assert_int_equal(NETSTRING_ERROR_TOO_LONG, netstring_read_fd(fd[reader], &buffer));
}

void test_read_fd_no_colon()
{
	char *ns = "999ab,";
	int fd[2];
	const int writer = 0;
	const int reader = 1;
	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    netstring_t* buffer = NULL;

	send(fd[writer], ns, strlen(ns), 0);
	assert_int_equal(NETSTRING_ERROR_NO_COLON, netstring_read_fd(fd[reader], &buffer));
}

void test_read_fd_no_comma()
{
	char *ns = "5:......";
	int fd[2];
	const int writer = 0;
	const int reader = 1;
	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    netstring_t* buffer = NULL;

	send(fd[writer], ns, strlen(ns), 0);
	assert_int_equal(NETSTRING_ERROR_NO_COMMA, netstring_read_fd(fd[reader], &buffer));
}

///
// Test for netstring_encode_new
//
void test_encode_new()
{
	char *ns;
	char *data = "foobar-bizbaz";
	int len = netstring_encode_new(&ns, data, strlen(data));
	assert_int_equal(17, len);
	char *temp = malloc(len+1);
	memcpy(temp, ns, len);
	temp[len] = '\0';
	assert_string_equal("13:foobar-bizbaz,", temp);
}

// put the tests into a fixture...
//
void test_fixture_read_fd(void)
{
	printf("Running read_fd tests...\n");
	test_fixture_start();
	run_test(test_read_fd);
	run_test(test_read_fd_leading_zero);
	run_test(test_read_fd_no_length);
	run_test(test_read_fd_too_long);
	run_test(test_read_fd_no_colon);
	run_test(test_read_fd_no_comma);
	test_fixture_end();
}

void test_fixture_read_evbuffer(void)
{
	printf("Running read_evbuffer tests...\n");
	test_fixture_start();
	run_test(test_read_evbuffer);
	run_test(test_read_evbuffer_one_chunk);
	run_test(test_read_evbuffer_leading_zero);
	run_test(test_read_evbuffer_no_length);
	run_test(test_read_evbuffer_too_long);
//	run_test(test_read_evbuffer_no_colon);
//	...skip due to TODO in netstring.c
	run_test(test_read_evbuffer_no_comma);
	test_fixture_end();
}

void test_fixture_encode(void)
{
	printf("Running encode_new tests...\n");
	test_fixture_start();
	run_test(test_encode_new);
	test_fixture_end();
}

//
// put the fixture into a suite...
//
void all_tests(void)
{
	test_fixture_read_fd();
	test_fixture_read_evbuffer();
	test_fixture_encode();
}

//
// run the suite!
//
int main(int argc, char** argv)
{
	return run_tests(all_tests);
}

