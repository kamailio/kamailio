/*
 * Copyright (C) 2005-2006 iptelorg GmbH
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
 *
 */

/*!
 * \file
 * \brief Kamailio core :: Select result buffers
 * \author mma
 * \ingroup core
 * Module: \ref core
 */

#ifndef SELECT_BUFFER_H
#define SELECT_BUFFER_H

#include "str.h"

/**
 * Request for space from buffer
 *
 * Returns:  NULL  memory allocation failure (no more space)
 *           pointer to the space on success
 */

char* get_static_buffer(int req_size);

/** Internal function - called before request is going to be processed
 *
 * Reset offset to unused space
 */
int reset_static_buffer(void);

int str_to_static_buffer(str* res, str* s);
int int_to_static_buffer(str* res, int val);
int uint_to_static_buffer(str* res, unsigned int val);
int uint_to_static_buffer_ex(str* res, unsigned int val, int base, int pad);

#endif /* SELECT_BUFFER_H */
