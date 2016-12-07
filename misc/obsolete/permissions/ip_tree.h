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

#ifndef _IP_TREE_H_
#define _IP_TREE_H_ 1

#include <stdio.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ip_addr.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../trim.h"
#include "../../ut.h"

/* 
   Implements algorithm for testing if particular address is in a iP adrress set.
   IP adresses may also describe a subnetwork, i.e. only prefix is valuable and trailing part
   of address has no effect for decision. The algorithm is common, both IPv4 and IPv6 
   are supported.
   
   Reduce(match)/decide(fork) algorithm is applied to minimize memory consumption and looping.
   As many bits as possible are matched in leaf (longest prefix match) . If does not match "match_bit_num" then IP is not in set 
   otherwise next bit decides what is next leaf and matching goes on.
   If there is not next leaf then algorithm is over (it's subnet address or special case
   particular IP address, in this case address is matched completly).
   
   Examples of ip tree {prefix_match_len, prefix_match, next}:

	0.0.0.0/0, i.e. all addresses
	{0, {}, {NULL, NULL}}

	128.0.0.0/1
	{1, {0x80}, {NULL, NULL}}   

	127.0.1.0/24
	{24, {0x7F, 0x00, 0x01}, {NULL, NULL}}

	127.0.1.0/24, 127.0.2.0/24
	01111111.00000000.00000001.00000000 01111111.00000000.00000010.00000000
	{22, {0x7F, 0x00, 0x00}, {
		{1, {0x80}, {NULL, NULL}},
		{1, {0x00}, {NULL, NULL}}
		}
	}
   
	127.0.0.0/32, 127.0.0.1/32
	{31, {0x7F, 0x00, 0x00, 0x00}, {NULL, NULL}}

	192.168.5.64/26, 192.168.5.15/32, 10.0.0.0/8
	11000000.10101000.00000101.01-000000 11000000.10101000.00000101.00001111 00001010-00000000.00000000.00000000

	{0, {}, {
		{7, {00010100}, {NULL, NULL}},
		{24, {10000001,01010000,00001010}, {
			{6, {00111100}, {NULL, NULL}},
			{0, {}, {NULL, NULL}}
		}}
	}}
				

 */

struct ip_tree_leaf {
	unsigned int prefix_match_len;  /* next prefix_match_len must be equal to next bit in IP address being compared */
	struct ip_tree_leaf *next[2];	 /* tree goes on in leaf based on first bit following prefix_match, if next[0] && next[1] are null then IP matches - it's subnet address */
	unsigned char prefix_match[0]; /* match_bits div 8 + 1, the same representation as ip address */
};

struct ip_tree_find {
	struct ip_tree_leaf *leaf;
	unsigned int leaf_prefix_match_len;
	unsigned char *leaf_prefix_match;
	unsigned char leaf_prefix_match_mask;
	unsigned char *ip;
	unsigned int ip_len;
	unsigned char ip_mask;
};

#define IP_TREE_FIND_NOT_FOUND 0
#define IP_TREE_FIND_FOUND 1
#define IP_TREE_FIND_FOUND_UPPER_SET 2

extern void ip_tree_init(struct ip_tree_leaf **tree);
extern void ip_tree_destroy(struct ip_tree_leaf **tree, int leaves_only, int use_shm);
extern int ip_tree_find_ip(struct ip_tree_leaf *tree, unsigned char *ip, unsigned int ip_len, struct ip_tree_find *h);
extern int ip_tree_add_ip(struct ip_tree_leaf **tree, unsigned char *ip, unsigned int ip_len, int use_shm);
extern void ip_tree_print(FILE *stream, struct ip_tree_leaf *tree, unsigned int indent);
extern str ip_tree_mask_to_str(unsigned char *pm, unsigned int len);
#endif
