/*
 * sha and other hashing utilities
 *
 * Copyright (C) 2014 1&1 Germany
 *
 * This file is part of Kamailio, a free SIP server.
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
/*! @defgroup srutils Various utilities
 *
 * Kamailio core library.
 */
/*!
* \file
* \brief srutils :: SHA and other hashing utilities
* \ingroup srutils
* Module: \ref srutils
*/

#include "../../md5.h"
#include "../../ut.h"
#include "shautils.h"

/*! \brief Compute MD5 checksum */
void compute_md5(char *dst, char *src, int src_len)
{
	MD5_CTX context;
	unsigned char digest[16];
	MD5Init (&context);
  	MD5Update (&context, src, src_len);
	U_MD5Final (digest, &context);
	string2hex(digest, 16, dst);
}

/*! \brief Compute SHA256 checksum */
void compute_sha256(char *dst, u_int8_t *src, int src_len)
{
	SHA256_CTX ctx256;
	SHA256_Init(&ctx256);
	SHA256_Update(&ctx256, src, src_len);
	SHA256_End(&ctx256, dst);
}

/*! \brief Compute SHA384 checksum */
void compute_sha384(char *dst, u_int8_t *src, int src_len)
{
	SHA384_CTX ctx384;
	SHA384_Init(&ctx384);
	SHA384_Update(&ctx384, src, src_len);
	SHA384_End(&ctx384, dst);
}

/*! \brief Compute SHA512 checksum */
void compute_sha512(char *dst, u_int8_t *src, int src_len)
{
	SHA512_CTX ctx512;
	SHA512_Init(&ctx512);
	SHA512_Update(&ctx512, src, src_len);
	SHA512_End(&ctx512, dst);
}
