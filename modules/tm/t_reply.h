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



#ifndef _T_REPLY_H
#define _T_REPLY_H

#include "defs.h"


#include "h_table.h"

/* reply processing status */
enum rps {
	/* something bad happened */
	RPS_ERROR=0,	
	/* transaction completed but we still accept the reply */
	RPS_PUSHED_AFTER_COMPLETION,
	/* reply dscarded */
	RPS_DISCARDED,
	/* reply stored for later processing */
	RPS_STORE,
	/* transaction completed */
	RPS_COMPLETED,
	/* provisional reply not affecting transaction state */
	RPS_PROVISIONAL
};

enum route_mode { MODE_REQUEST=1, MODE_ONREPLY_REQUEST };
extern enum route_mode rmode;

/* branch bitmap type */
typedef unsigned int branch_bm_t;

/* reply export types */
typedef int (*treply_f)( struct sip_msg* p_msg,
	unsigned int code, char * text );
#ifdef VOICE_MAIL
typedef int (*treply_wb_f)( struct sip_msg* p_msg,
	unsigned int code, char * text, char * body, 
	char * new_header, char * to_tag);
#endif

#define LOCK_REPLIES(_t) lock(&(_t)->reply_mutex )
#define UNLOCK_REPLIES(_t) unlock(&(_t)->reply_mutex )

/* This function is called whenever a reply for our module is received;
 * we need to register this function on module initialization;
 * Returns :   0 - core router stops
 *             1 - core router relay statelessly
 */
int t_on_reply( struct sip_msg  *p_msg ) ;


/* Retransmits the last sent inbound reply.
 * Returns  -1 - error
 *           1 - OK
 */
int t_retransmit_reply( /* struct sip_msg * */  );


/* send a UAS reply
 * Warning: 'buf' and 'len' should already have been build.
 * returns 1 if everything was OK or -1 for erro
 */
#ifdef VOICE_MAIL

int t_reply_light( struct cell *trans, char* buf, unsigned int len,
		   unsigned int code, char * text,
		   char *to_tag, unsigned int to_tag_len);

int t_reply_with_body( struct sip_msg* p_msg, unsigned int code, 
		       char * text, char * body, char * new_header, char * to_tag );

#endif

/* send a UAS reply
 * returns 1 if everything was OK or -1 for erro
 */
int t_reply( struct cell *t, struct sip_msg * , unsigned int , char * );
/* the same as t_reply, except it does not claim
   REPLY_LOCK -- useful to be called within reply
   processing
*/
int t_reply_unsafe( struct cell *t, struct sip_msg * , unsigned int , char * );


enum rps relay_reply( struct cell *t, struct sip_msg *p_msg, int branch, 
	unsigned int msg_status, branch_bm_t *cancel_bitmap );

enum rps local_reply( struct cell *t, struct sip_msg *p_msg, int branch,
    unsigned int msg_status, branch_bm_t *cancel_bitmap );

void set_final_timer( /* struct s_table *h_table,*/ struct cell *t );

void cleanup_uac_timers( struct cell *t );

void on_negative_reply( struct cell* t, struct sip_msg* msg,
	int code, void *param  );

/* set which 'reply' structure to take if only negative
   replies arrive 
*/
int t_on_negative( unsigned int go_to );
unsigned int get_on_negative();

int t_retransmit_reply( struct cell *t );

void tm_init_tags();

#endif

