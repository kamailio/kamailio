/*
 * sha and other hashing utilities
 *
 * Copyright (C) 2014 1&1 Germany
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
/*!
* \file
* \brief core/crypto :: SHA and other hashing utilities
* \ingroup core/crypto
* Module: \ref core/crypto
*/

#ifndef _SHAUTILS_H_
#define _SHAUTILS_H_

#include "sha256.h"

void compute_md5(char *dst, char *src, int src_len);

void compute_sha1_raw(unsigned char *dst, u_int8_t *src, int src_len);

void compute_sha1(char *dst, u_int8_t *src, int src_len);

void compute_sha256(char *dst, u_int8_t *src, int src_len);

void compute_sha384(char *dst, u_int8_t *src, int src_len);

void compute_sha512(char *dst, u_int8_t *src, int src_len);

int hex_to_bytes(const char *hex_str, unsigned char *bytes, int max_bytes);

void bytes_to_hex(const unsigned char *bytes, size_t len, char *hex);

#endif
