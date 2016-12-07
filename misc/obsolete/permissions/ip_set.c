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

#include "ip_set.h"
#include "../../resolve.h"
#include <stdio.h>
#include <string.h>

void ip_set_init(struct ip_set *ip_set, int use_shm) {
	memset(ip_set, 0, sizeof(*ip_set));
	ip_set->use_shm = use_shm;
	ip_tree_init(&ip_set->ipv4_tree);
	ip_tree_init(&ip_set->ipv6_tree);
}

void ip_set_destroy(struct ip_set *ip_set) {
	ip_tree_destroy(&ip_set->ipv4_tree, 0, ip_set->use_shm);
	ip_tree_destroy(&ip_set->ipv6_tree, 0, ip_set->use_shm);
}

int ip_set_add_ip(struct ip_set *ip_set, struct ip_addr *ip, unsigned int network_prefix) {
	switch (ip->af) {
		case AF_INET:
			return ip_tree_add_ip(&ip_set->ipv4_tree, ip->u.addr, (ip->len*8<network_prefix)?ip->len*8:network_prefix, ip_set->use_shm);
		case AF_INET6:
			return ip_tree_add_ip(&ip_set->ipv6_tree, ip->u.addr, (ip->len*8<network_prefix)?ip->len*8:network_prefix, ip_set->use_shm);
		default:
			return -1;				
		}
}

int ip_set_ip_exists(struct ip_set *ip_set, struct ip_addr *ip) {
	struct ip_tree_find h;
	switch (ip->af) {
		case AF_INET:
			return ip_tree_find_ip(ip_set->ipv4_tree, ip->u.addr, ip->len*8, &h) > 0;
		case AF_INET6:
			return ip_tree_find_ip(ip_set->ipv6_tree, ip->u.addr, ip->len*8, &h) > 0;
		default:
			return -1;				
		}
}

void ip_set_print(FILE *stream, struct ip_set *ip_set) {
	fprintf(stream, "IPv4:\n");
	ip_tree_print(stream, ip_set->ipv4_tree, 2);
	fprintf(stream, "IPv6:\n");
	ip_tree_print(stream, ip_set->ipv6_tree, 2);
}

int ip_set_add_list(struct ip_set *ip_set, str ip_set_s){

	str ip_s, mask_s;
				 
	/* parse comma delimited string of IPs and masks,  e.g. 1.2.3.4,2.3.5.3/24,[abcd:12456:2775:ab::7533],9.8.7.6/255.255.255.0 */
	while (ip_set_s.len) {
		while (ip_set_s.len && (*ip_set_s.s == ',' || *ip_set_s.s == ';' || *ip_set_s.s == ' ')) {
			ip_set_s.s++;
			ip_set_s.len--;
		}
		if (!ip_set_s.len) break;
		ip_s.s = ip_set_s.s;
		ip_s.len = 0;
		while (ip_s.len < ip_set_s.len && (ip_s.s[ip_s.len] != ',' && ip_s.s[ip_s.len] != ';' && ip_s.s[ip_s.len] != ' ' && ip_s.s[ip_s.len] != '/')) {
			ip_s.len++;
		}
		ip_set_s.s += ip_s.len;
		ip_set_s.len -= ip_s.len;
		mask_s.len = 0;
		mask_s.s=0;
		if (ip_set_s.len && ip_set_s.s[0] == '/') {
			ip_set_s.s++;
			ip_set_s.len--;
			mask_s.s = ip_set_s.s;
			while (mask_s.len < ip_set_s.len && (mask_s.s[mask_s.len] != ',' && mask_s.s[mask_s.len] != ';' && mask_s.s[mask_s.len] != ' ')) {
				mask_s.len++;
			}
			ip_set_s.s += mask_s.len;
			ip_set_s.len -= mask_s.len;
		}
		if (ip_set_add_ip_s(ip_set, ip_s, mask_s) < 0) {
			return -1;
		}
	}
	return 0;
}

int ip_set_add_ip_s(struct ip_set *ip_set, str ip_s, str mask_s){

	int fl;
	struct ip_addr *ip, ip_buff;
	unsigned int prefix, i;

	if ( ((ip = str2ip(&ip_s))==0)
					  && ((ip = str2ip6(&ip_s))==0)
									  ){
		ERR("ip_set_add_ip_s: string to ip conversion error '%.*s'\n", ip_s.len, ip_s.s);
		return -1;
	}
	ip_buff = *ip;

	if (mask_s.len > 0) {
		i = 0;
		fl = 0;
		while (i < mask_s.len &&
			   ((mask_s.s[i] >= '0' && mask_s.s[i] <= '9') || 
				(mask_s.s[i] >= 'a' && mask_s.s[i] <= 'f') || 
				(mask_s.s[i] >= 'A' && mask_s.s[i] <= 'F') ||
				mask_s.s[i] == '.' ||
				mask_s.s[i] == ':' ||
				mask_s.s[i] == '[' ||
				mask_s.s[i] == ']'
				) ) {
			fl |= mask_s.s[i] < '0' || mask_s.s[i] > '9';
			i++;
		}
		
		if (fl) {  /* 255.255.255.0 format */
			if ( ((ip = str2ip(&mask_s))==0)
					  && ((ip = str2ip6(&mask_s))==0)
				  ){
				ERR("ip_set_add_ip_s: string to ip mask conversion error '%.*s'\n", mask_s.len, mask_s.s);
				return -1;
			}
			if (ip_buff.af != ip->af) {
				ERR("ip_set_add_ip_s: IPv4 vs. IPv6 near '%.*s' vs. '%.*s'\n", ip_s.len, ip_s.s, mask_s.len, mask_s.s);
				return -1;
			}
			fl = 0;
			prefix = 0;
			for (i=0; i<ip->len; i++) {				
				unsigned char msk;				
				msk = 0x80;
				while (msk) {
					if ((ip->u.addr[i] & msk) != 0) {
						if (fl) {
							ERR("ip_set_add_ip_s: bad IP mask '%.*s'\n", mask_s.len, mask_s.s);
							return -1;
						}
						prefix++;
					}
					else {
						fl = 1;
					}
					msk /= 2;
				}
			}
		}	
		else {     /* 24 format */
			if (str2int(&mask_s, &prefix) < 0) {
				ERR("ip_set_add_ip_s: cannot convert mask '%.*s'\n", mask_s.len, mask_s.s);
				return -1;
			}				
		}
	}
	else {
		prefix = ip_buff.len*8;
	}
	if (ip_set_add_ip(ip_set, &ip_buff, prefix) < 0) {
		ERR("ip_set_add_ip_s: cannot add IP into ip set\n");
		return -1;
	}		
	return 0;
}



