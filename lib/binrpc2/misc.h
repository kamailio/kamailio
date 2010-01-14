/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of the BinRPC Library (libbinrpc).
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */


#ifndef __MISC_H__
#define __MISC_H__

#ifdef _LIBBINRPC_BUILD

#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>

#include "log.h"

#define __LOCAL	static inline

/* 
 * TODO: support for arch's not allowing unalligned addressing.
 */
#define wr8(src8, _loc_)	*(uint8_t *)(_loc_) = (uint8_t)src8
#define wr16(src16, _loc_)	*(uint16_t *)(_loc_) = (uint16_t)src16
#define wr32(src32, _loc_)	*(uint32_t *)(_loc_) = (uint32_t)src32
#define wr64(src64, _loc_)	*(uint64_t *)(_loc_) = (uint64_t)src64

#define rd8(_loc_)	(*(uint8_t *)(_loc_))
#define rd16(_loc_)	(*(uint16_t *)(_loc_))
#define rd32(_loc_)	(*(uint32_t *)(_loc_))
#define rd64(_loc_)	(*(uint64_t *)(_loc_))

/*
 * Decode a byte sequence as size_t integer.
 * @param ptr Stream start; contains the integer in network byte order.
 * @param len How many bytes to decode.
 * @return Unmarshalled integer.
 */
__LOCAL size_t ntohz(const uint8_t *ptr, size_t len)
{
	unsigned int i;
	register size_t val = 0;
#ifndef NDEBUG
	if (sizeof(size_t) < len)
		/* it might not be an error condition if leading 0s */
		WARN("length exceeds size_t storage.\n");
#endif
	for (i = 0; i < len; i ++) {
		val <<= 8;
		val |= (size_t)ptr[i];
	}
	return val;
}

/**
 * How many bytes a 'length' value needs for storage.
 * @param len Value whose storage size is needed.
 * @return Number of bytes needed for storage.
 */
__LOCAL size_t sizeofz(size_t val)
{
	register size_t size;
	size = 0;
	switch (val) {
		case 0:
			size = sizeof(uint8_t);
			break;
		
		default:
			while (val) {
				size ++;
				val >>= /* bits/byte */8;
			}
	}
	return size;
}

/**
 * Writes a size_t value to a buffer in network byte order, using minimum
 * space possible. This means it might use less than sizeof(size_t)!
 * At least one byte is written.
 * @param pos Where to write the value.
 * @param size What value to write.
 * @return Number of bytes used.
 */
__LOCAL size_t htonz(const uint8_t *pos, size_t size)
{
	size_t repr, k;

	repr = sizeofz(size);
	switch (repr) {
		case sizeof(uint8_t): wr8(size, pos); break;
		case sizeof(uint16_t): wr16(htons(size), pos); break;
		case sizeof(uint32_t): wr32(htonl(size), pos); break;
		default:
			for (k = repr - 1; size; k --) {
				wr8(size, pos + k);
				size >>= 8;
			}
	}
	return repr;
}
/**
 * Same as sizeofz, but for longs.
 * @see sizeofz.
 */
__LOCAL size_t sizeofl(signed long val)
{
	register unsigned long aval;
	register uint8_t msB;
	register size_t size;
	aval = (val < 0) ? -val : val; /* =labs(val) */
	size = 0;
	do {
		msB = aval;
		size ++;
	} while ((aval >>= 8));
	return size + ((msB & 0x80) ? /*sign needs one more byte */1 : 0);
}

/**
 * Same as htonz, but for longs.
 * Returned value will always be a power of 2.
 * @see htonz.
 */
__LOCAL size_t lhton(const uint8_t *pos, signed long val)
{
	size_t repr;
	int i;
	int64_t val64;

	repr = sizeofl(val);
	switch (repr) {
		case sizeof(int8_t): wr8((int8_t)val, pos); break;
		case sizeof(int16_t): wr16(htons((int16_t)val), pos); break;
		case sizeof(int16_t) + 1: repr = sizeof(int32_t); /* no break; */
		case sizeof(int32_t): wr32(htonl((int32_t)val), pos); break;

		case sizeof(int32_t) + 1 ... sizeof(int64_t):
			/* TODO: 2x 32 */
			val64 = val;
			for (i = sizeof(int64_t) - 1; 0 <= i; i --) {
				wr8(val64, pos + i);
				val64 >>= 8;
			}
			repr = sizeof(int64_t);
			break;

		default:
			BUG("sizeofi(%d)=%zd!\n", val, repr);
			abort();
	}
	return repr;
}

/**
 * Ceil Long Storage Size
 * @see lhton
 */
__LOCAL size_t clss(size_t ss)
{
	switch (ss) {
		case sizeof(int8_t): return sizeof(int8_t);
		case sizeof(int16_t): return sizeof(int16_t);
		case sizeof(int16_t) + 1: 
		case sizeof(int32_t): return sizeof(int32_t);
		case sizeof(int32_t) + 1 ... sizeof(int64_t): return sizeof(int64_t);
		default:
			BUG("invalid long storage size: %zd.\n", ss);
			abort();
	}
}

/**
 * Same as ntohz, but for longs.
 * @param pos Where to read the long from.
 * @param size Representation size of the long. It must be a power of 2.
 * @param _val Where to return the read value.
 * @return State of operation.
 * @see ntohz.
 */
__LOCAL bool lntoh(const uint8_t *pos, size_t size, signed long *_val)
{
	long val;
	int i;
	uint64_t v64;
	switch (size) {
		case sizeof(int8_t): val = (int8_t)rd8(pos); break;
		case sizeof(int16_t): val = (int16_t)ntohs(rd16(pos)); break;
		case sizeof(int32_t): val = (int32_t)ntohl(rd32(pos)); break;

		case sizeof(int64_t):
			v64 = 0;
			for (i = 0; i < size; i ++) {
				v64 <<= 8;
				v64 |= rd8(pos);
			}
			val = (int64_t)v64;
			break;

		default:
			ERR("unsupported storage size for long (%d).\n", size);
			return false;
	}
	*_val = val;
	return true;
}

#endif /* _LIBBINRPC_BUILD */

#endif /* __MISC_H__ */
