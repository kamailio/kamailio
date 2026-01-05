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
int hex_to_bytes(const char *hex_str, unsigned char *bytes, int max_bytes)
{
	int len;
	int byte_len;
	int i;
	char *endptr;
	long val;

	len = strlen(hex_str);
	if(len % 2 != 0)
		return -1; /* Invalid hex string */

	byte_len = len / 2;
	if(byte_len > max_bytes)
		return -1; /* Too many bytes */

	for(i = 0; i < byte_len; i++) {
		char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};

		/* Check for valid hex characters before calling strtol */
		if(!isxdigit((unsigned char)hex_byte[0])
				|| !isxdigit((unsigned char)hex_byte[1])) {
			return -1; /* Invalid hex character */
		}

		errno = 0;
		val = strtol(hex_byte, &endptr, 16);

		/* Check for conversion errors */
		if(errno != 0 || endptr == hex_byte || *endptr != '\0') {
			return -1; /* Conversion failed */
		}

		/* Check for out of range values */
		if(val < 0 || val > 255) {
			return -1; /* Value out of byte range */
		}

		bytes[i] = (unsigned char)val;
	}

	return byte_len;
}

/* Convert bytes to hex string */
void bytes_to_hex(const unsigned char *bytes, size_t len, char *hex)
{
	for(size_t i = 0; i < len; i++) {
		sprintf(hex + 2 * i, "%02x", bytes[i]);
	}
	hex[2 * len] = '\0';
}
