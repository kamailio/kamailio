/* trusted.h v 0.1 2003/10/18
 *
 * Header file for trusted.c implementing allow_trusted function
 *
 * Copyright (C) 2003 Juha Heinanen
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

#ifndef TRUSTED_H
#define TRUSTED_H
		
#include "../../parser/msg_parser.h"


extern struct trusted_list ***hash_table;     /* Pointer to current hash table pointer */
extern struct trusted_list **hash_table_1;   /* Pointer to hash table 1 */
extern struct trusted_list **hash_table_2;   /* Pointer to hash table 2 */


/*
 * Initialize data structures
 */
int init_trusted(void);


/*
 * Open database connections if necessary
 */
int init_child_trusted(int rank);


/*
 * Close connections and release memory
 */
void clean_trusted(void);


/*
 * Check if request comes from trusted ip address with matching from URI
 */
int allow_trusted(struct sip_msg* _msg, char* _s1, char* _s2);


#endif /* TRUSTED_H */
