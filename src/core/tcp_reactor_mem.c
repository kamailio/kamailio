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

/**
 * @file
 * @brief Mutex-wrapped pkg allocator for PROC_TCP_MAIN (see tcp_reactor_mem.h).
 */

#include "dprint.h"
#include "mem/pkg.h"
#include "tcp_reactor_mem.h"

#ifdef PKG_MALLOC
/* PKG_MALLOC build: pkg_* is served by Kamailio's own lock-free per-process
 * allocator via the _pkg_root vtable, which is what we must serialize. When
 * PKG_MALLOC is NOT defined pkg_* expands straight to the (thread-safe) system
 * malloc and _pkg_root is bypassed entirely, so there is nothing to lock - see
 * the no-op install at the bottom of this file. */

#include <pthread.h>

/* The single mutex that serializes all pkg allocation in this process once the
 * wrappers are installed, plus the saved originals we delegate to. Only ever
 * touched in PROC_TCP_MAIN (mode 2). */
static pthread_mutex_t _ksr_pkg_lock = PTHREAD_MUTEX_INITIALIZER;
static sr_malloc_f _ksr_pkg_orig_xmalloc;
static sr_malloc_f _ksr_pkg_orig_xmallocxz;
static sr_realloc_f _ksr_pkg_orig_xrealloc;
static sr_realloc_f _ksr_pkg_orig_xreallocxf;
static sr_free_f _ksr_pkg_orig_xfree;

/* The wrappers just take the mutex and delegate to the saved original. Two
 * sets: the DBG_SR_MEMORY build carries (file, func, line, mname) through the
 * allocator vtable, the normal build does not - match both signatures exactly. */
#ifdef DBG_SR_MEMORY
static void *_ksr_pkg_l_xmalloc(void *mbp, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xmalloc(mbp, size, file, func, line, mname);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void *_ksr_pkg_l_xmallocxz(void *mbp, size_t size, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xmallocxz(mbp, size, file, func, line, mname);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void *_ksr_pkg_l_xrealloc(void *mbp, void *ptr, size_t size,
		const char *file, const char *func, unsigned int line,
		const char *mname)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xrealloc(mbp, ptr, size, file, func, line, mname);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void *_ksr_pkg_l_xreallocxf(void *mbp, void *ptr, size_t size,
		const char *file, const char *func, unsigned int line,
		const char *mname)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xreallocxf(mbp, ptr, size, file, func, line, mname);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void _ksr_pkg_l_xfree(void *mbp, void *ptr, const char *file,
		const char *func, unsigned int line, const char *mname)
{
	pthread_mutex_lock(&_ksr_pkg_lock);
	_ksr_pkg_orig_xfree(mbp, ptr, file, func, line, mname);
	pthread_mutex_unlock(&_ksr_pkg_lock);
}
#else  /* !DBG_SR_MEMORY */
static void *_ksr_pkg_l_xmalloc(void *mbp, size_t size)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xmalloc(mbp, size);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void *_ksr_pkg_l_xmallocxz(void *mbp, size_t size)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xmallocxz(mbp, size);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void *_ksr_pkg_l_xrealloc(void *mbp, void *ptr, size_t size)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xrealloc(mbp, ptr, size);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void *_ksr_pkg_l_xreallocxf(void *mbp, void *ptr, size_t size)
{
	void *p;
	pthread_mutex_lock(&_ksr_pkg_lock);
	p = _ksr_pkg_orig_xreallocxf(mbp, ptr, size);
	pthread_mutex_unlock(&_ksr_pkg_lock);
	return p;
}
static void _ksr_pkg_l_xfree(void *mbp, void *ptr)
{
	pthread_mutex_lock(&_ksr_pkg_lock);
	_ksr_pkg_orig_xfree(mbp, ptr);
	pthread_mutex_unlock(&_ksr_pkg_lock);
}
#endif /* DBG_SR_MEMORY */

void tcp_reactor_pkg_lock_install(void)
{
	if(_ksr_pkg_orig_xmalloc != NULL)
		return; /* already installed */
	_ksr_pkg_orig_xmalloc = _pkg_root.xmalloc;
	_ksr_pkg_orig_xmallocxz = _pkg_root.xmallocxz;
	_ksr_pkg_orig_xrealloc = _pkg_root.xrealloc;
	_ksr_pkg_orig_xreallocxf = _pkg_root.xreallocxf;
	_ksr_pkg_orig_xfree = _pkg_root.xfree;

	/* Only the alloc/free/realloc paths are wrapped - status/info/report are
	 * diagnostic and not on the hot concurrent path. */
	_pkg_root.xmalloc = _ksr_pkg_l_xmalloc;
	_pkg_root.xmallocxz = _ksr_pkg_l_xmallocxz;
	_pkg_root.xrealloc = _ksr_pkg_l_xrealloc;
	_pkg_root.xreallocxf = _ksr_pkg_l_xreallocxf;
	_pkg_root.xfree = _ksr_pkg_l_xfree;

	LM_WARN("PROC_TCP_MAIN pkg allocator serialized for reactor threads\n");
}

#else /* !PKG_MALLOC */

/* System-malloc build: pkg_malloc/pkg_free are the (thread-safe) libc
 * allocator, so the reactor pool needs no pkg serialization - nothing to do. */
void tcp_reactor_pkg_lock_install(void)
{
	LM_DBG("pkg uses the system allocator (no PKG_MALLOC); reactor pkg lock "
		   "not needed\n");
}

#endif /* PKG_MALLOC */
