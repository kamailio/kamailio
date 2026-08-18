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
 * @brief Serialize the per-process pkg allocator for the threaded TCP reactor.
 *
 * pkg_malloc/pkg_free are served by Kamailio's own per-process allocator
 * (_pkg_root vtable), which is deliberately lock-free under Kamailio's
 * classic one-thread-per-process model. The full reactor (tcp_main_threads
 * == 2) breaks that model: PROC_TCP_MAIN runs an io_wait thread plus a pool
 * of reader threads that all touch this process's pkg heap (reactor job
 * alloc/free, WS frame encode, TLS/WSS, config/event routes on the pool).
 * Unlocked concurrent pkg_malloc/pkg_free corrupts the heap freelist.
 *
 * Rather than locking every call site, this wraps the _pkg_root vtable
 * (xmalloc/xmallocxz/xrealloc/xreallocxf/xfree) with one process-local mutex.
 * _pkg_root is per-process, so installing the wrapper in PROC_TCP_MAIN leaves
 * worker processes and the tcp_main_threads 0/1 paths untouched.
 *
 * Orthogonal to the reactor's other thread-safety measures (shared read
 * buffers, io_h ownership, per-connection shields) - this covers pkg only.
 */

#ifndef _tcp_reactor_mem_h
#define _tcp_reactor_mem_h

/**
 * @brief Install the mutex-wrapped pkg allocator into _pkg_root.
 *
 * Idempotent. Must be called from PROC_TCP_MAIN (mode 2 only), once, BEFORE any
 * reactor pool thread is spawned, so the vtable swap itself cannot race. After
 * this returns, every pkg_malloc/pkg_free in this process is serialized by a
 * single mutex. No-op-safe to call more than once.
 *
 * In a build without PKG_MALLOC (pkg_* == thread-safe system malloc) this is a
 * no-op: there is no lock-free allocator to protect.
 */
void tcp_reactor_pkg_lock_install(void);

#endif /* _tcp_reactor_mem_h */
