/*
 * Copyright (C) 2026 S-P Chan
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* pthread_mutexattr_setrobust()/pthread_mutex_consistent() need _GNU_SOURCE
 * (or _POSIX_C_SOURCE >= 200809) on glibc; must be set before any include.
 * Robust mutexes are a POSIX option not available on every platform (notably
 * macOS). The build probes for them and defines HAVE_PTHREAD_MUTEX_ROBUST (see
 * cmake/lock_methods.cmake). Without it the mutex is still process-shared but
 * not robust: a process dying while holding the lock is not recovered - which is
 * acceptable for the current in-process reactor use. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "dprint.h"
#include "tcp_cond.h"

int tcp_cond_init(tcp_cond_t *cond)
{
	pthread_mutexattr_t mattr;
	pthread_condattr_t cattr;

	/* process-shared + robust mutex */
	if(pthread_mutexattr_init(&mattr) != 0) {
		LM_ERR("failed to init mutex attributes\n");
		return -1;
	}
	if(pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED) != 0
#ifdef HAVE_PTHREAD_MUTEX_ROBUST
			|| pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST) != 0
#endif
			|| pthread_mutex_init(&cond->m, &mattr) != 0) {
		LM_ERR("failed to set up process-shared mutex\n");
		pthread_mutexattr_destroy(&mattr);
		return -1;
	}
	pthread_mutexattr_destroy(&mattr);

	/* process-shared condition variable */
	if(pthread_condattr_init(&cattr) != 0) {
		LM_ERR("failed to init cond attributes\n");
		pthread_mutex_destroy(&cond->m);
		return -1;
	}
	if(pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED) != 0
			|| pthread_cond_init(&cond->c, &cattr) != 0) {
		LM_ERR("failed to set up process-shared cond\n");
		pthread_condattr_destroy(&cattr);
		pthread_mutex_destroy(&cond->m);
		return -1;
	}
	pthread_condattr_destroy(&cattr);

	return 0;
}

void tcp_cond_destroy(tcp_cond_t *cond)
{
	pthread_cond_destroy(&cond->c);
	pthread_mutex_destroy(&cond->m);
}

#ifdef HAVE_PTHREAD_MUTEX_ROBUST
/* Recover a robust mutex whose previous owner died while holding it: we now own
 * the lock, but the data it protects may be inconsistent. Marking it consistent
 * keeps the mutex usable; without this it would become ENOTRECOVERABLE and every
 * future lock attempt would fail. */
static void tcp_cond_recover_owner_dead(tcp_cond_t *cond)
{
	LM_WARN("robust mutex %p owner died; recovering (state may be partial)\n",
			(void *)cond);
	if(pthread_mutex_consistent(&cond->m) != 0)
		LM_ERR("failed to make robust mutex %p consistent\n", (void *)cond);
}
#endif /* HAVE_PTHREAD_MUTEX_ROBUST */

void tcp_cond_lock(tcp_cond_t *cond)
{
	int ret;

	ret = pthread_mutex_lock(&cond->m);
#ifdef HAVE_PTHREAD_MUTEX_ROBUST
	if(ret == EOWNERDEAD) {
		tcp_cond_recover_owner_dead(cond);
		return;
	}
#endif /* HAVE_PTHREAD_MUTEX_ROBUST */
	if(ret != 0) {
		LM_ERR("pthread_mutex_lock failed: %s (%d)\n", strerror(ret), ret);
	}
}

void tcp_cond_unlock(tcp_cond_t *cond)
{
	int ret;

	ret = pthread_mutex_unlock(&cond->m);
	if(ret != 0)
		LM_ERR("pthread_mutex_unlock failed: %s (%d)\n", strerror(ret), ret);
}

void tcp_cond_wait(tcp_cond_t *cond)
{
	int ret;

	/* re-acquires the mutex on wakeup; that re-acquire can also report a dead
	 * previous owner on a robust mutex */
	ret = pthread_cond_wait(&cond->c, &cond->m);
#ifdef HAVE_PTHREAD_MUTEX_ROBUST
	if(ret == EOWNERDEAD) {
		tcp_cond_recover_owner_dead(cond);
		return;
	}
#endif /* HAVE_PTHREAD_MUTEX_ROBUST */
	if(ret != 0) {
		LM_ERR("pthread_cond_wait failed: %s (%d)\n", strerror(ret), ret);
	}
}

void tcp_cond_signal(tcp_cond_t *cond)
{
	int ret;

	ret = pthread_cond_signal(&cond->c);
	if(ret != 0)
		LM_ERR("pthread_cond_signal failed: %s (%d)\n", strerror(ret), ret);
}

void tcp_cond_broadcast(tcp_cond_t *cond)
{
	int ret;

	ret = pthread_cond_broadcast(&cond->c);
	if(ret != 0)
		LM_ERR("pthread_cond_broadcast failed: %s (%d)\n", strerror(ret), ret);
}
