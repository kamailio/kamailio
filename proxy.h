/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef proxy_h
#define proxy_h

#include <netdb.h>
#include "ip_addr.h"
#include "str.h"

struct proxy_l{
	struct proxy_l* next;
	str name; /* original name */
	struct hostent host; /* addresses */
	unsigned short port;
	unsigned short reserved; /*align*/
	
	/* socket ? */

	int addr_idx;	/* crt. addr. idx. */
	int ok; /* 0 on error */
	/*statisticis*/
	int tx;
	int tx_bytes;
	int errors;
};

extern struct proxy_l* proxies;

struct proxy_l* add_proxy(str* name, unsigned short port);
struct proxy_l* mk_proxy(str* name, unsigned short port);
struct proxy_l* mk_proxy_from_ip(struct ip_addr* ip, unsigned short port);
void free_proxy(struct proxy_l* p);


#endif

