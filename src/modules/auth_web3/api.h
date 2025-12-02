/*
 * Web3 Authentication  - API
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

#ifndef _AUTH_WEB3_API_H_
#define _AUTH_WEB3_API_H_

#include "../../core/parser/digest/digest.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/sr_module.h"

/*
 * Web3 authentication function signature
 * This function performs blockchain-based authentication
 */
typedef int (*web3_digest_authenticate_f)(
		struct sip_msg *msg, str *realm, hdr_types_t hftype, str *method);

/*
 * Web3 check response function signature
 * This is the core blockchain verification function
 */
typedef int (*web3_check_response_f)(dig_cred_t *cred, str *method);

/**
 * @brief Web3 Authentication API structure
 */
typedef struct web3_auth_api
{
	web3_digest_authenticate_f digest_authenticate;
	web3_check_response_f check_response;
} web3_auth_api_t;

typedef int (*bind_web3_auth_f)(web3_auth_api_t *api);

/**
 * @brief Load the Web3 Authentication API
 */
static inline int web3_auth_load_api(web3_auth_api_t *api)
{
	bind_web3_auth_f bindweb3auth;

	bindweb3auth = (bind_web3_auth_f)find_export("bind_web3_auth", 0, 0);
	if(bindweb3auth == 0) {
		LM_ERR("cannot find bind_web3_auth\n");
		return -1;
	}
	if(bindweb3auth(api) == -1) {
		LM_ERR("cannot bind web3auth api\n");
		return -1;
	}
	return 0;
}

#endif /* _AUTH_WEB3_API_H_ */