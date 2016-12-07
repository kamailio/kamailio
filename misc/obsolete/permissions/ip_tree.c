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

#include "ip_tree.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include <stdio.h>
#include <string.h>

void ip_tree_init(struct ip_tree_leaf **tree) {
	*tree = NULL;
}

void ip_tree_destroy(struct ip_tree_leaf **tree, int leaves_only, int use_shm) {
	int i;
	if (*tree) {
		for (i=0; i<=1; i++) {
			if ((*tree)->next[i])
				ip_tree_destroy(&(*tree)->next[i], 0, use_shm);
		}
		if (!leaves_only) {
			if (use_shm) 
			    shm_free(*tree);
			else
				pkg_free(*tree);
			*tree = NULL;
		}
	}
}

int ip_tree_find_ip(struct ip_tree_leaf *tree, unsigned char *ip, unsigned int ip_len, struct ip_tree_find *h) {
	
	h->leaf = tree;
		
	h->ip = ip;
	h->ip_len = ip_len;	
	h->ip_mask = 0x80;
	if (!tree) return IP_TREE_FIND_NOT_FOUND;
	
	do {
	
		h->leaf_prefix_match_mask = 0x80;
		h->leaf_prefix_match = &h->leaf->prefix_match[0];
		h->leaf_prefix_match_len = 0;
		if (h->ip_len == 0)
			return IP_TREE_FIND_FOUND_UPPER_SET;
		while (h->leaf_prefix_match_len < h->leaf->prefix_match_len) {
			if (((*(h->ip) & h->ip_mask) == 0) != ((*(h->leaf_prefix_match) & h->leaf_prefix_match_mask) == 0)) {
				return IP_TREE_FIND_NOT_FOUND;
			}
			h->leaf_prefix_match_len++;
			h->ip_len--;
			if (unlikely(h->ip_len == 0))
				return IP_TREE_FIND_FOUND_UPPER_SET;
			
			if (unlikely(h->ip_mask == 0x01)) {
				h->ip_mask = 0x80;
				h->ip++;
			}
			else {
				h->ip_mask /= 2; /* >> 1 */
			}
			if (unlikely(h->leaf_prefix_match_mask == 0x01)) {
				h->leaf_prefix_match_mask = 0x80;
				h->leaf_prefix_match++;
			}
			else {
				h->leaf_prefix_match_mask /= 2; /* >> 1 */
			}
		}

		h->leaf = h->leaf->next[(*(h->ip) & h->ip_mask) != 0];
		if (unlikely(h->ip_mask == 0x01)) {
			h->ip_mask = 0x80;
			h->ip++;
		}
		else {
			h->ip_mask /= 2; /* >> 1 */
		}
		h->ip_len--;
		
	} while (h->leaf);
	return IP_TREE_FIND_FOUND;
}

static inline struct ip_tree_leaf *ip_tree_malloc_leaf(unsigned int ip_len, int use_shm) {
	int n;
	n = sizeof(struct ip_tree_leaf)+((ip_len==0)?0:((ip_len-1)/8 +1));
	if (use_shm) 
		return shm_malloc(n);
	else
		return pkg_malloc(n);
}

int ip_tree_add_ip(struct ip_tree_leaf **tree, unsigned char *ip, unsigned int ip_len, int use_shm) {
	struct ip_tree_find h;
	struct ip_tree_leaf *l0, *l1;
	int ret, i, n;
	unsigned char mask, *pm;
	ret = ip_tree_find_ip(*tree, ip, ip_len, &h);
								
	switch (ret) {
		case IP_TREE_FIND_FOUND_UPPER_SET:
			/* ip covers wider subnet range than already defined range, we can delete all subleaves and reduce prefix match match */
			h.leaf->prefix_match_len = h.leaf_prefix_match_len;
			ip_tree_destroy(&h.leaf, 1, use_shm);
			break;
			
		case IP_TREE_FIND_FOUND:
			/* ip is already in set */
			break;
			
		case IP_TREE_FIND_NOT_FOUND:
			if (h.leaf) {
				/* split leaf into two leaves */
				n = h.ip_len - 1;
				l1 = ip_tree_malloc_leaf(n, use_shm);
				if (!l1) return -1;
				l1->prefix_match_len = n;
				for (i=0; i<=1; i++)
					l1->next[i] = NULL;				
				n = h.leaf->prefix_match_len - h.leaf_prefix_match_len - 1;
				l0 = ip_tree_malloc_leaf(n, use_shm);
				if (!l0) {
					ip_tree_destroy(&l1, 0, use_shm);
					return -1;
				}
				l0->prefix_match_len = n;
				for (i=0; i<=1; i++)
					l0->next[i] = h.leaf->next[i];
				i = (*h.leaf_prefix_match & h.leaf_prefix_match_mask) != 0;
				h.leaf->next[i] = l0;
				h.leaf->next[!i] = l1;
				
				n = h.leaf_prefix_match_len;
				
				/* copy remaining leaf prefix match bits, first non matched bit is treated in next decision therefore is skipped */
				mask = 0x80;
				pm = l0->prefix_match;
				while (1) {
					h.leaf_prefix_match_len++;
					if (h.leaf_prefix_match_len >= h.leaf->prefix_match_len)
						break;
					if (unlikely(h.leaf_prefix_match_mask == 0x01)) {
						h.leaf_prefix_match_mask = 0x80;
						h.leaf_prefix_match++;
					}
					else {
						h.leaf_prefix_match_mask /= 2; /* >> 1 */
					}
					if (mask == 0x80) 
						*pm = 0x00;
					if ((*h.leaf_prefix_match) & h.leaf_prefix_match_mask)
						*pm |= mask;
					if (mask == 0x01) {
						mask = 0x80;
						pm++;
					}
					else {
						mask /= 2;  /* >> 1 */
					}
				}
				h.leaf->prefix_match_len = n;
				
				/* copy remaining ip bits, first non matched bit is treated in next decision therefore is skipped */
                mask = 0x80;
                pm = l1->prefix_match;
				while (1) {
					h.ip_len--;
					if (h.ip_len <= 0)
						break;
					if (unlikely(h.ip_mask == 0x01)) {
						h.ip_mask = 0x80;
						h.ip++;
					}
					else {
						h.ip_mask /= 2; /* >> 1 */
					}
					if (mask == 0x80) 
						*pm = 0x00;
					if ((*h.ip) & h.ip_mask)
						*pm |= mask;
					if (mask == 0x01) {
						mask = 0x80;
						pm++;
					}
					else {
						mask /= 2;  /* >> 1 */
					}
				}
			}
			else {
				/* it's first leaf in tree */
				*tree = ip_tree_malloc_leaf(ip_len, use_shm);
				if (!*tree) return -1;
				(*tree)->prefix_match_len = ip_len;
				if (ip_len > 0) {
					for (i = 0; i <= (ip_len -1) / 8; i++) {
						(*tree)->prefix_match[i] = ip[i];
					}
				}
				for (i=0; i<=1; i++)
					(*tree)->next[i] = NULL;
			}
			break;
		default: /* BUG */			
			ret = -1;
	}
	return ret;
}

void ip_tree_print(FILE *stream, struct ip_tree_leaf *tree, unsigned int indent) {
	unsigned int i, j;

	if (!tree) {
		fprintf(stream, "nil\n"); 
	}
	else {
		str s;
		s = ip_tree_mask_to_str(tree->prefix_match, tree->prefix_match_len);
		fprintf(stream, "match %d bits {%.*s}\n", tree->prefix_match_len, s.len, s.s);
		for (j=0; j<=1; j++) {
			for (i=0; i<indent; i++) fprintf(stream, " ");
			fprintf(stream, "%d:", j);
			ip_tree_print(stream, tree->next[j], indent+2);
		}		
	}
}

str ip_tree_mask_to_str(unsigned char *pm, unsigned int len) {
	unsigned char mask;
	unsigned int i;
	static char buf[129];
	str s;
	
	s.s = buf;
	if (len>=sizeof(buf))
	    len = sizeof(buf)-1;
	s.len = len;
	buf[len] = '\0';
	for (i=0, mask=0x80; i<len; i++) {
		buf[i] = (*pm & mask)?'1':'0';
		if (mask == 0x01) {
			mask = 0x80;
			pm++;
		}
		else {
			mask /= 2;
		}
	}
	return s;
}
