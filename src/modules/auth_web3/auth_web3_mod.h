/*
 * Web3 Authentication Module
 *
 * Copyright (C) 2025 Jonathan Kandel
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef AUTH_WEB3_MOD_H
#define AUTH_WEB3_MOD_H

#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"
#include "../../modules/auth/api.h"
#include "web3_imple.h"
#include <curl/curl.h>

/*
 * Web3 Authentication Module Parameters
 */
extern char *web3_authentication_rpc_url;
extern char *web3_authentication_contract_address;
extern char *web3_ens_registry_address;
extern char *web3_ens_rpc_url;
extern int web3_contract_debug_mode;
extern int web3_rpc_timeout;

/*
 * Base auth module API
 */
extern auth_api_s_t auth_api;

#endif /* AUTH_WEB3_MOD_H */