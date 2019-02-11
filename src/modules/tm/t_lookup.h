/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */



#ifndef _T_LOOKUP_H
#define _T_LOOKUP_H

#include "defs.h"


#include "config.h"
#include "t_funcs.h"

#define T_UNDEFINED  ( (struct cell*) -1 )
#define T_NULL_CELL       ( (struct cell*) 0 )

#define T_BR_UNDEFINED (-1)

extern unsigned int     global_msg_id;



void init_t(void);
int init_rb( struct retr_buf *rb, struct sip_msg *msg );

typedef struct cell* (*tlookup_original_f)( struct sip_msg* p_msg );
struct cell* t_lookupOriginalT( struct sip_msg* p_msg );

int t_reply_matching( struct sip_msg* , int* );

typedef int (*tlookup_request_f)(struct sip_msg*, int, int*);

int t_lookup_request( struct sip_msg* p_msg, int leave_new_locked,
						int* canceled);
int t_newtran( struct sip_msg* p_msg );

int _add_branch_label( struct cell *trans,
    char *str, int *len, int branch );
int add_branch_label( struct cell *trans, 
	struct sip_msg *p_msg, int branch );

/* releases T-context */
int t_unref( struct sip_msg *p_msg);
typedef int (*tunref_f)( struct sip_msg *p_msg);

typedef int (*tcheck_f)(struct sip_msg*, int*);

/* old t_check version (no e2eack support) */
int t_check(struct sip_msg* , int *branch );
/* new version, e2eack and different return convention */
int t_check_msg(struct sip_msg* , int *branch );

typedef struct cell * (*tgett_f)(void);
struct cell *get_t(void);

typedef int (*tgett_branch_f)(void);
int get_t_branch(void);

/* use carefully or better not at all -- current transaction is 
 * primarily set by lookup functions */
typedef void (*tsett_f)(struct cell *t, int branch);
void set_t(struct cell *t, int branch);


#define T_GET_TI       "t_get_trans_ident"
#define T_LOOKUP_IDENT "t_lookup_ident"
#define T_IS_LOCAL     "t_is_local"

typedef int (*tislocal_f)(struct sip_msg*);
typedef int (*tnewtran_f)(struct sip_msg*);
typedef int (*tget_ti_f)(struct sip_msg*, unsigned int*, unsigned int*);
typedef int (*tlookup_ident_f)(struct cell**, unsigned int, unsigned int);
typedef int (*trelease_f)(struct sip_msg*);
typedef int (*tlookup_callid_f)(struct cell **, str, str);
typedef int (*tset_fr_f)(struct sip_msg*, unsigned int, unsigned int);

int t_is_local(struct sip_msg*);
int t_get_trans_ident(struct sip_msg* p_msg, unsigned int* hash_index, unsigned int* label);
int t_lookup_ident(struct cell** trans, unsigned int hash_index, unsigned int label);
/* lookup a transaction by callid and cseq */
int t_lookup_callid(struct cell** trans, str callid, str cseq);

int t_set_fr(struct sip_msg* msg, unsigned int fr_inv_to, unsigned int fr_to );
int t_reset_fr(void);
#ifdef TM_DIFF_RT_TIMEOUT
int t_set_retr(struct sip_msg* msg, unsigned int t1_to, unsigned int t2_to);
int t_reset_retr(void);
#endif
int t_set_max_lifetime(struct sip_msg* msg, unsigned int eol_inv,
											unsigned int eol_noninv);
int t_reset_max_lifetime(void);

#ifdef WITH_AS_SUPPORT
/**
 * Returns the hash coordinates of the transaction current CANCEL is targeting.
 */
int t_get_canceled_ident(struct sip_msg *msg, unsigned int *hash_index, 
		unsigned int *label);
typedef int (*t_get_canceled_ident_f)(struct sip_msg *msg, 
		unsigned int *hash_index, unsigned int *label);
#endif /* WITH_AS_SUPPORT */

/**
 * required by TMX (K/O extensions)
 */
#define WITH_TM_CTX
#ifdef WITH_TM_CTX

typedef struct _tm_ctx {
	int branch_index;
} tm_ctx_t;

typedef tm_ctx_t* (*tm_ctx_get_f)(void);

tm_ctx_t* tm_ctx_get(void);
void tm_ctx_init(void);
void tm_ctx_set_branch_index(int v);

#else

#define tm_ctx_get() NULL
#define tm_ctx_init()
#define tm_ctx_set_branch_index(v)

#endif /* WITH_TM_CTX */

#endif
