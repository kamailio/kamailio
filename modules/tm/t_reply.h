/*
 * $Id$
 */


#ifndef _T_REPLY_H
#define _T_REPLY_H

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

/* branch bitmap type */
typedef unsigned int branch_bm_t;

/* reply export types */
typedef int (*treply_f)( struct sip_msg* p_msg,
	unsigned int code, char * text );

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
 * returns 1 if everything was OK or -1 for erro
 */
int t_reply( struct cell *t, struct sip_msg * , unsigned int , char * );
/* the same as t_reply, except it does not claim
   REPLY_LOCK -- useful to be called within reply
   processing
*/
int t_reply_unsafe( struct cell *t, struct sip_msg * , unsigned int , char * );


enum rps t_should_relay_response( struct cell *Trans, int new_code, 
	int branch, int *should_store, int *should_relay, 
	branch_bm_t *cancel_bitmap  );

void cleanup_after_final( struct s_table *h_table, struct cell *t,
	unsigned int status );

enum rps relay_reply( struct cell *t, struct sip_msg *p_msg, int branch, 
	unsigned int msg_status, branch_bm_t *cancel_bitmap );

enum rps local_reply( struct cell *t, struct sip_msg *p_msg, int branch,
    unsigned int msg_status, branch_bm_t *cancel_bitmap );

int store_reply( struct cell *trans, int branch, struct sip_msg *rpl);

void set_final_timer( /* struct s_table *h_table,*/ struct cell *t );

void cleanup_uac_timers( struct cell *t );

char *build_ack( struct sip_msg* rpl, struct cell *trans, int branch ,
	int *ret_len);

void on_negative_reply( struct cell* t, struct sip_msg* msg,
	int code, void *param  );

/* set which 'reply' structure to take if only negative
   replies arrive 
*/
int t_on_negative( unsigned int go_to );
unsigned int get_on_negative();

int t_retransmit_reply( struct cell *t );

#endif

