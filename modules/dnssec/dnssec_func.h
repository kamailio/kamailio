/*
 * $Id$
 *
 * DNSSEC module
 *
 * Copyright (C) 2013 mariuszbi@gmail.com
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2013-03	initial implementation
 */

#ifndef DNSSEC_FUNC_H
#define DNSSEC_FUNC_H

struct hostent;

typedef enum {
	QUERY_DONT_VALIDATE = 1<<0,
	QUERY_IGNORE_SKEW = 1<<1,
	QUERY_AC_DETAIL = 1<<2,
	QUERY_NO_DLV = 1<<3,
	QUERY_NO_EDNS0_FALLBACK = 1<<4,
	QUERY_RECURSE = 1<<5,
	QUERY_SKIP_RESOLVER = 1<<6,
	QUERY_SKIP_CACHE = 1<<7
} query_flags_t;

int dnssec_res_init(void);
unsigned int set_context_flags(unsigned int flags);
struct hostent* dnssec_gethostbyname(const char *);
struct hostent* dnssec_gethostbyname2(const char *, int);
int dnssec_res_search(const char*, int, int, unsigned char*, int);


#endif // DNSSEC_FUNC_H

