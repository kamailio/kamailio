/*
 * $Id$
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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

#ifndef _DSET_H
#define _DSET_H

#include "qvalue.h"

struct sip_msg;

/* 
 * Add a new branch to current transaction 
 */
int append_branch(struct sip_msg* msg, char* uri, int uri_len, char* dst_uri, int dst_uri_len, qvalue_t q);


/* 
 * Iterate through the list of transaction branches 
 */
void init_branch_iterator(void);


/*
 * Get the next branch in the current transaction
 */
char* next_branch(int* len, qvalue_t* q, char** dst_uri, int* dst_len);


/*
 * Empty the array of branches
 */
void clear_branches(void);


/*
 * Create a Contact header field from the
 * list of current branches
 */
char* print_dset(struct sip_msg* msg, int* len);


/* 
 * Set the q value of the Request-URI
 */
void set_ruri_q(qvalue_t q);


/* 
 * Get the q value of the Request-URI
 */
qvalue_t get_ruri_q(void);

int get_request_uri(struct sip_msg* _m, str* _u);
int rewrite_uri(struct sip_msg* _m, str* _s);


#endif /* _DSET_H */
