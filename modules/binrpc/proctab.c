/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */

#ifdef EXTRA_DEBUG
#include <assert.h>
#include <stdlib.h>
#endif
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "proctab.h"

struct pt_entry {
	pid_t pid;
	union {
		int fd;
		struct ctrl_socket *cs;
	};
	size_t conns;
};

static struct pt_entry *pt;
static size_t pt_size = 0;


int pt_init(size_t count)
{
	if (! (pt = pkg_malloc(count * sizeof(struct pt_entry)))) {
		ERR("out of pkg memory.\n");
		return -1;
	}
	pt_size = count;
	memset(pt, 0, pt_size * sizeof(struct pt_entry));
	return 0;
}

int pt_ins(pid_t pid, int fd)
{
	static size_t idx = 0;

#ifdef EXTRA_DEBUG
	assert(idx < pt_size);
#endif
	DEBUG("PT insertion @#%zd, PID: %d, fd=%d.\n", idx, pid, fd);

	pt[idx].pid = pid;
	pt[idx].fd = fd;
	idx ++;
	return 0;
}

ssize_t pt_least_loaded(size_t *load)
{
	int i, choice = -1;
	size_t min;

	for (i = 0, min = INT_MAX; i < pt_size; i ++)
		if (pt[i].conns < min) {
			min = pt[i].conns;
			choice = i;
		}
#ifdef EXTRA_DEBUG
	assert(min < INT_MAX);
	assert(0 <= choice);
#else
	BUG("no worker available for new connection!\n");
#endif
	*load = min;
	DEBUG("least loaded @#%d, with %zd connections.\n", choice, min);
	return choice;
}

struct ctrl_socket *pt_update_load(ssize_t worker, int direction)
{
#ifdef EXTRA_DEBUG
	assert(worker < pt_size);
#endif
	if (direction < 0)
		pt[worker].conns --;
	else
		pt[worker].conns ++;
	DEBUG("binrpc worker PID#%d loaded with %zd connection(s) "
			"(after %s).\n", pt[worker].pid, pt[worker].conns, 
			(direction < 0) ? "unloading" : "loading");
	return pt[worker].cs;
}

pid_t pt_pid(ssize_t worker)
{
#ifdef EXTRA_DEBUG
	assert(worker < pt_size);
#endif
	return pt[worker].pid;
}

ssize_t pt_worker(pid_t child)
{
	ssize_t worker = -1, i;
	for (i = 0; i < pt_size; i ++)
		if (pt[i].pid == child)
			return i;
	BUG("worker index for PID#%d not found.\n", child);
#ifdef EXTRA_DEBUG
	abort();
#endif
	return worker;
}

int pt_fd2cs(void)
{
	int i;
	struct ctrl_socket *cs;

	for (i = 0; i < pt_size; i ++) {
		if (! (cs = add_fdpass_socket(pt[i].fd, pt[i].pid))) {
			ERR("failed to turn sock descriptor into control struct.\n");
			return -1;
		} else {
			pt[i].cs = cs;
		}
	}
	return 0;
}

void pt_free(int has_cs)
{
	int i;

	if (! has_cs)
		for (i = 0; i < pt_size; i ++)
			close(pt[i].fd);
	pkg_free(pt);
}
