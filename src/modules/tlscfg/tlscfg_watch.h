/*
 * tlscfg module - TLS profile management companion for the tls module
 *
 * Copyright (C) 2026 Aurora Innovation
 *
 * Author: Daniel Donoghue
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file tlscfg_watch.h
 * @brief Cert file mtime watcher declarations
 * @ingroup tlscfg
 */

#ifndef _TLSCFG_WATCH_H_
#define _TLSCFG_WATCH_H_

/**
 * @brief Timer callback — runs every 1 second
 *
 * Handles two jobs:
 * 1. Debounce: if dirty flag set and debounce_interval has passed,
 *    flush config and trigger tls.reload
 * 2. Cert watch: every cert_check_interval ticks, check cert file mtimes
 */
void tlscfg_watch_timer(unsigned int ticks, void *param);

/**
 * @brief Trigger tls.reload via internal RPC invocation
 * @return 0 on success, -1 on error
 */
int tlscfg_tls_reload(void);

/**
 * @brief Refresh stored cert mtimes from disk
 *
 * Updates cert_mtime on all profiles to their current on-disk values.
 * Call after a successful tls.reload to prevent the cert watcher from
 * triggering a redundant reload.
 */
void tlscfg_refresh_cert_mtimes(void);

#endif /* _TLSCFG_WATCH_H_ */
