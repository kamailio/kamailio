/* 
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief Kamailio core :: MD5 digest support
 * \ingroup core
 * Module: \ref core
 */

#ifndef _MD5UTILS_H
#define _MD5UTILS_H

#include "str.h"

#define MD5_LEN	32

/*!
  * \brief Calculate a MD5 digests over a string array
  * 
  * Calculate a MD5 digests over a string array and stores the result in the
  * destination char array. This function assumes 32 bytes in the destination
  * buffer.
  * \param dst destination
  * \param src string input array
  * \param size elements in the input array
  */
void MD5StringArray (char *dst, str src[], int size);

#endif /* _MD5UTILS_H */
