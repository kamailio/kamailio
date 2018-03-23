/*
 * Functions that operate on IP addresses
 *
 * Copyright (C) 2012 Hugh Waite (crocodile-rcs.com)
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
 */


#ifndef _IPOPS_DETAILED_IP_TYPE_H
#define _IPOPS_DETAILED_IP_TYPE_H

#define IPv4RANGES_SIZE 17
#define IPv6RANGES_SIZE 29

#include <stdint.h>
#include "../../core/str.h"

typedef struct ip4_node {
    uint32_t value;
    char *ip_type;
    uint32_t sub_mask;
} ip4_node;

typedef struct ip6_node {
    uint32_t value[4];
    char *ip_type;
    uint32_t sub_mask[4];
} ip6_node;

void ipv6ranges_hton();
void ipv4ranges_hton();
int ip6_iptype(str string_ip, char **res);
int ip4_iptype(str string_ip, char **res);

#endif
