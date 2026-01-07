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
/*! @defgroup core/crypto Various crypto utilities
 *
 * Kamailio core library.
 */
/*!
* \file
* \brief core/crypto :: SHA and other hashing utilities
* \ingroup core/crypto
* Module: \ref core/crypto
*/
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "../../core/crypto/md5.h"
#include "../../core/ut.h"
#include "shautils.h"

/*! \brief Compute MD5 checksum */
void compute_md5(char *dst, char *src, int src_len)
{
	MD5_CTX context;
	unsigned char digest[16];
	MD5Init(&context);
	MD5Update(&context, src, src_len);
	U_MD5Final(digest, &context);
	string2hex(digest, 16, dst);
}

/*! \brief Compute SHA1 checksum raw */
void compute_sha1_raw(unsigned char *dst, u_int8_t *src, int src_len)
{
	SHA1_CTX ctx1;
	sr_SHA1_Init(&ctx1);
	sr_SHA1_Update(&ctx1, src, src_len);
	sr_SHA1_Final(dst, &ctx1);
}

/*! \brief Compute SHA1 checksum hex */
void compute_sha1(char *dst, u_int8_t *src, int src_len)
{
	SHA1_CTX ctx1;
	sr_SHA1_Init(&ctx1);
	sr_SHA1_Update(&ctx1, src, src_len);
	sr_SHA1_End(&ctx1, dst);
}

/*! \brief Compute SHA256 checksum */
void compute_sha256(char *dst, u_int8_t *src, int src_len)
{
	SHA256_CTX ctx256;
	sr_SHA256_Init(&ctx256);
	sr_SHA256_Update(&ctx256, src, src_len);
	sr_SHA256_End(&ctx256, dst);
}

/*! \brief Compute SHA384 checksum */
void compute_sha384(char *dst, u_int8_t *src, int src_len)
{
	SHA384_CTX ctx384;
	sr_SHA384_Init(&ctx384);
	sr_SHA384_Update(&ctx384, src, src_len);
	sr_SHA384_End(&ctx384, dst);
}

/*! \brief Compute SHA512 checksum */
void compute_sha512(char *dst, u_int8_t *src, int src_len)
{
	SHA512_CTX ctx512;
	sr_SHA512_Init(&ctx512);
	sr_SHA512_Update(&ctx512, src, src_len);
	sr_SHA512_End(&ctx512, dst);
}


/**
 * Convert hex string to bytes
 */
int hex_to_bytes(const char *hex_str, unsigned char *bytes, size_t max_bytes)
{
	size_t len;
	size_t byte_len;
	size_t i;
	char hex_byte[3];
	char *endptr;
	long val;

	if(!hex_str || !bytes) {
		LM_ERR("Invalid input parameters to hex_to_bytes\n");
		return -1;
	}

	/* Skip 0x prefix if present */
	if(strncmp(hex_str, "0x", 2) == 0 || strncmp(hex_str, "0X", 2) == 0) {
		hex_str += 2;
	}

	len = strlen(hex_str);
	if(len % 2 != 0) {
		LM_ERR("Invalid hex string length: %zu (must be even)\n", len);
		return -1;
	}

	byte_len = len / 2;
	if(byte_len > max_bytes) {
		LM_ERR("Hex string too long: %zu bytes (max: %zu)\n", byte_len,
				max_bytes);
		return -1;
	}

	for(i = 0; i < byte_len; i++) {
		hex_byte[0] = hex_str[i * 2];
		hex_byte[1] = hex_str[i * 2 + 1];
		hex_byte[2] = '\0';

		errno = 0;
		val = strtol(hex_byte, &endptr, 16);

		/* Check for conversion errors */
		if(errno != 0 || endptr == hex_byte || *endptr != '\0' || val < 0
				|| val > 255) {
			LM_ERR("Invalid hex byte at position %zu: %s\n", i, hex_byte);
			return -1;
		}

		bytes[i] = (unsigned char)val;
	}

	return (int)byte_len;
}

/**
 * Convert bytes to hex string
 * @param bytes Input byte array
 * @param len Length of byte array
 * @param hex_str Output hex string buffer
 * @param hex_str_size Size of hex_str buffer (must be at least 2*len + 1)
 * @return 0 on success, -1 on error
 */
int bytes_to_hex(const unsigned char *bytes, size_t len, char *hex_str,
		size_t hex_str_size)
{
	size_t i;
	size_t required_size;
	int ret;

	if(!bytes || !hex_str) {
		LM_ERR("Invalid input parameters to bytes_to_hex\n");
		return -1;
	}

	/* Check buffer size: need 2 chars per byte + null terminator */
	required_size = 2 * len + 1;
	if(hex_str_size < required_size) {
		LM_ERR("Buffer too small for bytes_to_hex: need %zu, got %zu\n",
				required_size, hex_str_size);
		return -1;
	}

	for(i = 0; i < len; i++) {
		ret = snprintf(hex_str + 2 * i, hex_str_size - 2 * i, "%02x", bytes[i]);
		if(ret < 0 || ret >= (int)(hex_str_size - 2 * i)) {
			LM_ERR("snprintf failed in bytes_to_hex at position %zu\n", i);
			return -1;
		}
	}
	hex_str[2 * len] = '\0';

	return 0;
}
