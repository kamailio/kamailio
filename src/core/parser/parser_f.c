/*
 * parser helper  functions
 *
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

/** @file
 * @brief Parser :: helper functions
 *
 * @ingroup parser
 */


#include  "parser_f.h"
#include "../ut.h"

/** @brief returns pointer to next line or after the end of buffer */
char* eat_line(char* buffer, unsigned int len)
{
	char* nl;

	/* jku .. replace for search with a library function; not conforming
 		  as I do not care about CR
	*/
	nl=(char *)q_memchr( buffer, '\n', len );
	if ( nl ) { 
		if ( nl + 1 < buffer+len)  nl++;
		if (( nl+1<buffer+len) && * nl=='\r')  nl++;
	} else  nl=buffer+len;
	return nl;
}

