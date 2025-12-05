/*
 * Web3 Authentication - Core Authentication Functions
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

#ifndef _AUTH_WEB3_H_
#define _AUTH_WEB3_H_

#include "../../core/parser/digest/digest.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/str.h"
#include "../../modules/auth/api.h"
#include <curl/curl.h>

/*
 * Web3 response structure for curl callbacks
 */
struct Web3ResponseData
{
	char *memory;
	size_t size;
};

/*
 * Core Web3 authentication functions
 */

/**
 * Main web3 authentication check function
 * @param cred Digest credentials
 * @param method SIP method
 * @return Authentication result
 */
int auth_web3_check_response(dig_cred_t *cred, str *method);

/**
 * Digest authentication function
 * @param msg SIP message
 * @param realm Authentication realm
 * @param hftype Header field type
 * @param method SIP method
 * @return Authentication result
 */
int web3_digest_authenticate(
		struct sip_msg *msg, str *realm, hdr_types_t hftype, str *method);

/**
 * @brief Curl callback function for Web3 RPC responses
 * @param contents Response data
 * @param size Size of each element
 * @param nmemb Number of elements
 * @param data Web3ResponseData structure
 * @return Number of bytes processed
 */
size_t web3_curl_callback(void *contents, size_t size, size_t nmemb,
		struct Web3ResponseData *data);

// ENS validation functions
int web3_ens_validate(const char *username, dig_cred_t *cred, str *method);

// New ENS owner resolution function
int web3_ens_get_owner_address(const char *ens_name, char *owner_address);

// Legacy compatibility function (now calls web3_ens_get_owner_address)
int web3_ens_resolve_address(const char *ens_name, char *resolved_address);

// Oasis contract functions
int web3_oasis_get_wallet_address(const char *username, char *wallet_address);

#endif /* _AUTH_WEB3_H_ */