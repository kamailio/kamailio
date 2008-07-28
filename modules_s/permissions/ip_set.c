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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ip_set.h"
#include "../../resolve.h"
#include <stdio.h>
#include <string.h>

void ip_set_init(struct ip_set *ip_set) {
	memset(ip_set, 0, sizeof(*ip_set));
	ip_tree_init(&ip_set->ipv4_tree);
	#ifdef USE_IPV6
	ip_tree_init(&ip_set->ipv6_tree);
	#endif
}

void ip_set_destroy(struct ip_set *ip_set) {
	ip_tree_destroy(&ip_set->ipv4_tree, 0);
	#ifdef USE_IPV6
	ip_tree_destroy(&ip_set->ipv6_tree, 0);
	#endif
}

int ip_set_add_ip(struct ip_set *ip_set, struct ip_addr *ip, unsigned int network_prefix) {
	switch (ip->af) {
		case AF_INET:
			return ip_tree_add_ip(&ip_set->ipv4_tree, ip->u.addr, (ip->len*8<network_prefix)?ip->len*8:network_prefix);
	#ifdef USE_IPV6
		case AF_INET6:
			return ip_tree_add_ip(&ip_set->ipv6_tree, ip->u.addr, (ip->len*8<network_prefix)?ip->len*8:network_prefix);
	#endif
		default:
			return -1;				
		}
}

int ip_set_ip_exists(struct ip_set *ip_set, struct ip_addr *ip) {
	struct ip_tree_find h;
	switch (ip->af) {
		case AF_INET:
			return ip_tree_find_ip(ip_set->ipv4_tree, ip->u.addr, ip->len*8, &h) > 0;
	#ifdef USE_IPV6
		case AF_INET6:
			return ip_tree_find_ip(ip_set->ipv6_tree, ip->u.addr, ip->len*8, &h) > 0;
	#endif
		default:
			return -1;				
		}
}

void ip_set_print(FILE *stream, struct ip_set *ip_set) {
	fprintf(stream, "IPv4:\n");
	ip_tree_print(stream, ip_set->ipv4_tree, 2);
	#ifdef USE_IPV6
	fprintf(stream, "IPv6:\n");
	ip_tree_print(stream, ip_set->ipv6_tree, 2);
	#endif
}

int ip_set_parse(struct ip_set *ip_set, str ip_set_s){

	str s;
	struct ip_addr *ip;
	unsigned int prefix;
	
	ip_set_init(ip_set);

	/* parse comma delimited string of IPs and masks,  e.g. 1.2.3.4,2.3.5.3/24,[abcd:12456:2775:ab::7533] */
	while (ip_set_s.len) {
		while (ip_set_s.len && (*ip_set_s.s == ',' || *ip_set_s.s == ';' || *ip_set_s.s == ' ')) {			
			ip_set_s.s++;
			ip_set_s.len--;
		}
		s.s = ip_set_s.s;
		s.len = 0;
		while (s.len < ip_set_s.len && (s.s[s.len] != ',' && s.s[s.len] != ';' && s.s[s.len] != ' ' && s.s[s.len] != '/')) {
			s.len++;
		}
		if ( ((ip = str2ip(&s))==0)
			#ifdef  USE_IPV6
						  && ((ip = str2ip6(&s))==0)
			#endif
										  ){
			ip_set_destroy(ip_set);
			ERR("ip_set_parse: string to ip conversion error near '%.*s'\n", ip_set_s.len, ip_set_s.s);
			return -1;
		}
		ip_set_s.s += s.len;
		ip_set_s.len -= s.len;
		if (ip_set_s.len && ip_set_s.s[0] == '/') {
			ip_set_s.s++;
			ip_set_s.len--;
			s.s = ip_set_s.s;
			s.len = 0;
			while (s.len < ip_set_s.len && (s.s[s.len] >= '0' && s.s[s.len] <= '9')) {
				s.len++;
			}
			if (str2int(&s, &prefix) < 0) {
				ERR("ip_set_parse: cannot convert mask near '%.*s'\n", ip_set_s.len, ip_set_s.s);
				ip_set_destroy(ip_set);
				return -1;
			}				
			ip_set_s.s += s.len;
			ip_set_s.len -= s.len;
		}
		else {
			prefix = ip->len*8;
		}
		if (ip_set_add_ip(ip_set, ip, prefix) < 0) {
			ERR("ip_set_parse: cannot add IP into ip set\n");
			ip_set_destroy(ip_set);
			return -1;
		}		
	}
	return 0;		
}


