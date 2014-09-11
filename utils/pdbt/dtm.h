/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _DTM_H_
#define _DTM_H_




#include "common.h"




typedef int32_t dtm_node_index_t;




struct dtm_node_t {
	dtm_node_index_t child[10];
	carrier_t carrier;
} __attribute__ ((packed));




/*
 The PDB data in the given file is loaded into memory via mmap.
*/
struct dtm_node_t *dtm_load(char *filename);

/*
 Find the longest prefix match of number in mroot.
 Set *carrier according to value in dtree.
 Return the number of matched digits.
 In case no match is found, return -1.
*/
int dtm_longest_match(struct dtm_node_t *mroot, const char *number, int numberlen, carrier_t *carrier);




#endif
