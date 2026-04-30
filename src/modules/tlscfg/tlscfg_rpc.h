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
 * @file tlscfg_rpc.h
 * @brief RPC command handler declarations
 * @ingroup tlscfg
 */

#ifndef _TLSCFG_RPC_H_
#define _TLSCFG_RPC_H_

#include "../../core/rpc.h"

void tlscfg_rpc_profile_add(rpc_t *rpc, void *ctx);
void tlscfg_rpc_profile_update(rpc_t *rpc, void *ctx);
void tlscfg_rpc_profile_remove(rpc_t *rpc, void *ctx);
void tlscfg_rpc_profile_list(rpc_t *rpc, void *ctx);
void tlscfg_rpc_profile_get(rpc_t *rpc, void *ctx);
void tlscfg_rpc_profile_enable(rpc_t *rpc, void *ctx);
void tlscfg_rpc_profile_disable(rpc_t *rpc, void *ctx);
void tlscfg_rpc_cert_check(rpc_t *rpc, void *ctx);
void tlscfg_rpc_cert_notify(rpc_t *rpc, void *ctx);
void tlscfg_rpc_reload(rpc_t *rpc, void *ctx);

#endif /* _TLSCFG_RPC_H_ */
