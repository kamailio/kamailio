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
 * Background / why this exists
 * ----------------------------
 * When Kamailio is built with PKG_MALLOC defined (the default), pkg_malloc/
 * pkg_free/pkg_realloc are served by Kamailio's *own* per-process allocator
 * (q_malloc / f_malloc / tlsf via the _pkg_root vtable), NOT by libc malloc.
 * That allocator is deliberately lock-free: Kamailio's classic model is one
 * thread per process, so private (pkg) memory never needs synchronization.
 * (Building without PKG_MALLOC routes pkg_* to the thread-safe system malloc,
 * but you lose Kamailio's pkg accounting, leak tracking and the qm_* debug
 * fragment checks - so that is not the answer here.)
 *
 * The full TCP reactor (global tcp_main_threads == 2) breaks the one-thread-
 * per-process assumption: PROC_TCP_MAIN runs the io_wait (epoll) thread plus a
 * pool of reader threads, and they all share this one process's pkg heap:
 *   - io_wait thread: reactor job alloc/free, io housekeeping
 *   - pool threads:   WS frame encode (keepalive/pong/close), TLS/WSS, and any
 *                     config/event route run on the pool (faked_msg_init, ...)
 * Concurrent unlocked pkg_malloc/pkg_free across those threads corrupts the
 * heap freelist (seen as qm_debug_check_frag "end overwritten" aborts).
 *
 * Chasing every individual pkg call site with its own lock can never be
 * exhaustive. Instead we serialize the allocator itself, once, for this one
 * process: wrap the _pkg_root vtable entries (xmalloc / xmallocxz / xrealloc /
 * xreallocxf / xfree) with a single process-local mutex. Because _pkg_root is a
 * per-process global (each fork gets its own copy), installing the wrappers in
 * PROC_TCP_MAIN affects ONLY PROC_TCP_MAIN - worker processes and the legacy
 * tcp_main_threads 0/1 paths keep the fast lock-free allocator untouched.
 *
 * This only serializes pkg. It is orthogonal to the other reactor thread-safety
 * measures (shared read buffers, io_h ownership, per-connection shields), which
 * are handled where that state lives.
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
