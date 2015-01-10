/*
 * PIKE module
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
#ifndef __PIKE_TOP_H
#define __PIKE_TOP_H

#include "ip_tree.h"
#include "../../ip_addr.h"

struct TopListItem_t {
	int             addr_len;
	unsigned char   ip_addr[45];	/*!< Make room for IPv6 */
	unsigned int  	leaf_hits[2];
	unsigned int  	hits[2];
	unsigned int    expires;	/*!< in seconds */
	node_status_t   status;

	struct TopListItem_t *next;
};

// returns 1 when OK and 0 when failed
int pike_top_add_entry( unsigned char *ip_addr, int addr_len, unsigned short leaf_hits[2], unsigned short hits[2], unsigned int expires, node_status_t status );

struct TopListItem_t *pike_top_get_root();
void pike_top_list_clear();

/* helpful functions */
char *pike_top_print_addr( unsigned char *ip_addr, int addrlen, char *buff, int buffsize );


#endif // PIKE_TOP_H
