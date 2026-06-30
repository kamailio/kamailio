/*
 * Copyright (C) 2025 S-P Chan
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

#ifndef _TCP_COND_H_
#define _TCP_COND_H_

#include <pthread.h>

/*
 * Cross-process condition variable for the TCP reactor thread pool
 * (tcp_main_threads == 2).
 *
 * The struct is meant to live in shared memory: TCP worker processes signal it
 * to submit write jobs, while the pool threads inside PROC_TCP_MAIN wait on it.
 * Because waiters and signalers are different PIDs sharing one shm page, both
 * the mutex and the condvar are PTHREAD_PROCESS_SHARED.
 *
 * The mutex is additionally PTHREAD_MUTEX_ROBUST: if a process dies while
 * holding the lock, the next tcp_cond_lock()/tcp_cond_wait() recovers it
 * (EOWNERDEAD -> pthread_mutex_consistent) instead of leaving every future
 * locker stuck at ENOTRECOVERABLE.
 */
typedef struct tcp_cond
{
	pthread_mutex_t m;
	pthread_cond_t c;
} tcp_cond_t;

/* Initialize a tcp_cond_t (typically allocated in shared memory).
 * Returns 0 on success, -1 on error. */
int tcp_cond_init(tcp_cond_t *cond);

/* Destroy a tcp_cond_t. Does not free the memory holding it. */
void tcp_cond_destroy(tcp_cond_t *cond);

/* Lock / unlock the embedded mutex. tcp_cond_lock() transparently recovers a
 * robust mutex abandoned by a crashed owner. */
void tcp_cond_lock(tcp_cond_t *cond);
void tcp_cond_unlock(tcp_cond_t *cond);

/* Wait on the condition. Must be called with the mutex held (via
 * tcp_cond_lock()). Recovers the robust mutex if it was abandoned while being
 * re-acquired on wakeup. */
void tcp_cond_wait(tcp_cond_t *cond);

/* Wake one / all waiters. */
void tcp_cond_signal(tcp_cond_t *cond);
void tcp_cond_broadcast(tcp_cond_t *cond);

#endif /* _TCP_COND_H_ */
