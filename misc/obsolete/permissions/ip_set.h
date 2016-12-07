/* $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
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

#ifndef _IP_SET_H_
#define _IP_SET_H_

#include "ip_tree.h"
#include "../../ip_addr.h"
#include <stdio.h>

/* ip_set stuff, combines IPv4 and IPv6 tree in one set */
   
struct ip_set {
	int use_shm;
	struct ip_tree_leaf *ipv4_tree;
	struct ip_tree_leaf *ipv6_tree;	
};

extern void ip_set_init(struct ip_set *ip_set, int use_shm);
extern void ip_set_destroy(struct ip_set *ip_set);
extern int ip_set_add_ip(struct ip_set *ip_set, struct ip_addr *ip, unsigned int network_prefix);
extern int ip_set_add_ip_s(struct ip_set *ip_set, str ip_s, str mask_s);
extern int ip_set_ip_exists(struct ip_set *ip_set, struct ip_addr *ip);
extern void ip_set_print(FILE *stream, struct ip_set *ip_set);
extern int ip_set_add_list(struct ip_set *ip_set, str ip_set_s);

#endif
