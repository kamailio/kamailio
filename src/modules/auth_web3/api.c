/*
 * Web3 Authentication  - API Implementation
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

#include "api.h"
#include "../../core/dprint.h"
#include "../../core/sr_module.h"
#include "auth_web3_mod.h"
#include "web3_imple.h"

/**
 * @brief Bind Web3 authentication API
 * @param api API structure to fill
 * @return 0 on success, -1 on error
 */
int bind_web3_auth(web3_auth_api_t *api)
{
	if(!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* Bind the core functions */
	api->digest_authenticate = web3_digest_authenticate;
	api->check_response = auth_web3_check_response;

	LM_INFO("Web3 Authentication API successfully bound\n");
	return 0;
}
