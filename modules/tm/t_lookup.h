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
/*
 * History:
 * --------
 *  2003-02-24  s/T_NULL/T_NULL_CELL/ to avoid redefinition conflict w/
 *               nameser_compat.h (andrei)
 *  2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 */



#ifndef _T_LOOKUP_H
#define _T_LOOKUP_H

#include "defs.h"


#include "config.h"
#include "t_funcs.h"

#define T_UNDEFINED  ( (struct cell*) -1 )
#define T_NULL_CELL       ( (struct cell*) 0 )

extern unsigned int     global_msg_id;
extern int ruri_matching;
extern int via1_matching;

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

typedef struct cell * (*tgett_f)(void);
struct cell *get_t();

/* use carefully or better not at all -- current transaction is 
 * primarily set by lookup functions */
void set_t(struct cell *t);


#define T_GET_TI       "t_get_trans_ident"
#define T_LOOKUP_IDENT "t_lookup_ident"
#define T_IS_LOCAL     "t_is_local"

typedef int (*tislocal_f)(struct sip_msg*);
typedef int (*tnewtran_f)(struct sip_msg*);
typedef int (*tget_ti_f)(struct sip_msg*, unsigned int*, unsigned int*);
typedef int (*tlookup_ident_f)(struct cell**, unsigned int, unsigned int);

int t_is_local(struct sip_msg*);
int t_get_trans_ident(struct sip_msg* p_msg, unsigned int* hash_index, unsigned int* label);
int t_lookup_ident(struct cell** trans, unsigned int hash_index, unsigned int label);
/* lookup a transaction by callid and cseq */
int t_lookup_callid(struct cell** trans, str callid, str cseq);

#endif

