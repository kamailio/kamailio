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

#include "ip_addr.h"
#include "qvalue.h"
#include "flags.h"

struct sip_msg;

extern unsigned int nr_branches;


/* 
 * Add a new branch to current transaction 
 */
int append_branch(struct sip_msg* msg, char* uri, int uri_len, char* dst_uri, int dst_uri_len, 
		  qvalue_t q, struct socket_info* force_socket);


int km_append_branch(struct sip_msg* msg, str* uri, str* dst_uri, str* path,
					 qvalue_t q, unsigned int flags, struct socket_info* force_socket);


/* 
 * Iterate through the list of transaction branches 
 */
void init_branch_iterator(void);


/*
 * Get the next branch in the current transaction
 */
char* next_branch(int* len, qvalue_t* q, char** dst_uri, int* dst_len, struct socket_info** force_socket);


char* get_branch( unsigned int i, int* len, qvalue_t* q, str* dst_uri,
				  str* path, unsigned int *flags, struct socket_info** force_socket);


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

/**
 * Set a per-branch flag to 1.
 *
 * This function sets the value of one particular branch flag to 1.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param flag Number of the flag to be set (starting with 0)
 * @return 1 on success, -1 on failure.
 */
int setbflag(unsigned int branch, flag_t flag);

/**
 * Reset a per-branch flag value to 0.
 *
 * This function resets the value of one particular branch flag to 0.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param flag Number of the flag to be reset (starting with 0)
 * @return 1 on success, -1 on failure.
 */
int resetbflag(unsigned int branch, flag_t flag);

/**
 * Determine if a branch flag is set.
 *
 * This function tests the value of one particular per-branch flag.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param flag Number of the flag to be tested (starting with 0)
 * @return 1 if the branch flag is set, -1 if not or on failure.
 */
int isbflagset(unsigned int branch, flag_t flag);

/**
 * Get the value of all branch flags for a branch
 *
 * This function returns the value of all branch flags
 * combined in a single variable.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param res A pointer to a variable to store the result
 * @return 1 on success, -1 on failure
 */
int getbflagsval(unsigned int branch, flag_t* res);

/**
 * Set the value of all branch flags at once for a given branch.
 *
 * This function sets the value of all branch flags for a given
 * branch at once.
 * @param branch Number of the branch (0 for the main Request-URI branch)
 * @param val All branch flags combined into a single variable
 * @return 1 on success, -1 on failure
 */
int setbflagsval(unsigned int branch, flag_t val);

#endif /* _DSET_H */
