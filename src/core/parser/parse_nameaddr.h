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
 *
 */

/*! \file
 * \brief Parser :: Parse name-addr part
 *
 * \ingroup parser
 */

#ifndef PARSE_NAMEADDR_H
#define PARSE_NAMEADDR_H

#include <stdio.h>
#include "../str.h"

/*! \brief
 * Name-addr structure, see RFC3261 for more details
 */
typedef struct name_addr {
	str name;   /*!< Display name part */
	str uri;    /*!< Uri part without surrounding <> */
	int len;    /*!< Total length of the field (including all
		    * whitechars present in the parsed message */
} name_addr_t;


/*! \brief
 * Parse name-addr part, the given string can be longer,
 * parsing will stop when closing > is found
 */
int parse_nameaddr(str* _s, name_addr_t* _a);


/*! \brief
 * Print a name-addr structure, just for debugging
 */
void print_nameaddr(FILE* _o, name_addr_t* _a);


#endif /* PARSE_NAMEADDR_H */
