/* 
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
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
 * \brief SIP-router core :: md5 hash support
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
