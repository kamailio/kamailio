/*
 * authenticate header
 */

/*#include "../../parser/msg_parser.h"
*/

int radius_log_reply(struct cell* t, struct sip_msg* msg);
int radius_log_ack(struct cell* t, struct sip_msg* msg);
int rad_acc_request( struct sip_msg *rq, char * comment, char  *foo);
void rad_acc_missed_report( struct cell* t, struct sip_msg *reply,
	unsigned int code );


