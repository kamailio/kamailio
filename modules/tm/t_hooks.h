/*
 * $Id$
 */

#ifndef _HOOKS_H
#define _HOOKS_H

#include "h_table.h"
#include "t_funcs.h"

typedef enum { TMCB_REPLY,  TMCB_E2EACK, TMCB_REPLY_IN, 
	TMCB_REQUEST_OUT, TMCB_END } tmcb_type;

/* 
	TMCB_REPLY	-  a reply has been sent out
				  (no chance to change anything; still good enough
				  for reporting callbacks do not need to change anything
				  and better do not utilize TMCB_REPLY_IN, which would
				  resulting in the callback spending its time in
				  REPLY_LOCK unnecessarily)
	TMCB_E2EACK - presumably, an end2end ACK was received and
				  is about to be processed statelessly (note that
				  we cannot reliably determine that an e2e really
				  does belong to a transaction -- it is about guessing;
				  the reason is e2e ACKs are stand-alone transactions
				  which may have r-uri/via different from INVITE and
				  take a different path; however, with RR, chances are
				  good to match (though transaction matching won't work
				  with spirals -- as all transaction instances "match"
				  the request and only the first will be taken)
	TMCB_REPLY_IN - a reply was received and is about to be forwarded;
	TMCB_REQUEST_OUT - a request was received and is about to be fwd-ed
	TMCB_END	- just a bumper
*/

typedef void (transaction_cb) ( struct cell* t, struct sip_msg* msg );

struct tm_callback_s {
	int id;
	transaction_cb* callback;
	struct tm_callback_s* next;
};


extern struct tm_callback_s* callback_array[ TMCB_END ];

typedef int (*register_tmcb_f)(tmcb_type cbt, transaction_cb f);

int register_tmcb( tmcb_type cbt, transaction_cb f );
void callback_event( tmcb_type cbt, struct cell *trans,
	struct sip_msg *msg );

#endif
