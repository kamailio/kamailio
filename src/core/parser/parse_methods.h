/*
 * Copyright (c) 2004 Juha Heinanen
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

/*! \file
 * \brief Parser :: Parse Methods
 *
 * \ingroup parser
 */


#ifndef PARSE_METHODS_H
#define PARSE_METHODS_H

#include "../str.h"
#include "msg_parser.h"

#define ALL_METHODS 0xffffffff

/* 
 * Parse comma separated list of methods pointed by _body and assign their
 * enum bits to _methods.  Returns 1 on success and 0 on failure.
 */
int parse_methods(const str* const _body, unsigned int* const _methods);

int parse_method_name(const str* const s, enum request_method* const method);


#endif /* PARSE_METHODS_H */
