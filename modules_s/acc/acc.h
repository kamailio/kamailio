/*
 * $Id$
 */

int acc_request( struct sip_msg *rq, char* comment, char *foo);
void acc_missed_report( struct cell* t, struct sip_msg *reply,
	unsigned int code );
void acc_reply_report(  struct cell* t , struct sip_msg *reply,
	unsigned int code);
void acc_ack_report(  struct cell* t , struct sip_msg *ack );
