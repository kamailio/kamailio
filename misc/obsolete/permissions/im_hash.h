/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 *
 */

#ifndef _IM_HASH_H
#define _IM_HASH_H

#include "../../ip_addr.h"
#include "../../locking.h"

/* linked list of entries */
typedef struct im_entry {
	struct ip_addr	ip;
	unsigned short	port;
	str		avp_val;
	unsigned int	mark;

	struct im_entry	*next;
} im_entry_t;

/* hash table for the entries */
typedef struct im_hash {
	im_entry_t	**entries;

	gen_lock_t	read_lock;	/* lock for reader_count variable */
	gen_lock_t	write_lock;	/* lock for writer processes */
	int		reader_count;	/* number of reader processes */
	int		writer_demand;	/* writer processes set this flag */
} im_hash_t;

/* global variable for DB cache */
extern im_hash_t	*IM_HASH;

/* parse ipv4 or ipv6 address
 */
int parse_ip(str *s, struct ip_addr *ip, unsigned short *port);

/* hash function for ipmatch hash table
 * in case of ipv4:
 *    summarizes the 4 unsigned char values
 * in case of ipv6:
 *    summarizes the 1st, 5th, 9th, and 13th unsigned char values
 */
unsigned int im_hash(struct ip_addr *ip);

/* init global IM_HASH structure */
int init_im_hash(void);

/* free memory allocated for the global cache */
void destroy_im_hash(void);

/* create a new impatch hash table */
im_entry_t **new_im_hash(void);

/* free the memory allocated for an ipmatch hash table,
 * and purge out entries
 */
void free_im_hash(im_entry_t **hash);

/* free the memory allocated for an ipmatch hash table,
 * but do not purge out entries
 */
void delete_im_hash(im_entry_t **hash);

/* create a new ipmatch entry and insert it into the hash table
 * return value
 *   0: success
 *  -1: error
 */
int insert_im_hash(char *ip, char *avp_val, unsigned int mark,
			im_entry_t **hash);

#endif /* _IM_HASH_H */
