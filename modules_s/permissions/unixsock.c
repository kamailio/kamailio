/*
 * $Id$
 *
 * UNIX Domain Socket Interface
 *
 * Copyright (C) 2003 Juha Heinanen
 * Copyright (C) 2004 FhG FOKUS
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

#include "fifo.h"
#include "../../str.h"
#include "../../unixsock_server.h"
#include "../../ut.h"
#include "hash.h"
#include "trusted.h"
#include "unixsock.h"


/*
 * Fifo function to reload trusted table
 */
static int trusted_reload(str* msg)
{
	if (reload_trusted_table () == 1) {
		unixsock_reply_asciiz("200 OK\n");
		unixsock_reply_send();
		return 0;
	} else {
		unixsock_reply_asciiz("400 Trusted table reload failed\n");
		unixsock_reply_send();
		return -1;
	}
}


/* 
 * Print domains stored in hash table 
 */
static int hash_table_print_unixsock(struct trusted_list** hash_table)
{
	int i;
	struct trusted_list *np;

	for (i = 0; i < HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			if (unixsock_reply_printf("%4d <%.*s, %d, %s>\n", i,
						  np->src_ip.len, ZSW(np->src_ip.s),
						  np->proto,
						  np->pattern) < 0) {
				LOG(L_ERR, "hash_table_print: No memory left\n");
				return -1;
			}
			np = np->next;
		}
	}
	return 0;
}


/*
 * Fifo function to print trusted entries from current hash table
 */
static int trusted_dump(str* msg)
{
	unixsock_reply_asciiz("200 OK\n");
	if (hash_table_print_unixsock(*hash_table) < 0) {
		unixsock_reply_reset();
		unixsock_reply_asciiz("500 Error while creating reply\n");
		unixsock_reply_send();
		return -1;
	}
	unixsock_reply_send();
	return 1;
}


/*
 * Register domain fifo functions
 */
int init_trusted_unixsock(void) 
{
	if (unixsock_register_cmd("trusted_reload", trusted_reload) < 0) {
		LOG(L_CRIT, "init_trusted_unixsock: Cannot register trusted_reload\n");
		return -1;
	}

	if (unixsock_register_cmd("trusted_dump", trusted_dump) < 0) {
		LOG(L_CRIT, "init_trusted_unixsock: Cannot register trusted_dump\n");
		return -1;
	}

	return 0;
}
