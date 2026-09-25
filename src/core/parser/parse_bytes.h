/*
 * This file is part of Kamailio, a free SIP server.
 *
 * Copyright (C) 2026 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.

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

/*! \file
 * \brief Parser :: Byte packing and ASCII case-folding helpers
 *
 * \ingroup parser
 */

#ifndef PARSE_BYTES_H
#define PARSE_BYTES_H

#include <stdint.h>

inline static uint32_t ksr_read_u24le(const char *val)
{
	return (uint32_t)(unsigned char)val[0]
		   | ((uint32_t)(unsigned char)val[1] << 8)
		   | ((uint32_t)(unsigned char)val[2] << 16);
}

inline static uint32_t ksr_read_u32le(const char *val)
{
	return ksr_read_u24le(val) | ((uint32_t)(unsigned char)val[3] << 24);
}

inline static uint32_t ksr_read_u24be(const char *val)
{
	return ((uint32_t)(unsigned char)val[0] << 16)
		   | ((uint32_t)(unsigned char)val[1] << 8)
		   | (uint32_t)(unsigned char)val[2];
}

inline static uint32_t ksr_read_u32be(const char *val)
{
	return (ksr_read_u24be(val) << 8) | (uint32_t)(unsigned char)val[3];
}

/* Fast ASCII folding for values known to contain alphabetic token bytes. */
inline static unsigned int ksr_ascii_lower_u8(unsigned char val)
{
	return (unsigned int)val | 0x20U;
}

inline static uint32_t ksr_ascii_lower_u24(uint32_t val)
{
	return val | UINT32_C(0x00202020);
}

inline static uint32_t ksr_ascii_lower_u32(uint32_t val)
{
	return val | UINT32_C(0x20202020);
}

#endif /* PARSE_BYTES_H */
