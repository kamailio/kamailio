/*
 * $Id$
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
struct cell* t_lookupOriginalT(  struct s_table* hash_table,
	struct sip_msg* p_msg );
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


#endif

