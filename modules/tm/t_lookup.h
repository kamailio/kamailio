/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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



#ifndef _T_LOOKUP_H
#define _T_LOOKUP_H

#include "config.h"
#include "t_funcs.h"

#define T_UNDEFINED  ( (struct cell*) -1 )
#define T_NULL       ( (struct cell*) 0 )

#ifdef _OBSOLETED
extern struct cell      *T;
#endif

extern unsigned int     global_msg_id;

void init_t();
int init_rb( struct retr_buf *rb, struct sip_msg *msg );
struct cell* t_lookupOriginalT( struct sip_msg* p_msg );
int t_reply_matching( struct sip_msg* , int* );
int t_lookup_request( struct sip_msg* p_msg , int leave_new_locked );
int t_newtran( struct sip_msg* p_msg );

int _add_branch_label( struct cell *trans,
    char *str, int *len, int branch );
int add_branch_label( struct cell *trans, 
	struct sip_msg *p_msg, int branch );

/* releases T-context */
int t_unref( struct sip_msg *p_msg);

/* function returns:
 *      -1 - transaction wasn't found
 *       1 - transaction found
 */
int t_check( struct sip_msg* , int *branch );

struct cell *get_t();

/* use carefully or better not at all -- current transaction is 
 * primarily set by lookup functions */
void set_t(struct cell *t);


#endif

